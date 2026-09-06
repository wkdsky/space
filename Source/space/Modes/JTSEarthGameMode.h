// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"

#include "JTSEarthGameMode.generated.h"

class AJTSGameState;
class UWorld;

/** Earth resource placement values owned by the Earth chapter ruleset. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSEarthResourceSpawnSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth|Resources", meta = (ClampMin = "0", UIMin = "0"))
	int32 FuelPickupCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth|Resources", meta = (ClampMin = "0", UIMin = "0"))
	int32 WaterPickupCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth|Resources", meta = (ClampMin = "0", UIMin = "0"))
	int32 FoodPickupCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth|Resources", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpawnHalfExtentX = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth|Resources", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpawnHalfExtentY = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth|Resources", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinimumPickupSpacing = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth|Resources", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float PlayerExclusionRadius = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth|Resources", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpacecraftExclusionRadius = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth|Resources", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EdgePadding = 100.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJTSEarthCollectionFinished);

/**
 * Ruleset for the Earth launch prototype.
 */
UCLASS()
class SPACE_API AJTSEarthGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJTSEarthGameMode();

	/** Returns the configured duration of the Earth resource collection phase. */
	UFUNCTION(BlueprintPure, Category = "Earth|Collection")
	float GetEarthCollectionDuration() const;

	/** Starts the one-time Earth resource collection phase for this level. */
	UFUNCTION(BlueprintCallable, Category = "Earth|Collection")
	void StartEarthCollection();

	/** Returns whether the Earth resource collection phase is active. */
	UFUNCTION(BlueprintPure, Category = "Earth|Collection")
	bool IsEarthCollectionActive() const;

	UFUNCTION(BlueprintPure, Category = "Earth|Flight")
	float GetMinimumFuelRequired() const;

	UFUNCTION(BlueprintPure, Category = "Earth|Flight")
	float GetLaunchSequenceDuration() const;

	/** Returns the wait time before the successful Earth launch travels to the configured Moon level. */
	UFUNCTION(BlueprintPure, Category = "Earth|Transition")
	float GetMoonTransitionDelay() const;

	/** Ends the Earth resource collection phase without changing player or spacecraft inventories. */
	UFUNCTION(BlueprintCallable, Category = "Earth|Collection")
	void FinishEarthCollection();

	UFUNCTION(BlueprintCallable, Category = "Earth|Flight")
	void ResolveLaunchOutcome();

	/** Broadcast once when the Earth resource collection phase ends. */
	UPROPERTY(BlueprintAssignable, Category = "Earth|Collection")
	FOnJTSEarthCollectionFinished OnEarthCollectionFinished;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void StartLaunchSequence();
	void BeginMoonTravel();
	void TravelToMoon();
	bool ResolveMoonLevelPackageName(FString& OutPackageName) const;
	AJTSGameState* GetJTSGameState() const;

	/** Configurable Earth resource collection duration, in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Earth|Collection", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float EarthCollectionDuration = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Earth|Flight", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", Delta = "1.0"))
	float MinimumFuelRequired = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Earth|Flight", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float LaunchSequenceDuration = 2.0f;

	/** Moon map selected by the Earth GameMode Blueprint. This remains unset in native C++ on purpose. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Earth|Transition", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UWorld> MoonLevel;

	/** Delay shown by the Moon arrival transition UI before normal level travel begins. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Earth|Transition", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MoonTransitionDelay = 2.0f;

	/** Settings applied to the Earth level's JTSResourceSpawnArea before it generates pickups. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Earth|Resources", meta = (AllowPrivateAccess = "true", ShowOnlyInnerProperties))
	FJTSEarthResourceSpawnSettings ResourceSpawnSettings;

	FTimerHandle EarthCollectionTimerHandle;
	FTimerHandle LaunchSequenceTimerHandle;
	FTimerHandle MoonTransitionTimerHandle;

	bool bEarthCollectionStarted = false;
	bool bEarthCollectionFinished = false;
	bool bLaunchSequenceStarted = false;
	bool bLaunchOutcomeResolved = false;
	bool bMoonTravelScheduled = false;
};
