// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSMoonSurfaceController.h"

#include "CollisionQueryParams.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
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
#include "space/World/JTSMoonSurfaceGameplayData.h"
#include "space/World/JTSMoonResourceSpawner.h"
#include "space/World/JTSMoonWorldActor.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetSurfaceAnchor.h"
#include "space/World/JTSMoonAntActor.h"
#include "space/World/JTSMoonAntNestActor.h"

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
	bReplicates = true;
	bAlwaysRelevant = true;
	MoonGameplaySettingsClass = AJTSMoonGameMode::StaticClass();
	MoonCorpseClass = AJTSMoonCorpseActor::StaticClass();
}

AJTSMoonSurfaceController* AJTSMoonSurfaceController::FindMoonSurfaceController(
	const UObject* WorldContextObject,
	FName RequestedPlanetId)
{
	UWorld* const World = WorldContextObject != nullptr ? WorldContextObject->GetWorld() : nullptr;
	if (const AJTSMoonGameMode* const LegacyGameMode = World != nullptr ? World->GetAuthGameMode<AJTSMoonGameMode>() : nullptr)
	{
		AJTSMoonSurfaceController* const Controller = LegacyGameMode->GetMoonSurfaceController();
		if (IsValid(Controller)
			&& (RequestedPlanetId.IsNone() || Controller->GetPlanetId() == RequestedPlanetId))
		{
			return Controller;
		}
	}

	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<AJTSMoonSurfaceController> ControllerIt(World); ControllerIt; ++ControllerIt)
	{
		AJTSMoonSurfaceController* const Controller = *ControllerIt;
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

bool AJTSMoonSurfaceController::SupportsPlanet(const AJTSPlanetAnchor* Planet) const
{
	return IsValid(Planet)
		&& !PlanetId.IsNone()
		&& PlanetId == Planet->GetPlanetId();
}

bool AJTSMoonSurfaceController::InitializeSurfaceGameplay(const FJTSSurfaceGameplayContext& Context)
{
	if (!HasAuthority() || !Context.HasRequiredRuntimeActors() || !SupportsPlanet(Context.Planet))
	{
		UE_LOG(LogTemp, Error, TEXT("Moon surface controller %s rejected an invalid SpaceWorld context: Planet=%s Player=%s Spacecraft=%s."),
			*GetName(),
			*GetNameSafe(Context.Planet),
			*GetNameSafe(Context.Player),
			*GetNameSafe(Context.Spacecraft));
		return false;
	}

	ApplySurfaceGameplayContext(Context);
	for (AJTSCharacter* const Player : Context.Players)
	{
		RegisterSurfacePlayer(Player);
	}
	return InitializeSurfaceGameplay();
}

void AJTSMoonSurfaceController::RegisterSurfacePlayer(AJTSCharacter* Player)
{
	if (!IsValid(Player))
	{
		return;
	}
	ActivePlayers.RemoveAll([](const TWeakObjectPtr<AJTSCharacter>& Candidate) { return !Candidate.IsValid(); });
	ActivePlayers.AddUnique(Player);
	RegisterSurfaceRuntimeActor(Player);
}

TArray<AJTSCharacter*> AJTSMoonSurfaceController::GetActivePlayers() const
{
	TArray<AJTSCharacter*> Result;
	for (const TWeakObjectPtr<AJTSCharacter>& Player : ActivePlayers)
	{
		if (Player.IsValid()) Result.Add(Player.Get());
	}
	return Result;
}

void AJTSMoonSurfaceController::ShutdownSurfaceGameplay()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpeditionConsumptionTimerHandle);
		World->GetTimerManager().ClearTimer(SurfaceInitializationTimerHandle);
	}

	if (HasAuthority())
	{
		ClearGeneratedMoonAntNests();
	}
	bSurfaceGameplayInitializationRequested = false;
	bSurfaceGameplayInitialized = false;
	bMissingSpacecraftLogged = false;
	FoodConsumptionAccumulator = 0.0;
	WaterConsumptionAccumulator = 0.0;
}

bool AJTSMoonSurfaceController::IsSurfaceGameplayReady() const
{
	return IsSurfaceGameplayInitialized();
}

