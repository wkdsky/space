// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "space/World/JTSMoonResourceSpawner.h"
#include "space/World/JTSMoonSurfaceGameplaySettings.h"

#include "JTSMoonGameMode.generated.h"

class AActor;
class AJTSCharacter;
class AJTSMoonCorpseActor;
class AJTSMoonAntActor;
class AJTSMoonAntNestActor;
class AJTSMoonSurfaceController;
class AJTSSpacecraftActor;
enum class EJTSEquipmentType : uint8;

/** Ruleset for the first playable Moon exploration level. */
UCLASS()
class SPACE_API AJTSMoonGameMode : public AGameModeBase, public IJTSMoonSurfaceGameplaySettings
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

	/** Legacy MoonPrototype's instance of the same runtime controller used by streamed MoonSurface. */
	AJTSMoonSurfaceController* GetMoonSurfaceController() const;

	const FJTSMoonResourceSpawnSettings& GetMoonResourceSpawnSettings() const;

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

	/** Configured runtime MoonAnt class. Nests use the native legacy class only when this is null. */
	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt")
	TSubclassOf<AJTSMoonAntActor> GetMoonAntActorClass() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Nests")
	TSubclassOf<AJTSMoonAntNestActor> GetMoonAntNestActorClass() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Nests")
	int32 GetMoonAntNestCount() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Nests")
	float GetMoonAntNestOuterRadiusAroundCorpse() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Nests")
	float GetMoonAntNestMinDistanceFromCorpse() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Nests")
	float GetMoonAntNestMinDistanceFromShip() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Nests")
	float GetMoonAntNestBaseMinSpacing() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Nests")
	float GetMoonAntNestCandidateSpacingScaleMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Nests")
	float GetMoonAntNestCandidateSpacingScaleMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Nests")
	float GetMoonAntNestVisualScaleVariationMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Nests")
	float GetMoonAntNestVisualScaleVariationMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Nests")
	float GetMoonAntNestInnerWeight() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Nests")
	float GetMoonAntNestMidWeight() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Nests")
	float GetMoonAntNestOuterWeight() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Spawning")
	int32 GetMaxActiveMoonAntsPerNest() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Spawning")
	float GetMoonAntSpawnChance() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Spawning")
	float GetMoonAntSpawnIntervalMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Spawning")
	float GetMoonAntSpawnIntervalMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Spawning")
	float GetMoonAntSpawnNearWeight() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Spawning")
	float GetMoonAntSpawnMidWeight() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Spawning")
	float GetMoonAntSpawnFarWeight() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Spawning")
	float GetMoonAntSpawnNearDistanceMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Spawning")
	float GetMoonAntSpawnNearDistanceMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Spawning")
	float GetMoonAntSpawnMidDistanceMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Spawning")
	float GetMoonAntSpawnMidDistanceMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Spawning")
	float GetMoonAntSpawnFarDistanceMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Spawning")
	float GetMoonAntSpawnFarDistanceMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Behavior")
	float GetMoonAntRoamSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Behavior")
	float GetMoonAntRoamRadius() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Behavior")
	float GetMoonAntMaxHomeRadius() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Behavior")
	float GetMoonAntRoamRetargetIntervalMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Behavior")
	float GetMoonAntRoamRetargetIntervalMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Behavior")
	float GetMoonAntSurfaceDurationMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Behavior")
	float GetMoonAntSurfaceDurationMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Behavior")
	float GetMoonAntEmergingDuration() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Behavior")
	float GetMoonAntHitReactionDuration() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Behavior")
	float GetMoonAntBurrowDuration() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Behavior")
	float GetMoonAntFleeSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Behavior")
	float GetMoonAntFleeDurationMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Behavior")
	float GetMoonAntFleeDurationMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Behavior")
	float GetMoonAntMaxFleeDistance() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Behavior")
	float GetMoonAntTurnSpeed() const;

	/** Retained only for serialized Blueprint/API compatibility. MoonAnts never use chase behavior. */
	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "MoonAnts are neutral and do not chase players."))
	float GetMoonAntChaseSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "MoonAnts do not use a returning state."))
	float GetMoonAntReturnSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "MoonAnts do not aggro players."))
	float GetMoonAntAggroRadius() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "MoonAnts do not aggro players."))
	float GetMoonAntLoseAggroRadius() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "MoonAnts do not react to player proximity."))
	float GetMoonAntStopDistanceFromPlayer() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use GetMoonAntRoamSpeed."))
	float GetMoonAntWanderSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use GetMoonAntRoamRadius."))
	float GetMoonAntWanderRadius() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use GetMoonAntRoamRetargetIntervalMin."))
	float GetMoonAntWanderRetargetIntervalMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use GetMoonAntRoamRetargetIntervalMax."))
	float GetMoonAntWanderRetargetIntervalMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use GetMoonAntSurfaceDurationMax."))
	float GetMoonAntLifetime() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Ground")
	float GetMoonAntGroundTraceStartHeight() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Ground")
	float GetMoonAntGroundTraceDistance() const;

	/** Serialized compatibility only. Runtime MoonAnt death now uses UJTSHealthComponent. */
	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "MoonAnt health is configured on AJTSMoonAntActor via MoonAntMaxHealth."))
	int32 GetMoonAntPunchHitsToKill() const;

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Combat")
	int32 GetMoonAntNestPunchHitsToDestroy() const;

	/** Shared Moon ground trace used by MoonAnt Nests and moving MoonAnts. */
	bool ResolveMoonGroundLocation(
		const FVector& CandidateLocation,
		FVector& OutGroundLocation,
		const AActor* AdditionalIgnoredActor = nullptr) const;

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
	AJTSMoonSurfaceController* EnsureMoonSurfaceController();

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

	/** Assign BP_MoonAnt here in the active Moon GameMode Blueprint. Null intentionally uses the native fallback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSMoonAntActor> MoonAntActorClass;

	/** Runtime-only MoonAnt Nest class; it defaults to the native legacy class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSMoonAntNestActor> MoonAntNestActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 MoonAntNestCount = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntNestOuterRadiusAroundCorpse = 1800.0f;

	/** Additional corpse clearance is combined with the corpse visual bounds before selecting a nest position. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntNestMinDistanceFromCorpse = 180.0f;

	/** Keeps random MoonAnt Nests out of the spacecraft landmark's immediate area. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntNestMinDistanceFromShip = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntNestBaseMinSpacing = 220.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntNestCandidateSpacingScaleMin = 0.80f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntNestCandidateSpacingScaleMax = 1.20f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntNestVisualScaleVariationMin = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntNestVisualScaleVariationMax = 1.15f;

	/** Overlapping corpse-centered zones use these relative selection weights. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntNestInnerWeight = 0.60f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntNestMidWeight = 0.28f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntNestOuterWeight = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 MaxActiveMoonAntsPerNest = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MoonAntSpawnChance = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntSpawnIntervalMin = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntSpawnIntervalMax = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnNearWeight = 0.62f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnMidWeight = 0.27f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnFarWeight = 0.11f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnNearDistanceMin = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnNearDistanceMax = 190.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnMidDistanceMin = 160.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnMidDistanceMax = 360.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnFarDistanceMin = 320.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntSpawnFarDistanceMax = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntRoamSpeed = 85.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntRoamRadius = 420.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntMaxHomeRadius = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntRoamRetargetIntervalMin = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntRoamRetargetIntervalMax = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntSurfaceDurationMin = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntSurfaceDurationMax = 14.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntEmergingDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntHitReactionDuration = 0.10f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntFleeSpeed = 260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntFleeDurationMin = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntFleeDurationMax = 2.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntMaxFleeDistance = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntBurrowDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntTurnSpeed = 540.0f;

	/** Serialized compatibility only. None of these legacy settings participate in MoonAnt behavior. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use MoonAntRoamSpeed."))
	float MoonAntWanderSpeed = 140.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "MoonAnts never chase players."))
	float MoonAntChaseSpeed = 320.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "MoonAnts do not use a returning state."))
	float MoonAntReturnSpeed = 220.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "MoonAnts never aggro players."))
	float MoonAntAggroRadius = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "MoonAnts never aggro players."))
	float MoonAntLoseAggroRadius = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "MoonAnts do not react to player proximity."))
	float MoonAntStopDistanceFromPlayer = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use MoonAntRoamRadius."))
	float MoonAntWanderRadius = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use MoonAntRoamRetargetIntervalMin."))
	float MoonAntWanderRetargetIntervalMin = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use MoonAntRoamRetargetIntervalMax."))
	float MoonAntWanderRetargetIntervalMax = 3.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use MoonAntSurfaceDurationMax."))
	float MoonAntLifetime = 40.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Ground", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonAntGroundTraceStartHeight = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Ground", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntGroundTraceDistance = 3000.0f;

	/** Serialized compatibility only; no MoonAnt runtime code reads this counter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use MoonAntMaxHealth on AJTSMoonAntActor."))
	int32 MoonAntPunchHitsToKill = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 MoonAntNestPunchHitsToDestroy = 3;

	/** Optional class for the legacy runtime actor. It shares its implementation with streamed MoonSurface. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Runtime", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSMoonSurfaceController> MoonSurfaceControllerClass;

	TWeakObjectPtr<AJTSMoonSurfaceController> MoonSurfaceController;
};
