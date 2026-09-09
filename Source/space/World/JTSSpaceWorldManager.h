// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "JTSSpaceWorldManager.generated.h"

class AJTSPlanetAnchor;
class AJTSMoonGameMode;
class AJTSMoonSurfaceController;
class ULevel;
class ULevelStreamingDynamic;
class UObject;
class USceneComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnJTSSpaceWorldInitialSurfaceLevelReady, AJTSMoonSurfaceController*);

/** High-level state shared by the persistent space world and the flight component. */
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
 * Small persistent-world coordinator for the current logical planet, travel state,
 * and explicit surface streaming requests. It intentionally owns no resource,
 * combat, inventory, or Moon-surface gameplay.
 */
UCLASS(BlueprintType)
class SPACE_API AJTSSpaceWorldManager : public AActor
{
	GENERATED_BODY()

public:
	AJTSSpaceWorldManager();

	/** Finds the single manager in the supplied world's persistent level. */
	static AJTSSpaceWorldManager* FindSpaceWorldManager(const UObject* WorldContextObject);

	/** Resolves CurrentPlanet from the explicitly configured anchor or an exact InitialPlanetId match. */
	void InitializeCurrentPlanet();

	/** Starts the one-time SpaceWorld arrival path and waits for the configured surface to be loaded and visible. */
	void BeginInitialArrival();

	/** Native notification used by SpaceWorldGameMode to place persistent player and spacecraft actors after level visibility. */
	FOnJTSSpaceWorldInitialSurfaceLevelReady& OnInitialSurfaceLevelReady();

	UFUNCTION(BlueprintPure, Category = "Space World|State")
	AJTSPlanetAnchor* GetCurrentPlanet() const;

	UFUNCTION(BlueprintPure, Category = "Space World|State")
	EJTSSpaceTravelState GetCurrentTravelState() const;

	UFUNCTION(BlueprintPure, Category = "Space World|State")
	bool IsSurfaceState() const;

	UFUNCTION(BlueprintCallable, Category = "Space World|State")
	void SetCurrentPlanet(AJTSPlanetAnchor* NewCurrentPlanet);

	UFUNCTION(BlueprintCallable, Category = "Space World|State")
	void SetTravelState(EJTSSpaceTravelState NewTravelState);

	/** Explicitly creates or re-enables a dynamic streaming instance for a planet's soft surface world. */
	UFUNCTION(BlueprintCallable, Category = "Space World|Streaming")
	bool RequestSurfaceLoad(AJTSPlanetAnchor* Planet, bool bMakeVisible = true);

	/** Explicitly requests unload. Automatic unload remains disabled by default. */
	UFUNCTION(BlueprintCallable, Category = "Space World|Streaming")
	bool RequestSurfaceUnload(AJTSPlanetAnchor* Planet);

	UFUNCTION(BlueprintPure, Category = "Space World|Streaming")
	bool IsSurfaceLevelLoaded(const AJTSPlanetAnchor* Planet) const;

	UFUNCTION(BlueprintPure, Category = "Space World|Streaming")
	bool IsSurfaceLevelVisible(const AJTSPlanetAnchor* Planet) const;

	/** Returns the loaded streaming level for a planet, if it is currently available. */
	ULevel* GetLoadedSurfaceLevel(const AJTSPlanetAnchor* Planet) const;

	/** Surface controller registration is scoped by stable PlanetId, never by a world-wide first actor search. */
	void RegisterSurfaceController(AJTSMoonSurfaceController* Controller);
	void UnregisterSurfaceController(AJTSMoonSurfaceController* Controller);
	AJTSMoonSurfaceController* GetSurfaceController(FName PlanetId) const;
	AJTSMoonSurfaceController* GetCurrentSurfaceController() const;
	void NotifySurfaceGameplayInitialized(AJTSMoonSurfaceController* Controller);

	UFUNCTION(BlueprintPure, Category = "Space World|Arrival")
	bool IsSurfaceGameplayReady() const;

	/** Called by flight code; only performs threshold streaming when explicitly enabled in the manager. */
	void HandleFlightAltitude(float SurfaceAltitude);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FName GetStreamingKey(const AJTSPlanetAnchor* Planet) const;
	ULevelStreamingDynamic* FindSurfaceStreamingLevel(const AJTSPlanetAnchor* Planet) const;
	AJTSMoonSurfaceController* EnsureSurfaceController(AJTSPlanetAnchor* Planet);
	void PollInitialSurfaceArrival();
	void LogDebugState() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Space World", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Optional explicit initial planet. If unset, InitialPlanetId must match exactly one persistent-level PlanetAnchor. */
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

	/** Prototype safety switch. It is deliberately false so MoonSurface remains loaded during first flight checks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Streaming", meta = (AllowPrivateAccess = "true"))
	bool bEnableSurfaceUnload = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDebugSpaceTravel = false;

	/** Strong references to the dynamic streaming requests, keyed by stable PlanetId. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<ULevelStreamingDynamic>> SurfaceStreamingLevels;

	/** Controllers are registered by logical planet rather than discovered from all actors in the persistent world. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<AJTSMoonSurfaceController>> SurfaceControllers;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Space World|Arrival", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSMoonSurfaceController> MoonSurfaceControllerClass;

	/** Uses the existing Moon GameMode Blueprint only as reusable balance/configuration data for a dynamic surface controller. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Arrival", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSMoonGameMode> MoonGameplaySettingsClass;

	FTimerHandle DebugTimerHandle;
	FTimerHandle InitialArrivalTimerHandle;
	FOnJTSSpaceWorldInitialSurfaceLevelReady InitialSurfaceLevelReadyDelegate;
	bool bInitialArrivalStarted = false;
	bool bInitialSurfaceLevelReady = false;
	bool bInitialSurfaceGameplayReady = false;
};
