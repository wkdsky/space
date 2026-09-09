// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSMoonSurfaceController.h"

#include "CollisionQueryParams.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RandomStream.h"
#include "space/Components/JTSPlayerEquipmentComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Items/JTSWorldPickupItemType.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "space/Player/JTSCharacter.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Systems/JTSMoonWrapSubsystem.h"
#include "space/World/JTSMoonCorpseActor.h"
#include "space/World/JTSMoonResourceSpawner.h"
#include "space/World/JTSMoonWorldActor.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSRoachActor.h"
#include "space/World/JTSRoachNestActor.h"
#include "space/World/JTSSpaceWorldManager.h"

namespace
{
	constexpr double SecondsPerMinute = 60.0;

	template <typename TActorType>
	void GetSurfaceActors(const ULevel* SurfaceLevel, TArray<TActorType*>& OutActors)
	{
		OutActors.Reset();
		if (!IsValid(SurfaceLevel))
		{
			return;
		}

		for (AActor* const Actor : SurfaceLevel->Actors)
		{
			if (TActorType* const TypedActor = Cast<TActorType>(Actor); IsValid(TypedActor))
			{
				OutActors.Add(TypedActor);
			}
		}
	}
}

AJTSMoonSurfaceController::AJTSMoonSurfaceController()
{
	PrimaryActorTick.bCanEverTick = false;
	MoonGameplaySettingsClass = AJTSMoonGameMode::StaticClass();
}

AJTSMoonSurfaceController* AJTSMoonSurfaceController::FindMoonSurfaceController(
	const UObject* WorldContextObject,
	FName RequestedPlanetId)
{
	if (const AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(WorldContextObject))
	{
		const FName PlanetIdToFind = !RequestedPlanetId.IsNone()
			? RequestedPlanetId
			: (IsValid(Manager->GetCurrentPlanet()) ? Manager->GetCurrentPlanet()->GetPlanetId() : NAME_None);
		if (AJTSMoonSurfaceController* const Controller = Manager->GetSurfaceController(PlanetIdToFind))
		{
			return Controller;
		}
	}

	const UWorld* const World = WorldContextObject != nullptr ? WorldContextObject->GetWorld() : nullptr;
	if (const AJTSMoonGameMode* const LegacyGameMode = World != nullptr ? World->GetAuthGameMode<AJTSMoonGameMode>() : nullptr)
	{
		AJTSMoonSurfaceController* const Controller = LegacyGameMode->GetMoonSurfaceController();
		if (IsValid(Controller)
			&& (RequestedPlanetId.IsNone() || Controller->GetPlanetId() == RequestedPlanetId))
		{
			return Controller;
		}
	}

	return nullptr;
}

FName AJTSMoonSurfaceController::GetPlanetId() const
{
	return PlanetId;
}

bool AJTSMoonSurfaceController::IsSurfaceGameplayInitialized() const
{
	return bSurfaceGameplayInitialized;
}

void AJTSMoonSurfaceController::ConfigureLegacyRuntime(AJTSMoonGameMode* InLegacyGameMode)
{
	LegacySettingsSource = InLegacyGameMode;
	if (IsValid(InLegacyGameMode))
	{
		MoonGameplaySettingsClass = InLegacyGameMode->GetClass();
	}

	if (UJTSMoonWrapSubsystem* const MoonWrap = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UJTSMoonWrapSubsystem>() : nullptr)
	{
		MoonWrap->RefreshConfiguration();
	}
}

void AJTSMoonSurfaceController::SetOwningPlanet(AJTSPlanetAnchor* InOwningPlanet)
{
	OwningPlanet = InOwningPlanet;
	if (IsValid(InOwningPlanet) && !InOwningPlanet->GetPlanetId().IsNone())
	{
		PlanetId = InOwningPlanet->GetPlanetId();
	}

	if (UJTSMoonWrapSubsystem* const MoonWrap = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UJTSMoonWrapSubsystem>() : nullptr)
	{
		MoonWrap->RefreshConfiguration();
	}
}

void AJTSMoonSurfaceController::SetMoonGameplaySettingsClass(TSubclassOf<AJTSMoonGameMode> InMoonGameplaySettingsClass)
{
	if (!LegacySettingsSource.IsValid() && InMoonGameplaySettingsClass != nullptr)
	{
		MoonGameplaySettingsClass = InMoonGameplaySettingsClass;
	}
}

