// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

#include "JTSSpaceWorldManager.generated.h"

class AActor;
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
 * planet content streaming. It deliberately does not know Moon-only gameplay, Fake Moon wrapping,
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

	/** Set by the GameMode once the initial Character has been snapped; this gates startup/input readiness, never gravity. */
	void SetSurfaceGameplayReady(bool bReady);

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

	/** Called by future flight code after movement; the value is centre-relative approximate altitude. */
	void HandleFlightAltitude(float ApproximateAltitude);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RegisterPersistentPlanetAnchors();
	FName GetPlanetKey(const AJTSPlanetAnchor* Planet) const;
	ULevelStreamingDynamic* FindPlanetContentStreamingLevel(const AJTSPlanetAnchor* Planet) const;
	void LogDebugState() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Space World", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Optional explicit initial planet. If unset, InitialPlanetId must match exactly one registered anchor. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Space World|State", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetAnchor> InitialPlanet;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Space World|State", meta = (AllowPrivateAccess = "true"))
	FName InitialPlanetId = TEXT("Moon");

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Space World|State", meta = (AllowPrivateAccess = "true"))
	EJTSSpaceTravelState InitialTravelState = EJTSSpaceTravelState::Surface;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Space World|State", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetAnchor> CurrentPlanet;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Space World|State", meta = (AllowPrivateAccess = "true"))
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
	bool bSurfaceGameplayReady = false;
};
