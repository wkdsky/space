// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Modes/JTSMoonGameMode.h"

#include "CollisionQueryParams.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"
#include "space/Components/JTSPlayerEquipmentComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Items/JTSWorldPickupItemType.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Systems/JTSMoonWrapSubsystem.h"
#include "space/UI/JTSPrototypeHUD.h"
#include "space/World/JTSMoonCorpseActor.h"
#include "space/World/JTSRoachNestActor.h"

namespace
{
	constexpr double SecondsPerMinute = 60.0;
}

AJTSMoonGameMode::AJTSMoonGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	DefaultPawnClass = AJTSCharacter::StaticClass();
	PlayerControllerClass = AJTSPlayerController::StaticClass();
	GameStateClass = AJTSGameState::StaticClass();
	HUDClass = AJTSPrototypeHUD::StaticClass();
	RoachNestActorClass = AJTSRoachNestActor::StaticClass();
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
	if (CachedSpacecraft.IsValid())
	{
		return CachedSpacecraft.Get();
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<AJTSSpacecraftActor> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			CachedSpacecraft = *It;
			return *It;
		}
	}

	return nullptr;
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

int32 AJTSMoonGameMode::GetRoachNestCount() const
{
	return FMath::Max(0, RoachNestCount);
}

float AJTSMoonGameMode::GetRoachNestRadiusAroundCorpse() const
{
	return FMath::Max(1.0f, RoachNestRadiusAroundCorpse);
}

float AJTSMoonGameMode::GetRoachNestSpawnWeightNearCorpse() const
{
	return FMath::Clamp(RoachNestSpawnWeightNearCorpse, 0.0f, 1.0f);
}

float AJTSMoonGameMode::GetRoachNestMinDistanceFromCorpse() const
{
	return FMath::Clamp(RoachNestMinDistanceFromCorpse, 0.0f, GetRoachNestRadiusAroundCorpse());
}

float AJTSMoonGameMode::GetRoachNestMinDistanceFromShip() const
{
	return FMath::Max(0.0f, RoachNestMinDistanceFromShip);
}

float AJTSMoonGameMode::GetRoachNestMinSpacing() const
{
	return FMath::Max(1.0f, RoachNestMinSpacing);
}

float AJTSMoonGameMode::GetRoachSpawnChance() const
{
	return FMath::Clamp(RoachSpawnChance, 0.0f, 1.0f);
}

float AJTSMoonGameMode::GetRoachSpawnIntervalMin() const
{
	return FMath::Max(0.1f, RoachSpawnIntervalMin);
}

float AJTSMoonGameMode::GetRoachSpawnIntervalMax() const
{
	return FMath::Max(GetRoachSpawnIntervalMin(), RoachSpawnIntervalMax);
}

float AJTSMoonGameMode::GetRoachSpawnOffset() const
{
	return FMath::Max(1.0f, RoachSpawnOffset);
}

float AJTSMoonGameMode::GetRoachCrawlSpeed() const
{
	return FMath::Max(1.0f, RoachCrawlSpeed);
}

float AJTSMoonGameMode::GetRoachEscapeSpeed() const
{
	return FMath::Max(1.0f, RoachEscapeSpeed);
}

float AJTSMoonGameMode::GetRoachEscapeDuration() const
{
	return FMath::Max(0.1f, RoachEscapeDuration);
}

float AJTSMoonGameMode::GetRoachLifetime() const
{
	return FMath::Max(0.1f, RoachLifetime);
}

float AJTSMoonGameMode::GetRoachEmergingDuration() const
{
	return FMath::Max(0.0f, RoachEmergingDuration);
}

float AJTSMoonGameMode::GetRoachHitReactionDuration() const
{
	return FMath::Max(0.0f, RoachHitReactionDuration);
}

float AJTSMoonGameMode::GetRoachBurrowTime() const
{
	return FMath::Max(0.0f, RoachBurrowTime);
}

float AJTSMoonGameMode::GetRoachGroundTraceStartHeight() const
{
	return FMath::Max(0.0f, RoachGroundTraceStartHeight);
}

float AJTSMoonGameMode::GetRoachGroundTraceDistance() const
{
	return FMath::Max(1.0f, RoachGroundTraceDistance);
}

