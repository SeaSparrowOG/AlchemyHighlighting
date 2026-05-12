#include "Hooks/hooks.h"

#include "RE/Offset.h"
#include "IngredientData/StoredData.h"

#undef GetObject

namespace Hooks {
	bool Install() {
		SECTION_SEPARATOR;
		logger::info("Installing hooks..."sv);
		constexpr std::size_t allocSize = 1 * 14u;
		SKSE::AllocTrampoline(allocSize);

		bool success = UpdateItemFocusHook::Install();
		return success;
	}

	inline bool UpdateItemFocusHook::Install() {
		logger::info("  >Installing Set Ingredient Item Card Data Hook..."sv);
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
		auto* alciObj = baseObj ? baseObj->As<RE::IngredientItem>() : nullptr;
		if (!alciObj) {
			LOG_DEBUG("Couldn't cast to ALCI."sv);
			return;
		}

		auto& alciEffects = alciObj->effects;
		if (alciEffects.empty() || alciEffects.size() != 4) {
			LOG_DEBUG("Size mismatch: {}"sv, alciEffects.size());
			return;
		}

		using IndexType = int;
		RE::GFxValue effectLabel;
		for (IndexType i = 0; i < 4; ++i) {
			auto* currentEffect = alciEffects[i];
			auto* currentBaseEffect = currentEffect ? currentEffect->baseEffect : nullptr;
			if (!currentBaseEffect) {
				LOG_DEBUG("No base effect."sv);
				continue;
			}

			const float mag = currentEffect->GetMagnitude();
			const float baseMag = IngredientData::GetAverageEffectMagnitude(currentBaseEffect);

			int color = 0xFFFFFF;
			if (mag < baseMag) {
				color = 0xFF0000;
			}
			else if (mag > baseMag) {
				color = 0x00FF00;
			}
			else {
				LOG_DEBUG("Effect is standard."sv);
				continue;
			}

			std::string labelName = "EffectLabel" + std::to_string(i);
			if (!itemInfo.GetMember(labelName.c_str(), &effectLabel)) {
				LOG_DEBUG("Couldn't get {}"sv, labelName);
				continue;
			}
			if (!effectLabel.SetMember("textColor", color)) {
				LOG_DEBUG("Couldn't set color."sv);
			}
		}
	}
}