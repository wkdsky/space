// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

#include "JTSSpaceWorldManager.generated.h"

class AActor;
class AJTSSpacecraftActor;
class AJTSPlanetAnchor;
class ULevel;
class ULevelStreamingDynamic;
class UObject;
class USceneComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnJTSSpaceWorldLandingRequested, AJTSPlanetAnchor*);

/** High-level state shared by the persistent SpaceWorld, the character, and future flight code. */
UENUM(BlueprintType)
enum class EJTSSpaceTravelState : uint8
{
	Surface UMETA(DisplayName = "Surface"),
	Takeoff UMETA(DisplayName = "Takeoff"),
	SpaceFlight UMETA(DisplayName = "Space Flight"),
	Approach UMETA(DisplayName = "Approach"),
	Landing UMETA(DisplayName = "Landing")
};

/**
 * Persistent-world coordinator for real gameplay planets, current travel state, and optional
 * planet content streaming. It deliberately does not know Moon-only gameplay, surface presentation,
 * or AJTSMoonSurfaceController.
 */
UCLASS(BlueprintType)
class SPACE_API AJTSSpaceWorldManager : public AActor
{
	GENERATED_BODY()

public:
	AJTSSpaceWorldManager();

	/** Finds the single persistent-world manager using a small cache after its first lookup. */
	static AJTSSpaceWorldManager* FindSpaceWorldManager(const UObject* WorldContextObject);

	/** Registers a persistent or activated gameplay planet. Duplicate non-empty PlanetId values are rejected. */
	bool RegisterPlanet(AJTSPlanetAnchor* Planet);
	void UnregisterPlanet(AJTSPlanetAnchor* Planet);

	/** Resolves CurrentPlanet from InitialPlanet or an exact InitialPlanetId match in the registered planet set. */
	void InitializeCurrentPlanet();

	UFUNCTION(BlueprintPure, Category = "Space World|Planets")
	AJTSPlanetAnchor* FindPlanetById(FName PlanetId);

	/** Finds the nearest registered real gameplay planet. Never relies on world actor iteration order. */
	UFUNCTION(BlueprintPure, Category = "Space World|Planets")
	AJTSPlanetAnchor* FindNearestGameplayPlanet(const FVector& WorldPosition, bool bRequireGravityInfluence = true);