const AJTSMoonGameMode* AJTSMoonSurfaceController::GetMoonSettings() const
{
	if (LegacySettingsSource.IsValid())
	{
		return LegacySettingsSource.Get();
	}

	TSubclassOf<AJTSMoonGameMode> SettingsClass = MoonGameplaySettingsClass;
	if (SettingsClass == nullptr)
	{
		SettingsClass = AJTSMoonGameMode::StaticClass();
	}
	return SettingsClass != nullptr ? SettingsClass->GetDefaultObject<AJTSMoonGameMode>() : nullptr;
}

AJTSSpacecraftActor* AJTSMoonSurfaceController::GetSpacecraft() const
{
	if (CachedSpacecraft.IsValid())
	{
		return CachedSpacecraft.Get();
	}

	TArray<AJTSSpacecraftActor*> SurfaceSpacecraft;
	GetSurfaceActors(GetSurfaceLevel(), SurfaceSpacecraft);
	if (SurfaceSpacecraft.IsEmpty())
	{
		return nullptr;
	}

	SurfaceSpacecraft.Sort([](const AJTSSpacecraftActor& Left, const AJTSSpacecraftActor& Right)
	{
		return Left.GetPathName() < Right.GetPathName();
	});
	if (SurfaceSpacecraft.Num() > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface %s has %d spacecraft actors; using %s."),
			*PlanetId.ToString(), SurfaceSpacecraft.Num(), *GetNameSafe(SurfaceSpacecraft[0]));
	}

	CachedSpacecraft = SurfaceSpacecraft[0];
	return CachedSpacecraft.Get();
}

void AJTSMoonSurfaceController::SetSurfaceSpacecraft(AJTSSpacecraftActor* InSpacecraft)
{
	CachedSpacecraft = InSpacecraft;
	RegisterSurfaceRuntimeActor(InSpacecraft);
}

void AJTSMoonSurfaceController::RegisterSurfaceRuntimeActor(AActor* RuntimeActor)
{
	if (!IsValid(RuntimeActor))
	{
		return;
	}

	RegisteredSurfaceRuntimeActors.RemoveAll([](const TWeakObjectPtr<AActor>& Candidate)
	{
		return !Candidate.IsValid();
	});
	RegisteredSurfaceRuntimeActors.AddUnique(RuntimeActor);
}

AJTSMoonWorldActor* AJTSMoonSurfaceController::GetMoonWorldActor() const
{
	if (IsValid(MoonWorldActor))
	{
		return MoonWorldActor.Get();
	}
	if (CachedMoonWorld.IsValid())
	{
		return CachedMoonWorld.Get();
	}

	TArray<AJTSMoonWorldActor*> SurfaceMoonWorldActors;
	GetSurfaceActors(GetSurfaceLevel(), SurfaceMoonWorldActors);
	if (SurfaceMoonWorldActors.IsEmpty())
	{
		return nullptr;
	}

	SurfaceMoonWorldActors.Sort([](const AJTSMoonWorldActor& Left, const AJTSMoonWorldActor& Right)
	{
		return Left.GetPathName() < Right.GetPathName();
	});
	if (SurfaceMoonWorldActors.Num() > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface %s has %d JTSMoonWorldActor instances; using %s."),
			*PlanetId.ToString(), SurfaceMoonWorldActors.Num(), *GetNameSafe(SurfaceMoonWorldActors[0]));
	}

	CachedMoonWorld = SurfaceMoonWorldActors[0];
	return CachedMoonWorld.Get();
}

bool AJTSMoonSurfaceController::OwnsSurfaceActor(const AActor* Candidate) const
{
	if (!IsValid(Candidate))
	{
		return false;
	}

	if (Candidate->GetLevel() == GetSurfaceLevel())
	{
		return true;
	}

	return RegisteredSurfaceRuntimeActors.ContainsByPredicate([Candidate](const TWeakObjectPtr<AActor>& RegisteredActor)
	{
		return RegisteredActor.Get() == Candidate;
	});
}

ULevel* AJTSMoonSurfaceController::GetSurfaceLevel() const
{
	return GetLevel();
}

FTransform AJTSMoonSurfaceController::GetSurfacePlayerSpawnTransform(const FTransform& FallbackTransform) const
{
	if (IsValid(SurfacePlayerSpawnAnchor))
	{
		return SurfacePlayerSpawnAnchor->GetActorTransform();
	}

	return bUseSurfacePlayerSpawnTransform ? SurfacePlayerSpawnTransform : FallbackTransform;
}

