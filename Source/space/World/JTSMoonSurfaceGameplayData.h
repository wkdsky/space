// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "space/World/JTSMoonSurfaceGameplaySettings.h"

#include "JTSMoonSurfaceGameplayData.generated.h"

class AJTSMoonAntActor;
class AJTSMoonAntNestActor;

/**
 * Blueprint-configured Moon content and balance data for a real SpaceWorld planet surface.
 *
 * This owns asset/class selection and gameplay values. It deliberately owns no level Actor
 * references; Moon Controller and Level Blueprints provide those instances at runtime.
 */
UCLASS(BlueprintType)
class SPACE_API UJTSMoonSurfaceGameplayData : public UPrimaryDataAsset, public IJTSMoonSurfaceGameplaySettings
{
	GENERATED_BODY()

public:
	virtual int32 GetCrewCount() const override;
	virtual float GetFoodConsumptionPerPersonPerMinute() const override;
	virtual float GetWaterConsumptionPerPersonPerMinute() const override;
	virtual float GetConsumptionTickInterval() const override;
	virtual float GetMinimumConsumptionUnit() const override;

	virtual int32 GetPickaxeRockCost() const override;
	virtual int32 GetBackpackRockCost() const override;
	virtual int32 GetBackpackOreCost() const override;
	virtual int32 GetKnifeRockCost() const override;
	virtual int32 GetKnifeOreCost() const override;
	virtual int32 GetAxeRockCost() const override;
	virtual int32 GetAxeOreCost() const override;

	virtual int32 GetLargeRockTotalYieldUnits() const override;
	virtual int32 GetOreDepositTotalYieldUnits() const override;
	virtual float GetPickupMaxDistance() const override;
	virtual float GetPickupAcquireRadius() const override;
	virtual float GetPickupRetainRadius() const override;
	virtual float GetPickupAimRayRadius() const override;
	virtual const FJTSMoonResourceSpawnSettings& GetMoonResourceSpawnSettings() const override;
	virtual float GetSpacecraftMarkerShowDistance() const override;
	virtual float GetSpacecraftMarkerScreenSafeMargin() const override;
	virtual float GetPickupDropUpwardSpeed() const override;
	virtual float GetPickupDropHorizontalSpeed() const override;
	virtual float GetAttackRange() const override;
	virtual float GetAttackAimRadius() const override;
	virtual float GetAttackCooldown() const override;

	virtual TSubclassOf<AJTSMoonAntActor> GetMoonAntActorClass() const override;
	virtual TSubclassOf<AJTSMoonAntNestActor> GetMoonAntNestActorClass() const override;
	virtual int32 GetMoonAntNestCount() const override;
	virtual float GetMoonAntNestOuterRadiusAroundCorpse() const override;
	virtual float GetMoonAntNestMinDistanceFromCorpse() const override;
	virtual float GetMoonAntNestMinDistanceFromShip() const override;
	virtual float GetMoonAntNestBaseMinSpacing() const override;
	virtual float GetMoonAntNestCandidateSpacingScaleMin() const override;
	virtual float GetMoonAntNestCandidateSpacingScaleMax() const override;
	virtual float GetMoonAntNestVisualScaleVariationMin() const override;
	virtual float GetMoonAntNestVisualScaleVariationMax() const override;
	virtual float GetMoonAntNestInnerWeight() const override;
	virtual float GetMoonAntNestMidWeight() const override;
	virtual float GetMoonAntNestOuterWeight() const override;

	virtual int32 GetMaxActiveMoonAntsPerNest() const override;
	virtual float GetMoonAntSpawnChance() const override;
	virtual float GetMoonAntSpawnIntervalMin() const override;
	virtual float GetMoonAntSpawnIntervalMax() const override;
	virtual float GetMoonAntSpawnNearWeight() const override;
	virtual float GetMoonAntSpawnMidWeight() const override;
	virtual float GetMoonAntSpawnFarWeight() const override;
	virtual float GetMoonAntSpawnNearDistanceMin() const override;
	virtual float GetMoonAntSpawnNearDistanceMax() const override;
	virtual float GetMoonAntSpawnMidDistanceMin() const override;
	virtual float GetMoonAntSpawnMidDistanceMax() const override;
	virtual float GetMoonAntSpawnFarDistanceMin() const override;
	virtual float GetMoonAntSpawnFarDistanceMax() const override;

