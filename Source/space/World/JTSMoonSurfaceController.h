// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "space/World/JTSPlanetSurfaceGameplay.h"

#include "JTSMoonSurfaceController.generated.h"

class AActor;
class AJTSCharacter;
class AJTSMoonCorpseActor;
class AJTSMoonResourceSpawner;
class AJTSPlanetAnchor;
class AJTSPlanetSurfaceAnchor;
class AJTSMoonAntActor;
class AJTSMoonAntNestActor;
class AJTSSpacecraftActor;
class IJTSMoonSurfaceGameplaySettings;
class UJTSMoonSurfaceGameplayData;
class ULevel;

/**
 * Moon-specific gameplay controller for the active PlanetAnchor surface.
 * It is intentionally not a planet definition, gravity owner, camera owner, or streaming authority.
 */
UCLASS(BlueprintType)
class SPACE_API AJTSMoonSurfaceController : public AActor, public IJTSPlanetSurfaceGameplay
{
	GENERATED_BODY()

public:
	AJTSMoonSurfaceController();

	/** Resolves a controller active in the current SpaceWorld. */
	static AJTSMoonSurfaceController* FindMoonSurfaceController(const UObject* WorldContextObject, FName RequestedPlanetId = NAME_None);

	UFUNCTION(BlueprintPure, Category = "Moon|Surface")
	FName GetPlanetId() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Surface")
	bool IsSurfaceGameplayInitialized() const;

	/** IJTSPlanetSurfaceGameplay implementation used by SpaceWorld GameMode after arrival completes. */
	virtual bool SupportsPlanet(const AJTSPlanetAnchor* Planet) const override;
	virtual bool InitializeSurfaceGameplay(const FJTSSurfaceGameplayContext& Context) override;
	virtual void RegisterSurfacePlayer(AJTSCharacter* Player) override;
	virtual void ShutdownSurfaceGameplay() override;
	virtual bool IsSurfaceGameplayReady() const override;

	/** Binds the active real gameplay planet. SpaceWorld normally supplies this through its context. */
	void SetOwningPlanet(AJTSPlanetAnchor* InOwningPlanet);

	/** Returns the active SpaceWorld Data Asset settings. */
	const IJTSMoonSurfaceGameplaySettings* GetMoonSettings() const;
	const UJTSMoonSurfaceGameplayData* GetMoonGameplayData() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Surface")
	AJTSPlanetAnchor* GetOwningPlanet() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Surface")
	bool IsUsingRealPlanetSurfaceGameplay() const;

	/** The one Moon-surface ship found in this controller's streamed level, or explicitly supplied by SpaceWorldGameMode. */
	AJTSSpacecraftActor* GetSpacecraft() const;
	void SetSurfaceSpacecraft(AJTSSpacecraftActor* InSpacecraft);
	/** Registers the persistent player/ship owned by SpaceWorldGameMode as part of this active surface. */
	UFUNCTION(BlueprintCallable, Category = "Moon|Surface")
	void RegisterSurfaceRuntimeActor(AActor* RuntimeActor);
	TArray<AJTSCharacter*> GetActivePlayers() const;

	/** Spawns the one real-mesh Moon corpse at an explicitly authored surface anchor. It never initializes MoonAnts or resources. */
	UFUNCTION(BlueprintCallable, Category = "Moon|Real Surface")
	AJTSMoonCorpseActor* SpawnCorpseAtPlanetSurfaceAnchor(AJTSPlanetSurfaceAnchor* InCorpseSurfaceAnchor);

	/** Uses the controller's optional real-mesh corpse anchor. */
	UFUNCTION(BlueprintCallable, Category = "Moon|Real Surface")
	AJTSMoonCorpseActor* SpawnConfiguredCorpseAtPlanetSurfaceAnchor();

	/** True only for actors belonging to this streamed Moon surface instance. */
	bool OwnsSurfaceActor(const AActor* Candidate) const;
	ULevel* GetSurfaceLevel() const;

