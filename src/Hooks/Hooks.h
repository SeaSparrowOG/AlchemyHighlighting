#pragma once

namespace Hooks {
	bool Install();

	struct UpdateItemFocusHook
	{
		inline static bool Install();
		inline static void UpdateItemFocus(RE::CraftingSubMenus::AlchemyMenu* a_this, RE::GFxValue* a_data);
		inline static REL::Relocation<decltype(UpdateItemFocus)> _updateItemFocus;
	};

	struct RequestInventoryMenuItemCardHook
	{
		inline static bool InstallRequestInventoryMenuItemCardHook();
		inline static void InventoryShowItemData(RE::ItemCard* a_this, RE::InventoryEntryData* a_item, bool a_ignoreStolen);
		inline static REL::Relocation<decltype(InventoryShowItemData)> _inventoryShowItemData;
	};

	struct RequestContainerMenuItemCardHook
	{
		inline static bool InstallRequestContainerMenuItemCardHook();
		inline static void ContainerShowItemData(RE::ItemCard* a_this, RE::InventoryEntryData* a_item, bool a_ignoreStolen);
		inline static REL::Relocation<decltype(ContainerShowItemData)> _containerShowItemData;
	};

	struct RequestBarterMenuItemCardHook
	{
		inline static bool InstallRequestBarterMenuItemCardHook();
		inline static void BarterShowItemData(RE::ItemCard* a_this, RE::InventoryEntryData* a_item, bool a_ignoreStolen);
		inline static REL::Relocation<decltype(BarterShowItemData)> _barterShowItemData;
	};

	struct RequestGiftMenuItemCardHook
	{
		inline static bool InstallRequestGiftMenuItemCardHook();
		inline static void GiftShowItemData(RE::ItemCard* a_this, RE::InventoryEntryData* a_item, bool a_ignoreStolen);
		inline static REL::Relocation<decltype(GiftShowItemData)> _giftShowItemData;
	};
}