void AJTSMoonSurfaceController::ConfigureLegacyRuntime(AJTSMoonGameMode* InLegacyGameMode)
{
	bUsingRealPlanetSurfaceGameplay = false;
	ActiveMoonGameplayData = nullptr;
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

const IJTSMoonSurfaceGameplaySettings* AJTSMoonSurfaceController::GetMoonSettings() const
{
	if (bUsingRealPlanetSurfaceGameplay)
	{
		if (IsValid(ActiveMoonGameplayData))
		{
			return static_cast<const IJTSMoonSurfaceGameplaySettings*>(ActiveMoonGameplayData.Get());
		}
		if (IsValid(MoonGameplayData))
		{
			return static_cast<const IJTSMoonSurfaceGameplaySettings*>(MoonGameplayData.Get());
		}

		return nullptr;
	}

	if (LegacySettingsSource.IsValid())
	{
		return static_cast<const IJTSMoonSurfaceGameplaySettings*>(LegacySettingsSource.Get());
	}

	TSubclassOf<AJTSMoonGameMode> SettingsClass = MoonGameplaySettingsClass;
	if (SettingsClass == nullptr)
	{
		SettingsClass = AJTSMoonGameMode::StaticClass();
	}
	return SettingsClass != nullptr
		? static_cast<const IJTSMoonSurfaceGameplaySettings*>(SettingsClass->GetDefaultObject<AJTSMoonGameMode>())
		: nullptr;
}

const UJTSMoonSurfaceGameplayData* AJTSMoonSurfaceController::GetMoonGameplayData() const
{
	if (IsValid(ActiveMoonGameplayData))
	{
		return ActiveMoonGameplayData.Get();
	}

	return bUsingRealPlanetSurfaceGameplay && IsValid(MoonGameplayData) ? MoonGameplayData.Get() : nullptr;
}

AJTSPlanetAnchor* AJTSMoonSurfaceController::GetOwningPlanet() const
{
	return OwningPlanet.Get();
}

bool AJTSMoonSurfaceController::IsUsingRealPlanetSurfaceGameplay() const
{
	return bUsingRealPlanetSurfaceGameplay && OwningPlanet.IsValid();
}

AJTSSpacecraftActor* AJTSMoonSurfaceController::GetSpacecraft() const
{
	if (CachedSpacecraft.IsValid())
	{
		return CachedSpacecraft.Get();
	}
	if (IsUsingRealPlanetSurfaceGameplay())
	{
		return nullptr;
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

void AJTSMoonSurfaceController::ApplySurfaceGameplayContext(const FJTSSurfaceGameplayContext& Context)
{
	OwningPlanet = Context.Planet;
	PlanetId = Context.Planet->GetPlanetId();
	bUsingRealPlanetSurfaceGameplay = true;
	LegacySettingsSource.Reset();
	ActiveMoonGameplayData = Cast<UJTSMoonSurfaceGameplayData>(Context.GameplayData);

	SetSurfaceSpacecraft(Context.Spacecraft);
	RegisterSurfacePlayer(Context.Player);
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
	if (AJTSCharacter* const Character = Cast<AJTSCharacter>(RuntimeActor))
	{
		ActivePlayers.RemoveAll([](const TWeakObjectPtr<AJTSCharacter>& Candidate) { return !Candidate.IsValid(); });
		ActivePlayers.AddUnique(Character);
	}
}

AJTSMoonCorpseActor* AJTSMoonSurfaceController::SpawnCorpseAtPlanetSurfaceAnchor(
	AJTSPlanetSurfaceAnchor* InCorpseSurfaceAnchor)
{
	if (!HasAuthority())
	{
		return nullptr;
	}
	if (RealSurfaceMoonCorpse.IsValid())
	{
		return RealSurfaceMoonCorpse.Get();
	}

	if (!IsValid(InCorpseSurfaceAnchor))
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface controller %s received an invalid real-surface corpse anchor."), *GetName());
		return nullptr;
	}

	FTransform SurfaceTransform;
	if (!InCorpseSurfaceAnchor->GetSurfaceTransform(SurfaceTransform))
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface controller %s could not resolve corpse anchor %s on a real gameplay mesh."),
			*GetName(), *InCorpseSurfaceAnchor->GetName());
		return nullptr;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	TSubclassOf<AJTSMoonCorpseActor> CorpseClass = MoonCorpseClass;
	if (CorpseClass == nullptr)
	{
		CorpseClass = AJTSMoonCorpseActor::StaticClass();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = TEXT("JTSRealSurfaceMoonCorpse");
	SpawnParameters.OverrideLevel = GetLevel();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AJTSMoonCorpseActor* const Corpse = World->SpawnActor<AJTSMoonCorpseActor>(CorpseClass, SurfaceTransform, SpawnParameters);
	if (!IsValid(Corpse))
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface controller %s could not spawn its real-surface corpse."), *GetName());
		return nullptr;
	}

	if (!Corpse->SnapToPlanetSurfaceAnchor(InCorpseSurfaceAnchor))
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface controller %s failed to snap corpse %s to its real surface anchor."),
			*GetName(), *Corpse->GetName());
		Corpse->Destroy();
		return nullptr;
	}

	RealSurfaceMoonCorpse = Corpse;
	RegisterSurfaceRuntimeActor(Corpse);
	return Corpse;
}

