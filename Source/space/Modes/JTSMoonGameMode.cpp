// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Modes/JTSMoonGameMode.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/UI/JTSPrototypeHUD.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "space/World/JTSMoonAntActor.h"
#include "space/World/JTSMoonAntNestActor.h"

AJTSMoonGameMode::AJTSMoonGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	DefaultPawnClass = AJTSCharacter::StaticClass();
	PlayerControllerClass = AJTSPlayerController::StaticClass();
	GameStateClass = AJTSGameState::StaticClass();
	HUDClass = AJTSPrototypeHUD::StaticClass();
	MoonAntNestActorClass = AJTSMoonAntNestActor::StaticClass();
	MoonSurfaceControllerClass = AJTSMoonSurfaceController::StaticClass();
}

int32 AJTSMoonGameMode::GetCrewCount() const
{
	return FMath::Max(0, CrewCount);
}

float AJTSMoonGameMode::GetFoodConsumptionPerPersonPerMinute() const
{
	return FMath::Max(0.0f, FoodConsumptionPerPersonPerMinute);
}

float AJTSMoonGameMode::GetWaterConsumptionPerPersonPerMinute() const
{
	return FMath::Max(0.0f, WaterConsumptionPerPersonPerMinute);
}

float AJTSMoonGameMode::GetConsumptionTickInterval() const
{
	return FMath::Max(0.0f, ConsumptionTickInterval);
}

float AJTSMoonGameMode::GetMinimumConsumptionUnit() const
{
	return FMath::Max(0.0f, MinimumConsumptionUnit);
}

int32 AJTSMoonGameMode::GetPickaxeRockCost() const
{
	return FMath::Max(1, PickaxeRockCost);
}

int32 AJTSMoonGameMode::GetBackpackRockCost() const
{
	return FMath::Max(1, BackpackRockCost);
}

int32 AJTSMoonGameMode::GetBackpackOreCost() const
{
	return FMath::Max(1, BackpackOreCost);
}

int32 AJTSMoonGameMode::GetKnifeRockCost() const
{
	return FMath::Max(0, KnifeRockCost);
}

int32 AJTSMoonGameMode::GetKnifeOreCost() const
{
	return FMath::Max(0, KnifeOreCost);
}

int32 AJTSMoonGameMode::GetAxeRockCost() const
{
	return FMath::Max(0, AxeRockCost);
}

int32 AJTSMoonGameMode::GetAxeOreCost() const
{
	return FMath::Max(0, AxeOreCost);
}

int32 AJTSMoonGameMode::GetLargeRockTotalYieldUnits() const
{
	return FMath::Max(1, LargeRockTotalYieldUnits);
}

int32 AJTSMoonGameMode::GetOreDepositTotalYieldUnits() const
{
	return FMath::Max(1, OreDepositTotalYieldUnits);
}

float AJTSMoonGameMode::GetPickupMaxDistance() const
{
	return FMath::Max(50.0f, PickupMaxDistance);
}

float AJTSMoonGameMode::GetPickupAcquireRadius() const
{
	return FMath::Max(1.0f, PickupAcquireRadius);
}

float AJTSMoonGameMode::GetPickupRetainRadius() const
{
	return FMath::Max(GetPickupAcquireRadius(), PickupRetainRadius);
}

float AJTSMoonGameMode::GetPickupAimRayRadius() const
{
	return FMath::Max(1.0f, PickupAimRayRadius);
}

AJTSSpacecraftActor* AJTSMoonGameMode::GetSpacecraft() const
{
	if (AJTSMoonSurfaceController* const Controller = GetMoonSurfaceController())
	{
		if (AJTSSpacecraftActor* const Spacecraft = Controller->GetSpacecraft())
		{
			return Spacecraft;
		}
	}

	UWorld* const World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr)
	{
		return nullptr;
	}

	for (AActor* const Actor : World->PersistentLevel->Actors)
	{
		if (AJTSSpacecraftActor* const Spacecraft = Cast<AJTSSpacecraftActor>(Actor); IsValid(Spacecraft))
		{
			return Spacecraft;
		}
	}

	return nullptr;
}

