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
#include "space/World/JTSRoachActor.h"
#include "space/World/JTSRoachNestActor.h"

AJTSMoonGameMode::AJTSMoonGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	DefaultPawnClass = AJTSCharacter::StaticClass();
	PlayerControllerClass = AJTSPlayerController::StaticClass();
	GameStateClass = AJTSGameState::StaticClass();
	HUDClass = AJTSPrototypeHUD::StaticClass();
	AntNestActorClass = AJTSRoachNestActor::StaticClass();
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

TSubclassOf<AJTSRoachActor> AJTSMoonGameMode::GetAntActorClass() const
{
	return AntActorClass;
}

TSubclassOf<AJTSRoachNestActor> AJTSMoonGameMode::GetAntNestActorClass() const
{
	TSubclassOf<AJTSRoachNestActor> ResolvedAntNestActorClass = AntNestActorClass;
	if (ResolvedAntNestActorClass == nullptr)
	{
		ResolvedAntNestActorClass = AJTSRoachNestActor::StaticClass();
	}
	return ResolvedAntNestActorClass;
}

int32 AJTSMoonGameMode::GetAntNestCount() const
{
	return FMath::Max(0, AntNestCount);
}

float AJTSMoonGameMode::GetAntNestOuterRadiusAroundCorpse() const
{
	return FMath::Max(1.0f, AntNestOuterRadiusAroundCorpse);
}

float AJTSMoonGameMode::GetAntNestMinDistanceFromCorpse() const
{
	return FMath::Clamp(AntNestMinDistanceFromCorpse, 0.0f, GetAntNestOuterRadiusAroundCorpse());
}

float AJTSMoonGameMode::GetAntNestMinDistanceFromShip() const
{
	return FMath::Max(0.0f, AntNestMinDistanceFromShip);
}

float AJTSMoonGameMode::GetAntNestBaseMinSpacing() const
{
	return FMath::Max(1.0f, AntNestBaseMinSpacing);
}

float AJTSMoonGameMode::GetAntNestCandidateSpacingScaleMin() const
{
	return FMath::Max(0.1f, AntNestCandidateSpacingScaleMin);
}

float AJTSMoonGameMode::GetAntNestCandidateSpacingScaleMax() const
{
	return FMath::Max(GetAntNestCandidateSpacingScaleMin(), AntNestCandidateSpacingScaleMax);
}

float AJTSMoonGameMode::GetAntNestVisualScaleVariationMin() const
{
	return FMath::Max(0.1f, AntNestVisualScaleVariationMin);
}

float AJTSMoonGameMode::GetAntNestVisualScaleVariationMax() const
{
	return FMath::Max(GetAntNestVisualScaleVariationMin(), AntNestVisualScaleVariationMax);
}

float AJTSMoonGameMode::GetAntNestInnerWeight() const
{
	return FMath::Max(0.0f, AntNestInnerWeight);
}

float AJTSMoonGameMode::GetAntNestMidWeight() const
{
	return FMath::Max(0.0f, AntNestMidWeight);
}

float AJTSMoonGameMode::GetAntNestOuterWeight() const
{
	return FMath::Max(0.0f, AntNestOuterWeight);
}

int32 AJTSMoonGameMode::GetMaxActiveAntsPerNest() const
{
	return FMath::Max(0, MaxActiveAntsPerNest);
}

float AJTSMoonGameMode::GetAntSpawnChance() const
{
	return FMath::Clamp(AntSpawnChance, 0.0f, 1.0f);
}

float AJTSMoonGameMode::GetAntSpawnIntervalMin() const
{
	return FMath::Max(0.1f, AntSpawnIntervalMin);
}

float AJTSMoonGameMode::GetAntSpawnIntervalMax() const
{
	return FMath::Max(GetAntSpawnIntervalMin(), AntSpawnIntervalMax);
}

float AJTSMoonGameMode::GetAntSpawnNearWeight() const
{
	return FMath::Max(0.0f, AntSpawnNearWeight);
}

float AJTSMoonGameMode::GetAntSpawnMidWeight() const
{
	return FMath::Max(0.0f, AntSpawnMidWeight);
}

float AJTSMoonGameMode::GetAntSpawnFarWeight() const
{
	return FMath::Max(0.0f, AntSpawnFarWeight);
}

float AJTSMoonGameMode::GetAntSpawnNearDistanceMin() const
{
	return FMath::Max(0.0f, AntSpawnNearDistanceMin);
}

float AJTSMoonGameMode::GetAntSpawnNearDistanceMax() const
{
	return FMath::Max(GetAntSpawnNearDistanceMin(), AntSpawnNearDistanceMax);
}

float AJTSMoonGameMode::GetAntSpawnMidDistanceMin() const
{
	return FMath::Max(0.0f, AntSpawnMidDistanceMin);
}

float AJTSMoonGameMode::GetAntSpawnMidDistanceMax() const
{
	return FMath::Max(GetAntSpawnMidDistanceMin(), AntSpawnMidDistanceMax);
}

float AJTSMoonGameMode::GetAntSpawnFarDistanceMin() const
{
	return FMath::Max(0.0f, AntSpawnFarDistanceMin);
}

