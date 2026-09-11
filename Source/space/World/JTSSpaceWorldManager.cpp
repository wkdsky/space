// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSSpaceWorldManager.h"

#include "Components/SceneComponent.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "space/World/JTSPlanetAnchor.h"

namespace
{
	/** The manager is looked up frequently by character components, so cache the persistent actor after one lookup. */
	TMap<const UWorld*, TWeakObjectPtr<AJTSSpaceWorldManager>> GSpaceWorldManagers;
}

AJTSSpaceWorldManager::AJTSSpaceWorldManager()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

AJTSSpaceWorldManager* AJTSSpaceWorldManager::FindSpaceWorldManager(const UObject* WorldContextObject)
{
	UWorld* const World = WorldContextObject != nullptr ? WorldContextObject->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	if (const TWeakObjectPtr<AJTSSpaceWorldManager>* const CachedManager = GSpaceWorldManagers.Find(World))
	{
		if (AJTSSpaceWorldManager* const Manager = CachedManager->Get(); IsValid(Manager))
		{
			return Manager;
		}

		GSpaceWorldManagers.Remove(World);
	}

	if (World->PersistentLevel == nullptr)
	{
		return nullptr;
	}

	for (AActor* const Actor : World->PersistentLevel->Actors)
	{
		if (AJTSSpaceWorldManager* const Manager = Cast<AJTSSpaceWorldManager>(Actor); IsValid(Manager))
		{
			GSpaceWorldManagers.Add(World, Manager);
			return Manager;
		}
	}

	return nullptr;
}

void AJTSSpaceWorldManager::BeginPlay()
{
	Super::BeginPlay();

	GSpaceWorldManagers.Add(GetWorld(), this);
	RegisterPersistentPlanetAnchors();
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
	if (const TWeakObjectPtr<AJTSSpaceWorldManager>* const CachedManager = GSpaceWorldManagers.Find(GetWorld());
		CachedManager != nullptr && CachedManager->Get() == this)
	{
		GSpaceWorldManagers.Remove(GetWorld());
	}

	PlanetContentStreamingLevels.Empty();
	PlanetRegistry.Empty();

	Super::EndPlay(EndPlayReason);
}

bool AJTSSpaceWorldManager::RegisterPlanet(AJTSPlanetAnchor* Planet)
{
	if (!IsValid(Planet) || Planet->GetPlanetId().IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("Space World Manager ignored a PlanetAnchor without a valid PlanetId."));
		return false;
	}

	const FName PlanetKey = GetPlanetKey(Planet);
	if (const TWeakObjectPtr<AJTSPlanetAnchor>* const ExistingEntry = PlanetRegistry.Find(PlanetKey))
	{
		if (AJTSPlanetAnchor* const ExistingPlanet = ExistingEntry->Get(); IsValid(ExistingPlanet) && ExistingPlanet != Planet)
		{
			UE_LOG(LogTemp, Error, TEXT("Space World Manager found duplicate PlanetId %s on %s and %s. Planet ids must be unique."),
				*PlanetKey.ToString(),
				*ExistingPlanet->GetName(),
				*Planet->GetName());
			return false;
		}
	}

	PlanetRegistry.Add(PlanetKey, Planet);
	return true;
}

void AJTSSpaceWorldManager::UnregisterPlanet(AJTSPlanetAnchor* Planet)
{
	if (!IsValid(Planet))
	{
		return;
	}

	const FName PlanetKey = GetPlanetKey(Planet);
	if (const TWeakObjectPtr<AJTSPlanetAnchor>* const RegisteredPlanet = PlanetRegistry.Find(PlanetKey);
		RegisteredPlanet != nullptr && RegisteredPlanet->Get() == Planet)
	{
		PlanetRegistry.Remove(PlanetKey);
	}

	if (CurrentPlanet.Get() == Planet)
	{
		SetCurrentPlanet(nullptr);
		bSurfaceGameplayReady = false;
	}
}

