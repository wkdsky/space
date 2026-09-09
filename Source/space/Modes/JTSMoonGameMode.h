// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "space/World/JTSMoonResourceSpawner.h"

#include "JTSMoonGameMode.generated.h"

class AActor;
class AJTSCharacter;
class AJTSMoonCorpseActor;
class AJTSRoachActor;
class AJTSRoachNestActor;
class AJTSMoonSurfaceController;
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

	/** Configured runtime Ant class. Nests use the native legacy class only when this is null. */
	UFUNCTION(BlueprintPure, Category = "Moon|Ant")
	TSubclassOf<AJTSRoachActor> GetAntActorClass() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Nests")
	TSubclassOf<AJTSRoachNestActor> GetAntNestActorClass() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Nests")
	int32 GetAntNestCount() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Nests")
	float GetAntNestOuterRadiusAroundCorpse() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Nests")
	float GetAntNestMinDistanceFromCorpse() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Nests")
	float GetAntNestMinDistanceFromShip() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Nests")
	float GetAntNestBaseMinSpacing() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Nests")
	float GetAntNestCandidateSpacingScaleMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Nests")
	float GetAntNestCandidateSpacingScaleMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Nests")
	float GetAntNestVisualScaleVariationMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Nests")
	float GetAntNestVisualScaleVariationMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Nests")
	float GetAntNestInnerWeight() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Nests")
	float GetAntNestMidWeight() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Nests")
	float GetAntNestOuterWeight() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Spawning")
	int32 GetMaxActiveAntsPerNest() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Spawning")
	float GetAntSpawnChance() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Spawning")
	float GetAntSpawnIntervalMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Spawning")
	float GetAntSpawnIntervalMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Spawning")
	float GetAntSpawnNearWeight() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Spawning")
	float GetAntSpawnMidWeight() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Spawning")
	float GetAntSpawnFarWeight() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Spawning")
	float GetAntSpawnNearDistanceMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Spawning")
	float GetAntSpawnNearDistanceMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Spawning")
	float GetAntSpawnMidDistanceMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Spawning")
	float GetAntSpawnMidDistanceMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Spawning")
	float GetAntSpawnFarDistanceMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Spawning")
	float GetAntSpawnFarDistanceMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Behavior")
	float GetAntRoamSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Behavior")
	float GetAntRoamRadius() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Behavior")
	float GetAntMaxHomeRadius() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Behavior")
	float GetAntRoamRetargetIntervalMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Behavior")
	float GetAntRoamRetargetIntervalMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Behavior")
	float GetAntSurfaceDurationMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Behavior")
	float GetAntSurfaceDurationMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Behavior")
	float GetAntEmergingDuration() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Behavior")
	float GetAntHitReactionDuration() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Behavior")
	float GetAntBurrowDuration() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Behavior")
	float GetAntFleeSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Behavior")
	float GetAntFleeDurationMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Behavior")
	float GetAntFleeDurationMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Behavior")
	float GetAntMaxFleeDistance() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Behavior")
	float GetAntTurnSpeed() const;

	/** Retained only for serialized Blueprint/API compatibility. Moon Ants never use chase behavior. */
	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Moon Ants are neutral and do not chase players."))
	float GetAntChaseSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Moon Ants do not use a returning state."))
	float GetAntReturnSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Moon Ants do not aggro players."))
	float GetAntAggroRadius() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Moon Ants do not aggro players."))
	float GetAntLoseAggroRadius() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Moon Ants do not react to player proximity."))
	float GetAntStopDistanceFromPlayer() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use GetAntRoamSpeed."))
	float GetAntWanderSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use GetAntRoamRadius."))
	float GetAntWanderRadius() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use GetAntRoamRetargetIntervalMin."))
	float GetAntWanderRetargetIntervalMin() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use GetAntRoamRetargetIntervalMax."))
	float GetAntWanderRetargetIntervalMax() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use GetAntSurfaceDurationMax."))
	float GetAntLifetime() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Ground")
	float GetAntGroundTraceStartHeight() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Ground")
	float GetAntGroundTraceDistance() const;

	/** Serialized compatibility only. Runtime Ant death now uses UJTSHealthComponent. */
	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Moon Ant health is configured on AJTSRoachActor via AntMaxHealth."))
	int32 GetAntPunchHitsToKill() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ant|Combat")
	int32 GetAntNestPunchHitsToDestroy() const;

	/** Shared Moon ground trace used by Ant Nests and moving Ants. */
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSRoachActor> AntActorClass;

	/** Runtime-only Ant Nest class; it defaults to the native legacy class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Nests", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSRoachNestActor> AntNestActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 AntNestCount = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float AntNestOuterRadiusAroundCorpse = 1800.0f;

	/** Additional corpse clearance is combined with the corpse visual bounds before selecting a nest position. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntNestMinDistanceFromCorpse = 180.0f;

	/** Keeps random Ant Nests out of the spacecraft landmark's immediate area. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntNestMinDistanceFromShip = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float AntNestBaseMinSpacing = 220.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float AntNestCandidateSpacingScaleMin = 0.80f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float AntNestCandidateSpacingScaleMax = 1.20f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float AntNestVisualScaleVariationMin = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float AntNestVisualScaleVariationMax = 1.15f;

	/** Overlapping corpse-centered zones use these relative selection weights. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntNestInnerWeight = 0.60f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntNestMidWeight = 0.28f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Nests", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntNestOuterWeight = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 MaxActiveAntsPerNest = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float AntSpawnChance = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float AntSpawnIntervalMin = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float AntSpawnIntervalMax = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntSpawnNearWeight = 0.62f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntSpawnMidWeight = 0.27f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntSpawnFarWeight = 0.11f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntSpawnNearDistanceMin = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntSpawnNearDistanceMax = 190.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntSpawnMidDistanceMin = 160.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntSpawnMidDistanceMax = 360.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntSpawnFarDistanceMin = 320.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Spawning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntSpawnFarDistanceMax = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float AntRoamSpeed = 85.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float AntRoamRadius = 420.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float AntMaxHomeRadius = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float AntRoamRetargetIntervalMin = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float AntRoamRetargetIntervalMax = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float AntSurfaceDurationMin = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float AntSurfaceDurationMax = 14.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntEmergingDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntHitReactionDuration = 0.10f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float AntFleeSpeed = 260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float AntFleeDurationMin = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float AntFleeDurationMax = 2.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float AntMaxFleeDistance = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntBurrowDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Behavior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float AntTurnSpeed = 540.0f;

	/** Serialized compatibility only. None of these legacy settings participate in Moon Ant behavior. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use AntRoamSpeed."))
	float AntWanderSpeed = 140.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Moon Ants never chase players."))
	float AntChaseSpeed = 320.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Moon Ants do not use a returning state."))
	float AntReturnSpeed = 220.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Moon Ants never aggro players."))
	float AntAggroRadius = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Moon Ants never aggro players."))
	float AntLoseAggroRadius = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Moon Ants do not react to player proximity."))
	float AntStopDistanceFromPlayer = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use AntRoamRadius."))
	float AntWanderRadius = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use AntRoamRetargetIntervalMin."))
	float AntWanderRetargetIntervalMin = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use AntRoamRetargetIntervalMax."))
	float AntWanderRetargetIntervalMax = 3.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use AntSurfaceDurationMax."))
	float AntLifetime = 40.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Ground", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntGroundTraceStartHeight = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Ground", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float AntGroundTraceDistance = 3000.0f;

	/** Serialized compatibility only; no Ant runtime code reads this counter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use AntMaxHealth on AJTSRoachActor."))
	int32 AntPunchHitsToKill = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 AntNestPunchHitsToDestroy = 3;

	/** Optional class for the legacy runtime actor. It shares its implementation with streamed MoonSurface. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Runtime", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSMoonSurfaceController> MoonSurfaceControllerClass;

	TWeakObjectPtr<AJTSMoonSurfaceController> MoonSurfaceController;
};
