// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "space/World/JTSMoonResourceSpawner.h"

#include "JTSMoonSurfaceGameplaySettings.generated.h"

class AJTSMoonAntActor;
class AJTSMoonAntNestActor;

/**
 * Read-only Moon gameplay balance contract.
 *
 * Legacy Moon GameModes and the SpaceWorld Moon Primary Data Asset both implement this contract.
 * Runtime systems consume the contract rather than assuming that the owning World GameMode is the
 * source of Moon content configuration.
 */
UINTERFACE(MinimalAPI)
class UJTSMoonSurfaceGameplaySettings : public UInterface
{
	GENERATED_BODY()
};

class SPACE_API IJTSMoonSurfaceGameplaySettings
{
	GENERATED_BODY()

public:
	virtual int32 GetCrewCount() const = 0;
	virtual float GetFoodConsumptionPerPersonPerMinute() const = 0;
	virtual float GetWaterConsumptionPerPersonPerMinute() const = 0;
	virtual float GetConsumptionTickInterval() const = 0;
	virtual float GetMinimumConsumptionUnit() const = 0;

	virtual int32 GetPickaxeRockCost() const = 0;
	virtual int32 GetBackpackRockCost() const = 0;
	virtual int32 GetBackpackOreCost() const = 0;
	virtual int32 GetKnifeRockCost() const = 0;
	virtual int32 GetKnifeOreCost() const = 0;
	virtual int32 GetAxeRockCost() const = 0;
	virtual int32 GetAxeOreCost() const = 0;

	virtual int32 GetLargeRockTotalYieldUnits() const = 0;
	virtual int32 GetOreDepositTotalYieldUnits() const = 0;
	virtual float GetPickupMaxDistance() const = 0;
	virtual float GetPickupAcquireRadius() const = 0;
	virtual float GetPickupRetainRadius() const = 0;
	virtual float GetPickupAimRayRadius() const = 0;
	virtual const FJTSMoonResourceSpawnSettings& GetMoonResourceSpawnSettings() const = 0;
	virtual float GetSpacecraftMarkerShowDistance() const = 0;
	virtual float GetSpacecraftMarkerScreenSafeMargin() const = 0;
	virtual float GetPickupDropUpwardSpeed() const = 0;
	virtual float GetPickupDropHorizontalSpeed() const = 0;
	virtual float GetAttackRange() const = 0;
	virtual float GetAttackAimRadius() const = 0;
	virtual float GetAttackCooldown() const = 0;

	virtual TSubclassOf<AJTSMoonAntActor> GetMoonAntActorClass() const = 0;
	virtual TSubclassOf<AJTSMoonAntNestActor> GetMoonAntNestActorClass() const = 0;
	virtual int32 GetMoonAntNestCount() const = 0;
	virtual float GetMoonAntNestOuterRadiusAroundCorpse() const = 0;
	virtual float GetMoonAntNestMinDistanceFromCorpse() const = 0;
	virtual float GetMoonAntNestMinDistanceFromShip() const = 0;
	virtual float GetMoonAntNestBaseMinSpacing() const = 0;
	virtual float GetMoonAntNestCandidateSpacingScaleMin() const = 0;
	virtual float GetMoonAntNestCandidateSpacingScaleMax() const = 0;
	virtual float GetMoonAntNestVisualScaleVariationMin() const = 0;
	virtual float GetMoonAntNestVisualScaleVariationMax() const = 0;
	virtual float GetMoonAntNestInnerWeight() const = 0;
	virtual float GetMoonAntNestMidWeight() const = 0;
	virtual float GetMoonAntNestOuterWeight() const = 0;

	virtual int32 GetMaxActiveMoonAntsPerNest() const = 0;
	virtual float GetMoonAntSpawnChance() const = 0;
	virtual float GetMoonAntSpawnIntervalMin() const = 0;
	virtual float GetMoonAntSpawnIntervalMax() const = 0;
	virtual float GetMoonAntSpawnNearWeight() const = 0;
	virtual float GetMoonAntSpawnMidWeight() const = 0;
	virtual float GetMoonAntSpawnFarWeight() const = 0;
	virtual float GetMoonAntSpawnNearDistanceMin() const = 0;
	virtual float GetMoonAntSpawnNearDistanceMax() const = 0;
	virtual float GetMoonAntSpawnMidDistanceMin() const = 0;
	virtual float GetMoonAntSpawnMidDistanceMax() const = 0;
	virtual float GetMoonAntSpawnFarDistanceMin() const = 0;
	virtual float GetMoonAntSpawnFarDistanceMax() const = 0;

	virtual float GetMoonAntRoamSpeed() const = 0;
	virtual float GetMoonAntRoamRadius() const = 0;
	virtual float GetMoonAntMaxHomeRadius() const = 0;
	virtual float GetMoonAntRoamRetargetIntervalMin() const = 0;
	virtual float GetMoonAntRoamRetargetIntervalMax() const = 0;
	virtual float GetMoonAntSurfaceDurationMin() const = 0;
	virtual float GetMoonAntSurfaceDurationMax() const = 0;
	virtual float GetMoonAntEmergingDuration() const = 0;
	virtual float GetMoonAntHitReactionDuration() const = 0;
	virtual float GetMoonAntBurrowDuration() const = 0;
	virtual float GetMoonAntFleeSpeed() const = 0;
	virtual float GetMoonAntFleeDurationMin() const = 0;
	virtual float GetMoonAntFleeDurationMax() const = 0;
	virtual float GetMoonAntMaxFleeDistance() const = 0;
	virtual float GetMoonAntTurnSpeed() const = 0;
	virtual float GetMoonAntGroundTraceStartHeight() const = 0;
	virtual float GetMoonAntGroundTraceDistance() const = 0;
	virtual int32 GetMoonAntNestPunchHitsToDestroy() const = 0;
};
