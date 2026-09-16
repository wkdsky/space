// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "space/Items/JTSItemTypes.h"

#include "JTSItemDefinition.generated.h"

/**
 * Authorable definition for one item kind.  Runtime inventory stores FJTSItemInstance, never this
 * asset's mutable state, so one definition can safely serve every player and world pickup.
 */
UCLASS(BlueprintType)
class SPACE_API UJTSItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Item")
	bool HasCapability(EJTSItemCapability Capability) const;

	UFUNCTION(BlueprintPure, Category = "Item")
	bool IsStackable() const;

	UFUNCTION(BlueprintPure, Category = "Item")
	bool IsHoldable() const;

	UFUNCTION(BlueprintPure, Category = "Item")
	bool IsWearable() const;

	UFUNCTION(BlueprintPure, Category = "Item")
	bool IsRangedWeapon() const;

	UFUNCTION(BlueprintPure, Category = "Item")
	bool IsShopPurchasable() const;

	UFUNCTION(BlueprintPure, Category = "Item")
	bool MatchesShopCategory(EJTSShopCategory Category) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	EJTSItemId ItemId = EJTSItemId::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	EJTSItemCategory PrimaryCategory = EJTSItemCategory::Utility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TArray<EJTSShopCategory> ShopCategories;

	/** Short purpose tags shown on shop cards and used for future recommendation/sorting work. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TArray<FName> AffinityTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	FLinearColor AccentColor = FLinearColor(0.38f, 0.70f, 0.95f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capabilities", meta = (Bitmask, BitmaskEnum = "/Script/space.EJTSItemCapability"))
	int32 CapabilityMask = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 MaxStackSize = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	float DefaultDurability = -1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wearable")
	EJTSWearableSlot WearableSlot = EJTSWearableSlot::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wearable", meta = (ClampMin = "0"))
	int32 InventoryCapacityBonus = 0;

	/** Combat damage is intentionally independent from MiningWork. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float CombatDamage = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.05"))
	float MeleeAttackInterval = 0.45f;

	/** Work applied to a mining node by one valid hit. Zero means it cannot mine. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining", meta = (ClampMin = "0.0"))
	float MiningWork = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged", meta = (ClampMin = "0.0"))
	float RangedDamage = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged", meta = (ClampMin = "0.05"))
	float RangedFireInterval = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged", meta = (ClampMin = "100.0"))
	float RangedRange = 8000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged")
	bool bAutomaticFire = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TArray<FJTSItemCost> ShopCosts;
};
