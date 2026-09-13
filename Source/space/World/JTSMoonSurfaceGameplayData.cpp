// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSMoonSurfaceGameplayData.h"

#include "space/World/JTSMoonAntActor.h"
#include "space/World/JTSMoonAntNestActor.h"

int32 UJTSMoonSurfaceGameplayData::GetCrewCount() const { return FMath::Max(0, CrewCount); }
float UJTSMoonSurfaceGameplayData::GetFoodConsumptionPerPersonPerMinute() const { return FMath::Max(0.0f, FoodConsumptionPerPersonPerMinute); }
float UJTSMoonSurfaceGameplayData::GetWaterConsumptionPerPersonPerMinute() const { return FMath::Max(0.0f, WaterConsumptionPerPersonPerMinute); }
float UJTSMoonSurfaceGameplayData::GetConsumptionTickInterval() const { return FMath::Max(0.0f, ConsumptionTickInterval); }
float UJTSMoonSurfaceGameplayData::GetMinimumConsumptionUnit() const { return FMath::Max(0.0f, MinimumConsumptionUnit); }

int32 UJTSMoonSurfaceGameplayData::GetPickaxeRockCost() const { return FMath::Max(1, PickaxeRockCost); }
int32 UJTSMoonSurfaceGameplayData::GetBackpackRockCost() const { return FMath::Max(1, BackpackRockCost); }
int32 UJTSMoonSurfaceGameplayData::GetBackpackOreCost() const { return FMath::Max(1, BackpackOreCost); }
int32 UJTSMoonSurfaceGameplayData::GetKnifeRockCost() const { return FMath::Max(0, KnifeRockCost); }
int32 UJTSMoonSurfaceGameplayData::GetKnifeOreCost() const { return FMath::Max(0, KnifeOreCost); }
int32 UJTSMoonSurfaceGameplayData::GetAxeRockCost() const { return FMath::Max(0, AxeRockCost); }
int32 UJTSMoonSurfaceGameplayData::GetAxeOreCost() const { return FMath::Max(0, AxeOreCost); }

int32 UJTSMoonSurfaceGameplayData::GetLargeRockTotalYieldUnits() const { return FMath::Max(1, LargeRockTotalYieldUnits); }
int32 UJTSMoonSurfaceGameplayData::GetOreDepositTotalYieldUnits() const { return FMath::Max(1, OreDepositTotalYieldUnits); }
float UJTSMoonSurfaceGameplayData::GetPickupMaxDistance() const { return FMath::Max(50.0f, PickupMaxDistance); }
float UJTSMoonSurfaceGameplayData::GetPickupAcquireRadius() const { return FMath::Max(1.0f, PickupAcquireRadius); }
float UJTSMoonSurfaceGameplayData::GetPickupRetainRadius() const { return FMath::Max(GetPickupAcquireRadius(), PickupRetainRadius); }
float UJTSMoonSurfaceGameplayData::GetPickupAimRayRadius() const { return FMath::Max(1.0f, PickupAimRayRadius); }
const FJTSMoonResourceSpawnSettings& UJTSMoonSurfaceGameplayData::GetMoonResourceSpawnSettings() const { return MoonResourceSpawnSettings; }
float UJTSMoonSurfaceGameplayData::GetSpacecraftMarkerShowDistance() const { return FMath::Max(0.0f, SpacecraftMarkerShowDistance); }
float UJTSMoonSurfaceGameplayData::GetSpacecraftMarkerScreenSafeMargin() const { return FMath::Max(20.0f, SpacecraftMarkerScreenSafeMargin); }
float UJTSMoonSurfaceGameplayData::GetPickupDropUpwardSpeed() const { return FMath::Max(0.0f, PickupDropUpwardSpeed); }
float UJTSMoonSurfaceGameplayData::GetPickupDropHorizontalSpeed() const { return FMath::Max(0.0f, PickupDropHorizontalSpeed); }
float UJTSMoonSurfaceGameplayData::GetAttackRange() const { return FMath::Max(50.0f, AttackRange); }
float UJTSMoonSurfaceGameplayData::GetAttackAimRadius() const { return FMath::Max(1.0f, AttackAimRadius); }
float UJTSMoonSurfaceGameplayData::GetAttackCooldown() const { return FMath::Max(0.05f, AttackCooldown); }

