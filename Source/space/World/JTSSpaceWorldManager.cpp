// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSSpaceWorldManager.h"

#include "Components/SceneComponent.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "space/World/JTSPlanetAnchor.h"

AJTSSpaceWorldManager::AJTSSpaceWorldManager()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	MoonSurfaceControllerClass = AJTSMoonSurfaceController::StaticClass();
}

AJTSSpaceWorldManager* AJTSSpaceWorldManager::FindSpaceWorldManager(const UObject* WorldContextObject)
{
	UWorld* const World = WorldContextObject != nullptr ? WorldContextObject->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	if (World->PersistentLevel == nullptr)
	{
		return nullptr;
	}

	for (AActor* const Actor : World->PersistentLevel->Actors)
	{
		if (AJTSSpaceWorldManager* const Manager = Cast<AJTSSpaceWorldManager>(Actor); IsValid(Manager))
		{
			return Manager;
		}
	}

	return nullptr;
}

void AJTSSpaceWorldManager::BeginPlay()
{
	Super::BeginPlay();

	InitializeCurrentPlanet();

	if (bDebugSpaceTravel)
	{
		GetWorldTimerManager().SetTimer(DebugTimerHandle, this, &AJTSSpaceWorldManager::LogDebugState, 1.0f, true);
		LogDebugState();
	}
}

void AJTSSpaceWorldManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(DebugTimerHandle);
	GetWorldTimerManager().ClearTimer(InitialArrivalTimerHandle);
	SurfaceStreamingLevels.Empty();
	SurfaceControllers.Empty();

	Super::EndPlay(EndPlayReason);
}

void AJTSSpaceWorldManager::InitializeCurrentPlanet()
{
	if (IsValid(CurrentPlanet))
	{
		return;
	}

	AJTSPlanetAnchor* CandidatePlanet = InitialPlanet.Get();
	if (!IsValid(CandidatePlanet))
	{
		UWorld* const World = GetWorld();
		if (World == nullptr || InitialPlanetId.IsNone())
		{
			UE_LOG(LogTemp, Error, TEXT("Space World Manager requires InitialPlanet or InitialPlanetId before SpaceFlight can begin."));
			return;
		}

		if (World->PersistentLevel == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Space World Manager cannot resolve its initial PlanetAnchor because the persistent level is unavailable."));
			return;
		}

		int32 MatchingAnchorCount = 0;
		for (AActor* const Actor : World->PersistentLevel->Actors)
		{
			AJTSPlanetAnchor* const Anchor = Cast<AJTSPlanetAnchor>(Actor);
			if (!IsValid(Anchor)
				|| Anchor->GetPlanetId() != InitialPlanetId)
			{
				continue;
			}

			++MatchingAnchorCount;
			CandidatePlanet = Anchor;
		}

		if (MatchingAnchorCount != 1)
		{
			UE_LOG(LogTemp, Error, TEXT("Space World Manager found %d persistent PlanetAnchor actors for InitialPlanetId %s; exactly one is required."),
				MatchingAnchorCount,
				*InitialPlanetId.ToString());
			return;
		}
	}

	SetCurrentPlanet(CandidatePlanet);
	SetTravelState(InitialTravelState);
}

void AJTSSpaceWorldManager::BeginInitialArrival()
{
	if (bInitialArrivalStarted)
	{
		return;
	}

	bInitialArrivalStarted = true;
	bInitialSurfaceLevelReady = false;
	bInitialSurfaceGameplayReady = false;
	InitializeCurrentPlanet();

	AJTSPlanetAnchor* const Planet = CurrentPlanet.Get();
	if (!IsValid(Planet) || InitialTravelState != EJTSSpaceTravelState::Surface)
	{
		UE_LOG(LogTemp, Error, TEXT("Space World initial arrival requires a valid Surface-state initial planet."));
		return;
	}

	SetTravelState(EJTSSpaceTravelState::Surface);
	if (!RequestSurfaceLoad(Planet, true))
	{
		UE_LOG(LogTemp, Error, TEXT("Space World initial arrival could not request the surface for %s."), *Planet->GetPlanetId().ToString());
		return;
	}

	PollInitialSurfaceArrival();
}