AJTSMoonCorpseActor* AJTSMoonSurfaceController::SpawnConfiguredCorpseAtPlanetSurfaceAnchor()
{
	return SpawnCorpseAtPlanetSurfaceAnchor(CorpseSurfaceAnchor.Get());
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
	if (Candidate == this)
	{
		return true;
	}

	// SpaceWorld is persistent and can contain more than one planet. In that route level ownership is
	// deliberately too broad; actors are explicitly registered as part of the current surface context.
	if (IsUsingRealPlanetSurfaceGameplay())
	{
		return RegisteredSurfaceRuntimeActors.ContainsByPredicate([Candidate](const TWeakObjectPtr<AActor>& RegisteredActor)
		{
			return RegisteredActor.Get() == Candidate;
		});
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

}

void AJTSMoonSurfaceController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownSurfaceGameplay();
	CachedSpacecraft.Reset();
	ActivePlayers.Reset();
	CachedMoonWorld.Reset();
	LevelMoonCorpseLandmark.Reset();
	RealSurfaceMoonCorpse.Reset();
	CachedLevelMoonCorpseLandmarks.Reset();
	RegisteredSurfaceRuntimeActors.Reset();
	bLevelCorpseLandmarkSearchCompleted = false;
	ActiveMoonGameplayData = nullptr;
	bUsingRealPlanetSurfaceGameplay = false;

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
	if (!HasAuthority())
	{
		return false;
	}
	bSurfaceGameplayInitializationRequested = false;
	if (bSurfaceGameplayInitialized)
	{
		return true;
	}

	UWorld* const World = GetWorld();
	const IJTSMoonSurfaceGameplaySettings* const MoonSettings = GetMoonSettings();
	const bool bRequiresLegacyMoon = !IsUsingRealPlanetSurfaceGameplay();
	AJTSMoonWorldActor* const MoonWorld = bRequiresLegacyMoon ? GetMoonWorldActor() : nullptr;
	UJTSMoonWrapSubsystem* const MoonWrap = bRequiresLegacyMoon && World != nullptr ? World->GetSubsystem<UJTSMoonWrapSubsystem>() : nullptr;
	AJTSSpacecraftActor* const Spacecraft = GetSpacecraft();
	if (IsValid(Spacecraft))
	{
		Spacecraft->RestorePersistentStorage();
	}
	const bool bLegacyPrerequisitesReady = !bRequiresLegacyMoon
		|| (IsValid(MoonWorld) && IsValid(MoonWrap) && MoonWrap->IsConfiguredForMoon());
	if (World == nullptr || MoonSettings == nullptr || !bLegacyPrerequisitesReady || !IsValid(Spacecraft))
	{
		if (bRequiresLegacyMoon)
		{
			ScheduleInitializationRetry();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Moon surface controller %s cannot initialize real Moon gameplay: Settings=%s Spacecraft=%s."),
				*GetName(),
				MoonSettings != nullptr ? TEXT("Valid") : TEXT("Missing Data Asset"),
				*GetNameSafe(Spacecraft));
		}
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
		TEXT("JumpToSpace Moon Surface Ready: Planet=%s ResourceCount=%d SpawnRadius=%.1f MoonAntNests=%d"),
		*PlanetId.ToString(),
		FMath::Max(0, MoonSettings->GetMoonResourceSpawnSettings().TotalResourceCount),
		FMath::Max(0.0f, MoonSettings->GetMoonResourceSpawnSettings().SpawnRadius),
		MoonSettings->GetMoonAntNestCount());

	if (IsUsingRealPlanetSurfaceGameplay() && IsValid(CorpseSurfaceAnchor))
	{
		SpawnConfiguredCorpseAtPlanetSurfaceAnchor();
	}

	// Landmarks must exist before procedural nests and resources derive their exclusion zones.
	InitializeMoonLandmarksAndMoonAntNests();
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
	if (!HasAuthority())
	{
		return;
	}
	const IJTSMoonSurfaceGameplaySettings* const MoonSettings = GetMoonSettings();
	if (MoonSettings == nullptr)
	{
		return;
	}

	AJTSMoonResourceSpawner* ResourceSpawner = nullptr;
	if (IsUsingRealPlanetSurfaceGameplay())
	{
		ResourceSpawner = MoonResourceSpawner.Get();
		if (!IsValid(ResourceSpawner))
		{
			UE_LOG(LogTemp, Warning, TEXT("Moon surface %s has no explicitly configured MoonResourceSpawner for real SpaceWorld gameplay."),
				*PlanetId.ToString());
			return;
		}
	}
	else
	{
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
		ResourceSpawner = ResourceSpawners[0];
	}

	RegisterSurfaceRuntimeActor(ResourceSpawner);
	ResourceSpawner->SetSurfaceGameplayController(this);
	ResourceSpawner->ApplyMoonSpawnSettings(MoonSettings->GetMoonResourceSpawnSettings());
	ResourceSpawner->SetOwningPlanet(IsUsingRealPlanetSurfaceGameplay() ? GetOwningPlanet() : nullptr);
	ResourceSpawner->SetLandmarkExclusions(GetSpacecraft(), CachedLevelMoonCorpseLandmarks, GeneratedMoonAntNests);
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
	if (IsUsingRealPlanetSurfaceGameplay())
	{
		if (RealSurfaceMoonCorpse.IsValid())
		{
			LevelMoonCorpseLandmark = RealSurfaceMoonCorpse;
			CachedLevelMoonCorpseLandmarks.Add(RealSurfaceMoonCorpse);
			return RealSurfaceMoonCorpse.Get();
		}

		UE_LOG(LogTemp, Warning, TEXT("Moon surface %s has no configured real-surface corpse landmark; MoonAnt Nest generation is skipped."),
			*PlanetId.ToString());
		return nullptr;
	}

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
		UE_LOG(LogTemp, Warning, TEXT("Moon surface %s has no corpse landmark; MoonAnt Nest generation is skipped."), *PlanetId.ToString());
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