FTransform AJTSMoonSurfaceController::GetSurfaceSpacecraftSpawnTransform(const FTransform& FallbackTransform) const
{
	if (IsValid(SurfaceSpacecraftSpawnAnchor))
	{
		return SurfaceSpacecraftSpawnAnchor->GetActorTransform();
	}

	return bUseSurfaceSpacecraftSpawnTransform ? SurfaceSpacecraftSpawnTransform : FallbackTransform;
}

void AJTSMoonSurfaceController::BeginPlay()
{
	Super::BeginPlay();

	if (UJTSMoonWrapSubsystem* const MoonWrap = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UJTSMoonWrapSubsystem>() : nullptr)
	{
		MoonWrap->RefreshConfiguration();
	}

	if (AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
	{
		Manager->RegisterSurfaceController(this);
	}
}

void AJTSMoonSurfaceController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpeditionConsumptionTimerHandle);
		World->GetTimerManager().ClearTimer(SurfaceInitializationTimerHandle);
	}

	if (AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
	{
		Manager->UnregisterSurfaceController(this);
	}

	ClearGeneratedAntNests();
	CachedSpacecraft.Reset();
	CachedMoonWorld.Reset();
	LevelMoonCorpseLandmark.Reset();
	CachedLevelMoonCorpseLandmarks.Reset();
	RegisteredSurfaceRuntimeActors.Reset();
	bLevelCorpseLandmarkSearchCompleted = false;
	bSurfaceGameplayInitializationRequested = false;
	bSurfaceGameplayInitialized = false;

	Super::EndPlay(EndPlayReason);
}

void AJTSMoonSurfaceController::RequestSurfaceGameplayInitialization()
{
	if (bSurfaceGameplayInitialized || bSurfaceGameplayInitializationRequested)
	{
		return;
	}

	bSurfaceGameplayInitializationRequested = true;
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &AJTSMoonSurfaceController::AttemptSurfaceGameplayInitialization);
	}
}

void AJTSMoonSurfaceController::AttemptSurfaceGameplayInitialization()
{
	InitializeSurfaceGameplay();
}

bool AJTSMoonSurfaceController::InitializeSurfaceGameplay()
{
	bSurfaceGameplayInitializationRequested = false;
	if (bSurfaceGameplayInitialized)
	{
		return true;
	}

	UWorld* const World = GetWorld();
	const AJTSMoonGameMode* const MoonSettings = GetMoonSettings();
	AJTSMoonWorldActor* const MoonWorld = GetMoonWorldActor();
	UJTSMoonWrapSubsystem* const MoonWrap = World != nullptr ? World->GetSubsystem<UJTSMoonWrapSubsystem>() : nullptr;
	AJTSSpacecraftActor* const Spacecraft = GetSpacecraft();
	if (IsValid(Spacecraft))
	{
		Spacecraft->RestoreStorageForMoonTravel();
	}
	if (World == nullptr
		|| !IsValid(MoonSettings)
		|| !IsValid(MoonWorld)
		|| !IsValid(MoonWrap)
		|| !MoonWrap->IsConfiguredForMoon()
		|| !IsValid(Spacecraft))
	{
		ScheduleInitializationRetry();
		return false;
	}

	FoodConsumptionAccumulator = 0.0;
	WaterConsumptionAccumulator = 0.0;
	bMissingSpacecraftLogged = false;
	LevelMoonCorpseLandmark.Reset();
	CachedLevelMoonCorpseLandmarks.Reset();
	bLevelCorpseLandmarkSearchCompleted = false;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("JumpToSpace Moon Surface Ready: Planet=%s ResourceCount=%d SpawnRadius=%.1f AntNests=%d"),
		*PlanetId.ToString(),
		FMath::Max(0, MoonSettings->GetMoonResourceSpawnSettings().TotalResourceCount),
		FMath::Max(0.0f, MoonSettings->GetMoonResourceSpawnSettings().SpawnRadius),
		MoonSettings->GetAntNestCount());

	// Landmarks must exist before procedural nests and resources derive their exclusion zones.
	InitializeMoonLandmarksAndAntNests();
	InitializeMoonResources();

	World->GetTimerManager().ClearTimer(ExpeditionConsumptionTimerHandle);
	const float ConsumptionInterval = MoonSettings->GetConsumptionTickInterval();
	if (ConsumptionInterval > 0.0f)
	{
		World->GetTimerManager().SetTimer(
			ExpeditionConsumptionTimerHandle,
			this,
			&AJTSMoonSurfaceController::ConsumeExpeditionSupplies,
			ConsumptionInterval,
			true,
			ConsumptionInterval);
	}

	if (AJTSGameState* const GameState = World->GetGameState<AJTSGameState>())
	{
		GameState->SetFailureReason(EJTSFailureReason::None);
		GameState->SetGameplayPhase(EJTSGameplayPhase::MoonExploration);
	}

	bSurfaceGameplayInitialized = true;
	if (AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
	{
		Manager->NotifySurfaceGameplayInitialized(this);
	}
	return true;
}

