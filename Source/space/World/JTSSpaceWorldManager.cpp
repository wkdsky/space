// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSSpaceWorldManager.h"

#include "space/Ships/JTSSpacecraftActor.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "space/Core/JTSGameState.h"
#include "space/Components/JTSSpacecraftFlightMovementComponent.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/World/JTSPlanetAnchor.h"

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
	RefreshLocalSky();

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
	RefreshLocalSky();
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

bool AJTSSpaceWorldManager::IsInterplanetaryCruiseActive() const
{
	return IsValid(CruiseOriginPlanet) && IsValid(CruiseDestinationPlanet);
}

AJTSPlanetAnchor* AJTSSpaceWorldManager::GetCruiseOriginPlanet() const
{
	return CruiseOriginPlanet.Get();
}

AJTSPlanetAnchor* AJTSSpaceWorldManager::GetCruiseDestinationPlanet() const
{
	return CruiseDestinationPlanet.Get();
}

float AJTSSpaceWorldManager::GetCruiseRemainingKilometers() const
{
	return CruiseRemainingKilometers;
}

float AJTSSpaceWorldManager::GetCruiseRouteKilometers() const
{
	return ResolveRouteKilometers(CruiseOriginPlanet.Get(), CruiseDestinationPlanet.Get());
}

float AJTSSpaceWorldManager::GetCruiseSpeedKilometersPerSecond() const
{
	return CruiseSpeedKilometersPerSecond;
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

void AJTSSpaceWorldManager::SetCruiseBodyHidden(AJTSPlanetAnchor* Planet, bool bHideBody) const
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
	}
}

void AJTSSpaceWorldManager::RestoreDisplacedCruiseSurface()
{
	AActor* const SurfaceActor = DisplacedCruiseSurfaceActor.Get();
	if (bCruiseSurfaceDisplaced && IsValid(SurfaceActor))
	{
		SurfaceActor->SetActorTransform(SavedCruiseSurfaceTransform);
		SurfaceActor->SetActorHiddenInGame(false);
		SurfaceActor->SetActorEnableCollision(true);
	}

	bCruiseSurfaceDisplaced = false;
	DisplacedCruiseSurfaceActor = nullptr;
}

void AJTSSpaceWorldManager::RebaseSkyWithSpacecraft(const FVector& WorldDelta) const
{
	UWorld* const World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr || WorldDelta.IsNearlyZero())
	{
		return;
	}

	for (AActor* const Actor : World->PersistentLevel->Actors)
	{
		if (!IsValid(Actor) || Actor->IsHidden())
		{
			continue;
		}

		const FVector Scale = Actor->GetActorScale3D();
		if (Scale.GetAbsMax() < 1000.0f)
		{
			continue;
		}

		Actor->SetActorLocation(Actor->GetActorLocation() + WorldDelta);
	}
}

void AJTSSpaceWorldManager::PresentCruiseDestination(const AJTSSpacecraftActor* Spacecraft)
{
	RestoreDisplacedCruiseSurface();
	if (!IsValid(Spacecraft) || !IsValid(CruiseDestinationPlanet))
	{
		return;
	}

	AActor* const SurfaceActor = CruiseDestinationPlanet->GetGameplaySurfaceActor();
	if (!IsValid(SurfaceActor))
	{
		return;
	}

	const float RouteKilometers = FMath::Max(GetCruiseRouteKilometers(), 1.0f);
	const float ArrivalFraction = FMath::Clamp(CruiseArrivalKilometers / RouteKilometers, 0.0001f, 0.25f);
	const float RemainingFraction = FMath::Clamp(CruiseRemainingKilometers / RouteKilometers, 0.0f, 1.0f);
	if (RemainingFraction > 0.35f)
	{
		SurfaceActor->SetActorHiddenInGame(true);
		return;
	}

	const float GrowthAlpha = FMath::Clamp(
		(0.35f - RemainingFraction) / FMath::Max(0.35f - ArrivalFraction, 0.01f),
		0.0f,
		1.0f);
	const FVector ApproachDirection = Spacecraft->GetActorForwardVector().GetSafeNormal();
	const FVector SafeDirection = ApproachDirection.IsNearlyZero() ? FVector::ForwardVector : ApproachDirection;
	const float AuthoredRadius = FMath::Max(1.0f, CruiseDestinationPlanet->GetApproximateRadius());
	const float VisualDistance = FMath::Lerp(AuthoredRadius * 40.0f, AuthoredRadius * 4.0f, GrowthAlpha);
	const FVector AnchorDelta = SurfaceActor->GetActorLocation() - CruiseDestinationPlanet->GetPlanetCenter();

	SavedCruiseSurfaceTransform = SurfaceActor->GetActorTransform();
	DisplacedCruiseSurfaceActor = SurfaceActor;
	bCruiseSurfaceDisplaced = true;
	SurfaceActor->SetActorLocation(Spacecraft->GetActorLocation() + SafeDirection * VisualDistance + AnchorDelta);
	SurfaceActor->SetActorScale3D(SavedCruiseSurfaceTransform.GetScale3D() * FMath::Lerp(0.015f, 1.0f, GrowthAlpha));
	SurfaceActor->SetActorHiddenInGame(false);
	SurfaceActor->SetActorEnableCollision(false);
}

