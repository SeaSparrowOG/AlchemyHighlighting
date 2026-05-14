#include "Hooks/hooks.h"

#include "RE/Offset.h"
#include "IngredientData/StoredData.h"
#include "Settings/INI/INISettings.h"

#undef GetObject

namespace Hooks {
	static bool IsEffectHarmful(RE::EffectSetting* effect) {
		using EffectFlag = RE::EffectSetting::EffectSettingData::Flag;
		auto* magAlchHostile = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("MagicAlchBeneficial"sv);
		if (!magAlchHostile) {
			return false;
		}
		return !effect->HasKeyword(magAlchHostile) || effect->data.flags.any(EffectFlag::kHostile);
	}

	static bool PlayerKnowsEffect(RE::IngredientItem* ingredient, std::uint16_t index) {
		auto& known = ingredient->gamedata.knownEffectFlags;
		return (known & (std::uint16_t(1) << index)) > 0; // I hate this, but the compiler hates the simpler form so tomatoh tomahto.
	}

	static bool PlayerHasNeededPerk() {
		bool requirePerk = Settings::INI::GetSetting<bool>(Settings::INI::PERK_REQUIRED.data()).value_or(false);
		auto perkFormRaw = Settings::INI::GetSetting<std::string>(Settings::INI::PERK_NEEDED.data()).value_or("Skyrim.esm|0x58218");

		if (!requirePerk) {
			return true;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* dh = RE::TESDataHandler::GetSingleton();
		if (!dh || !player) {
			return true;
		}

		auto parts = clib_util::string::split(perkFormRaw, "|");
		if (parts.size() != 2 ||
			!clib_util::string::is_only_hex(parts[1])) {
			return true;
		}

		if (!dh->LookupModByName(parts[0])) {
			return true;
		}

		auto perkID = clib_util::string::to_num<RE::FormID>(parts[1], true);
		auto* perk = dh->LookupForm<RE::BGSPerk>(perkID, parts[0]);
		if (!perk) {
			return true;
		}
		return player->HasPerk(perk);
	}

	static void HandleSimpleIndicator(RE::GFxValue& val, bool harmful, bool strong) {
		bool useAdditionalColors = Settings::INI::GetSetting<bool>(Settings::INI::SIMPLE_INDICATORS_COLORIZE.data()).value_or(false);
		//static bool appendAsSuperscript = Settings::INI::GetSetting<bool>(Settings::INI::SIMPLE_INDICATORS_SUPERSCRIPT.data()).value_or(false);
		auto harmfulStrongText = Settings::INI::GetSetting<std::string>(Settings::INI::SIMPLE_INDICATORS_HARMFUL_STRONG.data()).value_or("[++]");
		auto harmfulWeakText = Settings::INI::GetSetting<std::string>(Settings::INI::SIMPLE_INDICATORS_HARMFUL_WEAK.data()).value_or("[--]");
		auto beneficialStrongText = Settings::INI::GetSetting<std::string>(Settings::INI::SIMPLE_INDICATORS_BENEFICIAL_STRONG.data()).value_or("[++]");
		auto beneficialWeakText = Settings::INI::GetSetting<std::string>(Settings::INI::SIMPLE_INDICATORS_BENEFICIAL_WEAK.data()).value_or("[--]");
		long beneficialStrong = Settings::INI::GetSetting<long>(Settings::INI::COLOR_BENEFICIAL_STRONG.data()).value_or(0x00FF00);
		long beneficialWeak = Settings::INI::GetSetting<long>(Settings::INI::COLOR_BENEFICIAL_WEAK.data()).value_or(0xFF0000);
		long harmfulStrong = Settings::INI::GetSetting<long>(Settings::INI::COLOR_HARMFUL_STRONG.data()).value_or(0x00FF00);
		long harmfulWeak = Settings::INI::GetSetting<long>(Settings::INI::COLOR_HARMFUL_WEAK.data()).value_or(0xFF0000);

		RE::GFxValue htmlText;
		RE::GFxValue text;
		if (!val.GetMember("htmlText", &htmlText)) {
			return;
		}
		if (!val.GetMember("text", &text)) {
			return;
		}

		std::string raw = htmlText.GetString();
		std::string effectName = text.GetString();

		long colorRaw = 0xFF0000;
		std::string notation;
		if (harmful && strong) {
			notation = harmfulStrongText;
			colorRaw = harmfulStrong;
		}
		else if (harmful && !strong) {
			notation = harmfulWeakText;
			colorRaw = harmfulWeak;
		}
		else if (!harmful && strong) {
			notation = beneficialStrongText;
			colorRaw = beneficialStrong;
		}
		else {
			notation = beneficialWeakText;
			colorRaw = beneficialWeak;
		}

		//if (appendAsSuperscript) {
		//	notation = "<span baselineShift='superscript'>" + notation + "</span>";
		//}
		if (useAdditionalColors) {
			notation = fmt::format("<font color='#{:06X}'>{}</font>", colorRaw, notation);
		}

		clib_util::string::replace_first_instance(raw, effectName, fmt::format("{}{}", effectName, notation));
		htmlText.SetString(raw);
		val.SetMember("htmlText", htmlText);
	}

	static void ProcessIngredientIfNeeded(RE::GFxValue& a_itemInfo, 
		RE::IngredientItem* a_ingredient) 
	{
		long beneficialStrong = Settings::INI::GetSetting<long>(Settings::INI::COLOR_BENEFICIAL_STRONG.data()).value_or(0x00FF00);
		long beneficialWeak = Settings::INI::GetSetting<long>(Settings::INI::COLOR_BENEFICIAL_WEAK.data()).value_or(0xFF0000);
		long harmfulStrong = Settings::INI::GetSetting<long>(Settings::INI::COLOR_HARMFUL_STRONG.data()).value_or(0x00FF00);
		long harmfulWeak = Settings::INI::GetSetting<long>(Settings::INI::COLOR_HARMFUL_WEAK.data()).value_or(0xFF0000);
		bool allowUnknown = Settings::INI::GetSetting<bool>(Settings::INI::ENABLE_UNKNOWN.data()).value_or(false);
		bool useSimpleIndicators = Settings::INI::GetSetting<bool>(Settings::INI::SIMPLE_INDICATORS.data()).value_or(false);
		bool onlyWeak = Settings::INI::GetSetting<bool>(Settings::INI::ONLY_NEGATIVES.data()).value_or(false);
		bool onlyStrong = Settings::INI::GetSetting<bool>(Settings::INI::ONLY_POSITIVES.data()).value_or(false);
		
		if (!PlayerHasNeededPerk()) {
			return;
		}

		auto& alciEffects = a_ingredient->effects;
		if (alciEffects.empty() || alciEffects.size() != 4) {
			LOG_DEBUG("Size mismatch: {}"sv, alciEffects.size());
			return;
		}

		using IndexType = int;
		RE::GFxValue effectLabel;
		for (IndexType i = 0; i < 4; ++i) {
			if (!allowUnknown && !PlayerKnowsEffect(a_ingredient, static_cast<std::uint16_t>(i))) {
				continue;
			}

			auto* currentEffect = alciEffects[i];
			auto* currentBaseEffect = currentEffect ? currentEffect->baseEffect : nullptr;
			if (!currentBaseEffect) {
				LOG_DEBUG("No base effect."sv);
				continue;
			}
			const float mag = currentEffect->GetMagnitude();
			const float baseMag = IngredientData::GetAverageEffectMagnitude(currentBaseEffect);
			if (mag == baseMag) { // Potentially could backfire due to float inaccuracy, but works for vanilla.
				continue;
			}

			int color = 0xFFFFFF;
			bool harmful = IsEffectHarmful(currentBaseEffect);
			if ((onlyWeak && !harmful) || (onlyStrong && harmful)) {
				continue;
			}

			std::string labelName = "EffectLabel" + std::to_string(i);
			if (!a_itemInfo.GetMember(labelName.c_str(), &effectLabel)) {
				LOG_DEBUG("Couldn't get {}"sv, labelName);
				continue;
			}

			if (useSimpleIndicators) {
				HandleSimpleIndicator(effectLabel, harmful, mag > baseMag);
				continue;
			}

			if (harmful && mag > baseMag) {
				color = harmfulStrong;
			}
			else if (harmful && mag < baseMag) {
				color = harmfulWeak;
			}
			else if (!harmful && mag > baseMag) {
				color = beneficialStrong;
			}
			else if (!harmful && mag < baseMag) {
				color = beneficialWeak;
			}
			else {
				LOG_DEBUG("Effect is standard."sv);
				continue;
			}

			if (!effectLabel.SetMember("textColor", color)) {
				LOG_DEBUG("Couldn't set color."sv);
			}
		}
	}

	static void LoadItemInfoPathAndModify(std::string_view menuName, RE::FormID ingredientID) {
		auto* ingr = RE::TESForm::LookupByID<RE::IngredientItem>(ingredientID);
		if (!ingr) {
			LOG_DEBUG("Invalid ingredient."sv);
			return;
		}
		auto* ui = RE::UI::GetSingleton();
		if (!ui || !ui->IsMenuOpen(menuName)) {
			LOG_DEBUG("{} is closed."sv, menuName);
			return;
		}
		
		RE::GFxValue* root = nullptr;
		if (menuName == RE::InventoryMenu::MENU_NAME) {
			auto menuPtr = ui->GetMenu<RE::InventoryMenu>();
			root = menuPtr ? &menuPtr->root : nullptr;
		}
		else if (menuName == RE::ContainerMenu::MENU_NAME) {
			auto menuPtr = ui->GetMenu<RE::ContainerMenu>();
			root = menuPtr ? &menuPtr->root : nullptr;
		}
		else if (menuName == RE::BarterMenu::MENU_NAME) {
			auto menuPtr = ui->GetMenu<RE::BarterMenu>();
			root = menuPtr ? &menuPtr->root : nullptr;
		}
		else if (menuName == RE::GiftMenu::MENU_NAME) {
			auto menuPtr = ui->GetMenu<RE::GiftMenu>();
			root = menuPtr ? &menuPtr->root : nullptr;
		}
		else {
			return;
		}

		if (!root || root->IsUndefined() || root->IsNull() || !root->IsDisplayObject()) {
			LOG_DEBUG("Bad root."sv);
			return;
		}

		RE::GFxValue ItemCard_mc;
		RE::GFxValue itemInfo;

		bool useSkyUIPaths = false;
		auto* dh = RE::TESDataHandler::GetSingleton();
		if (dh && dh->LookupModByName("SkyUI_SE.esp")) {
			useSkyUIPaths = true;
		}

		if (useSkyUIPaths) {
			if (!root->GetMember("itemCard", &ItemCard_mc)) {
				LOG_DEBUG("Failed to get itemCard");
				return;
			}
			if (!ItemCard_mc.GetMember("itemInfo", &itemInfo)) {
				LOG_DEBUG("Failed to get itemInfo");
				return;
			}
		}
		else {
			if (!root->GetMember("ItemCard_mc", &ItemCard_mc)) {
				LOG_DEBUG("Failed to get ItemCard_mc");
				return;
			}
			if (!ItemCard_mc.GetMember("itemInfo", &itemInfo)) {
				LOG_DEBUG("Failed to get itemInfo");
				return;
			}
		}
		ProcessIngredientIfNeeded(ItemCard_mc, ingr);
	}

	bool Install() {
		SECTION_SEPARATOR;
		logger::info("Installing hooks..."sv);
		std::size_t allocSize = 0u;
		if (Settings::INI::ShouldInstallHook(Settings::INI::ENABLED_MENUS_ALCHEMY.data())) {
			allocSize += 14u; // 1 CALL
		}
		if (Settings::INI::ShouldInstallHook(Settings::INI::ENABLED_MENUS_INVENTORY.data())) {
			allocSize += 14u; // 1 CALL
		}
		if (Settings::INI::ShouldInstallHook(Settings::INI::ENABLED_MENUS_BARTER.data())) {
			allocSize += 14u; // 1 CALL
		}
		if (Settings::INI::ShouldInstallHook(Settings::INI::ENABLED_MENUS_CONTAINER.data())) {
			allocSize += 14u; // 1 CALL
		}
		if (Settings::INI::ShouldInstallHook(Settings::INI::ENABLED_MENUS_GIFT.data())) {
			allocSize += 14u; // 1 CALL
		}

		SKSE::AllocTrampoline(allocSize);

		bool success = true;
		success &= UpdateItemFocusHook::Install();
		success &= RequestInventoryMenuItemCardHook::InstallRequestInventoryMenuItemCardHook();
		success &= RequestContainerMenuItemCardHook::InstallRequestContainerMenuItemCardHook();
		success &= RequestBarterMenuItemCardHook::InstallRequestBarterMenuItemCardHook();
		success &= RequestGiftMenuItemCardHook::InstallRequestGiftMenuItemCardHook();
		return success;
	}

	inline bool UpdateItemFocusHook::Install() {
		logger::info("  >Installing Alchemy Menu hook..."sv);
		if (!Settings::INI::ShouldInstallHook(Settings::INI::ENABLED_MENUS_ALCHEMY.data())) {
			logger::info("    - User chose not to install Alchemy Menu hook."sv);
			return true;
		}

		REL::Relocation<std::uintptr_t> target{ RE::Offset::CraftingSubmenu::SetSelectedItem, 0x13 };
		if (!REL::make_pattern<"E9">().match(target.address())) {
			logger::info("    Failed to validate pattern. Aborting load."sv);
			return false;
		}
		auto& trampoline = SKSE::GetTrampoline();
		_updateItemFocus = trampoline.write_branch<5>(target.address(), &UpdateItemFocus);
		return true;
	}

	inline void UpdateItemFocusHook::UpdateItemFocus(RE::CraftingSubMenus::AlchemyMenu* a_this, RE::GFxValue* a_data)
	{
		_updateItemFocus(a_this, a_data);
		if (!a_this) {
			return;
		}

		auto& itemInfo = a_this->itemInfo;
		auto& selected = a_this->selectedIndexes;
		auto numSelected = selected.size();
		if (numSelected > 1) {
			LOG_DEBUG("Nothing selected."sv);
			return;
		}

		auto index = a_this->currentIngredientIdx;
		if (index < 0 || index >= a_this->ingredientEntries.size()) {
			LOG_DEBUG("Invalid selection - out of bounds ({})."sv, index);
			return;
		}

		auto& craftEntry = a_this->ingredientEntries[index];
		auto* entryData = craftEntry.ingredient;
		auto* baseObj = entryData ? entryData->GetObject() : nullptr;
		auto* ingrObj = baseObj ? baseObj->As<RE::IngredientItem>() : nullptr;
		if (!ingrObj) {
			LOG_DEBUG("Couldn't cast to ALCI."sv);
			return;
		}

		ProcessIngredientIfNeeded(itemInfo, ingrObj);
	}

	inline bool RequestInventoryMenuItemCardHook::InstallRequestInventoryMenuItemCardHook()
	{
		logger::info("  >Installing Inventory Menu hook..."sv);
		if (!Settings::INI::ShouldInstallHook(Settings::INI::ENABLED_MENUS_INVENTORY.data())) {
			logger::info("    - User chose not to install Inventory Meny hook."sv);
			return true;
		}

		REL::Relocation<std::uintptr_t> target{ RE::Offset::InventoryMenu::RequestItemCardInfo, 0x7A };
		if (!REL::make_pattern<"E8">().match(target.address())) {
			logger::info("    Failed to validate pattern. Aborting load."sv);
			return false;
		}

		auto& trampoline = SKSE::GetTrampoline();
		_inventoryShowItemData = trampoline.write_call<5>(target.address(), &InventoryShowItemData);
		return true;
	}

	inline void RequestInventoryMenuItemCardHook::InventoryShowItemData(RE::ItemCard* a_this, 
		RE::InventoryEntryData* a_item, 
		bool a_ignoreStolen)
	{
		_inventoryShowItemData(a_this, a_item, a_ignoreStolen);

		auto* baseObj = a_item ? a_item->GetObject() : nullptr;
		auto* ingrObj = baseObj ? baseObj->As<RE::IngredientItem>() : nullptr;
		auto ingrID = ingrObj ? ingrObj->GetFormID() : 0;

		// Potential Vanilla Path: ItemCardFadeHolder_mc.ItemCard_mc.itemInfo.Entry[0-4].textField.text
		SKSE::GetTaskInterface()->AddUITask([id = ingrID]() {
			LoadItemInfoPathAndModify(RE::InventoryMenu::MENU_NAME, id);
		});
	}

	inline bool RequestContainerMenuItemCardHook::InstallRequestContainerMenuItemCardHook()
	{
		logger::info("  >Installing Container Menu hook..."sv);
		if (!Settings::INI::ShouldInstallHook(Settings::INI::ENABLED_MENUS_CONTAINER.data())) {
			logger::info("    - User chose not to install Container Meny hook."sv);
			return true;
		}

		REL::Relocation<std::uintptr_t> target{ RE::Offset::ContainerMenu::RequestItemCardInfo, 0xB2 };
		if (!REL::make_pattern<"E8">().match(target.address())) {
			logger::info("    Failed to validate pattern. Aborting load."sv);
			return false;
		}

		auto& trampoline = SKSE::GetTrampoline();
		_containerShowItemData = trampoline.write_call<5>(target.address(), &ContainerShowItemData);
		return true;
	}

	inline void RequestContainerMenuItemCardHook::ContainerShowItemData(RE::ItemCard* a_this, 
		RE::InventoryEntryData* a_item, 
		bool a_ignoreStolen)
	{
		_containerShowItemData(a_this, a_item, a_ignoreStolen);

		auto* baseObj = a_item ? a_item->GetObject() : nullptr;
		auto* ingrObj = baseObj ? baseObj->As<RE::IngredientItem>() : nullptr;
		auto ingrID = ingrObj ? ingrObj->GetFormID() : 0;

		SKSE::GetTaskInterface()->AddUITask([id = ingrID]() {
			LoadItemInfoPathAndModify(RE::ContainerMenu::MENU_NAME, id);
			});
	}

	inline bool RequestBarterMenuItemCardHook::InstallRequestBarterMenuItemCardHook()
	{
		logger::info("  >Installing Barter Menu hook..."sv);
		if (!Settings::INI::ShouldInstallHook(Settings::INI::ENABLED_MENUS_BARTER.data())) {
			logger::info("    - User chose not to install Barter Meny hook."sv);
			return true;
		}

		REL::Relocation<std::uintptr_t> target{ RE::Offset::BarterMenu::RequestItemCardInfo, 0x80 };
		if (!REL::make_pattern<"E8">().match(target.address())) {
			logger::info("    Failed to validate pattern. Aborting load."sv);
			return false;
		}

		auto& trampoline = SKSE::GetTrampoline();
		_barterShowItemData = trampoline.write_call<5>(target.address(), &BarterShowItemData);
		return true;
	}

	inline void RequestBarterMenuItemCardHook::BarterShowItemData(RE::ItemCard* a_this,
		RE::InventoryEntryData* a_item, 
		bool a_ignoreStolen)
	{
		_barterShowItemData(a_this, a_item, a_ignoreStolen);

		auto* baseObj = a_item ? a_item->GetObject() : nullptr;
		auto* ingrObj = baseObj ? baseObj->As<RE::IngredientItem>() : nullptr;
		auto ingrID = ingrObj ? ingrObj->GetFormID() : 0;

		SKSE::GetTaskInterface()->AddUITask([id = ingrID]() {
			LoadItemInfoPathAndModify(RE::BarterMenu::MENU_NAME, id);
			});
	}

	inline bool RequestGiftMenuItemCardHook::InstallRequestGiftMenuItemCardHook()
	{
		logger::info("  >Installing Gift Menu hook..."sv);
		if (!Settings::INI::ShouldInstallHook(Settings::INI::ENABLED_MENUS_BARTER.data())) {
			logger::info("    - User chose not to install Gift Meny hook."sv);
			return true;
		}

		REL::Relocation<std::uintptr_t> target{ RE::Offset::GiftMenu::RequestItemCardInfo, 0x7A };
		if (!REL::make_pattern<"E8">().match(target.address())) {
			logger::info("    Failed to validate pattern. Aborting load."sv);
			return false;
		}

		auto& trampoline = SKSE::GetTrampoline();
		_giftShowItemData = trampoline.write_call<5>(target.address(), &GiftShowItemData);
		return true;
	}

	inline void RequestGiftMenuItemCardHook::GiftShowItemData(RE::ItemCard* a_this, 
		RE::InventoryEntryData* a_item, 
		bool a_ignoreStolen)
	{
		_giftShowItemData(a_this, a_item, a_ignoreStolen);

		auto* baseObj = a_item ? a_item->GetObject() : nullptr;
		auto* ingrObj = baseObj ? baseObj->As<RE::IngredientItem>() : nullptr;
		auto ingrID = ingrObj ? ingrObj->GetFormID() : 0;

		SKSE::GetTaskInterface()->AddUITask([id = ingrID]() {
			LoadItemInfoPathAndModify(RE::GiftMenu::MENU_NAME, id);
			});
	}
}