float AJTSMoonGameMode::GetAntSpawnFarDistanceMax() const
{
	return FMath::Max(GetAntSpawnFarDistanceMin(), AntSpawnFarDistanceMax);
}

float AJTSMoonGameMode::GetAntRoamSpeed() const
{
	return FMath::Max(1.0f, AntRoamSpeed);
}

float AJTSMoonGameMode::GetAntRoamRadius() const
{
	return FMath::Max(1.0f, AntRoamRadius);
}

float AJTSMoonGameMode::GetAntMaxHomeRadius() const
{
	return FMath::Max(GetAntRoamRadius(), AntMaxHomeRadius);
}

float AJTSMoonGameMode::GetAntRoamRetargetIntervalMin() const
{
	return FMath::Max(0.1f, AntRoamRetargetIntervalMin);
}

float AJTSMoonGameMode::GetAntRoamRetargetIntervalMax() const
{
	return FMath::Max(GetAntRoamRetargetIntervalMin(), AntRoamRetargetIntervalMax);
}

float AJTSMoonGameMode::GetAntSurfaceDurationMin() const
{
	return FMath::Max(0.1f, AntSurfaceDurationMin);
}

float AJTSMoonGameMode::GetAntSurfaceDurationMax() const
{
	return FMath::Max(GetAntSurfaceDurationMin(), AntSurfaceDurationMax);
}

float AJTSMoonGameMode::GetAntEmergingDuration() const
{
	return FMath::Max(0.0f, AntEmergingDuration);
}

float AJTSMoonGameMode::GetAntHitReactionDuration() const
{
	return FMath::Max(0.0f, AntHitReactionDuration);
}

float AJTSMoonGameMode::GetAntBurrowDuration() const
{
	return FMath::Max(0.0f, AntBurrowDuration);
}

float AJTSMoonGameMode::GetAntFleeSpeed() const
{
	return FMath::Max(1.0f, AntFleeSpeed);
}

float AJTSMoonGameMode::GetAntFleeDurationMin() const
{
	return FMath::Max(0.1f, AntFleeDurationMin);
}

float AJTSMoonGameMode::GetAntFleeDurationMax() const
{
	return FMath::Max(GetAntFleeDurationMin(), AntFleeDurationMax);
}

float AJTSMoonGameMode::GetAntMaxFleeDistance() const
{
	return FMath::Max(1.0f, AntMaxFleeDistance);
}

float AJTSMoonGameMode::GetAntTurnSpeed() const
{
	return FMath::Max(1.0f, AntTurnSpeed);
}

float AJTSMoonGameMode::GetAntChaseSpeed() const
{
	return FMath::Max(0.0f, AntChaseSpeed);
}

float AJTSMoonGameMode::GetAntReturnSpeed() const
{
	return FMath::Max(0.0f, AntReturnSpeed);
}

float AJTSMoonGameMode::GetAntAggroRadius() const
{
	return FMath::Max(0.0f, AntAggroRadius);
}

float AJTSMoonGameMode::GetAntLoseAggroRadius() const
{
	return FMath::Max(0.0f, AntLoseAggroRadius);
}

float AJTSMoonGameMode::GetAntStopDistanceFromPlayer() const
{
	return FMath::Max(0.0f, AntStopDistanceFromPlayer);
}

float AJTSMoonGameMode::GetAntWanderSpeed() const
{
	return FMath::Max(0.0f, AntWanderSpeed);
}

float AJTSMoonGameMode::GetAntWanderRadius() const
{
	return FMath::Max(0.0f, AntWanderRadius);
}

float AJTSMoonGameMode::GetAntWanderRetargetIntervalMin() const
{
	return FMath::Max(0.0f, AntWanderRetargetIntervalMin);
}

float AJTSMoonGameMode::GetAntWanderRetargetIntervalMax() const
{
	return FMath::Max(0.0f, AntWanderRetargetIntervalMax);
}

float AJTSMoonGameMode::GetAntLifetime() const
{
	return FMath::Max(0.0f, AntLifetime);
}

float AJTSMoonGameMode::GetAntGroundTraceStartHeight() const
{
	return FMath::Max(0.0f, AntGroundTraceStartHeight);
}

float AJTSMoonGameMode::GetAntGroundTraceDistance() const
{
	return FMath::Max(1.0f, AntGroundTraceDistance);
}

int32 AJTSMoonGameMode::GetAntPunchHitsToKill() const
{
	return FMath::Max(1, AntPunchHitsToKill);
}

int32 AJTSMoonGameMode::GetAntNestPunchHitsToDestroy() const
{
	return FMath::Max(1, AntNestPunchHitsToDestroy);
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
		TEXT("JumpToSpace Moon Config: Crew=%d FoodRate=%.2f WaterRate=%.2f ResourceCount=%d SpawnRadius=%.1f PickaxeCost=%d BackpackRockCost=%d BackpackOreCost=%d KnifeCost=%d/%d AxeCost=%d/%d LargeYield=%d OreYield=%d PickupMaxDistance=%.0f PickupAcquireRadius=%.0f PickupRetainRadius=%.0f PickupAimRayRadius=%.0f ShipMarkerDistance=%.0f AttackRange=%.0f AntNests=%d"),
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
		GetAntNestCount());

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
