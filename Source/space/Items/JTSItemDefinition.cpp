// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Items/JTSItemDefinition.h"

bool UJTSItemDefinition::HasCapability(EJTSItemCapability Capability) const
{
	return Capability != EJTSItemCapability::None
		&& (CapabilityMask & static_cast<int32>(Capability)) == static_cast<int32>(Capability);
}

bool UJTSItemDefinition::IsStackable() const
{
	// A stackable type can still have an effective limit of one at rank zero. The inventory's
	// PlayerState-driven stack rule owns that limit, so a later ability upgrade immediately applies
	// to existing resource definitions without requiring asset edits.
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