void AJTSMoonSurfaceController::ClearGeneratedMoonAntNests()
{
	if (!HasAuthority())
	{
		return;
	}
	for (TWeakObjectPtr<AJTSMoonAntNestActor>& Nest : GeneratedMoonAntNests)
	{
		if (Nest.IsValid())
		{
			Nest->Destroy();
		}
	}
	GeneratedMoonAntNests.Reset();
}

bool AJTSMoonSurfaceController::ResolveMoonGroundLocation(
	const FVector& CandidateLocation,
	FVector& OutGroundLocation,
	const AActor* AdditionalIgnoredActor) const
{
	UWorld* const World = GetWorld();
	const IJTSMoonSurfaceGameplaySettings* const MoonSettings = GetMoonSettings();
	if (World == nullptr || MoonSettings == nullptr)
	{
		return false;
	}

	if (IsUsingRealPlanetSurfaceGameplay())
	{
		AJTSPlanetAnchor* const Planet = GetOwningPlanet();
		if (!IsValid(Planet))
		{
			return false;
		}

		const float StartHeight = FMath::Max(0.0f, MoonSettings->GetMoonAntGroundTraceStartHeight());
		const float TraceDistance = FMath::Max(0.0f, MoonSettings->GetMoonAntGroundTraceDistance());
		const FVector RadialUp = Planet->GetRadialUpVector(CandidateLocation);
		FJTSPlanetSurfaceHit SurfaceHit;
		if (Planet->ProbeSurfaceAlongGravity(
			CandidateLocation + RadialUp * StartHeight,
			StartHeight + TraceDistance,
			SurfaceHit)
			|| Planet->ProjectPointToSurface(CandidateLocation, SurfaceHit))
		{
			OutGroundLocation = SurfaceHit.ImpactPoint;
			return true;
		}

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
	for (const TWeakObjectPtr<AJTSMoonAntNestActor>& Nest : GeneratedMoonAntNests)
	{
		if (Nest.IsValid())
		{
			TraceParams.AddIgnoredActor(Nest.Get());
		}
	}
	for (AJTSCharacter* const PlayerPawn : GetActivePlayers())
	{
		TraceParams.AddIgnoredActor(PlayerPawn);
	}
	for (AActor* const Actor : GetSurfaceLevel()->Actors)
	{
		if (AJTSMoonAntActor* const MoonAnt = Cast<AJTSMoonAntActor>(Actor); IsValid(MoonAnt))
		{
			TraceParams.AddIgnoredActor(MoonAnt);
		}
	}

	FHitResult GroundHit;
	const float StartHeight = MoonSettings->GetMoonAntGroundTraceStartHeight();
	const float TraceDistance = MoonSettings->GetMoonAntGroundTraceDistance();
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

bool AJTSMoonSurfaceController::IsMoonAntNestCandidateFarFromShip(
	const FVector2D& CandidateLogicalPosition,
	const FVector2D& ShipLogicalPosition) const
{
	const UWorld* const World = GetWorld();
	const UJTSMoonWrapSubsystem* const MoonWrap = World != nullptr ? World->GetSubsystem<UJTSMoonWrapSubsystem>() : nullptr;
	const IJTSMoonSurfaceGameplaySettings* const MoonSettings = GetMoonSettings();
	if (MoonSettings == nullptr)
	{
		return false;
	}

	const FVector2D Delta = IsValid(MoonWrap) && MoonWrap->IsConfiguredForMoon()
		? MoonWrap->ShortestWrappedDelta2D(ShipLogicalPosition, CandidateLogicalPosition)
		: CandidateLogicalPosition - ShipLogicalPosition;
	return Delta.Size() >= MoonSettings->GetMoonAntNestMinDistanceFromShip();
}

void AJTSMoonSurfaceController::InitializeMoonLandmarksAndMoonAntNests()
{
	if (!HasAuthority())
	{
		return;
	}
	ClearGeneratedMoonAntNests();

	UWorld* const World = GetWorld();
	const IJTSMoonSurfaceGameplaySettings* const MoonSettings = GetMoonSettings();
	UJTSMoonWrapSubsystem* const MoonWrap = World != nullptr ? World->GetSubsystem<UJTSMoonWrapSubsystem>() : nullptr;
	AJTSSpacecraftActor* const Spacecraft = GetSpacecraft();
	const bool bUseRealPlanetSurface = IsUsingRealPlanetSurfaceGameplay();
	if (World == nullptr
		|| MoonSettings == nullptr
		|| (!bUseRealPlanetSurface && (!IsValid(MoonWrap) || !MoonWrap->IsConfiguredForMoon()))
		|| !IsValid(Spacecraft))
	{
		return;
	}

	AJTSMoonCorpseActor* const Corpse = FindLevelCorpseLandmark();
	if (!IsValid(Corpse))
	{
		return;
	}

	const TSubclassOf<AJTSMoonAntNestActor> NestActorClass = MoonSettings->GetMoonAntNestActorClass();
	if (NestActorClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface %s has no MoonAnt Nest class configured."), *PlanetId.ToString());
		return;
	}

	const FVector ShipPhysicalLocation = Spacecraft->GetActorLocation();
	const FVector2D ShipLogicalPosition = !bUseRealPlanetSurface && IsValid(MoonWrap)
		? MoonWrap->GetLogicalPositionFromWorld(ShipPhysicalLocation)
		: FVector2D::ZeroVector;
	FRandomStream RandomStream(static_cast<int32>(FPlatformTime::Cycles64() & static_cast<uint64>(MAX_uint32)));
	const FVector CorpsePhysicalLocation = Corpse->GetActorLocation();
	const FVector2D CorpseLogicalPosition = !bUseRealPlanetSurface && IsValid(MoonWrap)
		? MoonWrap->GetLogicalPositionFromWorld(CorpsePhysicalLocation)
		: FVector2D::ZeroVector;
	TArray<FVector2D> AcceptedNestLogicalPositions;
	const int32 DesiredNestCount = MoonSettings->GetMoonAntNestCount();
	const int32 MaxNestAttempts = FMath::Max(64, DesiredNestCount * 48);
	const FBox CorpseBounds = Corpse->GetComponentsBoundingBox(true);
	const FVector CorpseBoundsExtent = CorpseBounds.IsValid ? CorpseBounds.GetExtent() : FVector::ZeroVector;
	const float CorpseMeshClearance = (bUseRealPlanetSurface
		? CorpseBoundsExtent.Size()
		: FVector2D(CorpseBoundsExtent.X, CorpseBoundsExtent.Y).Size()) + 50.0f;
	const float InnerNestRadius = FMath::Max(MoonSettings->GetMoonAntNestMinDistanceFromCorpse(), CorpseMeshClearance);
	const float OuterNestRadius = FMath::Max(InnerNestRadius, MoonSettings->GetMoonAntNestOuterRadiusAroundCorpse());
	const float InnerZoneMaxRadius = FMath::Max(InnerNestRadius, OuterNestRadius * 0.45f);
	const float MidZoneMinRadius = FMath::Clamp(OuterNestRadius * 0.35f, InnerNestRadius, OuterNestRadius);
	const float MidZoneMaxRadius = FMath::Max(MidZoneMinRadius, OuterNestRadius * 0.75f);
	const float OuterZoneMinRadius = FMath::Clamp(OuterNestRadius * 0.65f, InnerNestRadius, OuterNestRadius);
	const float InnerWeight = MoonSettings->GetMoonAntNestInnerWeight();
	const float MidWeight = MoonSettings->GetMoonAntNestMidWeight();
	const float OuterWeight = MoonSettings->GetMoonAntNestOuterWeight();
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

	if (bUseRealPlanetSurface)
	{
		AJTSPlanetAnchor* const Planet = GetOwningPlanet();
		if (!IsValid(Planet))
		{
			return;
		}

		const FVector ShipLocation = Spacecraft->GetActorLocation();
		const FVector CorpseLocation = Corpse->GetActorLocation();
		FJTSPlanetSurfaceFrame CorpseSurfaceFrame;
		if (!Planet->GetSurfaceFrameAt(CorpseLocation, Corpse->GetActorForwardVector(), CorpseSurfaceFrame))
		{
			UE_LOG(LogTemp, Warning, TEXT("Moon surface %s could not resolve a real-surface corpse frame for MoonAnt Nests."), *PlanetId.ToString());
			return;
		}

		TArray<FVector> AcceptedNestLocations;
		for (int32 Attempt = 0; Attempt < MaxNestAttempts && GeneratedMoonAntNests.Num() < DesiredNestCount; ++Attempt)
		{
			const float Angle = RandomStream.FRandRange(0.0f, UE_TWO_PI);
			const float CandidateRadius = ChooseNestRadius();
			const FVector CandidateTangent = (
				CorpseSurfaceFrame.Forward * FMath::Cos(Angle)
				+ CorpseSurfaceFrame.Right * FMath::Sin(Angle)).GetSafeNormal();
			FJTSPlanetSurfaceHit NestSurfaceHit;
			if (!Planet->ProjectPointToSurface(CorpseLocation + CandidateTangent * CandidateRadius, NestSurfaceHit))
			{
				continue;
			}

			const FVector CandidateLocation = NestSurfaceHit.ImpactPoint;
			const float CorpseDistance = Planet->ApproximateSurfaceArcDistance(CorpseLocation, CandidateLocation);
			if (CorpseDistance < InnerNestRadius || CorpseDistance > OuterNestRadius + 50.0f
				|| Planet->ApproximateSurfaceArcDistance(ShipLocation, CandidateLocation) < MoonSettings->GetMoonAntNestMinDistanceFromShip())
			{
				continue;
			}

			const float CandidateMinSpacing = MoonSettings->GetMoonAntNestBaseMinSpacing()
				* RandomStream.FRandRange(
					MoonSettings->GetMoonAntNestCandidateSpacingScaleMin(),
					MoonSettings->GetMoonAntNestCandidateSpacingScaleMax());
			bool bOverlapsExistingNest = false;
			for (const FVector& ExistingLocation : AcceptedNestLocations)
			{
				if (Planet->ApproximateSurfaceArcDistance(ExistingLocation, CandidateLocation) < CandidateMinSpacing)
				{
					bOverlapsExistingNest = true;
					break;
				}
			}
			if (bOverlapsExistingNest)
			{
				continue;
			}

			FJTSPlanetSurfaceFrame NestSurfaceFrame;
			if (!Planet->GetSurfaceFrameAt(CandidateLocation, CandidateTangent, NestSurfaceFrame))
			{
				continue;
			}
			const FVector NestForward = FQuat(
				NestSurfaceFrame.Up,
				RandomStream.FRandRange(0.0f, UE_TWO_PI)).RotateVector(NestSurfaceFrame.Forward);
			const FTransform NestTransform(
				FRotationMatrix::MakeFromXZ(NestForward, NestSurfaceFrame.Up).ToQuat(),
				CandidateLocation);
			AJTSMoonAntNestActor* const Nest = World->SpawnActorDeferred<AJTSMoonAntNestActor>(
				NestActorClass,
				NestTransform,
				Corpse,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!IsValid(Nest))
			{
				continue;
			}

			Nest->SetMoonAntActorClass(MoonSettings->GetMoonAntActorClass());
			Nest->SetMoonAntNestVisualScale(RandomStream.FRandRange(
				MoonSettings->GetMoonAntNestVisualScaleVariationMin(),
				MoonSettings->GetMoonAntNestVisualScaleVariationMax()));
			RegisterSurfaceRuntimeActor(Nest);
			Nest->FinishSpawning(NestTransform);
			Nest->PlaceOnPlanetSurface(Planet, CandidateLocation, NestForward);
			GeneratedMoonAntNests.Add(Nest);
			AcceptedNestLocations.Add(CandidateLocation);
		}

			UE_LOG(LogTemp, Log, TEXT("JumpToSpace MoonAnt Nests: Planet=%s Requested=%d Spawned=%d"),
			*PlanetId.ToString(), DesiredNestCount, GeneratedMoonAntNests.Num());
		return;
	}

	for (int32 Attempt = 0; Attempt < MaxNestAttempts && GeneratedMoonAntNests.Num() < DesiredNestCount; ++Attempt)
	{
		const float Angle = RandomStream.FRandRange(0.0f, UE_TWO_PI);
		FVector2D CandidateLogicalPosition = MoonWrap->CanonicalizePosition2D(
			CorpseLogicalPosition + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * ChooseNestRadius());
		if (!IsMoonAntNestCandidateFarFromShip(CandidateLogicalPosition, ShipLogicalPosition))
		{
			continue;
		}

		const float CandidateMinSpacing = MoonSettings->GetMoonAntNestBaseMinSpacing()
			* RandomStream.FRandRange(
				MoonSettings->GetMoonAntNestCandidateSpacingScaleMin(),
				MoonSettings->GetMoonAntNestCandidateSpacingScaleMax());
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
		AJTSMoonAntNestActor* const Nest = World->SpawnActorDeferred<AJTSMoonAntNestActor>(
			NestActorClass,
			NestTransform,
			Corpse,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!IsValid(Nest))
		{
			continue;
		}

		Nest->SetMoonAntActorClass(MoonSettings->GetMoonAntActorClass());
		Nest->SetMoonAntNestVisualScale(RandomStream.FRandRange(
			MoonSettings->GetMoonAntNestVisualScaleVariationMin(),
			MoonSettings->GetMoonAntNestVisualScaleVariationMax()));
		Nest->FinishSpawning(NestTransform);
		Nest->AdjustToGround(NestGroundLocation);
		GeneratedMoonAntNests.Add(Nest);
		AcceptedNestLogicalPositions.Add(CandidateLogicalPosition);
	}

	UE_LOG(LogTemp, Log, TEXT("JumpToSpace MoonAnt Nests: Planet=%s Requested=%d Spawned=%d"),
		*PlanetId.ToString(), DesiredNestCount, GeneratedMoonAntNests.Num());
}

void AJTSMoonSurfaceController::ConsumeExpeditionSupplies()
{
	if (!HasAuthority())
	{
		return;
	}
	const IJTSMoonSurfaceGameplaySettings* const MoonSettings = GetMoonSettings();
	AJTSSpacecraftActor* const Spacecraft = GetSpacecraft();
	if (MoonSettings == nullptr || !IsValid(Spacecraft))
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

	const double CrewCount = static_cast<double>(FMath::Max(1, GetActivePlayers().Num()));
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
	if (!HasAuthority())
	{
		return false;
	}
	const IJTSMoonSurfaceGameplaySettings* const MoonSettings = GetMoonSettings();
	const AJTSGameState* const GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
	if (MoonSettings == nullptr || !IsValid(GameState) || !GameState->IsMoonExploration()
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