TSubclassOf<AJTSMoonAntActor> UJTSMoonSurfaceGameplayData::GetMoonAntActorClass() const { return MoonAntActorClass; }
TSubclassOf<AJTSMoonAntNestActor> UJTSMoonSurfaceGameplayData::GetMoonAntNestActorClass() const
{
	if (MoonAntNestActorClass != nullptr)
	{
		return MoonAntNestActorClass;
	}

	return AJTSMoonAntNestActor::StaticClass();
}
int32 UJTSMoonSurfaceGameplayData::GetMoonAntNestCount() const { return FMath::Max(0, MoonAntNestCount); }
float UJTSMoonSurfaceGameplayData::GetMoonAntNestOuterRadiusAroundCorpse() const { return FMath::Max(1.0f, MoonAntNestOuterRadiusAroundCorpse); }
float UJTSMoonSurfaceGameplayData::GetMoonAntNestMinDistanceFromCorpse() const { return FMath::Clamp(MoonAntNestMinDistanceFromCorpse, 0.0f, GetMoonAntNestOuterRadiusAroundCorpse()); }
float UJTSMoonSurfaceGameplayData::GetMoonAntNestMinDistanceFromShip() const { return FMath::Max(0.0f, MoonAntNestMinDistanceFromShip); }
float UJTSMoonSurfaceGameplayData::GetMoonAntNestBaseMinSpacing() const { return FMath::Max(1.0f, MoonAntNestBaseMinSpacing); }
float UJTSMoonSurfaceGameplayData::GetMoonAntNestCandidateSpacingScaleMin() const { return FMath::Max(0.1f, MoonAntNestCandidateSpacingScaleMin); }
float UJTSMoonSurfaceGameplayData::GetMoonAntNestCandidateSpacingScaleMax() const { return FMath::Max(GetMoonAntNestCandidateSpacingScaleMin(), MoonAntNestCandidateSpacingScaleMax); }
float UJTSMoonSurfaceGameplayData::GetMoonAntNestVisualScaleVariationMin() const { return FMath::Max(0.1f, MoonAntNestVisualScaleVariationMin); }
float UJTSMoonSurfaceGameplayData::GetMoonAntNestVisualScaleVariationMax() const { return FMath::Max(GetMoonAntNestVisualScaleVariationMin(), MoonAntNestVisualScaleVariationMax); }
float UJTSMoonSurfaceGameplayData::GetMoonAntNestInnerWeight() const { return FMath::Max(0.0f, MoonAntNestInnerWeight); }
float UJTSMoonSurfaceGameplayData::GetMoonAntNestMidWeight() const { return FMath::Max(0.0f, MoonAntNestMidWeight); }
float UJTSMoonSurfaceGameplayData::GetMoonAntNestOuterWeight() const { return FMath::Max(0.0f, MoonAntNestOuterWeight); }

int32 UJTSMoonSurfaceGameplayData::GetMaxActiveMoonAntsPerNest() const { return FMath::Max(0, MaxActiveMoonAntsPerNest); }
float UJTSMoonSurfaceGameplayData::GetMoonAntSpawnChance() const { return FMath::Clamp(MoonAntSpawnChance, 0.0f, 1.0f); }
float UJTSMoonSurfaceGameplayData::GetMoonAntSpawnIntervalMin() const { return FMath::Max(0.1f, MoonAntSpawnIntervalMin); }
float UJTSMoonSurfaceGameplayData::GetMoonAntSpawnIntervalMax() const { return FMath::Max(GetMoonAntSpawnIntervalMin(), MoonAntSpawnIntervalMax); }
float UJTSMoonSurfaceGameplayData::GetMoonAntSpawnNearWeight() const { return FMath::Max(0.0f, MoonAntSpawnNearWeight); }
float UJTSMoonSurfaceGameplayData::GetMoonAntSpawnMidWeight() const { return FMath::Max(0.0f, MoonAntSpawnMidWeight); }
float UJTSMoonSurfaceGameplayData::GetMoonAntSpawnFarWeight() const { return FMath::Max(0.0f, MoonAntSpawnFarWeight); }
float UJTSMoonSurfaceGameplayData::GetMoonAntSpawnNearDistanceMin() const { return FMath::Max(0.0f, MoonAntSpawnNearDistanceMin); }
float UJTSMoonSurfaceGameplayData::GetMoonAntSpawnNearDistanceMax() const { return FMath::Max(GetMoonAntSpawnNearDistanceMin(), MoonAntSpawnNearDistanceMax); }
float UJTSMoonSurfaceGameplayData::GetMoonAntSpawnMidDistanceMin() const { return FMath::Max(0.0f, MoonAntSpawnMidDistanceMin); }
float UJTSMoonSurfaceGameplayData::GetMoonAntSpawnMidDistanceMax() const { return FMath::Max(GetMoonAntSpawnMidDistanceMin(), MoonAntSpawnMidDistanceMax); }
float UJTSMoonSurfaceGameplayData::GetMoonAntSpawnFarDistanceMin() const { return FMath::Max(0.0f, MoonAntSpawnFarDistanceMin); }
float UJTSMoonSurfaceGameplayData::GetMoonAntSpawnFarDistanceMax() const { return FMath::Max(GetMoonAntSpawnFarDistanceMin(), MoonAntSpawnFarDistanceMax); }

