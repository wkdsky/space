// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSSpaceWorldManager.h"

#include "Camera/CameraComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "space/Core/JTSGameState.h"
#include "space/Components/JTSSpacecraftFlightMovementComponent.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Player/JTSCharacter.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/World/JTSMoonAntActor.h"
#include "space/World/JTSMoonAntCorpsePickupActor.h"
#include "space/World/JTSMoonAntNestActor.h"
#include "space/World/JTSMoonCorpseActor.h"
#include "space/World/JTSMoonResourceActor.h"
#include "space/World/JTSMoonResourceSpawner.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetLandingSite.h"
#include "space/World/JTSPlanetSurfaceAnchor.h"


namespace
{
	/** The manager is looked up frequently by character components, so cache the persistent actor after one lookup. */
	TMap<const UWorld*, TWeakObjectPtr<AJTSSpaceWorldManager>> GSpaceWorldManagers;

	float ResolveSurfaceAltitude(
		const AJTSSpacecraftActor* Spacecraft,
		const AJTSPlanetAnchor* Planet,
		const FVector& SpacecraftLocation)
	{
		float SurfaceAltitude = 0.0f;
		if (IsValid(Spacecraft))
		{
			if (const UJTSSpacecraftFlightMovementComponent* const Movement = Spacecraft->GetFlightMovementComponent();
				Movement != nullptr && Movement->GetResolvedSurfaceAltitude(Planet, SurfaceAltitude))
			{
				return SurfaceAltitude;
			}
		}

		if (IsValid(Planet) && Planet->GetAltitudeAboveSurface(SpacecraftLocation, SurfaceAltitude))
		{
			return SurfaceAltitude;
		}

		// The authored radius remains a safe fallback for unloaded/invalid collision, never the
		// primary source for a loaded real planet.
		return IsValid(Planet) ? Planet->GetApproximateAltitude(SpacecraftLocation) : 0.0f;
	}
}

AJTSSpaceWorldManager::AJTSSpaceWorldManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

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
	if (HasAuthority())
	{
		InitializeCurrentPlanet();
	}
	RefreshCelestialVisibility(FVector::ZeroVector, 90.0f);

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

bool AJTSSpaceWorldManager::IsAirborneTravel() const
{
	return CurrentTravelState == EJTSSpaceTravelState::Takeoff
		|| CurrentTravelState == EJTSSpaceTravelState::SpaceFlight
		|| CurrentTravelState == EJTSSpaceTravelState::Approach
		|| CurrentTravelState == EJTSSpaceTravelState::Landing;
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
	if (!HasAuthority())
	{
		return;
	}
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
	if (AJTSGameState* const GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr)
	{
		GameState->SetCurrentPlanetId(IsValid(CurrentPlanet) ? CurrentPlanet->GetPlanetId().ToString() : FString());
	}
	RefreshCelestialVisibility(FVector::ZeroVector, 90.0f);
	if (UGameInstance* const GameInstance = GetGameInstance())
	{
		if (UJTSExpeditionSubsystem* const Expedition = GameInstance->GetSubsystem<UJTSExpeditionSubsystem>())
		{
			Expedition->SetCurrentPlanetId(IsValid(CurrentPlanet) ? CurrentPlanet->GetPlanetId().ToString() : FString());
		}
	}

	if (bDebugSpaceTravel)
	{
		UE_LOG(LogTemp, Log, TEXT("Space World current planet changed to %s."),
			IsValid(CurrentPlanet) ? *CurrentPlanet->GetPlanetId().ToString() : TEXT("None"));
	}
}