AJTSMoonSurfaceController* AJTSMoonGameMode::GetMoonSurfaceController() const
{
	return MoonSurfaceController.Get();
}

const FJTSMoonResourceSpawnSettings& AJTSMoonGameMode::GetMoonResourceSpawnSettings() const
{
	return MoonResourceSpawnSettings;
}

float AJTSMoonGameMode::GetSpacecraftMarkerShowDistance() const
{
	return FMath::Max(0.0f, SpacecraftMarkerShowDistance);
}

float AJTSMoonGameMode::GetSpacecraftMarkerScreenSafeMargin() const
{
	return FMath::Max(20.0f, SpacecraftMarkerScreenSafeMargin);
}

float AJTSMoonGameMode::GetPickupDropUpwardSpeed() const
{
	return FMath::Max(0.0f, PickupDropUpwardSpeed);
}

float AJTSMoonGameMode::GetPickupDropHorizontalSpeed() const
{
	return FMath::Max(0.0f, PickupDropHorizontalSpeed);
}

float AJTSMoonGameMode::GetAttackRange() const
{
	return FMath::Max(50.0f, AttackRange);
}

float AJTSMoonGameMode::GetAttackAimRadius() const
{
	return FMath::Max(1.0f, AttackAimRadius);
}

float AJTSMoonGameMode::GetAttackCooldown() const
{
	return FMath::Max(0.05f, AttackCooldown);
}

TSubclassOf<AJTSMoonAntActor> AJTSMoonGameMode::GetMoonAntActorClass() const
{
	return MoonAntActorClass;
}

TSubclassOf<AJTSMoonAntNestActor> AJTSMoonGameMode::GetMoonAntNestActorClass() const
{
	TSubclassOf<AJTSMoonAntNestActor> ResolvedMoonAntNestActorClass = MoonAntNestActorClass;
	if (ResolvedMoonAntNestActorClass == nullptr)
	{
		ResolvedMoonAntNestActorClass = AJTSMoonAntNestActor::StaticClass();
	}
	return ResolvedMoonAntNestActorClass;
}

int32 AJTSMoonGameMode::GetMoonAntNestCount() const
{
	return FMath::Max(0, MoonAntNestCount);
}

float AJTSMoonGameMode::GetMoonAntNestOuterRadiusAroundCorpse() const
{
	return FMath::Max(1.0f, MoonAntNestOuterRadiusAroundCorpse);
}

float AJTSMoonGameMode::GetMoonAntNestMinDistanceFromCorpse() const
{
	return FMath::Clamp(MoonAntNestMinDistanceFromCorpse, 0.0f, GetMoonAntNestOuterRadiusAroundCorpse());
}

float AJTSMoonGameMode::GetMoonAntNestMinDistanceFromShip() const
{
	return FMath::Max(0.0f, MoonAntNestMinDistanceFromShip);
}

float AJTSMoonGameMode::GetMoonAntNestBaseMinSpacing() const
{
	return FMath::Max(1.0f, MoonAntNestBaseMinSpacing);
}

float AJTSMoonGameMode::GetMoonAntNestCandidateSpacingScaleMin() const
{
	return FMath::Max(0.1f, MoonAntNestCandidateSpacingScaleMin);
}

float AJTSMoonGameMode::GetMoonAntNestCandidateSpacingScaleMax() const
{
	return FMath::Max(GetMoonAntNestCandidateSpacingScaleMin(), MoonAntNestCandidateSpacingScaleMax);
}

float AJTSMoonGameMode::GetMoonAntNestVisualScaleVariationMin() const
{
	return FMath::Max(0.1f, MoonAntNestVisualScaleVariationMin);
}

float AJTSMoonGameMode::GetMoonAntNestVisualScaleVariationMax() const
{
	return FMath::Max(GetMoonAntNestVisualScaleVariationMin(), MoonAntNestVisualScaleVariationMax);
}

float AJTSMoonGameMode::GetMoonAntNestInnerWeight() const
{
	return FMath::Max(0.0f, MoonAntNestInnerWeight);
}

float AJTSMoonGameMode::GetMoonAntNestMidWeight() const
{
	return FMath::Max(0.0f, MoonAntNestMidWeight);
}