FOnJTSSpaceWorldInitialSurfaceLevelReady& AJTSSpaceWorldManager::OnInitialSurfaceLevelReady()
{
	return InitialSurfaceLevelReadyDelegate;
}

FOnJTSSpaceWorldLandingRequested& AJTSSpaceWorldManager::OnLandingRequested()
{
	return LandingRequestedDelegate;
}

AJTSPlanetAnchor* AJTSSpaceWorldManager::GetCurrentPlanet() const
{
	return CurrentPlanet.Get();
}

EJTSSpaceTravelState AJTSSpaceWorldManager::GetCurrentTravelState() const
{
	return CurrentTravelState;
}

bool AJTSSpaceWorldManager::IsSurfaceState() const
{
	return CurrentTravelState == EJTSSpaceTravelState::Surface;
}

void AJTSSpaceWorldManager::SetCurrentPlanet(AJTSPlanetAnchor* NewCurrentPlanet)
{
	if (CurrentPlanet.Get() == NewCurrentPlanet)
	{
		return;
	}

	if (AJTSPlanetAnchor* const PreviousPlanet = CurrentPlanet.Get())
	{
		PreviousPlanet->SetActivePlanet(false);
	}

	CurrentPlanet = NewCurrentPlanet;
	if (AJTSPlanetAnchor* const ActivePlanet = CurrentPlanet.Get())
	{
		ActivePlanet->SetActivePlanet(true);
	}
}

void AJTSSpaceWorldManager::SetTravelState(EJTSSpaceTravelState NewTravelState)
{
	if (CurrentTravelState == NewTravelState)
	{
		return;
	}

	CurrentTravelState = NewTravelState;
	if (bDebugSpaceTravel)
	{
		const UEnum* const TravelStateEnum = StaticEnum<EJTSSpaceTravelState>();
		const FString StateName = TravelStateEnum != nullptr
			? TravelStateEnum->GetNameStringByValue(static_cast<int64>(CurrentTravelState))
			: TEXT("Unknown");
		UE_LOG(LogTemp, Log, TEXT("Space World travel state changed to %s."), *StateName);
	}
}

bool AJTSSpaceWorldManager::RequestSurfaceLoad(AJTSPlanetAnchor* Planet, bool bMakeVisible)
{
	if (!IsValid(Planet) || !Planet->HasSurfaceLevel())
	{
		return false;
	}

	const FName StreamingKey = GetStreamingKey(Planet);
	if (TObjectPtr<ULevelStreamingDynamic>* const ExistingLevel = SurfaceStreamingLevels.Find(StreamingKey))
	{
		if (ULevelStreamingDynamic* const StreamingLevel = ExistingLevel->Get())
		{
			StreamingLevel->SetShouldBeLoaded(true);
			StreamingLevel->SetShouldBeVisible(bMakeVisible);
			return true;
		}
	}

	bool bLoadSucceeded = false;
	ULevelStreamingDynamic* const StreamingLevel = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
		this,
		Planet->GetSurfaceLevel(),
		Planet->GetSurfaceFrameTransform(),
		bLoadSucceeded);
	if (!bLoadSucceeded || !IsValid(StreamingLevel))
	{
		UE_LOG(LogTemp, Warning, TEXT("Space World could not request surface level %s for planet %s."),
			*Planet->GetSurfaceLevel().ToSoftObjectPath().ToString(),
			*Planet->GetPlanetId().ToString());
		return false;
	}

	StreamingLevel->SetShouldBeLoaded(true);
	StreamingLevel->SetShouldBeVisible(bMakeVisible);
	SurfaceStreamingLevels.Add(StreamingKey, StreamingLevel);
	return true;
}

bool AJTSSpaceWorldManager::RequestSurfaceUnload(AJTSPlanetAnchor* Planet)
{
	ULevelStreamingDynamic* const StreamingLevel = FindSurfaceStreamingLevel(Planet);
	if (!IsValid(StreamingLevel))
	{
		return false;
	}

	StreamingLevel->SetShouldBeVisible(false);
	StreamingLevel->SetShouldBeLoaded(false);
	return true;
}