	/** Resolves a surface actor/attachment first, then falls back to the nearest active gameplay planet. */
	UFUNCTION(BlueprintPure, Category = "Space World|Planets")
	AJTSPlanetAnchor* FindPlanetOwningActor(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Space World|State")
	AJTSPlanetAnchor* GetCurrentPlanet() const;

	UFUNCTION(BlueprintPure, Category = "Space World|State")
	EJTSSpaceTravelState GetCurrentTravelState() const;

	UFUNCTION(BlueprintPure, Category = "Space World|State")
	bool IsSurfaceState() const;

	/**
	 * True while this exact CurrentPlanet is in Surface state. Gravity intentionally does not depend
	 * on mesh trace/snap readiness: a bound Surface character must retain radial gravity even when a
	 * surface trace fails.
	 */
	UFUNCTION(BlueprintPure, Category = "Space World|State")
	bool IsPlanetGameplayActive(const AJTSPlanetAnchor* Planet) const;

	UFUNCTION(BlueprintCallable, Category = "Space World|State")
	void SetCurrentPlanet(AJTSPlanetAnchor* NewCurrentPlanet);

	UFUNCTION(BlueprintCallable, Category = "Space World|State")
	void SetTravelState(EJTSSpaceTravelState NewTravelState);

	/** Set by the landing flow once initial gameplay actors exist; this gates startup/input readiness, never gravity. */
	void SetSurfaceGameplayReady(bool bReady);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Space World|State")
	bool IsSurfaceGameplayReady() const;

	/** Optional future content streaming for a planet. It is not a flat surface-level dependency. */
	UFUNCTION(BlueprintCallable, Category = "Space World|Content")
	bool RequestPlanetContentLoad(AJTSPlanetAnchor* Planet, bool bMakeVisible = true);

	UFUNCTION(BlueprintCallable, Category = "Space World|Content")
	bool RequestPlanetContentUnload(AJTSPlanetAnchor* Planet);

	UFUNCTION(BlueprintPure, Category = "Space World|Content")
	bool IsPlanetContentLoaded(const AJTSPlanetAnchor* Planet) const;

	UFUNCTION(BlueprintPure, Category = "Space World|Content")
	bool IsPlanetContentVisible(const AJTSPlanetAnchor* Planet) const;

	ULevel* GetLoadedPlanetContentLevel(const AJTSPlanetAnchor* Planet) const;

	/** Fired once when flight reaches an eligible real planet landing range. */
	FOnJTSSpaceWorldLandingRequested& OnLandingRequested();

	/** Called by authoritative flight code after movement; the value is resolved from the real surface when loaded. */
	void HandleFlightAltitude(float SurfaceAltitude);

	/**
	 * Server-authoritative flight handoff for the shared spacecraft. It releases the departure
	 * planet at the space-flight threshold, leaves deep space unbound, and acquires a new planet
	 * only after the craft enters that planet's non-overlapping influence range.
	 */
	void UpdateSpacecraftFlightState(AJTSSpacecraftActor* Spacecraft);

	/**
	 * Integrates the shared expedition's felt interplanetary distance. The spacecraft keeps its
	 * existing local flight; this scalar is what makes Moon↔Mars take minutes and stay reversible.
	 */
	void UpdateInterplanetaryCruise(AJTSSpacecraftActor* Spacecraft, float DeltaTime);

	UFUNCTION(BlueprintPure, Category = "Space World|Cruise")
	bool IsInterplanetaryCruiseActive() const;

	UFUNCTION(BlueprintPure, Category = "Space World|Cruise")
	AJTSPlanetAnchor* GetCruiseOriginPlanet() const;

	UFUNCTION(BlueprintPure, Category = "Space World|Cruise")
	AJTSPlanetAnchor* GetCruiseDestinationPlanet() const;

	/** Kilometres still to cover toward the current destination. Zero while cruise is inactive. */
	UFUNCTION(BlueprintPure, Category = "Space World|Cruise")
	float GetCruiseRemainingKilometers() const;

	UFUNCTION(BlueprintPure, Category = "Space World|Cruise")
	float GetCruiseRouteKilometers() const;

	/** Signed felt speed in kilometres per second. Positive closes on the destination. */
	UFUNCTION(BlueprintPure, Category = "Space World|Cruise")
	float GetCruiseSpeedKilometersPerSecond() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RegisterPersistentPlanetAnchors();
	FName GetPlanetKey(const AJTSPlanetAnchor* Planet) const;
	ULevelStreamingDynamic* FindPlanetContentStreamingLevel(const AJTSPlanetAnchor* Planet) const;
	void LogDebugState() const;
	void RefreshCruisePresentation();
	void RefreshLocalSky() const;
	void SetCruiseBodyHidden(AJTSPlanetAnchor* Planet, bool bHideBody) const;
	void RestoreDisplacedCruiseSurface();
	void PresentCruiseDestination(const AJTSSpacecraftActor* Spacecraft);
	void RebaseSkyWithSpacecraft(const FVector& WorldDelta) const;
	float ResolveRouteKilometers(const AJTSPlanetAnchor* Origin, const AJTSPlanetAnchor* Destination) const;
	bool SharesLocalTransfer(const AJTSPlanetAnchor* First, const AJTSPlanetAnchor* Second) const;
	bool IsLocalFamily(const AJTSPlanetAnchor* Focus, const AJTSPlanetAnchor* Candidate) const;
	AJTSPlanetAnchor* ResolveAimedCruisePlanet(const AJTSSpacecraftActor* Spacecraft) const;
	void HandoffCruiseToLocalFlight(AJTSSpacecraftActor* Spacecraft, AJTSPlanetAnchor* ArrivalPlanet);
	void ClearInterplanetaryCruise();

	UFUNCTION()
	void OnRep_SpaceWorldState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Space World", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Optional explicit initial planet. If unset, InitialPlanetId must match exactly one registered anchor. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Space World|State", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetAnchor> InitialPlanet;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Space World|State", meta = (AllowPrivateAccess = "true"))
	FName InitialPlanetId = TEXT("Moon");

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Space World|State", meta = (AllowPrivateAccess = "true"))
	EJTSSpaceTravelState InitialTravelState = EJTSSpaceTravelState::Surface;

	UPROPERTY(ReplicatedUsing = OnRep_SpaceWorldState, VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Space World|State", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetAnchor> CurrentPlanet;

	UPROPERTY(ReplicatedUsing = OnRep_SpaceWorldState, VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Space World|State", meta = (AllowPrivateAccess = "true"))
	EJTSSpaceTravelState CurrentTravelState = EJTSSpaceTravelState::Surface;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Content", meta = (AllowPrivateAccess = "true"))
	bool bEnablePlanetContentUnload = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDebugSpaceTravel = false;

	/** Strong references keep optional streaming requests alive; actors themselves are held by their levels. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<ULevelStreamingDynamic>> PlanetContentStreamingLevels;

	/** Small cached registry. It is populated by persistent-level discovery once and activation registration thereafter. */
	TMap<FName, TWeakObjectPtr<AJTSPlanetAnchor>> PlanetRegistry;

	FTimerHandle DebugTimerHandle;
	FOnJTSSpaceWorldLandingRequested LandingRequestedDelegate;
	bool bPlanetRegistryInitialized = false;
	UPROPERTY(ReplicatedUsing = OnRep_SpaceWorldState)
	bool bSurfaceGameplayReady = false;
	bool bLandingEligibilityAnnounced = false;

	/** Body the ship last released. Cruise can start from it after the influence bubble is left behind. */
	TWeakObjectPtr<AJTSPlanetAnchor> LastDepartedPlanet;

	FTransform SavedCruiseSurfaceTransform = FTransform::Identity;
	TWeakObjectPtr<AActor> DisplacedCruiseSurfaceActor;
	bool bCruiseSurfaceDisplaced = false;

	/** Reference body the current cruise leg left. Stays set while the leg can still be reversed. */
	UPROPERTY(ReplicatedUsing = OnRep_SpaceWorldState, VisibleInstanceOnly, Transient, Category = "Space World|Cruise", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetAnchor> CruiseOriginPlanet;

	UPROPERTY(ReplicatedUsing = OnRep_SpaceWorldState, VisibleInstanceOnly, Transient, Category = "Space World|Cruise", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetAnchor> CruiseDestinationPlanet;

	/** Kilometres travelled away from CruiseOriginPlanet along the current route. */
	UPROPERTY(ReplicatedUsing = OnRep_SpaceWorldState, VisibleInstanceOnly, Transient, Category = "Space World|Cruise", meta = (AllowPrivateAccess = "true"))
	float CruiseDistanceFromOriginKilometers = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_SpaceWorldState, VisibleInstanceOnly, Transient, Category = "Space World|Cruise", meta = (AllowPrivateAccess = "true"))
	float CruiseRemainingKilometers = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_SpaceWorldState, VisibleInstanceOnly, Transient, Category = "Space World|Cruise", meta = (AllowPrivateAccess = "true"))
	float CruiseSpeedKilometersPerSecond = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Cruise", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float CruiseReferenceDurationSeconds = 150.0f;

	/** Below this remaining distance the destination is already a local body and cruise ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Cruise", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float CruiseArrivalKilometers = 500.0f;
};