float AJTSMoonGameMode::GetMoonAntNestOuterWeight() const
{
	return FMath::Max(0.0f, MoonAntNestOuterWeight);
}

int32 AJTSMoonGameMode::GetMaxActiveMoonAntsPerNest() const
{
	return FMath::Max(0, MaxActiveMoonAntsPerNest);
}

float AJTSMoonGameMode::GetMoonAntSpawnChance() const
{
	return FMath::Clamp(MoonAntSpawnChance, 0.0f, 1.0f);
}

float AJTSMoonGameMode::GetMoonAntSpawnIntervalMin() const
{
	return FMath::Max(0.1f, MoonAntSpawnIntervalMin);
}

float AJTSMoonGameMode::GetMoonAntSpawnIntervalMax() const
{
	return FMath::Max(GetMoonAntSpawnIntervalMin(), MoonAntSpawnIntervalMax);
}

float AJTSMoonGameMode::GetMoonAntSpawnNearWeight() const
{
	return FMath::Max(0.0f, MoonAntSpawnNearWeight);
}

float AJTSMoonGameMode::GetMoonAntSpawnMidWeight() const
{
	return FMath::Max(0.0f, MoonAntSpawnMidWeight);
}

float AJTSMoonGameMode::GetMoonAntSpawnFarWeight() const
{
	return FMath::Max(0.0f, MoonAntSpawnFarWeight);
}

float AJTSMoonGameMode::GetMoonAntSpawnNearDistanceMin() const
{
	return FMath::Max(0.0f, MoonAntSpawnNearDistanceMin);
}

float AJTSMoonGameMode::GetMoonAntSpawnNearDistanceMax() const
{
	return FMath::Max(GetMoonAntSpawnNearDistanceMin(), MoonAntSpawnNearDistanceMax);
}

float AJTSMoonGameMode::GetMoonAntSpawnMidDistanceMin() const
{
	return FMath::Max(0.0f, MoonAntSpawnMidDistanceMin);
}

float AJTSMoonGameMode::GetMoonAntSpawnMidDistanceMax() const
{
	return FMath::Max(GetMoonAntSpawnMidDistanceMin(), MoonAntSpawnMidDistanceMax);
}

float AJTSMoonGameMode::GetMoonAntSpawnFarDistanceMin() const
{
	return FMath::Max(0.0f, MoonAntSpawnFarDistanceMin);
}

float AJTSMoonGameMode::GetMoonAntSpawnFarDistanceMax() const
{
	return FMath::Max(GetMoonAntSpawnFarDistanceMin(), MoonAntSpawnFarDistanceMax);
}

float AJTSMoonGameMode::GetMoonAntRoamSpeed() const
{
	return FMath::Max(1.0f, MoonAntRoamSpeed);
}

float AJTSMoonGameMode::GetMoonAntRoamRadius() const
{
	return FMath::Max(1.0f, MoonAntRoamRadius);
}

float AJTSMoonGameMode::GetMoonAntMaxHomeRadius() const
{
	return FMath::Max(GetMoonAntRoamRadius(), MoonAntMaxHomeRadius);
}

float AJTSMoonGameMode::GetMoonAntRoamRetargetIntervalMin() const
{
	return FMath::Max(0.1f, MoonAntRoamRetargetIntervalMin);
}

float AJTSMoonGameMode::GetMoonAntRoamRetargetIntervalMax() const
{
	return FMath::Max(GetMoonAntRoamRetargetIntervalMin(), MoonAntRoamRetargetIntervalMax);
}

float AJTSMoonGameMode::GetMoonAntSurfaceDurationMin() const
{
	return FMath::Max(0.1f, MoonAntSurfaceDurationMin);
}

float AJTSMoonGameMode::GetMoonAntSurfaceDurationMax() const
{
	return FMath::Max(GetMoonAntSurfaceDurationMin(), MoonAntSurfaceDurationMax);
}

float AJTSMoonGameMode::GetMoonAntEmergingDuration() const
{
	return FMath::Max(0.0f, MoonAntEmergingDuration);
}

float AJTSMoonGameMode::GetMoonAntHitReactionDuration() const
{
	return FMath::Max(0.0f, MoonAntHitReactionDuration);
}

