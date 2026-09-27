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

	/** True from the moment Space is pressed to leave the surface until the ship is parked again. */
	UFUNCTION(BlueprintPure, Category = "Space World|State")
	bool IsAirborneTravel() const;

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
	 * Keeps authored positions. Past the near-surface band each body scales down continuously
	 * across the open gap to the next body. A body is hidden only once its scaled disk covers
	 * less than MinimumNoticeableAngularDiameterDegrees on screen.
	 */
	void UpdateCelestialPresentation(const AJTSSpacecraftActor* Spacecraft);

	/** Kilometres of real separation between two bodies. Zero when either body is unconfigured. */
	UFUNCTION(BlueprintPure, Category = "Space World|Cruise")
	float GetRouteKilometers(const AJTSPlanetAnchor* Origin, const AJTSPlanetAnchor* Destination) const;

	/** True while a body's disk would cover at least MinimumNoticeableAngularDiameterDegrees. */
	bool IsPlanetNoticeableFrom(
		const AJTSPlanetAnchor* Planet,
		const AJTSSpacecraftActor* Viewer) const;

	/** Distance the navigation and sky should treat as separating the ship from this body, in centimetres. */
	float GetApparentRangeCentimeters(const AJTSSpacecraftActor* Spacecraft, const AJTSPlanetAnchor* Planet) const;

	/** Heliocentric cruise distance for the navigation dial, in centimetres. Grows and shrinks with travel progress along the route. */
	float GetCruiseRangeCentimeters(const AJTSSpacecraftActor* Spacecraft, const AJTSPlanetAnchor* Planet) const;

	/**
	 * Navigation-only height above a body's ground, in centimetres. Touched down reads zero.
	 * A short level climb already reads several times farther, and the top of the staff is a
	 * 120 km low orbit. Separate from both the level distance and the astronomical cruise range.
	 */
	float GetNavigationSurfaceRangeCentimeters(const AJTSSpacecraftActor* Spacecraft, const AJTSPlanetAnchor* Planet) const;

	/**
	 * True while this body is being drawn at its authored size for the ship. Surface interactables
	 * may exist only inside this band; outside it the mesh is already an impostor.
	 */
	bool IsPlanetPresentedAtAuthoredScale(const AJTSPlanetAnchor* Planet) const;

	void GetRegisteredPlanets(TArray<AJTSPlanetAnchor*>& OutPlanets) const;

	bool SharesLocalSky(const AJTSPlanetAnchor* First, const AJTSPlanetAnchor* Second) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RegisterPersistentPlanetAnchors();
	FName GetPlanetKey(const AJTSPlanetAnchor* Planet) const;
	ULevelStreamingDynamic* FindPlanetContentStreamingLevel(const AJTSPlanetAnchor* Planet) const;
	void LogDebugState() const;
	void RefreshCelestialVisibility(const FVector& ViewLocation, float HorizontalFieldOfViewDegrees);
	void SetCelestialBodyHidden(AJTSPlanetAnchor* Planet, bool bHideBody) const;
	void RestoreDisplacedCruiseSurface();
	float ResolveRouteKilometers(const AJTSPlanetAnchor* Origin, const AJTSPlanetAnchor* Destination) const;
	bool SharesLocalTransfer(const AJTSPlanetAnchor* First, const AJTSPlanetAnchor* Second) const;
	float ResolveLocalSeparationCentimeters(const AJTSPlanetAnchor* First, const AJTSPlanetAnchor* Second) const;
	const AJTSPlanetAnchor* FindNearestForeignPlanet(const AJTSPlanetAnchor* Reference) const;
	float ResolveTravelProgress(const AJTSSpacecraftActor* Spacecraft, const AJTSPlanetAnchor* Reference, const AJTSPlanetAnchor* Foreign) const;
	AJTSPlanetAnchor* ResolveReferencePlanet(const AJTSSpacecraftActor* Spacecraft) const;
	/** Nearest registered body to the ship that still has a heliocentric route. Used once deep space has cleared the current planet. */
	AJTSPlanetAnchor* FindNearestRoutedPlanet(const AJTSSpacecraftActor* Spacecraft) const;
	float ResolveApparentRangeCentimeters(const AJTSSpacecraftActor* Spacecraft, const AJTSPlanetAnchor* Planet) const;
	float ResolveCelestialScaleRatio(const AJTSPlanetAnchor* Planet, const AJTSSpacecraftActor* Spacecraft) const;
	float ResolveCelestialShrinkSpanCentimeters(const AJTSPlanetAnchor* Planet) const;
	float ResolvePresentationAltitude(const AJTSPlanetAnchor* Planet, const AJTSSpacecraftActor* Spacecraft) const;
	float ResolveAuthoredSurfaceRadius(const AJTSPlanetAnchor* Planet) const;
	void ApplyCelestialScale(AJTSPlanetAnchor* Planet, const AJTSSpacecraftActor* Spacecraft);
	void UpdateSurfaceContentPresentation(AJTSPlanetAnchor* Planet, const AJTSSpacecraftActor* Spacecraft);
	void SetSurfaceContentPresented(AJTSPlanetAnchor* Planet, bool bPresent);
	void CollectSurfaceContentActors(const AJTSPlanetAnchor* Planet, TArray<AActor*>& OutActors) const;
	static bool IsPersistentOccupant(const AActor* Actor);
	bool ActorBelongsToPlanet(const AActor* Actor, const AJTSPlanetAnchor* Planet) const;

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

	/** Body the ship last released. Retained so a return can still name the departure body. */
	TWeakObjectPtr<AJTSPlanetAnchor> LastDepartedPlanet;

	struct FSavedCelestialSurface
	{
		FVector Scale = FVector::OneVector;
		bool bCaptured = false;
	};

	TMap<TWeakObjectPtr<AActor>, FSavedCelestialSurface> SavedCelestialSurfaces;

	/** Planets whose surface actors are currently shown at their authored locations. */
	TSet<TWeakObjectPtr<AJTSPlanetAnchor>> PresentedSurfaceContentPlanets;

	/** A disk smaller than this is treated as invisible. About two vertical pixels at 90° FOV on 1080p. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Presentation", meta = (AllowPrivateAccess = "true", ClampMin = "0.001", UIMin = "0.01", UIMax = "1.0"))
	float MinimumNoticeableAngularDiameterDegrees = 0.1f;

	/**
	 * Scale reached at the far end of the shrink span. The fade is geometric, so the first
	 * metres past the release line only nudge the size and the disk dwindles after that.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Presentation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0001", ClampMax = "1.0", UIMin = "0.001", UIMax = "0.2"))
	float MinimumCelestialScaleRatio = 0.004f;

	/**
	 * Distance past the release shell over which a body eases from authored size down to
	 * MinimumCelestialScaleRatio. Zero uses the open gap to the nearest other body.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Presentation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float CelestialShrinkSpanCentimeters = 0.0f;

	/** Real separation at which a distant body is presented at its authored local size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Cruise", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float CruiseLocalHorizonKilometers = 500.0f;

	/**
	 * Altitude above the gameplay surface that stays at authored scale. Ship combat and surface
	 * interaction inside this band see the planet at the size it was placed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Presentation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AuthoredScaleAltitudeCentimeters = 50000.0f;

	/**
	 * Extra altitude the ship must climb before a full-scale body may start shrinking. The gap
	 * stops a ship skimming the 500 m line from scaling the planet every frame.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Presentation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AuthoredScaleReleaseMarginCentimeters = 8000.0f;

	/**
	 * Extra distance past the release line before surface actors are hidden. Content is already
	 * gone while the mesh is still essentially full size, so the pop is a distant speck.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Presentation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float SurfaceContentHideMarginCentimeters = 4000.0f;
};