int32 AJTSMoonGameMode::GetRoachPunchHitsToKill() const
{
	return FMath::Max(1, RoachPunchHitsToKill);
}

int32 AJTSMoonGameMode::GetRoachNestPunchHitsToDestroy() const
{
	return FMath::Max(1, RoachNestPunchHitsToDestroy);
}

bool AJTSMoonGameMode::TryCraftPickaxe(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft)
{
	return TryBuyWorkshopEquipment(Player, Spacecraft, EJTSEquipmentType::Pickaxe);
}

bool AJTSMoonGameMode::TryCraftBackpack(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft)
{
	return TryBuyWorkshopEquipment(Player, Spacecraft, EJTSEquipmentType::Backpack);
}

bool AJTSMoonGameMode::TryCraftKnife(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft)
{
	return TryBuyWorkshopEquipment(Player, Spacecraft, EJTSEquipmentType::Knife);
}

bool AJTSMoonGameMode::TryCraftAxe(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft)
{
	return TryBuyWorkshopEquipment(Player, Spacecraft, EJTSEquipmentType::Axe);
}

bool AJTSMoonGameMode::TryBuyWorkshopEquipment(
	AJTSCharacter* Player,
	AJTSSpacecraftActor* Spacecraft,
	EJTSEquipmentType EquipmentType)
{
	if (EquipmentType != EJTSEquipmentType::Pickaxe
		&& EquipmentType != EJTSEquipmentType::Backpack
		&& EquipmentType != EJTSEquipmentType::Knife
		&& EquipmentType != EJTSEquipmentType::Axe)
	{
		UE_LOG(LogTemp, Warning, TEXT("JumpToSpace Shop Buy: Result=Failed Reason=UnsupportedItem"));
		return false;
	}

	const TCHAR* ItemName = TEXT("Pickaxe");
	int32 RockCost = GetPickaxeRockCost();
	int32 OreCost = 0;
	switch (EquipmentType)
	{
	case EJTSEquipmentType::Backpack:
		ItemName = TEXT("Backpack");
		RockCost = GetBackpackRockCost();
		OreCost = GetBackpackOreCost();
		break;

	case EJTSEquipmentType::Knife:
		ItemName = TEXT("Knife");
		RockCost = GetKnifeRockCost();
		OreCost = GetKnifeOreCost();
		break;

	case EJTSEquipmentType::Axe:
		ItemName = TEXT("Axe");
		RockCost = GetAxeRockCost();
		OreCost = GetAxeOreCost();
		break;

	default:
		break;
	}
	auto LogBuyFailure = [ItemName, RockCost, OreCost](const TCHAR* Reason)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("JumpToSpace Shop Buy: Item=%s RockCost=%d OreCost=%d Result=Failed Reason=%s"),
			ItemName,
			RockCost,
			OreCost,
			Reason);
	};

	const AJTSGameState* const JTSGameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
	if (!IsValid(JTSGameState) || !JTSGameState->IsMoonExploration())
	{
		LogBuyFailure(TEXT("NotMoonExploration"));
		return false;
	}

	if (!IsValid(Player) || !IsValid(Spacecraft))
	{
		LogBuyFailure(TEXT("InvalidPlayerOrShip"));
		return false;
	}

	if (Player->GetNearbySpacecraft() != Spacecraft || !Spacecraft->IsPawnInBoardingRange(Player))
	{
		LogBuyFailure(TEXT("NotNearShip"));
		return false;
	}

	UJTSPlayerEquipmentComponent* const EquipmentComponent = Player->GetEquipmentComponent();
	if (!IsValid(EquipmentComponent))
	{
		LogBuyFailure(TEXT("MissingEquipment"));
		return false;
	}

	TMap<EJTSResourceType, int32> ResourceCosts;
	ResourceCosts.Add(EJTSResourceType::Rock, RockCost);
	if (OreCost > 0)
	{
		ResourceCosts.Add(EJTSResourceType::Ore, OreCost);
	}
	if (!Spacecraft->HasResource(EJTSResourceType::Rock, RockCost)
		|| (OreCost > 0 && !Spacecraft->HasResource(EJTSResourceType::Ore, OreCost)))
	{
		LogBuyFailure(TEXT("NotEnoughResources"));
		return false;
	}

	const bool bCanAutoEquip = !EquipmentComponent->HasEquippedItem(EquipmentType)
		&& EquipmentComponent->HasAvailableSlot();
	if (bCanAutoEquip)
	{
		if (!EquipmentComponent->TryEquipItem(EquipmentType))
		{
			LogBuyFailure(TEXT("AutoEquipFailed"));
			return false;
		}

		if (!Spacecraft->TryConsumeResourceAmounts(ResourceCosts))
		{
			EquipmentComponent->UnequipItem(EquipmentType);
			LogBuyFailure(TEXT("ConsumeFailedRolledBack"));
			return false;
		}

		UE_LOG(LogTemp, Log, TEXT("JumpToSpace Shop Buy: Item=%s Result=AutoEquipped"), ItemName);
		return true;
	}

	EJTSWorldPickupItemType PickupItemType = EJTSWorldPickupItemType::Pickaxe;
	switch (EquipmentType)
	{
	case EJTSEquipmentType::Backpack:
		PickupItemType = EJTSWorldPickupItemType::Backpack;
		break;

	case EJTSEquipmentType::Knife:
		PickupItemType = EJTSWorldPickupItemType::Knife;
		break;

	case EJTSEquipmentType::Axe:
		PickupItemType = EJTSWorldPickupItemType::Axe;
		break;

	default:
		break;
	}
	AJTSWorldPickupActor* const Pickup = AJTSWorldPickupActor::SpawnGameplayDrop(
		GetWorld(),
		PickupItemType,
		Player->GetActorLocation(),
		Player,
		Spacecraft,
		Player->GetActorForwardVector());
	if (!IsValid(Pickup))
	{
		LogBuyFailure(TEXT("DropSpawnFailed"));
		return false;
	}

	if (!Spacecraft->TryConsumeResourceAmounts(ResourceCosts))
	{
		Pickup->Destroy();
		LogBuyFailure(TEXT("ConsumeFailedRolledBack"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("JumpToSpace Shop Buy: Item=%s Result=DroppedNearPlayer"), ItemName);
	return true;
}

void AJTSMoonGameMode::BeginPlay()
{
	Super::BeginPlay();

	FoodConsumptionAccumulator = 0.0;
	WaterConsumptionAccumulator = 0.0;
	CachedSpacecraft.Reset();
	LevelMoonCorpseLandmark.Reset();
	CachedLevelMoonCorpseLandmarks.Reset();
	GeneratedRoachNests.Reset();
	bLevelCorpseLandmarkSearchCompleted = false;
	bMissingSpacecraftLogged = false;

	if (AJTSGameState* const JTSGameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr)
	{
		JTSGameState->SetFailureReason(EJTSFailureReason::None);
		JTSGameState->SetGameplayPhase(EJTSGameplayPhase::MoonExploration);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Jump to Space Moon GameMode could not enter MoonExploration because its GameState is unavailable."));
	}

	if (UWorld* const World = GetWorld())
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("JumpToSpace Moon Config: Crew=%d FoodRate=%.2f WaterRate=%.2f ResourceCount=%d SpawnRadius=%.1f PickaxeCost=%d BackpackRockCost=%d BackpackOreCost=%d KnifeCost=%d/%d AxeCost=%d/%d LargeYield=%d OreYield=%d PickupMaxDistance=%.0f PickupAcquireRadius=%.0f PickupRetainRadius=%.0f PickupAimRayRadius=%.0f ShipMarkerDistance=%.0f AttackRange=%.0f RoachNests=%d"),
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
			GetRoachNestCount());

		World->GetTimerManager().ClearTimer(MoonRuntimeInitializationTimerHandle);
		MoonRuntimeInitializationTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			this,
			&AJTSMoonGameMode::InitializeMoonRuntimeContent);

		World->GetTimerManager().ClearTimer(ExpeditionConsumptionTimerHandle);
		const float ConsumptionInterval = GetConsumptionTickInterval();
		if (ConsumptionInterval > 0.0f)
		{
			World->GetTimerManager().SetTimer(
				ExpeditionConsumptionTimerHandle,
				this,
				&AJTSMoonGameMode::ConsumeExpeditionSupplies,
				ConsumptionInterval,
				true,
				ConsumptionInterval);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Jump to Space Moon GameMode did not start expedition supply consumption because ConsumptionTickInterval is zero."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Jump to Space Moon GameMode could not start its expedition supply timer because its World is unavailable."));
	}
}

void AJTSMoonGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpeditionConsumptionTimerHandle);
		World->GetTimerManager().ClearTimer(MoonRuntimeInitializationTimerHandle);
		ClearGeneratedRoachNests();
	}

	FoodConsumptionAccumulator = 0.0;
	WaterConsumptionAccumulator = 0.0;
	CachedSpacecraft.Reset();
	LevelMoonCorpseLandmark.Reset();
	CachedLevelMoonCorpseLandmarks.Reset();
	bLevelCorpseLandmarkSearchCompleted = false;
	Super::EndPlay(EndPlayReason);
}

void AJTSMoonGameMode::InitializeMoonResources()
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	AJTSMoonResourceSpawner* ResourceSpawner = nullptr;
	int32 ResourceSpawnerCount = 0;
	for (TActorIterator<AJTSMoonResourceSpawner> It(World); It; ++It)
	{
		if (!IsValid(*It))
		{
			continue;
		}

		++ResourceSpawnerCount;
		if (ResourceSpawner == nullptr)
		{
			ResourceSpawner = *It;
		}
	}

	if (ResourceSpawner == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Jump to Space Moon GameMode found no AJTSMoonResourceSpawner. Moon exploration will start without automatically generated resources."));
		return;
	}

	if (ResourceSpawnerCount > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Jump to Space Moon GameMode found %d AJTSMoonResourceSpawner actors; using the first valid spawner only."), ResourceSpawnerCount);
	}

	ResourceSpawner->ApplyMoonSpawnSettings(MoonResourceSpawnSettings);
	ResourceSpawner->SetLandmarkExclusions(
		GetSpacecraft(),
		CachedLevelMoonCorpseLandmarks,
		GeneratedRoachNests);
	ResourceSpawner->GenerateResources();
}