bool AJTSSpaceWorldManager::IsSurfaceLevelLoaded(const AJTSPlanetAnchor* Planet) const
{
	const ULevelStreamingDynamic* const StreamingLevel = FindSurfaceStreamingLevel(Planet);
	return IsValid(StreamingLevel) && StreamingLevel->IsLevelLoaded();
}

bool AJTSSpaceWorldManager::IsSurfaceLevelVisible(const AJTSPlanetAnchor* Planet) const
{
	const ULevelStreamingDynamic* const StreamingLevel = FindSurfaceStreamingLevel(Planet);
	return IsValid(StreamingLevel) && StreamingLevel->IsLevelVisible();
}

ULevel* AJTSSpaceWorldManager::GetLoadedSurfaceLevel(const AJTSPlanetAnchor* Planet) const
{
	if (ULevelStreamingDynamic* const StreamingLevel = FindSurfaceStreamingLevel(Planet))
	{
		return StreamingLevel->GetLoadedLevel();
	}

	return nullptr;
}

void AJTSSpaceWorldManager::RegisterSurfaceController(AJTSMoonSurfaceController* Controller)
{
	if (!IsValid(Controller) || Controller->GetPlanetId().IsNone())
	{
		return;
	}

	SurfaceControllers.Add(Controller->GetPlanetId(), Controller);
}

void AJTSSpaceWorldManager::UnregisterSurfaceController(AJTSMoonSurfaceController* Controller)
{
	if (!IsValid(Controller))
	{
		return;
	}

	const bool bWasCurrentController = Controller == GetCurrentSurfaceController();
	const FName PlanetId = Controller->GetPlanetId();
	if (const TObjectPtr<AJTSMoonSurfaceController>* const Registered = SurfaceControllers.Find(PlanetId);
		Registered != nullptr && Registered->Get() == Controller)
	{
		SurfaceControllers.Remove(PlanetId);
	}

	if (bWasCurrentController)
	{
		bInitialSurfaceLevelReady = false;
		bInitialSurfaceGameplayReady = false;
	}
}

AJTSMoonSurfaceController* AJTSSpaceWorldManager::GetSurfaceController(FName PlanetId) const
{
	if (PlanetId.IsNone())
	{
		return nullptr;
	}

	if (const TObjectPtr<AJTSMoonSurfaceController>* const Controller = SurfaceControllers.Find(PlanetId))
	{
		return Controller->Get();
	}

	return nullptr;
}

AJTSMoonSurfaceController* AJTSSpaceWorldManager::GetCurrentSurfaceController() const
{
	return IsValid(CurrentPlanet) ? GetSurfaceController(CurrentPlanet->GetPlanetId()) : nullptr;
}

AJTSMoonSurfaceController* AJTSSpaceWorldManager::EnsureCurrentSurfaceController()
{
	return EnsureSurfaceController(CurrentPlanet.Get());
}

void AJTSSpaceWorldManager::NotifySurfaceGameplayInitialized(AJTSMoonSurfaceController* Controller)
{
	if (Controller != nullptr && Controller == GetCurrentSurfaceController())
	{
		bInitialSurfaceGameplayReady = true;
	}
}

bool AJTSSpaceWorldManager::IsSurfaceGameplayReady() const
{
	return bInitialSurfaceGameplayReady
		&& IsSurfaceState()
		&& IsValid(GetCurrentSurfaceController())
		&& GetCurrentSurfaceController()->IsSurfaceGameplayInitialized();
}

