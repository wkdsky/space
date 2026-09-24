// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "space/Items/JTSItemTypes.h"

#include "JTSItemDefinition.generated.h"

class UMaterialInterface;
class USoundBase;

/**
 * Authorable item-local pivots for a held visual.  GripTransform's origin is the point the
 * character hand closes around; MuzzleTransform's +X axis points out of the barrel.
 *
 * The runtime component provides prototype values so existing item assets keep working.  Set
 * bOverridePrototypeProfile for a real mesh and tune these values in the item Data Asset.
 */
USTRUCT(BlueprintType)
struct SPACE_API FJTSHeldItemPresentation
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held Item")
	bool bOverridePrototypeProfile = false;

	/** Model-local pivot that is placed directly at the character's hand socket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held Item", meta = (EditCondition = "bOverridePrototypeProfile", EditConditionHides))
	FTransform GripTransform = FTransform::Identity;

	/** Model-local muzzle point and direction.  Its +X axis must point out of the barrel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held Item", meta = (EditCondition = "bOverridePrototypeProfile", EditConditionHides))
	FTransform MuzzleTransform = FTransform::Identity;

	/** Visible prototype grip dimensions, in world centimetres relative to the engine cube. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held Item", meta = (EditCondition = "bOverridePrototypeProfile", EditConditionHides))
	FVector GripScale = FVector(0.16f, 0.12f, 0.32f);
};

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

	/** Per-item grip and muzzle pivots.  Kept in the Data Asset so art can configure real meshes without changing gameplay code. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation|Held")
	FJTSHeldItemPresentation HeldPresentation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capabilities", meta = (Bitmask, BitmaskEnum = "/Script/space.EJTSItemCapability"))
	int32 CapabilityMask = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 MaxStackSize = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	float DefaultDurability = -1.0f;

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

	/** Camera field of view while aiming this ranged item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged", meta = (ClampMin = "30.0", ClampMax = "120.0"))
	float RangedAimFOV = 60.0f;

	/** Camera-ray spread in degrees. ADS keeps precision without changing server hit authority. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Feel", meta = (ClampMin = "0.0", ClampMax = "12.0"))
	float RangedHipSpreadDegrees = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Feel", meta = (ClampMin = "0.0", ClampMax = "12.0"))
	float RangedAimSpreadDegrees = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Feel", meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float RangedViewKickDegrees = 0.8f;

	/** Presentation assets are selected per item in the Data Asset, never by gameplay code. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Presentation")
	TSoftObjectPtr<UMaterialInterface> RangedGlowMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Presentation")
	TSoftObjectPtr<USoundBase> RangedFireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Presentation")
	TSoftObjectPtr<USoundBase> RangedImpactSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TArray<FJTSItemCost> ShopCosts;
};
