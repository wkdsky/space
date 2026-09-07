// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "space/World/JTSMoonResourceSpawner.h"

#include "JTSMoonGameMode.generated.h"

class AJTSCharacter;
class AJTSMoonCorpseActor;
class AJTSRoachNestActor;
class AJTSSpacecraftActor;
enum class EJTSEquipmentType : uint8;

/** Ruleset for the first playable Moon exploration level. */
UCLASS()
class SPACE_API AJTSMoonGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJTSMoonGameMode();

	UFUNCTION(BlueprintPure, Category = "Moon|Survival")
	int32 GetCrewCount() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Survival")
	float GetFoodConsumptionPerPersonPerMinute() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Survival")
	float GetWaterConsumptionPerPersonPerMinute() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Survival")
	float GetConsumptionTickInterval() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Survival")
	float GetMinimumConsumptionUnit() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Crafting")
	int32 GetPickaxeRockCost() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Crafting")
	int32 GetBackpackRockCost() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Crafting")
	int32 GetBackpackOreCost() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Crafting")
	int32 GetKnifeRockCost() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Crafting")
	int32 GetKnifeOreCost() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Crafting")
	int32 GetAxeRockCost() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Crafting")
	int32 GetAxeOreCost() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Mining")
	int32 GetLargeRockTotalYieldUnits() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Mining")
	int32 GetOreDepositTotalYieldUnits() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Interaction")
	float GetPickupMaxDistance() const;

	/** Screen-space acquire radius at 1080p. InteractionComponent scales it by viewport height. */
	UFUNCTION(BlueprintPure, Category = "Moon|Interaction")
	float GetPickupAcquireRadius() const;

	/** Screen-space retain radius at 1080p used by the sticky pickup target. */
	UFUNCTION(BlueprintPure, Category = "Moon|Interaction")
	float GetPickupRetainRadius() const;

	/** Maximum world distance from the camera aim ray for a pickup candidate. */
	UFUNCTION(BlueprintPure, Category = "Moon|Interaction")
	float GetPickupAimRayRadius() const;

	/** Cached authoritative spacecraft used by Moon HUD and survival rules. */
	UFUNCTION(BlueprintPure, Category = "Moon|Navigation")
	AJTSSpacecraftActor* GetSpacecraft() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Navigation")
	float GetSpacecraftMarkerShowDistance() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Navigation")
	float GetSpacecraftMarkerScreenSafeMargin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Pickup")
	float GetPickupDropUpwardSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Pickup")
	float GetPickupDropHorizontalSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Combat")
	float GetAttackRange() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Combat")
	float GetAttackAimRadius() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Combat")
	float GetAttackCooldown() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	int32 GetRoachNestCount() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachNestRadiusAroundCorpse() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachNestSpawnWeightNearCorpse() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachNestMinDistanceFromCorpse() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachNestMinDistanceFromShip() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachNestMinSpacing() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachSpawnChance() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachSpawnIntervalMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachSpawnIntervalMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachSpawnOffset() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachCrawlSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachEscapeSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachEscapeDuration() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachLifetime() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachEmergingDuration() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachHitReactionDuration() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachBurrowTime() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachGroundTraceStartHeight() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	float GetRoachGroundTraceDistance() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	int32 GetRoachPunchHitsToKill() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	int32 GetRoachNestPunchHitsToDestroy() const;

	/** Performs the Moon workshop's pickaxe transaction against the supplied player's nearby ship. */
	UFUNCTION(BlueprintCallable, Category = "Moon|Crafting")
	bool TryCraftPickaxe(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft);

	/** Performs the Moon workshop's backpack transaction against the supplied player's nearby ship. */
	UFUNCTION(BlueprintCallable, Category = "Moon|Crafting")
	bool TryCraftBackpack(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft);

	UFUNCTION(BlueprintCallable, Category = "Moon|Crafting")
	bool TryCraftKnife(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft);

	UFUNCTION(BlueprintCallable, Category = "Moon|Crafting")
	bool TryCraftAxe(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void InitializeMoonResources();
	void InitializeMoonRuntimeContent();
	void InitializeMoonLandmarksAndRoachNests();
	AJTSMoonCorpseActor* FindLevelCorpseLandmark();
	void ClearGeneratedRoachNests();
	bool ResolveMoonGroundLocation(const FVector& CandidateLocation, FVector& OutGroundLocation) const;
	bool IsRoachNestCandidateFarFromShip(
		const FVector2D& CandidateLogicalPosition,
		const FVector2D& ShipLogicalPosition) const;
	void ConsumeExpeditionSupplies();
	bool TryBuyWorkshopEquipment(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft, EJTSEquipmentType EquipmentType);
	static int32 GetWholeConsumptionUnits(double Accumulator, double MinimumConsumptionUnit);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Survival", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 CrewCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Survival", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float FoodConsumptionPerPersonPerMinute = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Survival", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float WaterConsumptionPerPersonPerMinute = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Survival", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float ConsumptionTickInterval = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Survival", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MinimumConsumptionUnit = 0.1f;

	/** Settings applied to the Moon level's existing AJTSMoonResourceSpawner before generation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Resources", meta = (AllowPrivateAccess = "true", ShowOnlyInnerProperties))
	FJTSMoonResourceSpawnSettings MoonResourceSpawnSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Crafting", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 PickaxeRockCost = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Mining", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 LargeRockTotalYieldUnits = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Mining", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 OreDepositTotalYieldUnits = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Interaction", meta = (AllowPrivateAccess = "true", ClampMin = "50.0", UIMin = "50.0"))
	float PickupMaxDistance = 550.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Interaction", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PickupAcquireRadius = 165.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Interaction", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PickupRetainRadius = 225.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Interaction", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PickupAimRayRadius = 95.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Navigation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float SpacecraftMarkerShowDistance = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Navigation", meta = (AllowPrivateAccess = "true", ClampMin = "20.0", UIMin = "20.0"))
	float SpacecraftMarkerScreenSafeMargin = 56.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Pickup", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float PickupDropUpwardSpeed = 340.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Pickup", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float PickupDropHorizontalSpeed = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Crafting", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 BackpackRockCost = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Crafting", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 BackpackOreCost = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Crafting", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 KnifeRockCost = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Crafting", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 KnifeOreCost = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Crafting", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 AxeRockCost = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Crafting", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 AxeOreCost = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "50.0", UIMin = "50.0"))
	float AttackRange = 240.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float AttackAimRadius = 42.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "0.05", UIMin = "0.05"))
	float AttackCooldown = 0.35f;

	/** Runtime-only nest class; defaults to the native destructible nest. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSRoachNestActor> RoachNestActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 RoachNestCount = 9;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float RoachNestRadiusAroundCorpse = 1600.0f;

	/** 0 is an even-area distribution; 1 increasingly biases nests toward the corpse. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float RoachNestSpawnWeightNearCorpse = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float RoachNestMinDistanceFromCorpse = 180.0f;

	/** Keeps random nests out of the spacecraft landmark's immediate area. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float RoachNestMinDistanceFromShip = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float RoachNestMinSpacing = 260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float RoachSpawnChance = 0.38f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float RoachSpawnIntervalMin = 7.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float RoachSpawnIntervalMax = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float RoachSpawnOffset = 130.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float RoachCrawlSpeed = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float RoachEscapeSpeed = 700.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float RoachEscapeDuration = 1.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float RoachLifetime = 9.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float RoachEmergingDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float RoachHitReactionDuration = 0.16f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float RoachBurrowTime = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float RoachGroundTraceStartHeight = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float RoachGroundTraceDistance = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 RoachPunchHitsToKill = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 RoachNestPunchHitsToDestroy = 3;

	FTimerHandle ExpeditionConsumptionTimerHandle;
	FTimerHandle MoonRuntimeInitializationTimerHandle;
	double FoodConsumptionAccumulator = 0.0;
	double WaterConsumptionAccumulator = 0.0;
	mutable TWeakObjectPtr<AJTSSpacecraftActor> CachedSpacecraft;
	/** Selected once from level-placed corpse actors; never spawned or repositioned by this GameMode. */
	TWeakObjectPtr<AJTSMoonCorpseActor> LevelMoonCorpseLandmark;
	/** All valid level corpses found during the one-time lookup, used only for resource/nest exclusion. */
	TArray<TWeakObjectPtr<AJTSMoonCorpseActor>> CachedLevelMoonCorpseLandmarks;
	TArray<TWeakObjectPtr<AJTSRoachNestActor>> GeneratedRoachNests;
	bool bLevelCorpseLandmarkSearchCompleted = false;
	bool bMissingSpacecraftLogged = false;
};