void AJTSSpaceWorldManager::HandleFlightAltitude(float ExteriorAltitude)
{
	AJTSPlanetAnchor* const Planet = CurrentPlanet.Get();
	if (!IsValid(Planet))
	{
		return;
	}

	if (CurrentTravelState == EJTSSpaceTravelState::SpaceFlight
		&& ExteriorAltitude <= Planet->GetApproachTransitionAltitude())
	{
		SetTravelState(EJTSSpaceTravelState::Approach);
		RequestSurfaceLoad(Planet, true);
	}
	else if (CurrentTravelState == EJTSSpaceTravelState::SpaceFlight
		&& bEnableSurfaceUnload
		&& ExteriorAltitude >= Planet->GetSurfaceUnloadAltitude())
	{
		RequestSurfaceUnload(Planet);
	}

	if ((CurrentTravelState == EJTSSpaceTravelState::Approach
			|| CurrentTravelState == EJTSSpaceTravelState::Landing
			|| CurrentTravelState == EJTSSpaceTravelState::Surface)
		&& ExteriorAltitude <= Planet->GetSurfaceLoadAltitude())
	{
		RequestSurfaceLoad(Planet, true);
	}

	if (CurrentTravelState == EJTSSpaceTravelState::Approach
		&& ExteriorAltitude <= Planet->GetLandingAssistAltitude()
		&& IsSurfaceLevelLoaded(Planet)
		&& IsSurfaceLevelVisible(Planet))
	{
		SetTravelState(EJTSSpaceTravelState::Landing);
		LandingRequestedDelegate.Broadcast(Planet);
	}
}

FName AJTSSpaceWorldManager::GetStreamingKey(const AJTSPlanetAnchor* Planet) const
{
	if (!IsValid(Planet))
	{
		return NAME_None;
	}

	const FName PlanetId = Planet->GetPlanetId();
	return PlanetId.IsNone() ? Planet->GetFName() : PlanetId;
}

ULevelStreamingDynamic* AJTSSpaceWorldManager::FindSurfaceStreamingLevel(const AJTSPlanetAnchor* Planet) const
{
	if (!IsValid(Planet))
	{
		return nullptr;
	}

	if (const TObjectPtr<ULevelStreamingDynamic>* const StreamingLevel = SurfaceStreamingLevels.Find(GetStreamingKey(Planet)))
	{
		return StreamingLevel->Get();
	}

	return nullptr;
}

AJTSMoonSurfaceController* AJTSSpaceWorldManager::EnsureSurfaceController(AJTSPlanetAnchor* Planet)
{
	if (!IsValid(Planet))
	{
		return nullptr;
	}

	ULevel* const SurfaceLevel = GetLoadedSurfaceLevel(Planet);
	if (!IsValid(SurfaceLevel))
	{
		return nullptr;
	}

	if (AJTSMoonSurfaceController* const RegisteredController = GetSurfaceController(Planet->GetPlanetId()))
	{
		if (RegisteredController->GetSurfaceLevel() == SurfaceLevel)
		{
			RegisteredController->SetOwningPlanet(Planet);
			RegisteredController->SetMoonGameplaySettingsClass(MoonGameplaySettingsClass);
			return RegisteredController;
		}
	}

	AJTSMoonSurfaceController* Controller = nullptr;
	for (AActor* const Actor : SurfaceLevel->Actors)
	{
		AJTSMoonSurfaceController* const Candidate = Cast<AJTSMoonSurfaceController>(Actor);
		if (IsValid(Candidate) && Candidate->GetPlanetId() == Planet->GetPlanetId())
		{
			if (IsValid(Controller))
			{
				UE_LOG(LogTemp, Error, TEXT("Moon surface %s contains more than one MoonSurfaceController."), *Planet->GetPlanetId().ToString());
				return nullptr;
			}
			Controller = Candidate;
		}
	}

	if (!IsValid(Controller))
	{
		UWorld* const World = GetWorld();
		TSubclassOf<AJTSMoonSurfaceController> ControllerClass = MoonSurfaceControllerClass;
		if (ControllerClass == nullptr)
		{
			ControllerClass = AJTSMoonSurfaceController::StaticClass();
		}
		if (World == nullptr || ControllerClass == nullptr)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = FName(*FString::Printf(TEXT("MoonSurfaceController_%s"), *Planet->GetPlanetId().ToString()));
		SpawnParameters.OverrideLevel = SurfaceLevel;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Controller = World->SpawnActor<AJTSMoonSurfaceController>(ControllerClass, FTransform::Identity, SpawnParameters);
	}

	if (IsValid(Controller))
	{
		Controller->SetOwningPlanet(Planet);
		Controller->SetMoonGameplaySettingsClass(MoonGameplaySettingsClass);
		RegisterSurfaceController(Controller);
	}
	return Controller;
}

