// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Items/JTSItemDefinition.h"

bool UJTSItemDefinition::HasCapability(EJTSItemCapability Capability) const
{
	return Capability != EJTSItemCapability::None
		&& (CapabilityMask & static_cast<int32>(Capability)) == static_cast<int32>(Capability);
}

bool UJTSItemDefinition::IsStackable() const
{
	// The inventory applies the fixed per-slot cap to resource types; equipment stays unique.
	return PrimaryCategory == EJTSItemCategory::Resources
		|| HasCapability(EJTSItemCapability::StackableResource);
}

bool UJTSItemDefinition::IsHoldable() const
{
	return HasCapability(EJTSItemCapability::Holdable);
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