	/** Explicit surface anchors prevent L_SpaceWorld's persistent PlayerStart from becoming a Moon spawn. */
	FTransform GetSurfacePlayerSpawnTransform(const FTransform& FallbackTransform) const;
	FTransform GetSurfaceSpacecraftSpawnTransform(const FTransform& FallbackTransform) const;

	/** Resolves collision against the active Moon PlanetAnchor surface. */
	bool ResolveMoonGroundLocation(
		const FVector& CandidateLocation,
		FVector& OutGroundLocation) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool InitializeConfiguredSurfaceGameplay();
	void ApplySurfaceGameplayContext(const FJTSSurfaceGameplayContext& Context);
	void InitializeMoonResources();
	void InitializeMoonLandmarksAndMoonAntNests();
	AJTSMoonCorpseActor* FindLevelCorpseLandmark();
	void ClearGeneratedMoonAntNests();
	void ConsumeExpeditionSupplies();
	static int32 GetWholeConsumptionUnits(double Accumulator, double MinimumConsumptionUnit);

	/** Project-configured Moon balance and asset selection for real SpaceWorld gameplay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Surface|Settings", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSMoonSurfaceGameplayData> MoonGameplayData;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Moon|Surface", meta = (AllowPrivateAccess = "true"))
	FName PlanetId = TEXT("Moon");

	/** Prefer a placed PlayerStart/TargetPoint in L_MoonSurface for persistent-world arrival. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Moon|Surface|Arrival", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> SurfacePlayerSpawnAnchor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moon|Surface|Arrival", meta = (AllowPrivateAccess = "true"))
	bool bUseSurfacePlayerSpawnTransform = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moon|Surface|Arrival", meta = (AllowPrivateAccess = "true", EditCondition = "bUseSurfacePlayerSpawnTransform"))
	FTransform SurfacePlayerSpawnTransform = FTransform::Identity;

	/** Prefer the stable existing landing actor/TargetPoint in L_MoonSurface for a fallback ship spawn. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Moon|Surface|Arrival", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> SurfaceSpacecraftSpawnAnchor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moon|Surface|Arrival", meta = (AllowPrivateAccess = "true"))
	bool bUseSurfaceSpacecraftSpawnTransform = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moon|Surface|Arrival", meta = (AllowPrivateAccess = "true", EditCondition = "bUseSurfaceSpacecraftSpawnTransform"))
	FTransform SurfaceSpacecraftSpawnTransform = FTransform::Identity;

	/** An authored anchor determines the real-surface corpse placement. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Moon|Real Surface", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetSurfaceAnchor> CorpseSurfaceAnchor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Real Surface", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSMoonCorpseActor> MoonCorpseClass;

	/** Explicit generator binding for this real Moon surface. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Moon|Real Surface", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSMoonResourceSpawner> MoonResourceSpawner;

	TWeakObjectPtr<AJTSPlanetAnchor> OwningPlanet;
	TArray<TWeakObjectPtr<AJTSCharacter>> ActivePlayers;
	UPROPERTY(Transient)
	TObjectPtr<UJTSMoonSurfaceGameplayData> ActiveMoonGameplayData;
	mutable TWeakObjectPtr<AJTSSpacecraftActor> CachedSpacecraft;
	TWeakObjectPtr<AJTSMoonCorpseActor> LevelMoonCorpseLandmark;
	TWeakObjectPtr<AJTSMoonCorpseActor> RealSurfaceMoonCorpse;
	TArray<TWeakObjectPtr<AJTSMoonCorpseActor>> CachedLevelMoonCorpseLandmarks;
	TArray<TWeakObjectPtr<AJTSMoonAntNestActor>> GeneratedMoonAntNests;
	TArray<TWeakObjectPtr<AActor>> RegisteredSurfaceRuntimeActors;

	FTimerHandle ExpeditionConsumptionTimerHandle;
	double FoodConsumptionAccumulator = 0.0;
	double WaterConsumptionAccumulator = 0.0;
	bool bLevelCorpseLandmarkSearchCompleted = false;
	bool bMissingSpacecraftLogged = false;
	bool bSurfaceGameplayInitialized = false;
};