void AJTSMoonSurfaceController::ScheduleInitializationRetry()
{
	if (bSurfaceGameplayInitialized || bSurfaceGameplayInitializationRequested)
	{
		return;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	bSurfaceGameplayInitializationRequested = true;
	World->GetTimerManager().SetTimer(
		SurfaceInitializationTimerHandle,
		this,
		&AJTSMoonSurfaceController::AttemptSurfaceGameplayInitialization,
		0.10f,
		false);
}

void AJTSMoonSurfaceController::InitializeMoonResources()
{
	const AJTSMoonGameMode* const MoonSettings = GetMoonSettings();
	if (!IsValid(MoonSettings))
	{
		return;
	}

	TArray<AJTSMoonResourceSpawner*> ResourceSpawners;
	GetSurfaceActors(GetSurfaceLevel(), ResourceSpawners);
	if (ResourceSpawners.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface %s has no AJTSMoonResourceSpawner."), *PlanetId.ToString());
		return;
	}

	ResourceSpawners.Sort([](const AJTSMoonResourceSpawner& Left, const AJTSMoonResourceSpawner& Right)
	{
		return Left.GetPathName() < Right.GetPathName();
	});
	if (ResourceSpawners.Num() > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface %s has %d resource spawners; using %s."),
			*PlanetId.ToString(), ResourceSpawners.Num(), *GetNameSafe(ResourceSpawners[0]));
	}

	AJTSMoonResourceSpawner* const ResourceSpawner = ResourceSpawners[0];
	ResourceSpawner->ApplyMoonSpawnSettings(MoonSettings->GetMoonResourceSpawnSettings());
	ResourceSpawner->SetLandmarkExclusions(GetSpacecraft(), CachedLevelMoonCorpseLandmarks, GeneratedAntNests);
	ResourceSpawner->GenerateResources();
}

AJTSMoonCorpseActor* AJTSMoonSurfaceController::FindLevelCorpseLandmark()
{
	if (bLevelCorpseLandmarkSearchCompleted)
	{
		return LevelMoonCorpseLandmark.Get();
	}

	bLevelCorpseLandmarkSearchCompleted = true;
	LevelMoonCorpseLandmark.Reset();
	CachedLevelMoonCorpseLandmarks.Reset();

	TArray<AJTSMoonCorpseActor*> Corpses;
	GetSurfaceActors(GetSurfaceLevel(), Corpses);
	Corpses.Sort([](const AJTSMoonCorpseActor& Left, const AJTSMoonCorpseActor& Right)
	{
		return Left.GetPathName() < Right.GetPathName();
	});
	for (AJTSMoonCorpseActor* const Corpse : Corpses)
	{
		CachedLevelMoonCorpseLandmarks.Add(Corpse);
	}

	if (Corpses.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface %s has no corpse landmark; Ant Nest generation is skipped."), *PlanetId.ToString());
		return nullptr;
	}

	LevelMoonCorpseLandmark = Corpses[0];
	if (Corpses.Num() > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface %s has %d corpse landmarks; using %s."),
			*PlanetId.ToString(), Corpses.Num(), *GetNameSafe(Corpses[0]));
	}
	return Corpses[0];
}

void AJTSMoonSurfaceController::ClearGeneratedAntNests()
{
	for (TWeakObjectPtr<AJTSRoachNestActor>& Nest : GeneratedAntNests)
	{
		if (Nest.IsValid())
		{
			Nest->Destroy();
		}
	}
	GeneratedAntNests.Reset();
}