void AJTSSpaceWorldManager::InitializeCurrentPlanet()
{
	if (IsValid(CurrentPlanet))
	{
		return;
	}

	RegisterPersistentPlanetAnchors();

	AJTSPlanetAnchor* CandidatePlanet = InitialPlanet.Get();
	if (!IsValid(CandidatePlanet) && !InitialPlanetId.IsNone())
	{
		CandidatePlanet = FindPlanetById(InitialPlanetId);
	}

	if (!IsValid(CandidatePlanet))
	{
		UE_LOG(LogTemp, Error, TEXT("Space World Manager requires an InitialPlanet or one registered PlanetAnchor with InitialPlanetId %s."),
			*InitialPlanetId.ToString());
		return;
	}

	if (!RegisterPlanet(CandidatePlanet))
	{
		return;
	}

	SetCurrentPlanet(CandidatePlanet);
	SetTravelState(InitialTravelState);
}

AJTSPlanetAnchor* AJTSSpaceWorldManager::FindPlanetById(FName PlanetId)
{
	if (PlanetId.IsNone())
	{
		return nullptr;
	}

	RegisterPersistentPlanetAnchors();

	if (const TWeakObjectPtr<AJTSPlanetAnchor>* const Planet = PlanetRegistry.Find(PlanetId))
	{
		return Planet->Get();
	}

	return nullptr;
}

AJTSPlanetAnchor* AJTSSpaceWorldManager::FindNearestGameplayPlanet(const FVector& WorldPosition, bool bRequireGravityInfluence)
{
	RegisterPersistentPlanetAnchors();

	AJTSPlanetAnchor* NearestPlanet = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();

	for (auto It = PlanetRegistry.CreateIterator(); It; ++It)
	{
		AJTSPlanetAnchor* const Planet = It.Value().Get();
		if (!IsValid(Planet))
		{
			It.RemoveCurrent();
			continue;
		}

		// Planet selection is a gravity/space query. Terrain collision may stream later and must not
		// prevent a craft from binding to the planet or continuing its natural descent.
		if (!Planet->IsGravityEnabled()
			|| (bRequireGravityInfluence && !Planet->IsWithinGravityInfluence(WorldPosition)))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(WorldPosition, Planet->GetPlanetCenter());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestPlanet = Planet;
		}
	}

	return NearestPlanet;
}