void AJTSMoonGameMode::InitializeMoonRuntimeContent()
{
	// The fixed landmarks must exist before random content so nests and resources can avoid them.
	// This next-tick point runs after the Moon world, wrap subsystem, and level spacecraft are ready.
	InitializeMoonLandmarksAndRoachNests();
	InitializeMoonResources();
}

AJTSMoonCorpseActor* AJTSMoonGameMode::FindLevelCorpseLandmark()
{
	if (bLevelCorpseLandmarkSearchCompleted)
	{
		return LevelMoonCorpseLandmark.Get();
	}

	bLevelCorpseLandmarkSearchCompleted = true;
	LevelMoonCorpseLandmark.Reset();
	CachedLevelMoonCorpseLandmarks.Reset();

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("JumpToSpace Moon Corpse Landmark: Found=false. World unavailable; Roach Nest generation skipped."));
		return nullptr;
	}

	AJTSMoonCorpseActor* SelectedCorpse = nullptr;
	FString SelectedCorpsePath;
	int32 ValidCorpseCount = 0;
	for (TActorIterator<AJTSMoonCorpseActor> CorpseIt(World); CorpseIt; ++CorpseIt)
	{
		AJTSMoonCorpseActor* const Candidate = *CorpseIt;
		if (!IsValid(Candidate))
		{
			continue;
		}

		++ValidCorpseCount;
		CachedLevelMoonCorpseLandmarks.Add(Candidate);
		const FString CandidatePath = Candidate->GetPathName();
		if (!IsValid(SelectedCorpse) || CandidatePath < SelectedCorpsePath)
		{
			SelectedCorpse = Candidate;
			SelectedCorpsePath = CandidatePath;
		}
	}

	if (!IsValid(SelectedCorpse))
	{
		UE_LOG(LogTemp, Warning, TEXT("JumpToSpace Moon Corpse Landmark: Found=false. Roach Nest generation skipped."));
		return nullptr;
	}

	LevelMoonCorpseLandmark = SelectedCorpse;
	const FVector CorpseLocation = SelectedCorpse->GetActorLocation();
	if (ValidCorpseCount > 1)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("JumpToSpace Moon Corpse Landmark: Found=true Count=%d Name=%s Location=(%.0f, %.0f, %.0f) Selection=LexicalPath"),
			ValidCorpseCount,
			*GetNameSafe(SelectedCorpse),
			CorpseLocation.X,
			CorpseLocation.Y,
			CorpseLocation.Z);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("JumpToSpace Moon Corpse Landmark: Found=true Name=%s Location=(%.0f, %.0f, %.0f)"),
			*GetNameSafe(SelectedCorpse),
			CorpseLocation.X,
			CorpseLocation.Y,
			CorpseLocation.Z);
	}

	return SelectedCorpse;
}