bool AJTSMoonSurfaceController::ResolveMoonGroundLocation(
	const FVector& CandidateLocation,
	FVector& OutGroundLocation,
	const AActor* AdditionalIgnoredActor) const
{
	UWorld* const World = GetWorld();
	const AJTSMoonGameMode* const MoonSettings = GetMoonSettings();
	if (World == nullptr || !IsValid(MoonSettings))
	{
		return false;
	}

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(JTSMoonAntGroundTrace), false, this);
	if (IsValid(AdditionalIgnoredActor))
	{
		TraceParams.AddIgnoredActor(AdditionalIgnoredActor);
	}
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
	for (const TWeakObjectPtr<AJTSRoachNestActor>& Nest : GeneratedAntNests)
	{
		if (Nest.IsValid())
		{
			TraceParams.AddIgnoredActor(Nest.Get());
		}
	}
	if (APawn* const PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		TraceParams.AddIgnoredActor(PlayerPawn);
	}
	for (AActor* const Actor : GetSurfaceLevel()->Actors)
	{
		if (AJTSRoachActor* const Ant = Cast<AJTSRoachActor>(Actor); IsValid(Ant))
		{
			TraceParams.AddIgnoredActor(Ant);
		}
	}

	FHitResult GroundHit;
	const float StartHeight = MoonSettings->GetAntGroundTraceStartHeight();
	const float TraceDistance = MoonSettings->GetAntGroundTraceDistance();
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

bool AJTSMoonSurfaceController::IsAntNestCandidateFarFromShip(
	const FVector2D& CandidateLogicalPosition,
	const FVector2D& ShipLogicalPosition) const
{
	const UWorld* const World = GetWorld();
	const UJTSMoonWrapSubsystem* const MoonWrap = World != nullptr ? World->GetSubsystem<UJTSMoonWrapSubsystem>() : nullptr;
	const AJTSMoonGameMode* const MoonSettings = GetMoonSettings();
	if (!IsValid(MoonSettings))
	{
		return false;
	}

	const FVector2D Delta = IsValid(MoonWrap) && MoonWrap->IsConfiguredForMoon()
		? MoonWrap->ShortestWrappedDelta2D(ShipLogicalPosition, CandidateLogicalPosition)
		: CandidateLogicalPosition - ShipLogicalPosition;
	return Delta.Size() >= MoonSettings->GetAntNestMinDistanceFromShip();
}