AJTSPlanetAnchor* AJTSSpaceWorldManager::FindPlanetOwningActor(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	RegisterPersistentPlanetAnchors();

	for (auto It = PlanetRegistry.CreateIterator(); It; ++It)
	{
		AJTSPlanetAnchor* const Planet = It.Value().Get();
		if (!IsValid(Planet))
		{
			It.RemoveCurrent();
			continue;
		}

		if (Planet->OwnsGameplaySurfaceActor(Actor))
		{
			return Planet;
		}
	}

	return FindNearestGameplayPlanet(Actor->GetActorLocation());
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

bool AJTSSpaceWorldManager::IsPlanetGameplayActive(const AJTSPlanetAnchor* Planet) const
{
	return IsSurfaceState()
		&& IsValid(Planet)
		&& CurrentPlanet.Get() == Planet;
}

void AJTSSpaceWorldManager::SetCurrentPlanet(AJTSPlanetAnchor* NewCurrentPlanet)
{
	if (CurrentPlanet.Get() == NewCurrentPlanet)
	{
		return;
	}
	if (IsValid(NewCurrentPlanet) && !RegisterPlanet(NewCurrentPlanet))
	{
		return;
	}

	if (AJTSPlanetAnchor* const PreviousPlanet = CurrentPlanet.Get())
	{
		PreviousPlanet->SetActivePlanet(false);
	}

	CurrentPlanet = NewCurrentPlanet;
	bSurfaceGameplayReady = false;
	bLandingEligibilityAnnounced = false;

	if (AJTSPlanetAnchor* const ActivePlanet = CurrentPlanet.Get())
	{
		ActivePlanet->SetActivePlanet(true);
	}

	if (bDebugSpaceTravel)
	{
		UE_LOG(LogTemp, Log, TEXT("Space World current planet changed to %s."),
			IsValid(CurrentPlanet) ? *CurrentPlanet->GetPlanetId().ToString() : TEXT("None"));
	}
}

void AJTSSpaceWorldManager::SetTravelState(EJTSSpaceTravelState NewTravelState)
{
	if (CurrentTravelState == NewTravelState)
	{
		return;
	}

	CurrentTravelState = NewTravelState;
	if (CurrentTravelState == EJTSSpaceTravelState::Approach)
	{
		bLandingEligibilityAnnounced = false;
	}
	else if (CurrentTravelState != EJTSSpaceTravelState::Landing)
	{
		bLandingEligibilityAnnounced = false;
	}
	if (CurrentTravelState != EJTSSpaceTravelState::Surface)
	{
		bSurfaceGameplayReady = false;
	}
	if (bDebugSpaceTravel)
	{
		const UEnum* const TravelStateEnum = StaticEnum<EJTSSpaceTravelState>();
		const FString StateName = TravelStateEnum != nullptr
			? TravelStateEnum->GetNameStringByValue(static_cast<int64>(CurrentTravelState))
			: TEXT("Unknown");
		UE_LOG(LogTemp, Log, TEXT("Space World travel state changed to %s."), *StateName);
	}
}

void AJTSSpaceWorldManager::SetSurfaceGameplayReady(bool bReady)
{
	bSurfaceGameplayReady = bReady && IsValid(CurrentPlanet) && IsSurfaceState();
}

bool AJTSSpaceWorldManager::IsSurfaceGameplayReady() const
{
	return bSurfaceGameplayReady && IsPlanetGameplayActive(CurrentPlanet.Get());
}

bool AJTSSpaceWorldManager::RequestPlanetContentLoad(AJTSPlanetAnchor* Planet, bool bMakeVisible)
{
	if (!IsValid(Planet) || !Planet->HasPlanetContentLevel())
	{
		return false;
	}

	const FName PlanetKey = GetPlanetKey(Planet);
	if (TObjectPtr<ULevelStreamingDynamic>* const ExistingLevel = PlanetContentStreamingLevels.Find(PlanetKey))
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
		Planet->GetPlanetContentLevel(),
		Planet->GetPlanetContentTransform(),
		bLoadSucceeded);
	if (!bLoadSucceeded || !IsValid(StreamingLevel))
	{
		UE_LOG(LogTemp, Warning, TEXT("Space World could not request optional content level %s for planet %s."),
			*Planet->GetPlanetContentLevel().ToSoftObjectPath().ToString(),
			*Planet->GetPlanetId().ToString());
		return false;
	}

	StreamingLevel->SetShouldBeLoaded(true);
	StreamingLevel->SetShouldBeVisible(bMakeVisible);
	PlanetContentStreamingLevels.Add(PlanetKey, StreamingLevel);
	return true;
}

bool AJTSSpaceWorldManager::RequestPlanetContentUnload(AJTSPlanetAnchor* Planet)
{
	if (!bEnablePlanetContentUnload)
	{
		return false;
	}

	ULevelStreamingDynamic* const StreamingLevel = FindPlanetContentStreamingLevel(Planet);
	if (!IsValid(StreamingLevel))
	{
		return false;
	}

	StreamingLevel->SetShouldBeVisible(false);
	StreamingLevel->SetShouldBeLoaded(false);
	return true;
}

bool AJTSSpaceWorldManager::IsPlanetContentLoaded(const AJTSPlanetAnchor* Planet) const
{
	const ULevelStreamingDynamic* const StreamingLevel = FindPlanetContentStreamingLevel(Planet);
	return IsValid(StreamingLevel) && StreamingLevel->IsLevelLoaded();
}

bool AJTSSpaceWorldManager::IsPlanetContentVisible(const AJTSPlanetAnchor* Planet) const
{
	const ULevelStreamingDynamic* const StreamingLevel = FindPlanetContentStreamingLevel(Planet);
	return IsValid(StreamingLevel) && StreamingLevel->IsLevelVisible();
}

ULevel* AJTSSpaceWorldManager::GetLoadedPlanetContentLevel(const AJTSPlanetAnchor* Planet) const
{
	if (ULevelStreamingDynamic* const StreamingLevel = FindPlanetContentStreamingLevel(Planet))
	{
		return StreamingLevel->GetLoadedLevel();
	}

	return nullptr;
}

