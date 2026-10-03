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
#include "space/Interaction/IInteractable.h"
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
#include "space/World/JTSPlanetEnemySettlement.h"
#include "space/World/JTSPlanetSettlementEnemy.h"
#include "space/World/JTSPlanetSurfaceAnchor.h"


namespace
{
	/** The manager is looked up frequently by character components, so cache the persistent actor after one lookup. */
	TMap<const UWorld*, TWeakObjectPtr<AJTSSpaceWorldManager>> GSpaceWorldManagers;

	// The near-surface dial deliberately expands the compact level's metres into displayed
	// kilometres. The two handoffs are specified in those displayed units, not map kilometres.
	constexpr float SurfaceNavigationScaleLengthCentimeters = 8000.0f;
	constexpr float SurfaceNavigationLogGainCentimeters = 1200000.0f;
	constexpr float SurfaceNavigationLowOrbitCentimeters = 12000000.0f;

	float NavigationRangeFromAltitude(float AltitudeCentimeters)
	{
		return FMath::Min(SurfaceNavigationLowOrbitCentimeters,
			SurfaceNavigationLogGainCentimeters * FMath::Loge(
				1.0f + FMath::Max(0.0f, AltitudeCentimeters) / SurfaceNavigationScaleLengthCentimeters));
	}

	float AltitudeFromNavigationKilometers(float NavigationKilometers)
	{
		return SurfaceNavigationScaleLengthCentimeters
			* (FMath::Exp(FMath::Max(0.0f, NavigationKilometers) * 100000.0f
				/ SurfaceNavigationLogGainCentimeters) - 1.0f);
	}

	float CruiseTransitionAltitude()
	{
		return AltitudeFromNavigationKilometers(AJTSSpaceWorldManager::CruiseTransitionKilometers);
	}

	float SurfaceContentTransitionAltitude()
	{
		return AltitudeFromNavigationKilometers(AJTSSpaceWorldManager::SurfaceContentTransitionKilometers);
	}

	// A small linear onset makes the antigravity drive measurable immediately, while the
	// eased portion keeps the first kilometres of the compact map well below 0.01 AU.
	constexpr float CruiseLinearOnsetShare = 0.002f;

	float ResolveCruiseRouteShare(float Progress, float Exponent)
	{
		const float ClampedProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
		return CruiseLinearOnsetShare * ClampedProgress
			+ (1.0f - CruiseLinearOnsetShare) * FMath::InterpEaseInOut(
				0.0f, 1.0f, ClampedProgress, Exponent);
	}

	float ResolveCruiseRouteSlope(float Progress, float Exponent)
	{
		if (Progress < 0.0f || Progress >= 1.0f)
		{
			return 0.0f;
		}
		const float EaseSlope = Progress > 0.0f
			? Exponent * FMath::Pow(2.0f * FMath::Min(Progress, 1.0f - Progress), Exponent - 1.0f)
			: 0.0f;
		return CruiseLinearOnsetShare + (1.0f - CruiseLinearOnsetShare) * EaseSlope;
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

const AJTSPlanetAnchor* AJTSSpaceWorldManager::GetNavigationReferencePlanet(
	const AJTSSpacecraftActor* Spacecraft) const
{
	return ResolveReferencePlanet(Spacecraft);
}

EJTSSpaceTravelState AJTSSpaceWorldManager::GetCurrentTravelState() const
{
	return CurrentTravelState;
}

bool AJTSSpaceWorldManager::IsCruisePresentationActive(const AJTSSpacecraftActor* Spacecraft) const
{
	if (!IsValid(Spacecraft) || Spacecraft->IsLanded())
	{
		return false;
	}

	const AJTSPlanetAnchor* const Reference = ResolveReferencePlanet(Spacecraft);
	if (!IsValid(Reference))
	{
		return true;
	}

	// The field and the dial use the same height as the mesh. Replication of the travel
	// state can arrive a frame later on clients, so presentation follows position directly.
	return ResolvePresentationAltitude(Reference, Spacecraft) >= CruiseTransitionAltitude();
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
	if (IsSurfaceState())
	{
		RefreshCelestialVisibility(FVector::ZeroVector, 90.0f);
	}
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
		&& SurfaceAltitude >= CruiseTransitionAltitude())
	{
		SetTravelState(EJTSSpaceTravelState::SpaceFlight);
		return;
	}

	if (CurrentTravelState == EJTSSpaceTravelState::SpaceFlight
		&& SurfaceAltitude < CruiseTransitionAltitude())
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
		// Keep the departure planet as the low-flight reference until the shared 18 km
		// navigation handoff. Above it, the ship has no planet flight target.
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

		HandleFlightAltitude(ResolvePresentationAltitude(DeparturePlanet, Spacecraft));
		if (CurrentTravelState == EJTSSpaceTravelState::SpaceFlight)
		{
			LastDepartedPlanet = DeparturePlanet;
			Spacecraft->SetFlightTargetPlanet(nullptr);
			SetCurrentPlanet(nullptr);
		}
		return;
	}