void AJTSMoonSurfaceController::InitializeMoonLandmarksAndAntNests()
{
	ClearGeneratedAntNests();

	UWorld* const World = GetWorld();
	const AJTSMoonGameMode* const MoonSettings = GetMoonSettings();
	UJTSMoonWrapSubsystem* const MoonWrap = World != nullptr ? World->GetSubsystem<UJTSMoonWrapSubsystem>() : nullptr;
	AJTSSpacecraftActor* const Spacecraft = GetSpacecraft();
	if (World == nullptr
		|| !IsValid(MoonSettings)
		|| !IsValid(MoonWrap)
		|| !MoonWrap->IsConfiguredForMoon()
		|| !IsValid(Spacecraft))
	{
		return;
	}

	AJTSMoonCorpseActor* const Corpse = FindLevelCorpseLandmark();
	if (!IsValid(Corpse))
	{
		return;
	}

	const TSubclassOf<AJTSRoachNestActor> NestActorClass = MoonSettings->GetAntNestActorClass();
	if (NestActorClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface %s has no Ant Nest class configured."), *PlanetId.ToString());
		return;
	}

	const FVector ShipPhysicalLocation = Spacecraft->GetActorLocation();
	const FVector2D ShipLogicalPosition = MoonWrap->GetLogicalPositionFromWorld(ShipPhysicalLocation);
	FRandomStream RandomStream(static_cast<int32>(FPlatformTime::Cycles64() & static_cast<uint64>(MAX_uint32)));
	const FVector CorpsePhysicalLocation = Corpse->GetActorLocation();
	const FVector2D CorpseLogicalPosition = MoonWrap->GetLogicalPositionFromWorld(CorpsePhysicalLocation);
	TArray<FVector2D> AcceptedNestLogicalPositions;
	const int32 DesiredNestCount = MoonSettings->GetAntNestCount();
	const int32 MaxNestAttempts = FMath::Max(64, DesiredNestCount * 48);
	const FBox CorpseBounds = Corpse->GetComponentsBoundingBox(true);
	const FVector CorpseBoundsExtent = CorpseBounds.IsValid ? CorpseBounds.GetExtent() : FVector::ZeroVector;
	const float CorpseMeshClearance = FVector2D(CorpseBoundsExtent.X, CorpseBoundsExtent.Y).Size() + 50.0f;
	const float InnerNestRadius = FMath::Max(MoonSettings->GetAntNestMinDistanceFromCorpse(), CorpseMeshClearance);
	const float OuterNestRadius = FMath::Max(InnerNestRadius, MoonSettings->GetAntNestOuterRadiusAroundCorpse());
	const float InnerZoneMaxRadius = FMath::Max(InnerNestRadius, OuterNestRadius * 0.45f);
	const float MidZoneMinRadius = FMath::Clamp(OuterNestRadius * 0.35f, InnerNestRadius, OuterNestRadius);
	const float MidZoneMaxRadius = FMath::Max(MidZoneMinRadius, OuterNestRadius * 0.75f);
	const float OuterZoneMinRadius = FMath::Clamp(OuterNestRadius * 0.65f, InnerNestRadius, OuterNestRadius);
	const float InnerWeight = MoonSettings->GetAntNestInnerWeight();
	const float MidWeight = MoonSettings->GetAntNestMidWeight();
	const float OuterWeight = MoonSettings->GetAntNestOuterWeight();
	const float TotalWeight = InnerWeight + MidWeight + OuterWeight;

	auto ChooseNestRadius = [&RandomStream,
		InnerNestRadius,
		OuterNestRadius,
		InnerZoneMaxRadius,
		MidZoneMinRadius,
		MidZoneMaxRadius,
		OuterZoneMinRadius,
		InnerWeight,
		MidWeight,
		TotalWeight]()
	{
		float ZoneMinRadius = InnerNestRadius;
		float ZoneMaxRadius = InnerZoneMaxRadius;
		const float Selection = TotalWeight > KINDA_SMALL_NUMBER ? RandomStream.FRandRange(0.0f, TotalWeight) : 0.0f;
		if (TotalWeight > KINDA_SMALL_NUMBER && Selection >= InnerWeight)
		{
			if (Selection < InnerWeight + MidWeight)
			{
				ZoneMinRadius = MidZoneMinRadius;
				ZoneMaxRadius = MidZoneMaxRadius;
			}
			else
			{
				ZoneMinRadius = OuterZoneMinRadius;
				ZoneMaxRadius = OuterNestRadius;
			}
		}

		return FMath::Clamp(
			RandomStream.FRandRange(ZoneMinRadius, ZoneMaxRadius) + RandomStream.FRandRange(-30.0f, 30.0f),
			InnerNestRadius,
			OuterNestRadius);
	};

	for (int32 Attempt = 0; Attempt < MaxNestAttempts && GeneratedAntNests.Num() < DesiredNestCount; ++Attempt)
	{
		const float Angle = RandomStream.FRandRange(0.0f, UE_TWO_PI);
		FVector2D CandidateLogicalPosition = MoonWrap->CanonicalizePosition2D(
			CorpseLogicalPosition + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * ChooseNestRadius());
		if (!IsAntNestCandidateFarFromShip(CandidateLogicalPosition, ShipLogicalPosition))
		{
			continue;
		}

		const float CandidateMinSpacing = MoonSettings->GetAntNestBaseMinSpacing()
			* RandomStream.FRandRange(
				MoonSettings->GetAntNestCandidateSpacingScaleMin(),
				MoonSettings->GetAntNestCandidateSpacingScaleMax());
		bool bOverlapsExistingNest = false;
		for (const FVector2D& ExistingLogicalPosition : AcceptedNestLogicalPositions)
		{
			if (MoonWrap->ShortestWrappedDelta2D(ExistingLogicalPosition, CandidateLogicalPosition).Size() < CandidateMinSpacing)
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
			FVector2D(CorpsePhysicalLocation.X, CorpsePhysicalLocation.Y), CandidateLogicalPosition);
		FVector NestGroundLocation;
		if (!ResolveMoonGroundLocation(FVector(CandidatePhysicalXY.X, CandidatePhysicalXY.Y, CorpsePhysicalLocation.Z), NestGroundLocation))
		{
			continue;
		}

		const FTransform NestTransform(FRotator(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f), NestGroundLocation);
		AJTSRoachNestActor* const Nest = World->SpawnActorDeferred<AJTSRoachNestActor>(
			NestActorClass,
			NestTransform,
			Corpse,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!IsValid(Nest))
		{
			continue;
		}

		Nest->SetAntActorClass(MoonSettings->GetAntActorClass());
		Nest->SetAntNestVisualScale(RandomStream.FRandRange(
			MoonSettings->GetAntNestVisualScaleVariationMin(),
			MoonSettings->GetAntNestVisualScaleVariationMax()));
		Nest->FinishSpawning(NestTransform);
		Nest->AdjustToGround(NestGroundLocation);
		GeneratedAntNests.Add(Nest);
		AcceptedNestLogicalPositions.Add(CandidateLogicalPosition);
	}

	UE_LOG(LogTemp, Log, TEXT("JumpToSpace Moon Ant Nests: Planet=%s Requested=%d Spawned=%d"),
		*PlanetId.ToString(), DesiredNestCount, GeneratedAntNests.Num());
}

