// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "space/World/JTSMoonResourceSpawner.h"

#include "JTSMoonGameMode.generated.h"

class AJTSCharacter;
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

	/** Performs the Moon workshop's pickaxe transaction against the supplied player's nearby ship. */
	UFUNCTION(BlueprintCallable, Category = "Moon|Crafting")
	bool TryCraftPickaxe(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft);

	/** Performs the Moon workshop's backpack transaction against the supplied player's nearby ship. */
	UFUNCTION(BlueprintCallable, Category = "Moon|Crafting")
	bool TryCraftBackpack(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void InitializeMoonResources();
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

	FTimerHandle ExpeditionConsumptionTimerHandle;
	FTimerHandle MoonResourceInitializationTimerHandle;
	double FoodConsumptionAccumulator = 0.0;
	double WaterConsumptionAccumulator = 0.0;
	mutable TWeakObjectPtr<AJTSSpacecraftActor> CachedSpacecraft;
	bool bMissingSpacecraftLogged = false;
};