float UJTSMoonSurfaceGameplayData::GetMoonAntRoamSpeed() const { return FMath::Max(1.0f, MoonAntRoamSpeed); }
float UJTSMoonSurfaceGameplayData::GetMoonAntRoamRadius() const { return FMath::Max(1.0f, MoonAntRoamRadius); }
float UJTSMoonSurfaceGameplayData::GetMoonAntMaxHomeRadius() const { return FMath::Max(GetMoonAntRoamRadius(), MoonAntMaxHomeRadius); }
float UJTSMoonSurfaceGameplayData::GetMoonAntRoamRetargetIntervalMin() const { return FMath::Max(0.1f, MoonAntRoamRetargetIntervalMin); }
float UJTSMoonSurfaceGameplayData::GetMoonAntRoamRetargetIntervalMax() const { return FMath::Max(GetMoonAntRoamRetargetIntervalMin(), MoonAntRoamRetargetIntervalMax); }
float UJTSMoonSurfaceGameplayData::GetMoonAntSurfaceDurationMin() const { return FMath::Max(0.1f, MoonAntSurfaceDurationMin); }
float UJTSMoonSurfaceGameplayData::GetMoonAntSurfaceDurationMax() const { return FMath::Max(GetMoonAntSurfaceDurationMin(), MoonAntSurfaceDurationMax); }
float UJTSMoonSurfaceGameplayData::GetMoonAntEmergingDuration() const { return FMath::Max(0.0f, MoonAntEmergingDuration); }
float UJTSMoonSurfaceGameplayData::GetMoonAntHitReactionDuration() const { return FMath::Max(0.0f, MoonAntHitReactionDuration); }
float UJTSMoonSurfaceGameplayData::GetMoonAntBurrowDuration() const { return FMath::Max(0.0f, MoonAntBurrowDuration); }
float UJTSMoonSurfaceGameplayData::GetMoonAntFleeSpeed() const { return FMath::Max(1.0f, MoonAntFleeSpeed); }
float UJTSMoonSurfaceGameplayData::GetMoonAntFleeDurationMin() const { return FMath::Max(0.1f, MoonAntFleeDurationMin); }
float UJTSMoonSurfaceGameplayData::GetMoonAntFleeDurationMax() const { return FMath::Max(GetMoonAntFleeDurationMin(), MoonAntFleeDurationMax); }
float UJTSMoonSurfaceGameplayData::GetMoonAntMaxFleeDistance() const { return FMath::Max(1.0f, MoonAntMaxFleeDistance); }
float UJTSMoonSurfaceGameplayData::GetMoonAntTurnSpeed() const { return FMath::Max(1.0f, MoonAntTurnSpeed); }
float UJTSMoonSurfaceGameplayData::GetMoonAntGroundTraceStartHeight() const { return FMath::Max(0.0f, MoonAntGroundTraceStartHeight); }
float UJTSMoonSurfaceGameplayData::GetMoonAntGroundTraceDistance() const { return FMath::Max(1.0f, MoonAntGroundTraceDistance); }
int32 UJTSMoonSurfaceGameplayData::GetMoonAntNestPunchHitsToDestroy() const { return FMath::Max(1, MoonAntNestPunchHitsToDestroy); }