float AJTSMoonGameMode::GetMoonAntBurrowDuration() const
{
	return FMath::Max(0.0f, MoonAntBurrowDuration);
}

float AJTSMoonGameMode::GetMoonAntFleeSpeed() const
{
	return FMath::Max(1.0f, MoonAntFleeSpeed);
}

float AJTSMoonGameMode::GetMoonAntFleeDurationMin() const
{
	return FMath::Max(0.1f, MoonAntFleeDurationMin);
}

float AJTSMoonGameMode::GetMoonAntFleeDurationMax() const
{
	return FMath::Max(GetMoonAntFleeDurationMin(), MoonAntFleeDurationMax);
}

float AJTSMoonGameMode::GetMoonAntMaxFleeDistance() const
{
	return FMath::Max(1.0f, MoonAntMaxFleeDistance);
}

float AJTSMoonGameMode::GetMoonAntTurnSpeed() const
{
	return FMath::Max(1.0f, MoonAntTurnSpeed);
}

float AJTSMoonGameMode::GetMoonAntChaseSpeed() const
{
	return FMath::Max(0.0f, MoonAntChaseSpeed);
}

float AJTSMoonGameMode::GetMoonAntReturnSpeed() const
{
	return FMath::Max(0.0f, MoonAntReturnSpeed);
}

float AJTSMoonGameMode::GetMoonAntAggroRadius() const
{
	return FMath::Max(0.0f, MoonAntAggroRadius);
}

float AJTSMoonGameMode::GetMoonAntLoseAggroRadius() const
{
	return FMath::Max(0.0f, MoonAntLoseAggroRadius);
}

float AJTSMoonGameMode::GetMoonAntStopDistanceFromPlayer() const
{
	return FMath::Max(0.0f, MoonAntStopDistanceFromPlayer);
}

float AJTSMoonGameMode::GetMoonAntWanderSpeed() const
{
	return FMath::Max(0.0f, MoonAntWanderSpeed);
}

float AJTSMoonGameMode::GetMoonAntWanderRadius() const
{
	return FMath::Max(0.0f, MoonAntWanderRadius);
}

float AJTSMoonGameMode::GetMoonAntWanderRetargetIntervalMin() const
{
	return FMath::Max(0.0f, MoonAntWanderRetargetIntervalMin);
}

float AJTSMoonGameMode::GetMoonAntWanderRetargetIntervalMax() const
{
	return FMath::Max(0.0f, MoonAntWanderRetargetIntervalMax);
}

float AJTSMoonGameMode::GetMoonAntLifetime() const
{
	return FMath::Max(0.0f, MoonAntLifetime);
}

float AJTSMoonGameMode::GetMoonAntGroundTraceStartHeight() const
{
	return FMath::Max(0.0f, MoonAntGroundTraceStartHeight);
}

float AJTSMoonGameMode::GetMoonAntGroundTraceDistance() const
{
	return FMath::Max(1.0f, MoonAntGroundTraceDistance);
}

int32 AJTSMoonGameMode::GetMoonAntPunchHitsToKill() const
{
	return FMath::Max(1, MoonAntPunchHitsToKill);
}

int32 AJTSMoonGameMode::GetMoonAntNestPunchHitsToDestroy() const
{
	return FMath::Max(1, MoonAntNestPunchHitsToDestroy);
}

bool AJTSMoonGameMode::TryCraftPickaxe(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft)
{
	if (AJTSMoonSurfaceController* const Controller = EnsureMoonSurfaceController())
	{
		return Controller->TryCraftPickaxe(Player, Spacecraft);
	}

	return false;
}

bool AJTSMoonGameMode::TryCraftBackpack(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft)
{
	if (AJTSMoonSurfaceController* const Controller = EnsureMoonSurfaceController())
	{
		return Controller->TryCraftBackpack(Player, Spacecraft);
	}

	return false;
}

bool AJTSMoonGameMode::TryCraftKnife(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft)
{
	if (AJTSMoonSurfaceController* const Controller = EnsureMoonSurfaceController())
	{
		return Controller->TryCraftKnife(Player, Spacecraft);
	}

	return false;
}