void AJTSSpaceWorldManager::SetTravelState(EJTSSpaceTravelState NewTravelState)
{
	if (!HasAuthority())
	{
		return;
	}
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
	if (AJTSGameState* const GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr)
	{
		GameState->SetGameplayPhase(CurrentTravelState == EJTSSpaceTravelState::Surface
			? EJTSGameplayPhase::MoonExploration
			: EJTSGameplayPhase::SpaceFlight);
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
	if (!HasAuthority())
	{
		return;
	}
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

void AJTSSpaceWorldManager::HandleFlightAltitude(float SurfaceAltitude)
{
	if (!HasAuthority())
	{
		return;
	}
	AJTSPlanetAnchor* const Planet = CurrentPlanet.Get();
	if (!IsValid(Planet))
	{
		return;
	}

	if (CurrentTravelState == EJTSSpaceTravelState::Takeoff
		&& SurfaceAltitude >= Planet->GetSpaceFlightAltitude())
	{
		SetTravelState(EJTSSpaceTravelState::SpaceFlight);
		return;
	}

	if (CurrentTravelState == EJTSSpaceTravelState::SpaceFlight
		&& SurfaceAltitude <= Planet->GetApproachTransitionAltitude())
	{
		SetTravelState(EJTSSpaceTravelState::Approach);
		RequestPlanetContentLoad(Planet, true);
	}

	if (CurrentTravelState == EJTSSpaceTravelState::Approach
		&& SurfaceAltitude <= Planet->GetLandingAssistAltitude()
		&& !bLandingEligibilityAnnounced)
	{
		// Reaching an altitude only makes landing *eligible*. A concrete LandingSite union query
		// plus vehicle/surface validation is the only path that may transition to Landing.
		bLandingEligibilityAnnounced = true;
		LandingRequestedDelegate.Broadcast(Planet);
	}
}

void AJTSSpaceWorldManager::UpdateSpacecraftFlightState(AJTSSpacecraftActor* Spacecraft)
{
	if (!HasAuthority()
		|| !IsValid(Spacecraft)
		|| Spacecraft->IsLanded()
		|| Spacecraft->GetFlightState() == EJTSSpacecraftFlightState::LandingAssist)
	{
		return;
	}

	const FVector SpacecraftLocation = Spacecraft->GetActorLocation();
	AJTSPlanetAnchor* const CurrentPlanetAnchor = CurrentPlanet.Get();

	if (CurrentTravelState == EJTSSpaceTravelState::Takeoff)
	{
		// The departure planet remains the low-flight reference only until the configured
		// space-flight altitude. Above it, the ship deliberately has no planet target at all.
		AJTSPlanetAnchor* const DeparturePlanet = IsValid(Spacecraft->GetFlightPlanet())
			? Spacecraft->GetFlightPlanet()
			: CurrentPlanetAnchor;
		if (!IsValid(DeparturePlanet))
		{
			return;
		}

		if (Spacecraft->GetFlightPlanet() != DeparturePlanet)
		{
			Spacecraft->SetFlightTargetPlanet(DeparturePlanet);
		}

		HandleFlightAltitude(ResolveSurfaceAltitude(Spacecraft, DeparturePlanet, SpacecraftLocation));
		if (CurrentTravelState == EJTSSpaceTravelState::SpaceFlight)
		{
			Spacecraft->SetFlightTargetPlanet(nullptr);
		}
		return;
	}

	if (CurrentTravelState == EJTSSpaceTravelState::SpaceFlight)
	{
		// Influence ranges are configured per real planet and are guaranteed by level setup not to
		// overlap. Outside every range there is intentionally no current or flight planet, so old
		// Moon-centred camera, HUD, and landing queries cannot leak into deep space.
		AJTSPlanetAnchor* const NearbyPlanet = FindNearestGameplayPlanet(SpacecraftLocation, true);
		if (!IsValid(NearbyPlanet))
		{
			if (IsValid(CurrentPlanetAnchor))
			{
				LastDepartedPlanet = CurrentPlanetAnchor;
			}
			if (IsValid(Spacecraft->GetFlightPlanet()))
			{
				Spacecraft->SetFlightTargetPlanet(nullptr);
			}
			if (IsValid(CurrentPlanetAnchor))
			{
				SetCurrentPlanet(nullptr);
			}
			return;
		}

		const float Altitude = ResolveSurfaceAltitude(Spacecraft, NearbyPlanet, SpacecraftLocation);
		if (NearbyPlanet != CurrentPlanetAnchor)
		{
			SetCurrentPlanet(NearbyPlanet);
			Spacecraft->SetFlightTargetPlanet(NearbyPlanet);
		}

		const FVector RadialUp = NearbyPlanet->GetRadialUpVector(SpacecraftLocation).GetSafeNormal();
		const float RadialVelocity = FVector::DotProduct(Spacecraft->GetFlightVelocity(), RadialUp);
		const bool bMovingTowardPlanet = RadialVelocity < -KINDA_SMALL_NUMBER;
		const bool bHasArrivalTarget = Spacecraft->GetFlightPlanet() == NearbyPlanet;
		if (Altitude <= NearbyPlanet->GetApproachTransitionAltitude()
			&& bMovingTowardPlanet)
		{
			if (!bHasArrivalTarget)
			{
				Spacecraft->SetFlightTargetPlanet(NearbyPlanet);
			}
			HandleFlightAltitude(Altitude);
		}
		return;
	}

	if (CurrentTravelState == EJTSSpaceTravelState::Approach)
	{
		if (!IsValid(CurrentPlanetAnchor))
		{
			SetTravelState(EJTSSpaceTravelState::SpaceFlight);
			return;
		}

		const float Altitude = ResolveSurfaceAltitude(Spacecraft, CurrentPlanetAnchor, SpacecraftLocation);
		if (Altitude > CurrentPlanetAnchor->GetApproachTransitionAltitude())
		{
			SetTravelState(EJTSSpaceTravelState::SpaceFlight);
			return;
		}

		if (Spacecraft->GetFlightPlanet() != CurrentPlanetAnchor)
		{
			Spacecraft->SetFlightTargetPlanet(CurrentPlanetAnchor);
		}
		HandleFlightAltitude(Altitude);
	}
}

bool AJTSSpaceWorldManager::SharesLocalSky(
	const AJTSPlanetAnchor* First,
	const AJTSPlanetAnchor* Second) const
{
	return SharesLocalTransfer(First, Second);
}

float AJTSSpaceWorldManager::GetRouteKilometers(
	const AJTSPlanetAnchor* Origin,
	const AJTSPlanetAnchor* Destination) const
{
	return ResolveRouteKilometers(Origin, Destination);
}

float AJTSSpaceWorldManager::GetApparentRangeCentimeters(
	const AJTSSpacecraftActor* Spacecraft,
	const AJTSPlanetAnchor* Planet) const
{
	return ResolveApparentRangeCentimeters(Spacecraft, Planet);
}

float AJTSSpaceWorldManager::GetCruiseRangeCentimeters(
	const AJTSSpacecraftActor* Spacecraft,
	const AJTSPlanetAnchor* Planet) const
{
	if (!IsValid(Spacecraft) || !IsValid(Planet))
	{
		return 0.0f;
	}

	const AJTSPlanetAnchor* const Reference = ResolveReferencePlanet(Spacecraft);
	if (!IsValid(Reference))
	{
		return 0.0f;
	}

	const AJTSPlanetAnchor* const Foreign = Planet == Reference || SharesLocalTransfer(Reference, Planet)
		? FindNearestForeignPlanet(Reference)
		: Planet;
	if (!IsValid(Foreign) || Foreign == Reference)
	{
		return 0.0f;
	}

	const float RouteKilometers = ResolveRouteKilometers(Reference, Foreign);
	const FVector CorridorStart = Reference->GetPlanetCenter();
	const FVector Corridor = Foreign->GetPlanetCenter() - CorridorStart;
	const float CorridorLength = Corridor.Size();
	if (RouteKilometers <= KINDA_SMALL_NUMBER || CorridorLength <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	// The dial reads the ship along the straight corridor between the two bodies.
	// Flying toward a body shortens its reading. Flying away lengthens it. The previous
	// progress was distance from the reference only, so leaving that body made both
	// labels move, and swapping the reference swapped the two numbers.
	const float AlongCorridor = FVector::DotProduct(Spacecraft->GetActorLocation() - CorridorStart, Corridor)
		/ (CorridorLength * CorridorLength);
	const float TargetShare = Planet == Foreign ? 1.0f : 0.0f;
	return FMath::Max(0.0f, FMath::Abs(TargetShare - AlongCorridor) * RouteKilometers * 100000.0f);
}

float AJTSSpaceWorldManager::GetNavigationSurfaceRangeCentimeters(
	const AJTSSpacecraftActor* Spacecraft,
	const AJTSPlanetAnchor* Planet) const
{
	if (!IsValid(Spacecraft) || !IsValid(Planet))
	{
		return 0.0f;
	}

	const float LevelAltitude = FMath::Max(0.0f, ResolvePresentationAltitude(Planet, Spacecraft));
	if (LevelAltitude <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	// Navigation metres are a log of the level climb, not the climb itself.
	// Two metres off the ground read about 24 m. The authored-scale ceiling (500 m), where the
	// planet starts shrinking, reads about 7.2 km. The top of this staff is a low orbit: 120 km,
	// the band where KSP and Outer Wilds still treat you as flying over one world. Past it the
	// dial hands off to astronomical cruise and floors at <0.01 AU.
	constexpr float ScaleLengthCentimeters = 8000.0f;
	constexpr float LogGainCentimeters = 1200000.0f;
	constexpr float LowOrbitCentimeters = 12000000.0f;
	return FMath::Min(
		LowOrbitCentimeters,
		LogGainCentimeters * FMath::Loge(1.0f + LevelAltitude / ScaleLengthCentimeters));
}

void AJTSSpaceWorldManager::GetRegisteredPlanets(TArray<AJTSPlanetAnchor*>& OutPlanets) const
{
	OutPlanets.Reset();
	for (const TPair<FName, TWeakObjectPtr<AJTSPlanetAnchor>>& Entry : PlanetRegistry)
	{
		if (AJTSPlanetAnchor* const Planet = Entry.Value.Get(); IsValid(Planet))
		{
			OutPlanets.Add(Planet);
		}
	}
}

float AJTSSpaceWorldManager::ResolveRouteKilometers(
	const AJTSPlanetAnchor* Origin,
	const AJTSPlanetAnchor* Destination) const
{
	if (!IsValid(Origin) || !IsValid(Destination) || Origin == Destination)
	{
		return 0.0f;
	}

	const float OriginDistance = Origin->GetHeliocentricDistanceKilometers();
	const float DestinationDistance = Destination->GetHeliocentricDistanceKilometers();
	if (OriginDistance <= KINDA_SMALL_NUMBER || DestinationDistance <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::Abs(OriginDistance - DestinationDistance);
}

bool AJTSSpaceWorldManager::SharesLocalTransfer(const AJTSPlanetAnchor* First, const AJTSPlanetAnchor* Second) const
{
	if (!IsValid(First) || !IsValid(Second) || First == Second)
	{
		return false;
	}

	const FName FirstParent = First->GetParentPlanetId();
	const FName SecondParent = Second->GetParentPlanetId();
	return (!FirstParent.IsNone() && FirstParent == Second->GetPlanetId())
		|| (!SecondParent.IsNone() && SecondParent == First->GetPlanetId())
		|| (!FirstParent.IsNone() && FirstParent == SecondParent);
}

void AJTSSpaceWorldManager::SetCelestialBodyHidden(AJTSPlanetAnchor* Planet, bool bHideBody) const
{
	if (!IsValid(Planet))
	{
		return;
	}

	Planet->SetActorHiddenInGame(bHideBody);
	if (AActor* const SurfaceActor = Planet->GetGameplaySurfaceActor(); IsValid(SurfaceActor))
	{
		// Hide the mesh only. Landing and surface queries still need the authored collision.
		SurfaceActor->SetActorHiddenInGame(bHideBody);
		SurfaceActor->SetActorEnableCollision(true);
	}
}

float AJTSSpaceWorldManager::ResolveLocalSeparationCentimeters(
	const AJTSPlanetAnchor* First,
	const AJTSPlanetAnchor* Second) const
{
	if (!IsValid(First) || !IsValid(Second) || First == Second)
	{
		return 0.0f;
	}

	const float CenterDistance = FVector::Distance(First->GetPlanetCenter(), Second->GetPlanetCenter());
	const float Clearance = FMath::Max(0.0f, CenterDistance - First->GetVisualRadius() - Second->GetVisualRadius());
	return FMath::Max(Clearance, 1.0f);
}

AJTSPlanetAnchor* AJTSSpaceWorldManager::ResolveReferencePlanet(const AJTSSpacecraftActor* Spacecraft) const
{
	if (!IsValid(Spacecraft))
	{
		return CurrentPlanet.Get();
	}

	if (AJTSPlanetAnchor* const FlightPlanet = Spacecraft->GetFlightPlanet(); IsValid(FlightPlanet))
	{
		return FlightPlanet;
	}
	if (AJTSPlanetAnchor* const ActivePlanet = CurrentPlanet.Get(); IsValid(ActivePlanet))
	{
		return ActivePlanet;
	}
	if (AJTSPlanetAnchor* const DepartedPlanet = LastDepartedPlanet.Get(); IsValid(DepartedPlanet))
	{
		return DepartedPlanet;
	}
	return FindNearestRoutedPlanet(Spacecraft);
}

AJTSPlanetAnchor* AJTSSpaceWorldManager::FindNearestRoutedPlanet(const AJTSSpacecraftActor* Spacecraft) const
{
	if (!IsValid(Spacecraft))
	{
		return nullptr;
	}

	const FVector ShipLocation = Spacecraft->GetActorLocation();
	AJTSPlanetAnchor* NearestPlanet = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (const TPair<FName, TWeakObjectPtr<AJTSPlanetAnchor>>& Entry : PlanetRegistry)
	{
		AJTSPlanetAnchor* const Planet = Entry.Value.Get();
		if (!IsValid(Planet) || Planet->GetHeliocentricDistanceKilometers() <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(ShipLocation, Planet->GetPlanetCenter());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestPlanet = Planet;
		}
	}

	return NearestPlanet;
}

const AJTSPlanetAnchor* AJTSSpaceWorldManager::FindNearestForeignPlanet(const AJTSPlanetAnchor* Reference) const
{
	if (!IsValid(Reference))
	{
		return nullptr;
	}

	const AJTSPlanetAnchor* NearestPlanet = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (const TPair<FName, TWeakObjectPtr<AJTSPlanetAnchor>>& Entry : PlanetRegistry)
	{
		const AJTSPlanetAnchor* const Planet = Entry.Value.Get();
		if (!IsValid(Planet)
			|| Planet == Reference
			|| SharesLocalTransfer(Reference, Planet)
			|| ResolveRouteKilometers(Reference, Planet) <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(Reference->GetPlanetCenter(), Planet->GetPlanetCenter());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestPlanet = Planet;
		}
	}

	return NearestPlanet;
}

float AJTSSpaceWorldManager::ResolveTravelProgress(
	const AJTSSpacecraftActor* Spacecraft,
	const AJTSPlanetAnchor* Reference,
	const AJTSPlanetAnchor* Foreign) const
{
	if (!IsValid(Spacecraft) || !IsValid(Reference) || !IsValid(Foreign))
	{
		return 0.0f;
	}

	const float DepartureBubble = Reference->GetApproximateRadius() + Reference->GetGravityInfluenceRange();
	const float ArrivalBubble = Foreign->GetApproximateRadius() + Foreign->GetGravityInfluenceRange();
	const float Separation = FVector::Distance(Reference->GetPlanetCenter(), Foreign->GetPlanetCenter());
	const float Span = FMath::Max(Separation - DepartureBubble - ArrivalBubble, 1.0f);
	const float Travelled = FVector::Distance(Spacecraft->GetActorLocation(), Reference->GetPlanetCenter()) - DepartureBubble;
	return FMath::Clamp(Travelled / Span, 0.0f, 1.0f);
}

float AJTSSpaceWorldManager::ResolveCelestialScaleRatio(
	const AJTSPlanetAnchor* Planet,
	const AJTSSpacecraftActor* Spacecraft) const
{
	if (!IsValid(Planet) || !IsValid(Spacecraft))
	{
		return 1.0f;
	}

	// Near-surface flight, including ship-versus-creature range, stays at the authored mesh size.
	// The hold remembers the previous frame so skimming the boundary does not pulse the scale.
	const float Altitude = ResolvePresentationAltitude(Planet, Spacecraft);
	const float AuthoredAltitude = FMath::Max(0.0f, AuthoredScaleAltitudeCentimeters);
	const float ReleaseAltitude = AuthoredAltitude + FMath::Max(0.0f, AuthoredScaleReleaseMarginCentimeters);
	if (Altitude <= AuthoredAltitude)
	{
		return 1.0f;
	}

	const AActor* const SurfaceActor = Planet->GetGameplaySurfaceActor();
	const FSavedCelestialSurface* const SavedSurface = IsValid(SurfaceActor)
		? SavedCelestialSurfaces.Find(SurfaceActor)
		: nullptr;
	const bool bWasAuthored = SavedSurface == nullptr
		|| !SavedSurface->bCaptured
		|| !IsValid(SurfaceActor)
		|| SurfaceActor->GetActorScale3D().Equals(SavedSurface->Scale, 0.01f);
	if (bWasAuthored && Altitude < ReleaseAltitude)
	{
		return 1.0f;
	}

	// Apparent range is astronomical and jumps by orders of magnitude inside this compact map.
	// Scale follows the metres actually flown past the release shell so the disk shrinks every frame.
	const float LevelDistance = FMath::Max(
		FVector::Distance(Spacecraft->GetActorLocation(), Planet->GetPlanetCenter()),
		1.0f);
	const float ReleaseShell = ResolveAuthoredSurfaceRadius(Planet) + ReleaseAltitude;
	const float BeyondShell = FMath::Max(0.0f, LevelDistance - ReleaseShell);
	const float ShrinkSpan = FMath::Max(ResolveCelestialShrinkSpanCentimeters(Planet), 1.0f);
	const float Progress = FMath::Clamp(BeyondShell / ShrinkSpan, 0.0f, 1.0f);
	const float FarScale = FMath::Clamp(MinimumCelestialScaleRatio, 0.0001f, 1.0f);
	return FMath::Pow(FarScale, Progress);
}

float AJTSSpaceWorldManager::ResolveCelestialShrinkSpanCentimeters(const AJTSPlanetAnchor* Planet) const
{
	if (CelestialShrinkSpanCentimeters > KINDA_SMALL_NUMBER)
	{
		return CelestialShrinkSpanCentimeters;
	}

	if (!IsValid(Planet))
	{
		return 1000.0f;
	}

	const float ReleaseAltitude = FMath::Max(0.0f, AuthoredScaleAltitudeCentimeters)
		+ FMath::Max(0.0f, AuthoredScaleReleaseMarginCentimeters);
	const float LocalShell = ResolveAuthoredSurfaceRadius(Planet) + ReleaseAltitude;
	const AJTSPlanetAnchor* const Foreign = FindNearestForeignPlanet(Planet);
	if (!IsValid(Foreign))
	{
		return 200000.0f;
	}

	const float ForeignShell = ResolveAuthoredSurfaceRadius(Foreign) + ReleaseAltitude;
	const float Separation = FVector::Distance(Planet->GetPlanetCenter(), Foreign->GetPlanetCenter());
	return FMath::Max(Separation - LocalShell - ForeignShell, 1000.0f);
}

float AJTSSpaceWorldManager::ResolveAuthoredSurfaceRadius(const AJTSPlanetAnchor* Planet) const
{
	if (!IsValid(Planet))
	{
		return 1.0f;
	}

	const AActor* const SurfaceActor = Planet->GetGameplaySurfaceActor();
	const FSavedCelestialSurface* const SavedSurface = IsValid(SurfaceActor)
		? SavedCelestialSurfaces.Find(SurfaceActor)
		: nullptr;
	if (SavedSurface != nullptr && SavedSurface->bCaptured && IsValid(SurfaceActor))
	{
		const float CurrentScale = FMath::Max(SurfaceActor->GetActorScale3D().GetAbsMax(), 0.001f);
		const float AuthoredScale = FMath::Max(SavedSurface->Scale.GetAbsMax(), 0.001f);
		FVector BoundsOrigin = FVector::ZeroVector;
		FVector Extent = FVector::ZeroVector;
		SurfaceActor->GetActorBounds(false, BoundsOrigin, Extent);
		const float CurrentRadius = FMath::Max(Extent.GetAbsMax(), 1.0f);
		return FMath::Max(CurrentRadius * AuthoredScale / CurrentScale, 1.0f);
	}
	return FMath::Max(Planet->GetVisualRadius(), 1.0f);
}

float AJTSSpaceWorldManager::ResolvePresentationAltitude(
	const AJTSPlanetAnchor* Planet,
	const AJTSSpacecraftActor* Spacecraft) const
{
	if (!IsValid(Planet) || !IsValid(Spacecraft))
	{
		return 0.0f;
	}

	const float LevelDistance = FVector::Distance(Spacecraft->GetActorLocation(), Planet->GetPlanetCenter());
	const float AuthoredAltitude = FMath::Max(0.0f, LevelDistance - ResolveAuthoredSurfaceRadius(Planet));
	// A shrunk mesh reports altitude from its impostor surface. The 500 m band is measured from the
	// authored ground, so the trace is only trusted while the body is still full size.
	if (!IsPlanetPresentedAtAuthoredScale(Planet))
	{
		return AuthoredAltitude;
	}

	float SurfaceAltitude = 0.0f;
	if (Planet->GetAltitudeAboveSurface(Spacecraft->GetActorLocation(), SurfaceAltitude))
	{
		return FMath::Max(0.0f, SurfaceAltitude);
	}
	return AuthoredAltitude;
}

float AJTSSpaceWorldManager::ResolveApparentRangeCentimeters(
	const AJTSSpacecraftActor* Spacecraft,
	const AJTSPlanetAnchor* Planet) const
{
	if (!IsValid(Spacecraft) || !IsValid(Planet))
	{
		return 0.0f;
	}

	const float LevelDistance = FVector::Distance(Spacecraft->GetActorLocation(), Planet->GetPlanetCenter());
	const AJTSPlanetAnchor* const Reference = ResolveReferencePlanet(Spacecraft);
	if (!IsValid(Reference) || SharesLocalTransfer(Reference, Planet))
	{
		return LevelDistance;
	}

	const AJTSPlanetAnchor* const Foreign = Planet == Reference
		? FindNearestForeignPlanet(Reference)
		: Planet;
	const float RouteKilometers = ResolveRouteKilometers(Reference, Foreign);
	if (!IsValid(Foreign) || RouteKilometers <= KINDA_SMALL_NUMBER)
	{
		return LevelDistance;
	}

	const float Progress = ResolveTravelProgress(Spacecraft, Reference, Foreign);
	const float NearKilometers = FMath::Max(LevelDistance / 100000.0f, 0.001f);
	// Orders of magnitude, not a linear kilometre ramp. A 200 m body against a real
	// interplanetary route would otherwise cross the visible threshold in a single frame.
	// Progress 0 matches the level distance. Progress 1 matches the heliocentric route.
	const float ApparentKilometers = Planet == Reference
		? NearKilometers * FMath::Pow(RouteKilometers / NearKilometers, Progress)
		: RouteKilometers * FMath::Pow(NearKilometers / RouteKilometers, Progress);
	return FMath::Max(LevelDistance, ApparentKilometers * 100000.0f);
}

bool AJTSSpaceWorldManager::IsPlanetNoticeableFrom(
	const AJTSPlanetAnchor* Planet,
	const AJTSSpacecraftActor* Viewer) const
{
	if (!IsValid(Planet))
	{
		return false;
	}

	// The test is the disk the camera can see: the mesh after this frame's scale, at the
	// level distance. Apparent range stays on the radar and must not hide a still-large body.
	const float Radius = FMath::Max(1.0f, Planet->GetVisualRadius());
	const float Distance = FMath::Max(
		IsValid(Viewer)
			? FVector::Distance(Viewer->GetActorLocation(), Planet->GetPlanetCenter())
			: FVector::Distance(FVector::ZeroVector, Planet->GetPlanetCenter()),
		1.0f);
	if (Distance <= Radius)
	{
		return true;
	}

	const float AngularDiameterDegrees = FMath::RadiansToDegrees(2.0f * FMath::Atan(Radius / Distance));
	return AngularDiameterDegrees >= FMath::Max(0.001f, MinimumNoticeableAngularDiameterDegrees);
}

void AJTSSpaceWorldManager::ApplyCelestialScale(
	AJTSPlanetAnchor* Planet,
	const AJTSSpacecraftActor* Spacecraft)
{
	if (!IsValid(Planet))
	{
		return;
	}

	AActor* const SurfaceActor = Planet->GetGameplaySurfaceActor();
	if (!IsValid(SurfaceActor))
	{
		return;
	}

	FSavedCelestialSurface& SavedSurface = SavedCelestialSurfaces.FindOrAdd(SurfaceActor);
	if (!SavedSurface.bCaptured)
	{
		SavedSurface.Scale = SurfaceActor->GetActorScale3D();
		SavedSurface.bCaptured = true;
	}

	const float ScaleRatio = ResolveCelestialScaleRatio(Planet, Spacecraft);
	SurfaceActor->SetActorScale3D(SavedSurface.Scale * ScaleRatio);
	SurfaceActor->SetActorEnableCollision(true);
}

bool AJTSSpaceWorldManager::IsPlanetPresentedAtAuthoredScale(const AJTSPlanetAnchor* Planet) const
{
	if (!IsValid(Planet))
	{
		return false;
	}

	const AActor* const SurfaceActor = Planet->GetGameplaySurfaceActor();
	if (!IsValid(SurfaceActor))
	{
		return true;
	}

	const FSavedCelestialSurface* const SavedSurface = SavedCelestialSurfaces.Find(SurfaceActor);
	if (SavedSurface == nullptr || !SavedSurface->bCaptured)
	{
		return true;
	}

	return SurfaceActor->GetActorScale3D().Equals(SavedSurface->Scale, 0.01f);
}

bool AJTSSpaceWorldManager::IsPersistentOccupant(const AActor* Actor)
{
	return IsValid(Actor)
		&& (Actor->IsA<AJTSCharacter>() || Actor->IsA<AJTSSpacecraftActor>() || Actor->IsA<APawn>());
}

bool AJTSSpaceWorldManager::ActorBelongsToPlanet(const AActor* Actor, const AJTSPlanetAnchor* Planet) const
{
	if (!IsValid(Actor) || !IsValid(Planet) || Actor == Planet || IsPersistentOccupant(Actor))
	{
		return false;
	}
	if (Planet->OwnsGameplaySurfaceActor(Actor))
	{
		return false;
	}

	const FName PlanetId = Planet->GetPlanetId();
	if (const AJTSPlanetLandingSite* const LandingSite = Cast<AJTSPlanetLandingSite>(Actor))
	{
		return LandingSite->GetPlanetAnchor() == Planet
			|| (!PlanetId.IsNone() && LandingSite->GetPlanetId() == PlanetId);
	}
	if (const AJTSPlanetSurfaceAnchor* const SurfaceAnchor = Cast<AJTSPlanetSurfaceAnchor>(Actor))
	{
		return SurfaceAnchor->GetPlanetAnchor() == Planet;
	}
	if (const AJTSMoonAntActor* const Ant = Cast<AJTSMoonAntActor>(Actor))
	{
		return Ant->GetSurfacePlanet() == Planet;
	}
	if (const AJTSMoonAntNestActor* const Nest = Cast<AJTSMoonAntNestActor>(Actor))
	{
		return Nest->GetSurfacePlanet() == Planet;
	}
	if (const AJTSMoonAntCorpsePickupActor* const CorpsePickup = Cast<AJTSMoonAntCorpsePickupActor>(Actor))
	{
		return CorpsePickup->GetRealSurfacePlanet() == Planet;
	}
	if (const AJTSWorldPickupActor* const Pickup = Cast<AJTSWorldPickupActor>(Actor))
	{
		if (AJTSPlanetAnchor* const SurfacePlanet = Pickup->GetSurfacePlanet(); IsValid(SurfacePlanet))
		{
			return SurfacePlanet == Planet;
		}
	}
	if (const AJTSMoonResourceSpawner* const Spawner = Cast<AJTSMoonResourceSpawner>(Actor))
	{
		if (AJTSPlanetAnchor* const OwningPlanet = Spawner->GetOwningPlanet(); IsValid(OwningPlanet))
		{
			return OwningPlanet == Planet;
		}
	}
	if (const AJTSMoonResourceActor* const Resource = Cast<AJTSMoonResourceActor>(Actor))
	{
		static_cast<void>(Resource);
	}
	else if (const AJTSMoonCorpseActor* const Corpse = Cast<AJTSMoonCorpseActor>(Actor))
	{
		static_cast<void>(Corpse);
	}
	else if (!Actor->IsA<AJTSMoonResourceSpawner>() && !Actor->IsA<AJTSWorldPickupActor>())
	{
		return false;
	}

	if (const AActor* const ActorOwner = Actor->GetOwner(); IsValid(ActorOwner) && ActorOwner != Actor && ActorBelongsToPlanet(ActorOwner, Planet))
	{
		return true;
	}

	// Unbound rocks, spawners, and pickups sit on the authored mesh. Claim only the nearest body
	// inside its gameplay neighbourhood so a neighbour a few kilometres away cannot take them.
	const float AssociationRadius = Planet->GetApproximateRadius()
		+ FMath::Max(Planet->GetGravityInfluenceRange(), Planet->GetSpaceExitRange())
		+ AuthoredScaleAltitudeCentimeters
		+ AuthoredScaleReleaseMarginCentimeters
		+ SurfaceContentHideMarginCentimeters;
	if (FVector::Distance(Actor->GetActorLocation(), Planet->GetPlanetCenter()) > AssociationRadius)
	{
		return false;
	}

	const AJTSPlanetAnchor* NearestPlanet = Planet;
	float NearestDistance = FVector::DistSquared(Actor->GetActorLocation(), Planet->GetPlanetCenter());
	for (const TPair<FName, TWeakObjectPtr<AJTSPlanetAnchor>>& Entry : PlanetRegistry)
	{
		const AJTSPlanetAnchor* const Candidate = Entry.Value.Get();
		if (!IsValid(Candidate) || Candidate == Planet)
		{
			continue;
		}

		const float Distance = FVector::DistSquared(Actor->GetActorLocation(), Candidate->GetPlanetCenter());
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			NearestPlanet = Candidate;
		}
	}
	return NearestPlanet == Planet;
}

void AJTSSpaceWorldManager::CollectSurfaceContentActors(
	const AJTSPlanetAnchor* Planet,
	TArray<AActor*>& OutActors) const
{
	UWorld* const World = GetWorld();
	if (!IsValid(Planet) || World == nullptr)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* const Actor = *It;
		if (ActorBelongsToPlanet(Actor, Planet))
		{
			OutActors.Add(Actor);
		}
	}
}

void AJTSSpaceWorldManager::SetSurfaceContentPresented(AJTSPlanetAnchor* Planet, bool bPresent)
{
	if (!IsValid(Planet))
	{
		return;
	}

	TWeakObjectPtr<AJTSPlanetAnchor> PlanetKey(Planet);
	// Shown planets are stable. Hidden planets keep sweeping so a spawn that happens while the
	// ship is still far away cannot appear at the authored location beside a shrunken mesh.
	if (bPresent && PresentedSurfaceContentPlanets.Contains(PlanetKey))
	{
		return;
	}

	TArray<AActor*> SurfaceActors;
	CollectSurfaceContentActors(Planet, SurfaceActors);
	for (AActor* const Actor : SurfaceActors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		Actor->SetActorHiddenInGame(!bPresent);
		Actor->SetActorEnableCollision(bPresent);
		Actor->SetActorTickEnabled(bPresent);
		if (bPresent)
		{
			if (AJTSPlanetLandingSite* const LandingSite = Cast<AJTSPlanetLandingSite>(Actor))
			{
				LandingSite->RefreshRuntimeLandingMarker();
			}
		}
	}

	if (bPresent)
	{
		PresentedSurfaceContentPlanets.Add(PlanetKey);
	}
	else
	{
		PresentedSurfaceContentPlanets.Remove(PlanetKey);
	}
}

void AJTSSpaceWorldManager::UpdateSurfaceContentPresentation(
	AJTSPlanetAnchor* Planet,
	const AJTSSpacecraftActor* Spacecraft)
{
	if (!IsValid(Planet) || !IsValid(Spacecraft))
	{
		return;
	}

	const float Altitude = ResolvePresentationAltitude(Planet, Spacecraft);
	const float ShowAltitude = FMath::Max(0.0f, AuthoredScaleAltitudeCentimeters);
	const float HideAltitude = ShowAltitude
		+ FMath::Max(0.0f, AuthoredScaleReleaseMarginCentimeters)
		+ FMath::Max(0.0f, SurfaceContentHideMarginCentimeters);
	const bool bWasPresented = PresentedSurfaceContentPlanets.Contains(Planet);
	const bool bAtAuthoredScale = IsPlanetPresentedAtAuthoredScale(Planet);

	bool bPresent = bWasPresented;
	if (!bAtAuthoredScale || Altitude >= HideAltitude)
	{
		bPresent = false;
	}
	else if (Altitude <= ShowAltitude)
	{
		bPresent = true;
	}

	SetSurfaceContentPresented(Planet, bPresent);
}

void AJTSSpaceWorldManager::RestoreDisplacedCruiseSurface()
{
	for (TPair<TWeakObjectPtr<AActor>, FSavedCelestialSurface>& Entry : SavedCelestialSurfaces)
	{
		AActor* const SurfaceActor = Entry.Key.Get();
		if (Entry.Value.bCaptured && IsValid(SurfaceActor))
		{
			SurfaceActor->SetActorScale3D(Entry.Value.Scale);
			SurfaceActor->SetActorHiddenInGame(false);
			SurfaceActor->SetActorEnableCollision(true);
		}
	}

	SavedCelestialSurfaces.Reset();

	TArray<AJTSPlanetAnchor*> Planets;
	GetRegisteredPlanets(Planets);
	for (AJTSPlanetAnchor* const Planet : Planets)
	{
		SetSurfaceContentPresented(Planet, true);
	}
	PresentedSurfaceContentPlanets.Reset();
}

void AJTSSpaceWorldManager::RefreshCelestialVisibility(
	const FVector& ViewLocation,
	float HorizontalFieldOfViewDegrees)
{
	for (const TPair<FName, TWeakObjectPtr<AJTSPlanetAnchor>>& Entry : PlanetRegistry)
	{
		AJTSPlanetAnchor* const Planet = Entry.Value.Get();
		if (!IsValid(Planet))
		{
			continue;
		}

		static_cast<void>(ViewLocation);
		static_cast<void>(HorizontalFieldOfViewDegrees);
		const bool bNoticeable = IsPlanetNoticeableFrom(Planet, nullptr);
		SetCelestialBodyHidden(Planet, !bNoticeable);
	}
}

void AJTSSpaceWorldManager::UpdateCelestialPresentation(const AJTSSpacecraftActor* Spacecraft)
{
	if (!IsValid(Spacecraft))
	{
		RefreshCelestialVisibility(FVector::ZeroVector, 90.0f);
		return;
	}

	const FVector ViewLocation = Spacecraft->GetActorLocation();
	float FieldOfView = 90.0f;
	if (const UCameraComponent* const FlightCamera = Spacecraft->GetFlightCamera(); IsValid(FlightCamera))
	{
		FieldOfView = FlightCamera->FieldOfView;
	}

	for (const TPair<FName, TWeakObjectPtr<AJTSPlanetAnchor>>& Entry : PlanetRegistry)
	{
		AJTSPlanetAnchor* const Planet = Entry.Value.Get();
		if (!IsValid(Planet))
		{
			continue;
		}

		ApplyCelestialScale(Planet, Spacecraft);
		UpdateSurfaceContentPresentation(Planet, Spacecraft);
		static_cast<void>(FieldOfView);
		const bool bNoticeable = IsPlanetNoticeableFrom(Planet, Spacecraft);
		SetCelestialBodyHidden(Planet, !bNoticeable);
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

void AJTSSpaceWorldManager::OnRep_SpaceWorldState()
{
	if (IsValid(CurrentPlanet))
	{
		CurrentPlanet->SetActivePlanet(IsSurfaceState());
	}
	RestoreDisplacedCruiseSurface();
	RefreshCelestialVisibility(FVector::ZeroVector, 90.0f);
}

void AJTSSpaceWorldManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSSpaceWorldManager, CurrentPlanet);
	DOREPLIFETIME(AJTSSpaceWorldManager, CurrentTravelState);
	DOREPLIFETIME(AJTSSpaceWorldManager, bSurfaceGameplayReady);
}
