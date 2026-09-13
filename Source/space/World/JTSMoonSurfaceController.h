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
class AJTSMoonGameMode;
class AJTSMoonResourceSpawner;
class AJTSMoonWorldActor;
class AJTSPlanetAnchor;
class AJTSPlanetSurfaceAnchor;
class AJTSMoonAntActor;
class AJTSMoonAntNestActor;
class AJTSSpacecraftActor;
class IJTSMoonSurfaceGameplaySettings;
class UJTSMoonSurfaceGameplayData;
class ULevel;
enum class EJTSEquipmentType : uint8;

/**
 * Moon-specific gameplay controller. L_MoonPrototype keeps using it for Legacy Fake Moon runtime;
 * a later migration will reuse its Moon balance/initialization role for real-mesh Moon content.
 * It is intentionally not a planet definition, gravity owner, camera owner, or streaming authority.
 */
UCLASS(BlueprintType)
class SPACE_API AJTSMoonSurfaceController : public AActor, public IJTSPlanetSurfaceGameplay
{
	GENERATED_BODY()

public:
	AJTSMoonSurfaceController();

	/** Resolves either the legacy Moon controller or a controller active in the current SpaceWorld. */
	static AJTSMoonSurfaceController* FindMoonSurfaceController(const UObject* WorldContextObject, FName RequestedPlanetId = NAME_None);

	UFUNCTION(BlueprintPure, Category = "Moon|Surface")
	FName GetPlanetId() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Surface")
	bool IsSurfaceGameplayInitialized() const;

	/** IJTSPlanetSurfaceGameplay implementation used by SpaceWorld GameMode after arrival completes. */
	virtual bool SupportsPlanet(const AJTSPlanetAnchor* Planet) const override;
	virtual bool InitializeSurfaceGameplay(const FJTSSurfaceGameplayContext& Context) override;
	virtual void ShutdownSurfaceGameplay() override;
	virtual bool IsSurfaceGameplayReady() const override;

	/** Queues one initialization attempt after the surface has become visible and player/ship placement is complete. */
	void RequestSurfaceGameplayInitialization();

	/** Performs the Legacy one-shot initialization. SpaceWorld should call the Context overload. */
	bool InitializeSurfaceGameplay();

	/** Legacy L_MoonPrototype path: supplies the active GameMode as the settings source. */
	void ConfigureLegacyRuntime(AJTSMoonGameMode* InLegacyGameMode);

	/** Legacy flat-streamed compatibility path only. Real gameplay planets no longer use this binding. */
	void SetOwningPlanet(AJTSPlanetAnchor* InOwningPlanet);

	/** Supplies the legacy Moon GameMode Blueprint CDO used only by L_MoonPrototype. */
	void SetMoonGameplaySettingsClass(TSubclassOf<AJTSMoonGameMode> InMoonGameplaySettingsClass);

	/** Returns real-SpaceWorld Data Asset settings, or legacy GameMode settings only on the Legacy map. */
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

	/** Spawns the one real-mesh Moon corpse at an explicitly authored surface anchor. It never initializes MoonAnts or resources. */
	UFUNCTION(BlueprintCallable, Category = "Moon|Real Surface")
	AJTSMoonCorpseActor* SpawnCorpseAtPlanetSurfaceAnchor(AJTSPlanetSurfaceAnchor* InCorpseSurfaceAnchor);

	/** Uses the controller's optional real-mesh corpse anchor. */
	UFUNCTION(BlueprintCallable, Category = "Moon|Real Surface")
	AJTSMoonCorpseActor* SpawnConfiguredCorpseAtPlanetSurfaceAnchor();

	/** Resolves the placed Fake Moon configuration only inside this surface level. */
	AJTSMoonWorldActor* GetMoonWorldActor() const;

	/** True only for actors belonging to this streamed Moon surface instance. */
	bool OwnsSurfaceActor(const AActor* Candidate) const;
	ULevel* GetSurfaceLevel() const;

	/** Explicit surface anchors prevent L_SpaceWorld's persistent PlayerStart from becoming a Moon spawn. */
	FTransform GetSurfacePlayerSpawnTransform(const FTransform& FallbackTransform) const;
	FTransform GetSurfaceSpacecraftSpawnTransform(const FTransform& FallbackTransform) const;

