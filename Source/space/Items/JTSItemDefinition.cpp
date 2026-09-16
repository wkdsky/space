// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Items/JTSItemDefinition.h"

bool UJTSItemDefinition::HasCapability(EJTSItemCapability Capability) const
{
	return Capability != EJTSItemCapability::None
		&& (CapabilityMask & static_cast<int32>(Capability)) == static_cast<int32>(Capability);
}

bool UJTSItemDefinition::IsStackable() const
{
	// Raw expedition resources are deliberately physical one-unit pickups in the
	// current progression loop. This also protects existing data assets created
	// before the two-slot starting inventory rule was introduced.
	return PrimaryCategory != EJTSItemCategory::Resources
		&& HasCapability(EJTSItemCapability::StackableResource)
		&& MaxStackSize > 1;
}

bool UJTSItemDefinition::IsHoldable() const
{
	return HasCapability(EJTSItemCapability::Holdable);
}

bool UJTSItemDefinition::IsWearable() const
{
	return HasCapability(EJTSItemCapability::Wearable) && WearableSlot != EJTSWearableSlot::None;
}

bool UJTSItemDefinition::IsRangedWeapon() const
{
	return HasCapability(EJTSItemCapability::RangedWeapon);
}

bool UJTSItemDefinition::IsShopPurchasable() const
{
	return HasCapability(EJTSItemCapability::ShopPurchasable);
}

bool UJTSItemDefinition::MatchesShopCategory(EJTSShopCategory Category) const
{
	return Category == EJTSShopCategory::All || ShopCategories.Contains(Category);
}