void AJTSMoonSurfaceController::ConsumeExpeditionSupplies()
{
	const AJTSMoonGameMode* const MoonSettings = GetMoonSettings();
	AJTSSpacecraftActor* const Spacecraft = GetSpacecraft();
	if (!IsValid(MoonSettings) || !IsValid(Spacecraft))
	{
		FoodConsumptionAccumulator = 0.0;
		WaterConsumptionAccumulator = 0.0;
		if (!bMissingSpacecraftLogged)
		{
			UE_LOG(LogTemp, Error, TEXT("Moon surface %s cannot consume expedition supplies because its spacecraft is unavailable."), *PlanetId.ToString());
			bMissingSpacecraftLogged = true;
		}
		return;
	}
	bMissingSpacecraftLogged = false;

	const double ConsumptionTickSeconds = static_cast<double>(MoonSettings->GetConsumptionTickInterval());
	const double ConsumptionUnit = static_cast<double>(MoonSettings->GetMinimumConsumptionUnit());
	if (ConsumptionTickSeconds <= 0.0 || ConsumptionUnit <= 0.0)
	{
		return;
	}

	const double CrewCount = static_cast<double>(MoonSettings->GetCrewCount());
	FoodConsumptionAccumulator += static_cast<double>(MoonSettings->GetFoodConsumptionPerPersonPerMinute())
		* CrewCount * ConsumptionTickSeconds / SecondsPerMinute;
	WaterConsumptionAccumulator += static_cast<double>(MoonSettings->GetWaterConsumptionPerPersonPerMinute())
		* CrewCount * ConsumptionTickSeconds / SecondsPerMinute;

	const int32 FoodResourcesDue = GetWholeConsumptionUnits(FoodConsumptionAccumulator, ConsumptionUnit);
	const int32 WaterResourcesDue = GetWholeConsumptionUnits(WaterConsumptionAccumulator, ConsumptionUnit);
	if (FoodResourcesDue > 0)
	{
		Spacecraft->TryConsumeResource(EJTSResourceType::Food,
			FMath::Min(FoodResourcesDue, Spacecraft->GetResourceAmount(EJTSResourceType::Food)));
	}
	if (WaterResourcesDue > 0)
	{
		Spacecraft->TryConsumeResource(EJTSResourceType::Water,
			FMath::Min(WaterResourcesDue, Spacecraft->GetResourceAmount(EJTSResourceType::Water)));
	}

	FoodConsumptionAccumulator = FMath::Max(0.0, FoodConsumptionAccumulator - static_cast<double>(FoodResourcesDue) * ConsumptionUnit);
	WaterConsumptionAccumulator = FMath::Max(0.0, WaterConsumptionAccumulator - static_cast<double>(WaterResourcesDue) * ConsumptionUnit);
}

bool AJTSMoonSurfaceController::TryCraftPickaxe(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft)
{
	return TryBuyWorkshopEquipment(Player, Spacecraft, EJTSEquipmentType::Pickaxe);
}

bool AJTSMoonSurfaceController::TryCraftBackpack(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft)
{
	return TryBuyWorkshopEquipment(Player, Spacecraft, EJTSEquipmentType::Backpack);
}

