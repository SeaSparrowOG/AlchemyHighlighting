#pragma once

namespace Hooks {
	bool Install();

	struct UpdateItemFocusHook
	{
		inline static bool Install();
		inline static void UpdateItemFocus(RE::CraftingSubMenus::AlchemyMenu* a_this, RE::GFxValue* a_data);
		inline static REL::Relocation<decltype(UpdateItemFocus)> _updateItemFocus;
	};
}