FOnJTSSpaceWorldLandingRequested& AJTSSpaceWorldManager::OnLandingRequested()
{
	return LandingRequestedDelegate;
}

void AJTSSpaceWorldManager::HandleFlightAltitude(float ApproximateAltitude)
{
	AJTSPlanetAnchor* const Planet = CurrentPlanet.Get();
	if (!IsValid(Planet))
	{
		return;
	}

	if (CurrentTravelState == EJTSSpaceTravelState::SpaceFlight
		&& ApproximateAltitude <= Planet->GetApproachTransitionAltitude())
	{
		SetTravelState(EJTSSpaceTravelState::Approach);
		RequestPlanetContentLoad(Planet, true);
	}

	if (CurrentTravelState == EJTSSpaceTravelState::Approach
		&& ApproximateAltitude <= Planet->GetLandingAssistAltitude()
		&& !bLandingEligibilityAnnounced)
	{
		// Reaching an altitude only makes landing *eligible*. A concrete LandingSite union query
		// plus vehicle/surface validation is the only path that may transition to Landing.
		bLandingEligibilityAnnounced = true;
		LandingRequestedDelegate.Broadcast(Planet);
	}
}

void AJTSSpaceWorldManager::RegisterPersistentPlanetAnchors()
{
	if (bPlanetRegistryInitialized)
	{
		return;
	}

	bPlanetRegistryInitialized = true;
	UWorld* const World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr)
	{
		return;
	}

	for (AActor* const Actor : World->PersistentLevel->Actors)
	{
		if (AJTSPlanetAnchor* const Planet = Cast<AJTSPlanetAnchor>(Actor); IsValid(Planet))
		{
			RegisterPlanet(Planet);
		}
	}
}

FName AJTSSpaceWorldManager::GetPlanetKey(const AJTSPlanetAnchor* Planet) const
{
	if (!IsValid(Planet))
	{
		return NAME_None;
	}

	const FName PlanetId = Planet->GetPlanetId();
	return PlanetId.IsNone() ? Planet->GetFName() : PlanetId;
}

ULevelStreamingDynamic* AJTSSpaceWorldManager::FindPlanetContentStreamingLevel(const AJTSPlanetAnchor* Planet) const
{
	if (!IsValid(Planet))
	{
		return nullptr;
	}

	if (const TObjectPtr<ULevelStreamingDynamic>* const StreamingLevel = PlanetContentStreamingLevels.Find(GetPlanetKey(Planet)))
	{
		return StreamingLevel->Get();
	}

	return nullptr;
}

void AJTSSpaceWorldManager::LogDebugState() const
{
	const AJTSPlanetAnchor* const Planet = CurrentPlanet.Get();
	const UEnum* const TravelStateEnum = StaticEnum<EJTSSpaceTravelState>();
	const FString StateName = TravelStateEnum != nullptr
		? TravelStateEnum->GetNameStringByValue(static_cast<int64>(CurrentTravelState))
		: TEXT("Unknown");
	const FString PlanetName = IsValid(Planet) ? Planet->GetPlanetId().ToString() : TEXT("None");
	const float PlanetRadius = IsValid(Planet) ? Planet->GetApproximateRadius() : 0.0f;
	const ULevelStreamingDynamic* const ContentLevel = FindPlanetContentStreamingLevel(Planet);
	const TCHAR* const ContentState = !IsValid(ContentLevel)
		? TEXT("NotRequested")
		: (ContentLevel->IsLevelVisible() ? TEXT("Visible") : (ContentLevel->IsLevelLoaded() ? TEXT("Loaded") : TEXT("Unloaded")));

	UE_LOG(LogTemp, Log, TEXT("Space World: State=%s Planet=%s ApproximateRadius=%.1f SurfaceReady=%s Content=%s"),
		*StateName,
		*PlanetName,
		PlanetRadius,
		bSurfaceGameplayReady ? TEXT("true") : TEXT("false"),
		ContentState);
}
