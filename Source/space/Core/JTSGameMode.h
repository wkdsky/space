// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"

#include "JTSGameMode.generated.h"

class AJTSGameState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJTSEarthCollectionFinished);

/**
 * Native default ruleset for Jump to Space.
 */
UCLASS()
class SPACE_API AJTSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJTSGameMode();

	/** Returns the configured duration of the Earth resource collection phase. */
	UFUNCTION(BlueprintPure, Category = "Gameplay|Earth Collection")
	float GetEarthCollectionDuration() const;

	/** Starts the one-time Earth resource collection phase for this level. */
	UFUNCTION(BlueprintCallable, Category = "Gameplay|Earth Collection")
	void StartEarthCollection();

	/** Returns whether the Earth resource collection phase is active. */
	UFUNCTION(BlueprintPure, Category = "Gameplay|Earth Collection")
	bool IsEarthCollectionActive() const;

	UFUNCTION(BlueprintPure, Category = "Flight")
	float GetMinimumFuelRequired() const;

	UFUNCTION(BlueprintPure, Category = "Flight")
	float GetLaunchSequenceDuration() const;

	/** Ends the Earth resource collection phase without changing player or spacecraft inventories. */
	UFUNCTION(BlueprintCallable, Category = "Gameplay|Earth Collection")
	void FinishEarthCollection();

	UFUNCTION(BlueprintCallable, Category = "Flight")
	void ResolveLaunchOutcome();

	/** Broadcast once when the Earth resource collection phase ends. */
	UPROPERTY(BlueprintAssignable, Category = "Gameplay|Earth Collection")
	FOnJTSEarthCollectionFinished OnEarthCollectionFinished;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void StartLaunchSequence();
	AJTSGameState* GetJTSGameState() const;

	/** Configurable Earth resource collection duration, in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay|Earth Collection", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float EarthCollectionDuration = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MinimumFuelRequired = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float LaunchSequenceDuration = 2.0f;

	FTimerHandle EarthCollectionTimerHandle;
	FTimerHandle LaunchSequenceTimerHandle;

	bool bEarthCollectionStarted = false;
	bool bEarthCollectionFinished = false;
	bool bLaunchSequenceStarted = false;
	bool bLaunchOutcomeResolved = false;
};