bool AJTSMoonGameMode::TryCraftAxe(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft)
{
	if (AJTSMoonSurfaceController* const Controller = EnsureMoonSurfaceController())
	{
		return Controller->TryCraftAxe(Player, Spacecraft);
	}

	return false;
}

bool AJTSMoonGameMode::ResolveMoonGroundLocation(
	const FVector& CandidateLocation,
	FVector& OutGroundLocation,
	const AActor* AdditionalIgnoredActor) const
{
	if (AJTSMoonSurfaceController* const Controller = GetMoonSurfaceController())
	{
		return Controller->ResolveMoonGroundLocation(CandidateLocation, OutGroundLocation, AdditionalIgnoredActor);
	}

	return false;
}

AJTSMoonSurfaceController* AJTSMoonGameMode::EnsureMoonSurfaceController()
{
	if (MoonSurfaceController.IsValid())
	{
		return MoonSurfaceController.Get();
	}

	UWorld* const World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr)
	{
		return nullptr;
	}

	AJTSMoonSurfaceController* Controller = nullptr;
	for (AActor* const Actor : World->PersistentLevel->Actors)
	{
		AJTSMoonSurfaceController* const Candidate = Cast<AJTSMoonSurfaceController>(Actor);
		if (!IsValid(Candidate))
		{
			continue;
		}

		if (IsValid(Controller))
		{
			UE_LOG(LogTemp, Error, TEXT("L_MoonPrototype has more than one MoonSurfaceController; legacy Moon runtime cannot choose one."));
			return nullptr;
		}
		Controller = Candidate;
	}

	if (!IsValid(Controller))
	{
		TSubclassOf<AJTSMoonSurfaceController> ControllerClass = MoonSurfaceControllerClass;
		if (ControllerClass == nullptr)
		{
			ControllerClass = AJTSMoonSurfaceController::StaticClass();
		}
		if (ControllerClass == nullptr)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = TEXT("LegacyMoonSurfaceController");
		SpawnParameters.OverrideLevel = World->PersistentLevel;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Controller = World->SpawnActor<AJTSMoonSurfaceController>(ControllerClass, FTransform::Identity, SpawnParameters);
	}

	if (IsValid(Controller))
	{
		MoonSurfaceController = Controller;
		Controller->ConfigureLegacyRuntime(this);
	}
	return Controller;
}

void AJTSMoonGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("JumpToSpace Moon Config: Crew=%d FoodRate=%.2f WaterRate=%.2f ResourceCount=%d SpawnRadius=%.1f PickaxeCost=%d BackpackRockCost=%d BackpackOreCost=%d KnifeCost=%d/%d AxeCost=%d/%d LargeYield=%d OreYield=%d PickupMaxDistance=%.0f PickupAcquireRadius=%.0f PickupRetainRadius=%.0f PickupAimRayRadius=%.0f ShipMarkerDistance=%.0f AttackRange=%.0f MoonAntNests=%d"),
		GetCrewCount(),
		GetFoodConsumptionPerPersonPerMinute(),
		GetWaterConsumptionPerPersonPerMinute(),
		FMath::Max(0, MoonResourceSpawnSettings.TotalResourceCount),
		FMath::Max(0.0f, MoonResourceSpawnSettings.SpawnRadius),
		GetPickaxeRockCost(),
		GetBackpackRockCost(),
		GetBackpackOreCost(),
		GetKnifeRockCost(),
		GetKnifeOreCost(),
		GetAxeRockCost(),
		GetAxeOreCost(),
		GetLargeRockTotalYieldUnits(),
		GetOreDepositTotalYieldUnits(),
		GetPickupMaxDistance(),
		GetPickupAcquireRadius(),
		GetPickupRetainRadius(),
		GetPickupAimRayRadius(),
		GetSpacecraftMarkerShowDistance(),
		GetAttackRange(),
		GetMoonAntNestCount());

	if (AJTSMoonSurfaceController* const Controller = EnsureMoonSurfaceController())
	{
		Controller->RequestSurfaceGameplayInitialization();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("L_MoonPrototype could not create its Moon surface runtime controller."));
	}
}

void AJTSMoonGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MoonSurfaceController.Reset();
	Super::EndPlay(EndPlayReason);
}