void AJTSSpaceWorldManager::RefreshLocalSky() const
{
	const AJTSPlanetAnchor* const FocusPlanet = CurrentPlanet.Get();
	for (const TPair<FName, TWeakObjectPtr<AJTSPlanetAnchor>>& Entry : PlanetRegistry)
	{
		AJTSPlanetAnchor* const Planet = Entry.Value.Get();
		if (!IsValid(Planet))
		{
			continue;
		}

		// The arriving mesh is parked in front of the ship for the last part of a cruise.
		if (bCruiseSurfaceDisplaced && Planet == CruiseDestinationPlanet.Get())
		{
			continue;
		}

		const bool bVisibleHere = IsValid(FocusPlanet) && IsLocalFamily(FocusPlanet, Planet);
		SetCruiseBodyHidden(Planet, !bVisibleHere);
	}
}

void AJTSSpaceWorldManager::RefreshCruisePresentation()
{
	RestoreDisplacedCruiseSurface();
	RefreshLocalSky();
}

bool AJTSSpaceWorldManager::IsLocalFamily(const AJTSPlanetAnchor* Focus, const AJTSPlanetAnchor* Candidate) const
{
	if (!IsValid(Focus) || !IsValid(Candidate))
	{
		return false;
	}

	if (Focus == Candidate || SharesLocalTransfer(Focus, Candidate))
	{
		return true;
	}

	const FName FocusParent = Focus->GetParentPlanetId();
	return !FocusParent.IsNone() && Candidate->GetParentPlanetId() == FocusParent;
}

AJTSPlanetAnchor* AJTSSpaceWorldManager::ResolveAimedCruisePlanet(const AJTSSpacecraftActor* Spacecraft) const
{
	if (!IsValid(Spacecraft))
	{
		return nullptr;
	}

	const FVector Aim = Spacecraft->GetActorForwardVector().GetSafeNormal();
	if (Aim.IsNearlyZero())
	{
		return nullptr;
	}

	AJTSPlanetAnchor* BestPlanet = nullptr;
	float BestAlignment = 0.82f;
	const FVector SpacecraftLocation = Spacecraft->GetActorLocation();
	for (const TPair<FName, TWeakObjectPtr<AJTSPlanetAnchor>>& Entry : PlanetRegistry)
	{
		AJTSPlanetAnchor* const Planet = Entry.Value.Get();
		if (!IsValid(Planet))
		{
			continue;
		}

		const FVector ToPlanet = (Planet->GetPlanetCenter() - SpacecraftLocation).GetSafeNormal();
		const float Alignment = FVector::DotProduct(Aim, ToPlanet);
		if (Alignment > BestAlignment)
		{
			BestAlignment = Alignment;
			BestPlanet = Planet;
		}
	}

	return BestPlanet;
}