void AJTSSpaceWorldManager::PollInitialSurfaceArrival()
{
	if (!bInitialArrivalStarted || bInitialSurfaceLevelReady)
	{
		return;
	}

	AJTSPlanetAnchor* const Planet = CurrentPlanet.Get();
	if (!IsValid(Planet) || !IsSurfaceLevelLoaded(Planet) || !IsSurfaceLevelVisible(Planet))
	{
		GetWorldTimerManager().SetTimer(InitialArrivalTimerHandle, this, &AJTSSpaceWorldManager::PollInitialSurfaceArrival, 0.05f, false);
		return;
	}

	AJTSMoonSurfaceController* const Controller = EnsureSurfaceController(Planet);
	if (!IsValid(Controller))
	{
		GetWorldTimerManager().SetTimer(InitialArrivalTimerHandle, this, &AJTSSpaceWorldManager::PollInitialSurfaceArrival, 0.05f, false);
		return;
	}

	bInitialSurfaceLevelReady = true;
	GetWorldTimerManager().ClearTimer(InitialArrivalTimerHandle);
	InitialSurfaceLevelReadyDelegate.Broadcast(Controller);
}

void AJTSSpaceWorldManager::LogDebugState() const
{
	const AJTSPlanetAnchor* const Planet = CurrentPlanet.Get();
	const AJTSSpacecraftActor* Spacecraft = GetCurrentSurfaceController() != nullptr
		? GetCurrentSurfaceController()->GetSpacecraft()
		: nullptr;
	if (!IsValid(Spacecraft))
	{
		if (const UWorld* const World = GetWorld(); World != nullptr && World->PersistentLevel != nullptr)
		{
			for (AActor* const Actor : World->PersistentLevel->Actors)
			{
				if (const AJTSSpacecraftActor* const Candidate = Cast<AJTSSpacecraftActor>(Actor); IsValid(Candidate))
				{
					Spacecraft = Candidate;
					break;
				}
			}
		}
	}

	const float ShipAltitude = IsValid(Planet) && IsValid(Spacecraft)
		? Planet->GetExteriorAltitude(Spacecraft->GetActorLocation())
		: 0.0f;
	const ULevelStreamingDynamic* const SurfaceLevel = FindSurfaceStreamingLevel(Planet);
	const TCHAR* const SurfaceState = !IsValid(SurfaceLevel)
		? TEXT("NotRequested")
		: (SurfaceLevel->IsLevelVisible() ? TEXT("Visible") : (SurfaceLevel->IsLevelLoaded() ? TEXT("Loaded") : TEXT("Unloaded")));
	const UEnum* const TravelStateEnum = StaticEnum<EJTSSpaceTravelState>();
	const FString StateName = TravelStateEnum != nullptr
		? TravelStateEnum->GetNameStringByValue(static_cast<int64>(CurrentTravelState))
		: TEXT("Unknown");
	const FString PlanetName = IsValid(Planet) ? Planet->GetPlanetId().ToString() : TEXT("None");
	const float PlanetRadius = IsValid(Planet) ? Planet->GetPlanetRadius() : 0.0f;
	const FString SurfacePath = IsValid(Planet) && Planet->HasSurfaceLevel()
		? Planet->GetSurfaceLevel().ToSoftObjectPath().ToString()
		: TEXT("None");

	UE_LOG(LogTemp, Log, TEXT("Space World: State=%s Planet=%s ExteriorAltitude=%.1f PlanetRadius=%.1f Surface=%s (%s)"),
		*StateName,
		*PlanetName,
		ShipAltitude,
		PlanetRadius,
		SurfaceState,
		*SurfacePath);
}