void AJTSMoonGameMode::ClearGeneratedRoachNests()
{
	for (TWeakObjectPtr<AJTSRoachNestActor>& Nest : GeneratedRoachNests)
	{
		if (Nest.IsValid())
		{
			Nest->Destroy();
		}
	}
	GeneratedRoachNests.Reset();
}

bool AJTSMoonGameMode::ResolveMoonGroundLocation(const FVector& CandidateLocation, FVector& OutGroundLocation) const
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(JTSMoonLandmarkGroundTrace), false, this);
	if (AJTSSpacecraftActor* const Spacecraft = GetSpacecraft())
	{
		TraceParams.AddIgnoredActor(Spacecraft);
	}
	for (const TWeakObjectPtr<AJTSMoonCorpseActor>& Corpse : CachedLevelMoonCorpseLandmarks)
	{
		if (Corpse.IsValid())
		{
			TraceParams.AddIgnoredActor(Corpse.Get());
		}
	}
	for (const TWeakObjectPtr<AJTSRoachNestActor>& Nest : GeneratedRoachNests)
	{
		if (Nest.IsValid())
		{
			TraceParams.AddIgnoredActor(Nest.Get());
		}
	}
	for (TActorIterator<APawn> PawnIt(World); PawnIt; ++PawnIt)
	{
		if (IsValid(*PawnIt))
		{
			TraceParams.AddIgnoredActor(*PawnIt);
		}
	}

	const float StartHeight = GetRoachGroundTraceStartHeight();
	const float TraceDistance = GetRoachGroundTraceDistance();
	FHitResult GroundHit;
	if (!World->LineTraceSingleByChannel(
		GroundHit,
		CandidateLocation + FVector(0.0f, 0.0f, StartHeight),
		CandidateLocation + FVector(0.0f, 0.0f, StartHeight - TraceDistance),
		ECC_Visibility,
		TraceParams)
		|| !GroundHit.bBlockingHit)
	{
		return false;
	}

	OutGroundLocation = GroundHit.ImpactPoint;
	return true;
}

bool AJTSMoonGameMode::IsRoachNestCandidateFarFromShip(
	const FVector2D& CandidateLogicalPosition,
	const FVector2D& ShipLogicalPosition) const
{
	const UWorld* const World = GetWorld();
	const UJTSMoonWrapSubsystem* const MoonWrap = World != nullptr ? World->GetSubsystem<UJTSMoonWrapSubsystem>() : nullptr;
	const FVector2D Delta = IsValid(MoonWrap) && MoonWrap->IsConfiguredForMoon()
		? MoonWrap->ShortestWrappedDelta2D(ShipLogicalPosition, CandidateLogicalPosition)
		: CandidateLogicalPosition - ShipLogicalPosition;
	return Delta.Size() >= GetRoachNestMinDistanceFromShip();
}

