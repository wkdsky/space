// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "space/World/JTSMoonResourceSpawner.h"

#include "JTSMoonGameMode.generated.h"

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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void InitializeMoonResources();
	void ConsumeExpeditionSupplies();
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

	FTimerHandle ExpeditionConsumptionTimerHandle;
	FTimerHandle MoonResourceInitializationTimerHandle;
	double FoodConsumptionAccumulator = 0.0;
	double WaterConsumptionAccumulator = 0.0;
	bool bMissingSpacecraftLogged = false;
};
