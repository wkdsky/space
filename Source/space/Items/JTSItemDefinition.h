// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "space/Items/JTSItemTypes.h"
#include "space/Interaction/JTSMeleeTarget.h"

#include "JTSItemDefinition.generated.h"

class UMaterialInterface;
class UStaticMesh;
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

	/**
	 * Primitive-built silhouette. Body, barrel and sight are cubes sized in world centimetres relative to the
	 * engine cube and placed in item-local space; a zero sight scale hides the sight. Real meshes replace this later.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held Item", meta = (EditCondition = "bOverridePrototypeProfile", EditConditionHides))
	FVector BodyScale = FVector(0.48f, 0.18f, 0.14f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held Item", meta = (EditCondition = "bOverridePrototypeProfile", EditConditionHides))
	FVector BodyLocation = FVector(13.0f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held Item", meta = (EditCondition = "bOverridePrototypeProfile", EditConditionHides))
	FVector BarrelScale = FVector(0.26f, 0.08f, 0.08f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held Item", meta = (EditCondition = "bOverridePrototypeProfile", EditConditionHides))
	FVector BarrelLocation = FVector(48.0f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held Item", meta = (EditCondition = "bOverridePrototypeProfile", EditConditionHides))
	FVector SightScale = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held Item", meta = (EditCondition = "bOverridePrototypeProfile", EditConditionHides))
	FVector SightLocation = FVector(19.0f, 0.0f, 12.0f);

	/** Holds a second mirrored copy in the off hand (dual pistols). Ranged shots then alternate muzzles. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held Item")
	bool bDualWield = false;
};

/** Per-weapon ranged behavior that goes beyond one hitscan ray. A zero field means the feature is off. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSRangedMechanics
{
	GENERATED_BODY()

	/** Rounds per magazine; zero means the weapon has no magazine (heat weapons). Ammo itself is infinite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "0"))
	int32 MagazineSize = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "0.1"))
	float ReloadSeconds = 1.6f;

	/** Rays per shot. The shot's total damage is split between them. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shot", meta = (ClampMin = "1"))
	int32 Pellets = 1;

	/** Damage is full inside FalloffStart and fades to MinFalloffFraction at FalloffEnd (centimetres). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shot", meta = (ClampMin = "0.0"))
	float FalloffStartCm = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shot", meta = (ClampMin = "0.0"))
	float FalloffEndCm = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinFalloffFraction = 1.0f;

	/** Extra damageable targets a ray passes through; each pass keeps 75 percent of the damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shot", meta = (ClampMin = "0"))
	int32 PierceCount = 0;

	/** Arc chain: after a hit, jump to up to ChainTargets more nearby targets, losing ChainFalloff per jump. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chain", meta = (ClampMin = "0"))
	int32 ChainTargets = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chain", meta = (ClampMin = "0.0"))
	float ChainRangeCm = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chain", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float ChainFalloff = 0.3f;

	/** Energy weapons spend EnergyPerShot per shot and trickle back RechargePerSecond. Once the bar runs dry the weapon
	 * stays locked until it recharges past ResumeFraction (the red zone). Zero capacity means a magazine weapon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Energy", meta = (ClampMin = "0.0"))
	float EnergyCapacity = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Energy", meta = (ClampMin = "0.0"))
	float EnergyPerShot = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Energy", meta = (ClampMin = "0.0"))
	float RechargePerSecond = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Energy", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ResumeFraction = 0.3f;

	/** A positive speed fires a physical explosive projectile instead of a ray (centimetres per second). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	float ProjectileGravityScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	float BlastRadiusCm = 0.0f;

	/** Fraction of the direct damage that every skill-granted fragment budget shares. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FragmentDamageShare = 0.4f;
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

	/** Optional authored world pickup; unset items retain their prototype visual. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UStaticMesh> WorldPickupMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "1.0"))
	float WorldPickupWidth = 32.0f;

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

	/** What a swing of this item counts as for targets that react to the weapon kind (for example ant nests). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	EJTSMeleeAttackType MeleeAttackClass = EJTSMeleeAttackType::Improvised;

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

	/** Scales the shared per-level upgrade cost curve (Rock, Ore, then Organic from level five) for this weapon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "0.1"))
	float UpgradeCostMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged")
	FJTSRangedMechanics RangedMechanics;
};