	/** Resolves real PlanetAnchor collision on SpaceWorld Moon, otherwise retains the Legacy Fake Moon terrain trace. */
	bool ResolveMoonGroundLocation(
		const FVector& CandidateLocation,
		FVector& OutGroundLocation,
		const AActor* AdditionalIgnoredActor = nullptr) const;

	/** Shared Moon workshop transactions. */
	bool TryCraftPickaxe(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft);
	bool TryCraftBackpack(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft);
	bool TryCraftKnife(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft);
	bool TryCraftAxe(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Timer-friendly wrapper for the bool-returning initialization operation. */
	void AttemptSurfaceGameplayInitialization();
	void ScheduleInitializationRetry();
	void ApplySurfaceGameplayContext(const FJTSSurfaceGameplayContext& Context);
	void InitializeMoonResources();
	void InitializeMoonLandmarksAndMoonAntNests();
	AJTSMoonCorpseActor* FindLevelCorpseLandmark();
	void ClearGeneratedMoonAntNests();
	bool IsMoonAntNestCandidateFarFromShip(
		const FVector2D& CandidateLogicalPosition,
		const FVector2D& ShipLogicalPosition) const;
	void ConsumeExpeditionSupplies();
	bool TryBuyWorkshopEquipment(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft, EJTSEquipmentType EquipmentType);
	static int32 GetWholeConsumptionUnits(double Accumulator, double MinimumConsumptionUnit);

	/** Legacy map fallback only. Real SpaceWorld Moon config comes from MoonGameplayData or Context.GameplayData. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moon|Surface|Settings", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSMoonGameMode> MoonGameplaySettingsClass;

	/** Project-configured Moon balance and asset selection for real SpaceWorld gameplay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Surface|Settings", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSMoonSurfaceGameplayData> MoonGameplayData;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Moon|Surface", meta = (AllowPrivateAccess = "true"))
	FName PlanetId = TEXT("Moon");

	/** Optional explicit Fake Moon actor. If unset, discovery remains limited to this controller's level. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Moon|Surface", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSMoonWorldActor> MoonWorldActor;

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

	/** Real Planet_3 content only: an authored anchor replaces legacy fixed XY/World-Z corpse placement. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Moon|Real Surface", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetSurfaceAnchor> CorpseSurfaceAnchor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Real Surface", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSMoonCorpseActor> MoonCorpseClass;

	/** Explicit real-SpaceWorld generator binding. Legacy MoonPrototype retains its level scan fallback. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Moon|Real Surface", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSMoonResourceSpawner> MoonResourceSpawner;

	TWeakObjectPtr<AJTSMoonGameMode> LegacySettingsSource;
	TWeakObjectPtr<AJTSPlanetAnchor> OwningPlanet;
	TWeakObjectPtr<AJTSCharacter> CachedPlayer;
	UPROPERTY(Transient)
	TObjectPtr<UJTSMoonSurfaceGameplayData> ActiveMoonGameplayData;
	mutable TWeakObjectPtr<AJTSSpacecraftActor> CachedSpacecraft;
	mutable TWeakObjectPtr<AJTSMoonWorldActor> CachedMoonWorld;
	TWeakObjectPtr<AJTSMoonCorpseActor> LevelMoonCorpseLandmark;
	TWeakObjectPtr<AJTSMoonCorpseActor> RealSurfaceMoonCorpse;
	TArray<TWeakObjectPtr<AJTSMoonCorpseActor>> CachedLevelMoonCorpseLandmarks;
	TArray<TWeakObjectPtr<AJTSMoonAntNestActor>> GeneratedMoonAntNests;
	TArray<TWeakObjectPtr<AActor>> RegisteredSurfaceRuntimeActors;

	FTimerHandle ExpeditionConsumptionTimerHandle;
	FTimerHandle SurfaceInitializationTimerHandle;
	double FoodConsumptionAccumulator = 0.0;
	double WaterConsumptionAccumulator = 0.0;
	bool bLevelCorpseLandmarkSearchCompleted = false;
	bool bMissingSpacecraftLogged = false;
	bool bSurfaceGameplayInitializationRequested = false;
	bool bSurfaceGameplayInitialized = false;
	bool bUsingRealPlanetSurfaceGameplay = false;
};
