// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

#include "JTSMoonSurfaceController.generated.h"

class AActor;
class AJTSCharacter;
class AJTSMoonCorpseActor;
class AJTSMoonGameMode;
class AJTSMoonWorldActor;
class AJTSPlanetAnchor;
class AJTSPlanetSurfaceAnchor;
class AJTSRoachActor;
class AJTSRoachNestActor;
class AJTSSpacecraftActor;
class ULevel;
enum class EJTSEquipmentType : uint8;

/**
 * Moon-specific gameplay controller. L_MoonPrototype keeps using it for Legacy Fake Moon runtime;
 * a later migration will reuse its Moon balance/initialization role for real-mesh Moon content.
 * It is intentionally not a planet definition, gravity owner, camera owner, or streaming authority.
 */
UCLASS(BlueprintType)
class SPACE_API AJTSMoonSurfaceController : public AActor
{
	GENERATED_BODY()

public:
	AJTSMoonSurfaceController();

	/** Resolves the Legacy Moon controller through AJTSMoonGameMode without scanning planet actors. */
	static AJTSMoonSurfaceController* FindMoonSurfaceController(const UObject* WorldContextObject, FName RequestedPlanetId = NAME_None);

	UFUNCTION(BlueprintPure, Category = "Moon|Surface")
	FName GetPlanetId() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Surface")
	bool IsSurfaceGameplayInitialized() const;

	/** Queues one initialization attempt after the surface has become visible and player/ship placement is complete. */
	void RequestSurfaceGameplayInitialization();

	/** Performs the one-shot runtime initialization when all Moon surface prerequisites are ready. */
	bool InitializeSurfaceGameplay();

	/** Legacy L_MoonPrototype path: supplies the active GameMode as the settings source. */
	void ConfigureLegacyRuntime(AJTSMoonGameMode* InLegacyGameMode);

	/** Legacy flat-streamed compatibility path only. Real gameplay planets no longer use this binding. */
	void SetOwningPlanet(AJTSPlanetAnchor* InOwningPlanet);

	/** Supplies the legacy Moon GameMode Blueprint CDO used as a transitional balance-data source. */
	void SetMoonGameplaySettingsClass(TSubclassOf<AJTSMoonGameMode> InMoonGameplaySettingsClass);

	/** Returns the transitional Moon balance source; later migration will replace the GameMode CDO with Moon settings data. */
	const AJTSMoonGameMode* GetMoonSettings() const;

	/** The one Moon-surface ship found in this controller's streamed level, or explicitly supplied by SpaceWorldGameMode. */
	AJTSSpacecraftActor* GetSpacecraft() const;
	void SetSurfaceSpacecraft(AJTSSpacecraftActor* InSpacecraft);
	/** Registers the persistent player/ship owned by SpaceWorldGameMode as part of this active surface. */
	void RegisterSurfaceRuntimeActor(AActor* RuntimeActor);

	/** Spawns the one real-mesh Moon corpse at an explicitly authored surface anchor. It never initializes ants or resources. */
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

	/** Legacy Fake Moon World-Z terrain trace used by Ants, nests, and Ant corpse placement. */
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
	void InitializeMoonResources();
	void InitializeMoonLandmarksAndAntNests();
	AJTSMoonCorpseActor* FindLevelCorpseLandmark();
	void ClearGeneratedAntNests();
	bool IsAntNestCandidateFarFromShip(
		const FVector2D& CandidateLogicalPosition,
		const FVector2D& ShipLogicalPosition) const;
	void ConsumeExpeditionSupplies();
	bool TryBuyWorkshopEquipment(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft, EJTSEquipmentType EquipmentType);
	static int32 GetWholeConsumptionUnits(double Accumulator, double MinimumConsumptionUnit);

	/** Uses BP_MoonGameMode only as an optional balance-data template; it is never the owning World GameMode in SpaceWorld. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moon|Surface|Settings", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSMoonGameMode> MoonGameplaySettingsClass;

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

	TWeakObjectPtr<AJTSMoonGameMode> LegacySettingsSource;
	TWeakObjectPtr<AJTSPlanetAnchor> OwningPlanet;
	mutable TWeakObjectPtr<AJTSSpacecraftActor> CachedSpacecraft;
	mutable TWeakObjectPtr<AJTSMoonWorldActor> CachedMoonWorld;
	TWeakObjectPtr<AJTSMoonCorpseActor> LevelMoonCorpseLandmark;
	TWeakObjectPtr<AJTSMoonCorpseActor> RealSurfaceMoonCorpse;
	TArray<TWeakObjectPtr<AJTSMoonCorpseActor>> CachedLevelMoonCorpseLandmarks;
	TArray<TWeakObjectPtr<AJTSRoachNestActor>> GeneratedAntNests;
	TArray<TWeakObjectPtr<AActor>> RegisteredSurfaceRuntimeActors;

	FTimerHandle ExpeditionConsumptionTimerHandle;
	FTimerHandle SurfaceInitializationTimerHandle;
	double FoodConsumptionAccumulator = 0.0;
	double WaterConsumptionAccumulator = 0.0;
	bool bLevelCorpseLandmarkSearchCompleted = false;
	bool bMissingSpacecraftLogged = false;
	bool bSurfaceGameplayInitializationRequested = false;
	bool bSurfaceGameplayInitialized = false;
};