bool AJTSMoonSurfaceController::TryCraftKnife(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft)
{
	return TryBuyWorkshopEquipment(Player, Spacecraft, EJTSEquipmentType::Knife);
}

bool AJTSMoonSurfaceController::TryCraftAxe(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft)
{
	return TryBuyWorkshopEquipment(Player, Spacecraft, EJTSEquipmentType::Axe);
}

bool AJTSMoonSurfaceController::TryBuyWorkshopEquipment(
	AJTSCharacter* Player,
	AJTSSpacecraftActor* Spacecraft,
	EJTSEquipmentType EquipmentType)
{
	const AJTSMoonGameMode* const MoonSettings = GetMoonSettings();
	const AJTSGameState* const GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
	if (!IsValid(MoonSettings) || !IsValid(GameState) || !GameState->IsMoonExploration()
		|| !IsValid(Player) || !IsValid(Spacecraft)
		|| Player->GetNearbySpacecraft() != Spacecraft || !Spacecraft->IsPawnInBoardingRange(Player))
	{
		return false;
	}

	UJTSPlayerEquipmentComponent* const EquipmentComponent = Player->GetEquipmentComponent();
	if (!IsValid(EquipmentComponent))
	{
		return false;
	}

	int32 RockCost = MoonSettings->GetPickaxeRockCost();
	int32 OreCost = 0;
	EJTSWorldPickupItemType PickupItemType = EJTSWorldPickupItemType::Pickaxe;
	switch (EquipmentType)
	{
	case EJTSEquipmentType::Backpack:
		RockCost = MoonSettings->GetBackpackRockCost();
		OreCost = MoonSettings->GetBackpackOreCost();
		PickupItemType = EJTSWorldPickupItemType::Backpack;
		break;
	case EJTSEquipmentType::Knife:
		RockCost = MoonSettings->GetKnifeRockCost();
		OreCost = MoonSettings->GetKnifeOreCost();
		PickupItemType = EJTSWorldPickupItemType::Knife;
		break;
	case EJTSEquipmentType::Axe:
		RockCost = MoonSettings->GetAxeRockCost();
		OreCost = MoonSettings->GetAxeOreCost();
		PickupItemType = EJTSWorldPickupItemType::Axe;
		break;
	case EJTSEquipmentType::Pickaxe:
		break;
	default:
		return false;
	}

	if (!Spacecraft->HasResource(EJTSResourceType::Rock, RockCost)
		|| (OreCost > 0 && !Spacecraft->HasResource(EJTSResourceType::Ore, OreCost)))
	{
		return false;
	}

	TMap<EJTSResourceType, int32> ResourceCosts;
	ResourceCosts.Add(EJTSResourceType::Rock, RockCost);
	if (OreCost > 0)
	{
		ResourceCosts.Add(EJTSResourceType::Ore, OreCost);
	}

	const bool bCanAutoEquip = !EquipmentComponent->HasEquippedItem(EquipmentType) && EquipmentComponent->HasAvailableSlot();
	if (bCanAutoEquip)
	{
		if (!EquipmentComponent->TryEquipItem(EquipmentType))
		{
			return false;
		}
		if (!Spacecraft->TryConsumeResourceAmounts(ResourceCosts))
		{
			EquipmentComponent->UnequipItem(EquipmentType);
			return false;
		}
		return true;
	}

	AJTSWorldPickupActor* const Pickup = AJTSWorldPickupActor::SpawnGameplayDrop(
		GetWorld(), PickupItemType, Player->GetActorLocation(), Player, Spacecraft, Player->GetActorForwardVector());
	if (!IsValid(Pickup))
	{
		return false;
	}
	if (!Spacecraft->TryConsumeResourceAmounts(ResourceCosts))
	{
		Pickup->Destroy();
		return false;
	}
	return true;
}

int32 AJTSMoonSurfaceController::GetWholeConsumptionUnits(double Accumulator, double MinimumConsumptionUnit)
{
	if (!FMath::IsFinite(Accumulator) || !FMath::IsFinite(MinimumConsumptionUnit)
		|| Accumulator <= 0.0 || MinimumConsumptionUnit <= 0.0)
	{
		return 0;
	}

	const double WholeUnits = FMath::FloorToDouble((Accumulator / MinimumConsumptionUnit) + 1.0e-9);
	return WholeUnits >= static_cast<double>(MAX_int32) ? MAX_int32 : static_cast<int32>(WholeUnits);
}