	if (CurrentTravelState == EJTSSpaceTravelState::SpaceFlight)
	{
		// The near-flight shell is shared by all planets and is independent of the
		// craft's forward vector or radial velocity. Gravity may have faded already.
		AJTSPlanetAnchor* const NearbyPlanet = FindNearestGameplayPlanet(SpacecraftLocation, false);
		const bool bInsideNearFlightShell = IsValid(NearbyPlanet)
			&& ResolvePresentationAltitude(NearbyPlanet, Spacecraft) < CruiseTransitionAltitude();
		if (!bInsideNearFlightShell)
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

		if (NearbyPlanet != CurrentPlanetAnchor)
		{
			SetCurrentPlanet(NearbyPlanet);
		}
		if (Spacecraft->GetFlightPlanet() != NearbyPlanet)
		{
			Spacecraft->SetFlightTargetPlanet(NearbyPlanet);
		}
		HandleFlightAltitude(ResolvePresentationAltitude(NearbyPlanet, Spacecraft));
		return;
	}

	if (CurrentTravelState == EJTSSpaceTravelState::Approach)
	{
		if (!IsValid(CurrentPlanetAnchor))
		{
			SetTravelState(EJTSSpaceTravelState::SpaceFlight);
			return;
		}

		const float Altitude = ResolvePresentationAltitude(CurrentPlanetAnchor, Spacecraft);
		if (Altitude >= CruiseTransitionAltitude())
		{
			SetTravelState(EJTSSpaceTravelState::SpaceFlight);
			LastDepartedPlanet = CurrentPlanetAnchor;
			Spacecraft->SetFlightTargetPlanet(nullptr);
			SetCurrentPlanet(nullptr);
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

	const float LocalAltitudeCentimeters = FMath::Max(0.0f,
		FVector::Distance(Spacecraft->GetActorLocation(), Planet->GetPlanetCenter())
		- ResolveAuthoredSurfaceRadius(Planet));
	// A moon uses its configured parent as the route origin while retaining its own
	// physical range nearby. The route pair never depends on the ship's active planet.
	const AJTSPlanetAnchor* RouteOrigin = Planet;
	if (const TWeakObjectPtr<AJTSPlanetAnchor>* const Parent = PlanetRegistry.Find(Planet->GetParentPlanetId());
		Parent != nullptr && IsValid(Parent->Get()))
	{
		RouteOrigin = Parent->Get();
	}
	const AJTSPlanetAnchor* const Other = FindNearestForeignPlanet(RouteOrigin);
	if (!IsValid(Other))
	{
		return LocalAltitudeCentimeters;
	}

	const float RouteKilometers = ResolveRouteKilometers(RouteOrigin, Other);
	const float CorridorLength = FVector::Distance(RouteOrigin->GetPlanetCenter(), Other->GetPlanetCenter());
	if (RouteKilometers <= KINDA_SMALL_NUMBER || CorridorLength <= KINDA_SMALL_NUMBER)
	{
		return LocalAltitudeCentimeters;
	}

	// The nearby physical distance uses the contact's own center; the AU contribution
	// uses its stable route origin. Climbing in any direction accumulates cruise range.
	const float ReleaseAltitude = CruiseTransitionAltitude();
	const float LocalShell = ResolveAuthoredSurfaceRadius(RouteOrigin) + ReleaseAltitude;
	const float OtherShell = ResolveAuthoredSurfaceRadius(Other) + ReleaseAltitude;
	const float CruiseLength = FMath::Max(CorridorLength - LocalShell - OtherShell, 1.0f);
	const float Distance = FVector::Distance(Spacecraft->GetActorLocation(), RouteOrigin->GetPlanetCenter());
	const float Progress = (Distance - LocalShell) / CruiseLength;
	const float RouteShare = ResolveCruiseRouteShare(Progress,
		FMath::Max(1.0f, CruiseDistanceEaseExponent));
	return static_cast<float>(FMath::Max(0.0, static_cast<double>(LocalAltitudeCentimeters)
		+ static_cast<double>(RouteShare) * static_cast<double>(RouteKilometers) * 100000.0));
}

float AJTSSpaceWorldManager::GetNavigationSpeedCentimetersPerSecond(const AJTSSpacecraftActor* Spacecraft) const
{
	if (!IsValid(Spacecraft)) return 0.0f;
	const FVector Velocity = Spacecraft->GetFlightVelocity();
	if (Velocity.IsNearlyZero()) return 0.0f;
	const AJTSPlanetAnchor* const Reference = ResolveReferencePlanet(Spacecraft);
	if (!IsValid(Reference)) return Velocity.Size();

	const FVector ShipLocation = Spacecraft->GetActorLocation();
	if (!IsCruisePresentationActive(Spacecraft))
	{
		// Only radial climb uses the surface dial's logarithmic scale. Sideways flight
		// keeps its measured speed instead of falsely reading zero on a constant-altitude pass.
		const float Altitude = FMath::Max(0.0f, ResolvePresentationAltitude(Reference, Spacecraft));
		const float SurfaceDialRange = NavigationRangeFromAltitude(Altitude);
		const float RadialScale = SurfaceDialRange
			>= SurfaceNavigationLowOrbitCentimeters ? 0.0f
			: SurfaceNavigationLogGainCentimeters / (SurfaceNavigationScaleLengthCentimeters + Altitude);
		const FVector RadialUp = Reference->GetRadialUpVector(ShipLocation).GetSafeNormal();
		const float RadialVelocity = FVector::DotProduct(Velocity, RadialUp);
		return FMath::Sqrt((Velocity - RadialUp * RadialVelocity).SizeSquared()
			+ FMath::Square(RadialVelocity * RadialScale));
	}

	const AJTSPlanetAnchor* const Foreign = FindNearestForeignPlanet(Reference);
	if (!IsValid(Foreign)) return Velocity.Size();
	const float RouteKilometers = ResolveRouteKilometers(Reference, Foreign);
	const float CorridorLength = FVector::Distance(Foreign->GetPlanetCenter(), Reference->GetPlanetCenter());
	if (RouteKilometers <= KINDA_SMALL_NUMBER || CorridorLength <= KINDA_SMALL_NUMBER)
	{
		return Velocity.Size();
	}

	const float ReleaseAltitude = CruiseTransitionAltitude();
	const float DepartureShell = ResolveAuthoredSurfaceRadius(Reference) + ReleaseAltitude;
	const float ArrivalShell = ResolveAuthoredSurfaceRadius(Foreign) + ReleaseAltitude;
	const float CruiseLength = FMath::Max(CorridorLength - DepartureShell - ArrivalShell, 1.0f);
	const float Distance = FVector::Distance(ShipLocation, Reference->GetPlanetCenter());
	const float Progress = (Distance - DepartureShell) / CruiseLength;
	const float Exponent = FMath::Max(1.0f, CruiseDistanceEaseExponent);
	const float RouteScale = 1.0f + RouteKilometers * 100000.0f
		* ResolveCruiseRouteSlope(Progress, Exponent) / CruiseLength;
	// The speed is distance travelled through cruise space, so turning across the route
	// does not make an active antigravity drive appear to slow to ordinary m/s.
	return Velocity.Size() * RouteScale;
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

	// Preserve the surface dial's authored logarithmic climb, capped at its 120 km orbit mark.
	return NavigationRangeFromAltitude(LevelAltitude);
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
	// A client can reach a destination before the replicated flight target arrives. The
	// closest body inside the shared shell must win over the retained departure body.
	AJTSPlanetAnchor* NearbyPlanet = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (const TPair<FName, TWeakObjectPtr<AJTSPlanetAnchor>>& Entry : PlanetRegistry)
	{
		AJTSPlanetAnchor* const Candidate = Entry.Value.Get();
		if (!IsValid(Candidate) || !Candidate->IsGravityEnabled())
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(
			Spacecraft->GetActorLocation(), Candidate->GetPlanetCenter());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearbyPlanet = Candidate;
		}
	}
	if (IsValid(NearbyPlanet)
		&& ResolvePresentationAltitude(NearbyPlanet, Spacecraft) < CruiseTransitionAltitude())
	{
		return NearbyPlanet;
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

	const float Altitude = ResolvePresentationAltitude(Planet, Spacecraft);
	const float ReleaseAltitude = CruiseTransitionAltitude();
	if (Altitude <= ReleaseAltitude)
	{
		return 1.0f;
	}

	// Apparent range is astronomical and jumps by orders of magnitude inside this compact map.
	// Scale follows the metres actually flown past the release shell so the disk shrinks every frame.
	const float BeyondShell = Altitude - ReleaseAltitude;
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

	const float ReleaseAltitude = CruiseTransitionAltitude();
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
	// Retain real mesh height near gameplay. Higher up, smoothly converge on the
	// authored-radius shell before scaling begins; a scaled mesh cannot answer a
	// full-size surface trace, so this prevents an altitude jump at 18 km.
	if (!IsPlanetPresentedAtAuthoredScale(Planet))
	{
		return AuthoredAltitude;
	}

	float SurfaceAltitude = 0.0f;
	if (Planet->GetAltitudeAboveSurface(Spacecraft->GetActorLocation(), SurfaceAltitude))
	{
		const float BlendStart = AltitudeFromNavigationKilometers(6.0f);
		const float BlendEnd = AltitudeFromNavigationKilometers(14.0f);
		const float Blend = FMath::SmoothStep(BlendStart, BlendEnd, AuthoredAltitude);
		return FMath::Lerp(FMath::Max(0.0f, SurfaceAltitude), AuthoredAltitude, Blend);
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
	else if (!Actor->IsA<AJTSMoonResourceSpawner>()
		&& !Actor->IsA<AJTSWorldPickupActor>()
		&& !Actor->IsA<AJTSPlanetEnemySettlement>()
		&& !Actor->GetClass()->ImplementsInterface(UJTSPlanetSettlementEnemy::StaticClass())
		&& !Actor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
	{
		return false;
	}

	if (const AActor* const ActorOwner = Actor->GetOwner(); IsValid(ActorOwner) && ActorOwner != Actor && ActorBelongsToPlanet(ActorOwner, Planet))
	{
		return true;
	}

	// Unbound rocks, spawners, and pickups sit on the authored mesh. Claim only the nearest body
	// inside its gameplay neighbourhood so a neighbour a few kilometres away cannot take them.
	const float AssociationRadius = ResolveAuthoredSurfaceRadius(Planet)
		+ FMath::Max(FMath::Max(Planet->GetGravityInfluenceRange(), Planet->GetSpaceExitRange()),
			CruiseTransitionAltitude());
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
	SetSurfaceContentPresented(Planet, Altitude < SurfaceContentTransitionAltitude());
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
	if (IsSurfaceState())
	{
		RestoreDisplacedCruiseSurface();
		RefreshCelestialVisibility(FVector::ZeroVector, 90.0f);
	}
}

void AJTSSpaceWorldManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSSpaceWorldManager, CurrentPlanet);
	DOREPLIFETIME(AJTSSpaceWorldManager, CurrentTravelState);
	DOREPLIFETIME(AJTSSpaceWorldManager, bSurfaceGameplayReady);
}
