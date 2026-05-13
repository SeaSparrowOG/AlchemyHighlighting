#pragma once

namespace RE::Offset
{
	namespace BarterMenu
	{
		constexpr auto RequestItemCardInfo = REL::ID(50949); // + 0x80 = Update Item Card
	}
	namespace CraftingSubmenu
	{
		constexpr auto SetSelectedItem = REL::ID(51420); // + 0x13 = Update Item Card
	}
	namespace ContainerMenu
	{
		constexpr auto RequestItemCardInfo = REL::ID(51130); // + 0xB2 = Update Item Card
	}
	namespace GiftMenu
	{
		constexpr auto RequestItemCardInfo = REL::ID(51569); // + 0x7A = Update Item Card
	}
	namespace InventoryMenu
	{
		constexpr auto RequestItemCardInfo = REL::ID(51852); // + 0x7A = Update Item Card
	}
	namespace ItemCard
	{
		constexpr auto ShowItemData = REL::ID(51897);
	}
}