void AJTSMoonGameMode::InitializeMoonLandmarksAndRoachNests()
{
	ClearGeneratedRoachNests();

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("JumpToSpace Moon Roach Nests: initialization skipped because the World is unavailable."));
		return;
	}

	UJTSMoonWrapSubsystem* const MoonWrap = World->GetSubsystem<UJTSMoonWrapSubsystem>();
	if (!IsValid(MoonWrap) || !MoonWrap->IsConfiguredForMoon())
	{
		UE_LOG(LogTemp, Warning, TEXT("JumpToSpace Moon Roach Nests: initialization skipped because the Moon Wrap configuration is unavailable."));
		return;
	}

	AJTSSpacecraftActor* const Spacecraft = GetSpacecraft();
	if (!IsValid(Spacecraft))
	{
		UE_LOG(LogTemp, Warning, TEXT("JumpToSpace Moon Roach Nests: initialization skipped because the spacecraft is unavailable."));
		return;
	}

	AJTSMoonCorpseActor* const Corpse = FindLevelCorpseLandmark();
	if (!IsValid(Corpse))
	{
		return;
	}

	if (RoachNestActorClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("JumpToSpace Moon Roach Nests: initialization skipped because the nest class is unavailable."));
		return;
	}

	const FVector ShipPhysicalLocation = Spacecraft->GetActorLocation();
	const FVector2D ShipLogicalPosition = MoonWrap->GetLogicalPositionFromWorld(ShipPhysicalLocation);
	const int32 RandomSeed = static_cast<int32>(FPlatformTime::Cycles64() & static_cast<uint64>(MAX_uint32));
	FRandomStream RandomStream(RandomSeed);
	const FVector CorpsePhysicalLocation = Corpse->GetActorLocation();
	const FVector2D CorpseLogicalPosition = MoonWrap->GetLogicalPositionFromWorld(CorpsePhysicalLocation);
	TArray<FVector2D> AcceptedNestLogicalPositions;
	const int32 DesiredNestCount = GetRoachNestCount();
	const int32 MaxNestAttempts = FMath::Max(64, DesiredNestCount * 48);
	const FBox CorpseBounds = Corpse->GetComponentsBoundingBox(true);
	const FVector CorpseBoundsExtent = CorpseBounds.IsValid ? CorpseBounds.GetExtent() : FVector::ZeroVector;
	const float CorpseMeshClearance = FVector2D(CorpseBoundsExtent.X, CorpseBoundsExtent.Y).Size() + 50.0f;
	const float InnerNestRadius = FMath::Max(GetRoachNestMinDistanceFromCorpse(), CorpseMeshClearance);
	const float OuterNestRadius = FMath::Max(InnerNestRadius, GetRoachNestRadiusAroundCorpse());
	const float NearCorpseExponent = 1.0f + GetRoachNestSpawnWeightNearCorpse() * 3.0f;

	for (int32 Attempt = 0; Attempt < MaxNestAttempts && GeneratedRoachNests.Num() < DesiredNestCount; ++Attempt)
	{
		const float Angle = RandomStream.FRandRange(0.0f, UE_TWO_PI);
		const float RadiusAlpha = FMath::Pow(RandomStream.FRand(), NearCorpseExponent);
		const float Radius = FMath::Lerp(InnerNestRadius, OuterNestRadius, RadiusAlpha);
		FVector2D CandidateLogicalPosition = CorpseLogicalPosition + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
		CandidateLogicalPosition = MoonWrap->CanonicalizePosition2D(CandidateLogicalPosition);
		if (!IsRoachNestCandidateFarFromShip(CandidateLogicalPosition, ShipLogicalPosition))
		{
			continue;
		}

		bool bOverlapsExistingNest = false;
		for (const FVector2D& ExistingLogicalPosition : AcceptedNestLogicalPositions)
		{
			const FVector2D NestDelta = MoonWrap->ShortestWrappedDelta2D(ExistingLogicalPosition, CandidateLogicalPosition);
			if (NestDelta.Size() < GetRoachNestMinSpacing())
			{
				bOverlapsExistingNest = true;
				break;
			}
		}
		if (bOverlapsExistingNest)
		{
			continue;
		}

		const FVector2D CandidatePhysicalXY = MoonWrap->GetNearestPhysicalImage(
			FVector2D(CorpsePhysicalLocation.X, CorpsePhysicalLocation.Y),
			CandidateLogicalPosition);
		FVector NestGroundLocation;
		if (!ResolveMoonGroundLocation(
			FVector(CandidatePhysicalXY.X, CandidatePhysicalXY.Y, CorpsePhysicalLocation.Z),
			NestGroundLocation))
		{
			continue;
		}

		const FTransform NestTransform(FRotator(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f), NestGroundLocation);
		AJTSRoachNestActor* const Nest = World->SpawnActorDeferred<AJTSRoachNestActor>(
			RoachNestActorClass,
			NestTransform,
			Corpse,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!IsValid(Nest))
		{
			continue;
		}

		Nest->FinishSpawning(NestTransform);
		Nest->AdjustToGround(NestGroundLocation);
		GeneratedRoachNests.Add(Nest);
		AcceptedNestLogicalPositions.Add(CandidateLogicalPosition);
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("JumpToSpace Moon Roach Nests: Requested=%d Spawned=%d"),
		DesiredNestCount,
		GeneratedRoachNests.Num());
}