void AJTSSpaceWorldManager::ClearInterplanetaryCruise()
{
	CruiseOriginPlanet = nullptr;
	CruiseDestinationPlanet = nullptr;
	CruiseDistanceFromOriginKilometers = 0.0f;
	CruiseRemainingKilometers = 0.0f;
	CruiseSpeedKilometersPerSecond = 0.0f;
	RefreshCruisePresentation();
}

void AJTSSpaceWorldManager::HandoffCruiseToLocalFlight(AJTSSpacecraftActor* Spacecraft, AJTSPlanetAnchor* ArrivalPlanet)
{
	ClearInterplanetaryCruise();
	if (!IsValid(Spacecraft) || !IsValid(ArrivalPlanet))
	{
		return;
	}

	const FVector ApproachDirection = Spacecraft->GetActorForwardVector().GetSafeNormal();
	const FVector SafeDirection = ApproachDirection.IsNearlyZero() ? FVector::UpVector : ApproachDirection;
	const float ArrivalAltitude = ArrivalPlanet->GetApproximateRadius()
		+ FMath::Max(ArrivalPlanet->GetGravityInfluenceRange() * 0.55f, 1000.0f);
	const FVector ArrivalLocation = ArrivalPlanet->GetPlanetCenter() + SafeDirection * ArrivalAltitude;
	const FVector WorldDelta = ArrivalLocation - Spacecraft->GetActorLocation();
	Spacecraft->SetActorLocation(ArrivalLocation);
	RebaseSkyWithSpacecraft(WorldDelta);
	if (UJTSSpacecraftFlightMovementComponent* const Movement = Spacecraft->GetFlightMovementComponent())
	{
		Movement->StopMovementImmediately();
	}
	SetCurrentPlanet(ArrivalPlanet);
	Spacecraft->SetFlightTargetPlanet(ArrivalPlanet);
	LastDepartedPlanet = nullptr;
	SetTravelState(EJTSSpaceTravelState::SpaceFlight);
}