	virtual float GetMoonAntRoamSpeed() const override;
	virtual float GetMoonAntRoamRadius() const override;
	virtual float GetMoonAntMaxHomeRadius() const override;
	virtual float GetMoonAntRoamRetargetIntervalMin() const override;
	virtual float GetMoonAntRoamRetargetIntervalMax() const override;
	virtual float GetMoonAntSurfaceDurationMin() const override;
	virtual float GetMoonAntSurfaceDurationMax() const override;
	virtual float GetMoonAntEmergingDuration() const override;
	virtual float GetMoonAntHitReactionDuration() const override;
	virtual float GetMoonAntBurrowDuration() const override;
	virtual float GetMoonAntFleeSpeed() const override;
	virtual float GetMoonAntFleeDurationMin() const override;
	virtual float GetMoonAntFleeDurationMax() const override;
	virtual float GetMoonAntMaxFleeDistance() const override;
	virtual float GetMoonAntTurnSpeed() const override;
	virtual float GetMoonAntGroundTraceStartHeight() const override;
	virtual float GetMoonAntGroundTraceDistance() const override;
	virtual int32 GetMoonAntNestPunchHitsToDestroy() const override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Survival", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 CrewCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Survival", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float FoodConsumptionPerPersonPerMinute = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Survival", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float WaterConsumptionPerPersonPerMinute = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Survival", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float ConsumptionTickInterval = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Survival", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MinimumConsumptionUnit = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Resources", meta = (AllowPrivateAccess = "true", ShowOnlyInnerProperties))
	FJTSMoonResourceSpawnSettings MoonResourceSpawnSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Crafting", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 PickaxeRockCost = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Crafting", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 BackpackRockCost = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Crafting", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 BackpackOreCost = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Crafting", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 KnifeRockCost = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Crafting", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 KnifeOreCost = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Crafting", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 AxeRockCost = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Crafting", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 AxeOreCost = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Mining", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 LargeRockTotalYieldUnits = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Mining", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 OreDepositTotalYieldUnits = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Interaction", meta = (AllowPrivateAccess = "true", ClampMin = "50.0", UIMin = "50.0"))
	float PickupMaxDistance = 550.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Interaction", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PickupAcquireRadius = 165.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Interaction", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PickupRetainRadius = 225.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Interaction", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PickupAimRayRadius = 95.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Navigation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float SpacecraftMarkerShowDistance = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Navigation", meta = (AllowPrivateAccess = "true", ClampMin = "20.0", UIMin = "20.0"))
	float SpacecraftMarkerScreenSafeMargin = 56.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Pickup", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float PickupDropUpwardSpeed = 340.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Pickup", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float PickupDropHorizontalSpeed = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "50.0", UIMin = "50.0"))
	float AttackRange = 240.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float AttackAimRadius = 42.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "0.05", UIMin = "0.05"))
	float AttackCooldown = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSMoonAntActor> MoonAntActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSMoonAntNestActor> MoonAntNestActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 MoonAntNestCount = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntNestOuterRadiusAroundCorpse = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntNestMinDistanceFromCorpse = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntNestMinDistanceFromShip = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntNestBaseMinSpacing = 220.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntNestCandidateSpacingScaleMin = 0.80f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntNestCandidateSpacingScaleMax = 1.20f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntNestVisualScaleVariationMin = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntNestVisualScaleVariationMax = 1.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntNestInnerWeight = 0.60f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntNestMidWeight = 0.28f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntNestOuterWeight = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 MaxActiveMoonAntsPerNest = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MoonAntSpawnChance = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntSpawnIntervalMin = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntSpawnIntervalMax = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnNearWeight = 0.62f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnMidWeight = 0.27f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnFarWeight = 0.11f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnNearDistanceMin = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnNearDistanceMax = 190.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnMidDistanceMin = 160.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnMidDistanceMax = 360.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnFarDistanceMin = 320.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnFarDistanceMax = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntRoamSpeed = 85.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntRoamRadius = 420.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntMaxHomeRadius = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntRoamRetargetIntervalMin = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntRoamRetargetIntervalMax = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntSurfaceDurationMin = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntSurfaceDurationMax = 14.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntEmergingDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntHitReactionDuration = 0.10f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntFleeSpeed = 260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntFleeDurationMin = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntFleeDurationMax = 2.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntMaxFleeDistance = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntBurrowDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntTurnSpeed = 540.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Ground", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntGroundTraceStartHeight = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Ground", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntGroundTraceDistance = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 MoonAntNestPunchHitsToDestroy = 3;
};