void AJTSMoonGameMode::ConsumeExpeditionSupplies()
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	AJTSSpacecraftActor* const Spacecraft = GetSpacecraft();

	if (!IsValid(Spacecraft))
	{
		FoodConsumptionAccumulator = 0.0;
		WaterConsumptionAccumulator = 0.0;
		if (!bMissingSpacecraftLogged)
		{
			UE_LOG(LogTemp, Error, TEXT("Jump to Space Moon GameMode cannot consume expedition supplies because no spacecraft was found."));
			bMissingSpacecraftLogged = true;
		}
		return;
	}
	bMissingSpacecraftLogged = false;

	const double ConsumptionTickSeconds = static_cast<double>(GetConsumptionTickInterval());
	const double ConsumptionUnit = static_cast<double>(GetMinimumConsumptionUnit());
	if (ConsumptionTickSeconds <= 0.0 || ConsumptionUnit <= 0.0)
	{
		return;
	}

	const double SafeCrewCount = static_cast<double>(GetCrewCount());
	FoodConsumptionAccumulator += static_cast<double>(GetFoodConsumptionPerPersonPerMinute())
		* SafeCrewCount * ConsumptionTickSeconds / SecondsPerMinute;
	WaterConsumptionAccumulator += static_cast<double>(GetWaterConsumptionPerPersonPerMinute())
		* SafeCrewCount * ConsumptionTickSeconds / SecondsPerMinute;

	const int32 FoodResourcesDue = GetWholeConsumptionUnits(FoodConsumptionAccumulator, ConsumptionUnit);
	const int32 WaterResourcesDue = GetWholeConsumptionUnits(WaterConsumptionAccumulator, ConsumptionUnit);
	if (FoodResourcesDue <= 0 && WaterResourcesDue <= 0)
	{
		return;
	}

	const int32 FoodResourcesToConsume = FMath::Min(
		FoodResourcesDue,
		Spacecraft->GetResourceAmount(EJTSResourceType::Food));
	const int32 WaterResourcesToConsume = FMath::Min(
		WaterResourcesDue,
		Spacecraft->GetResourceAmount(EJTSResourceType::Water));

	if (FoodResourcesToConsume > 0)
	{
		Spacecraft->TryConsumeResource(EJTSResourceType::Food, FoodResourcesToConsume);
	}
	if (WaterResourcesToConsume > 0)
	{
		Spacecraft->TryConsumeResource(EJTSResourceType::Water, WaterResourcesToConsume);
	}

	FoodConsumptionAccumulator = FMath::Max(
		0.0,
		FoodConsumptionAccumulator - static_cast<double>(FoodResourcesDue) * ConsumptionUnit);
	WaterConsumptionAccumulator = FMath::Max(
		0.0,
		WaterConsumptionAccumulator - static_cast<double>(WaterResourcesDue) * ConsumptionUnit);
}

int32 AJTSMoonGameMode::GetWholeConsumptionUnits(double Accumulator, double MinimumConsumptionUnit)
{
	if (!FMath::IsFinite(Accumulator)
		|| !FMath::IsFinite(MinimumConsumptionUnit)
		|| Accumulator <= 0.0
		|| MinimumConsumptionUnit <= 0.0)
	{
		return 0;
	}

	const double WholeConsumptionUnits = FMath::FloorToDouble((Accumulator / MinimumConsumptionUnit) + 1.0e-9);
	return WholeConsumptionUnits >= static_cast<double>(MAX_int32)
		? MAX_int32
		: static_cast<int32>(WholeConsumptionUnits);
}