void AJTSSpaceWorldManager::UpdateInterplanetaryCruise(AJTSSpacecraftActor* Spacecraft, float DeltaTime)
{
	if (!HasAuthority()
		|| !IsValid(Spacecraft)
		|| Spacecraft->IsLanded()
		|| Spacecraft->GetFlightState() == EJTSSpacecraftFlightState::LandingAssist
		|| DeltaTime <= 0.0f)
	{
		return;
	}

	if (CurrentTravelState != EJTSSpaceTravelState::SpaceFlight)
	{
		if (IsInterplanetaryCruiseActive())
		{
			ClearInterplanetaryCruise();
		}
		return;
	}

	const AJTSPlanetAnchor* const LocalPlanet = FindNearestGameplayPlanet(Spacecraft->GetActorLocation(), true);
	if (IsValid(LocalPlanet))
	{
		LastDepartedPlanet = const_cast<AJTSPlanetAnchor*>(LocalPlanet);
		if (IsInterplanetaryCruiseActive())
		{
			ClearInterplanetaryCruise();
		}
		CruiseSpeedKilometersPerSecond = 0.0f;
		return;
	}

	const UJTSSpacecraftFlightMovementComponent* const Movement = Spacecraft->GetFlightMovementComponent();
	const float Throttle = Movement != nullptr ? FMath::Clamp(Movement->GetThrottleNormalized(), -1.0f, 1.0f) : 0.0f;
	AJTSPlanetAnchor* const DeparturePlanet = LastDepartedPlanet.Get();
	if (!IsInterplanetaryCruiseActive())
	{
		CruiseSpeedKilometersPerSecond = 0.0f;
		if (!IsValid(DeparturePlanet) || Throttle <= 0.05f)
		{
			return;
		}

		AJTSPlanetAnchor* const Destination = ResolveAimedCruisePlanet(Spacecraft);
		const float RouteKilometers = ResolveRouteKilometers(DeparturePlanet, Destination);
		if (!IsValid(Destination)
			|| Destination == DeparturePlanet
			|| IsLocalFamily(DeparturePlanet, Destination)
			|| RouteKilometers <= CruiseArrivalKilometers)
		{
			return;
		}

		CruiseOriginPlanet = DeparturePlanet;
		CruiseDestinationPlanet = Destination;
		CruiseDistanceFromOriginKilometers = 0.0f;
		CruiseRemainingKilometers = RouteKilometers;
		RefreshCruisePresentation();
	}

	AJTSPlanetAnchor* const AimedPlanet = ResolveAimedCruisePlanet(Spacecraft);
	if (IsValid(AimedPlanet) && AimedPlanet != CruiseDestinationPlanet.Get() && AimedPlanet != CruiseOriginPlanet.Get())
	{
		const float RetargetKilometers = ResolveRouteKilometers(CruiseOriginPlanet.Get(), AimedPlanet);
		const bool bLocalToCurrentRoute = IsLocalFamily(CruiseOriginPlanet.Get(), AimedPlanet)
			|| IsLocalFamily(CruiseDestinationPlanet.Get(), AimedPlanet);
		if (RetargetKilometers > CruiseArrivalKilometers && !bLocalToCurrentRoute)
		{
			// Keep the kilometres already flown. A much shorter divert stops just outside
			// arrival instead of inheriting a progress fraction that would finish it immediately.
			const float RetargetArrivalKilometers = FMath::Min(CruiseArrivalKilometers, RetargetKilometers * 0.02f);
			const float DistanceBeforeArrival = FMath::Max(0.0f, RetargetKilometers - RetargetArrivalKilometers - 1.0f);
			CruiseDestinationPlanet = AimedPlanet;
			CruiseDistanceFromOriginKilometers = FMath::Min(CruiseDistanceFromOriginKilometers, DistanceBeforeArrival);
			RefreshCruisePresentation();
		}
	}

	const float RouteKilometers = GetCruiseRouteKilometers();
	if (RouteKilometers <= CruiseArrivalKilometers || !IsValid(CruiseDestinationPlanet))
	{
		ClearInterplanetaryCruise();
		return;
	}

	const float ReferenceDuration = FMath::Max(1.0f, CruiseReferenceDurationSeconds);
	CruiseSpeedKilometersPerSecond = Throttle * (RouteKilometers / ReferenceDuration);
	CruiseDistanceFromOriginKilometers = FMath::Clamp(
		CruiseDistanceFromOriginKilometers + CruiseSpeedKilometersPerSecond * DeltaTime,
		0.0f,
		RouteKilometers);
	CruiseRemainingKilometers = RouteKilometers - CruiseDistanceFromOriginKilometers;
	PresentCruiseDestination(Spacecraft);

	const float ArrivalKilometers = FMath::Min(CruiseArrivalKilometers, RouteKilometers * 0.02f);
	if (CruiseDistanceFromOriginKilometers <= ArrivalKilometers && Throttle < 0.05f)
	{
		AJTSPlanetAnchor* const ReturnPlanet = CruiseOriginPlanet.Get();
		HandoffCruiseToLocalFlight(Spacecraft, ReturnPlanet);
		return;
	}

	if (CruiseRemainingKilometers <= ArrivalKilometers)
	{
		AJTSPlanetAnchor* const ArrivalPlanet = CruiseDestinationPlanet.Get();
		HandoffCruiseToLocalFlight(Spacecraft, ArrivalPlanet);
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
	if (!IsInterplanetaryCruiseActive())
	{
		RestoreDisplacedCruiseSurface();
	}
	RefreshLocalSky();
}

void AJTSSpaceWorldManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSSpaceWorldManager, CurrentPlanet);
	DOREPLIFETIME(AJTSSpaceWorldManager, CurrentTravelState);
	DOREPLIFETIME(AJTSSpaceWorldManager, bSurfaceGameplayReady);
	DOREPLIFETIME(AJTSSpaceWorldManager, CruiseOriginPlanet);
	DOREPLIFETIME(AJTSSpaceWorldManager, CruiseDestinationPlanet);
	DOREPLIFETIME(AJTSSpaceWorldManager, CruiseDistanceFromOriginKilometers);
	DOREPLIFETIME(AJTSSpaceWorldManager, CruiseRemainingKilometers);
	DOREPLIFETIME(AJTSSpaceWorldManager, CruiseSpeedKilometersPerSecond);
}
