// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"

#include "JTSGameState.generated.h"

class AJTSEarthGameMode;
class AJTSMoonGameMode;

/** The gameplay phase currently active in the level. */
UENUM(BlueprintType)
enum class EJTSGameplayPhase : uint8
{
	EarthCollection = 0 UMETA(DisplayName = "Earth Collection"),
	EarthCollectionFinished = 1 UMETA(DisplayName = "Earth Collection Finished"),
	Launching = 2 UMETA(DisplayName = "Launching"),
	EarthCaptureFailure = 3 UMETA(DisplayName = "Earth Capture Failure"),
	MoonArrivalSuccess = 4 UMETA(DisplayName = "Moon Arrival Success"),
	WaitingToStart = 5 UMETA(DisplayName = "Waiting To Start"),
	MoonExploration = 6 UMETA(DisplayName = "Moon Exploration")
};

UENUM(BlueprintType)
enum class EJTSFailureReason : uint8
{
	None UMETA(DisplayName = "None"),
	NoTimelyBoarding UMETA(DisplayName = "No Timely Boarding"),
	InsufficientFuel UMETA(DisplayName = "Insufficient Fuel"),
	NoSpacecraft UMETA(DisplayName = "No Spacecraft"),
	InvalidGameInstance UMETA(DisplayName = "Invalid Game Instance")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJTSGameplayPhaseChanged, EJTSGameplayPhase, NewGameplayPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJTSEarthCollectionTimeChanged, float, RemainingTime);

/**
 * Stores gameplay phase data for the current level and exposes it to future presentation systems.
 */
UCLASS()
class SPACE_API AJTSGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AJTSGameState();

	/** Returns the gameplay phase currently active in this world. */
	UFUNCTION(BlueprintPure, Category = "Gameplay")
	EJTSGameplayPhase GetGameplayPhase() const;

	/** Returns the remaining duration of the Earth resource collection phase, in seconds. */
	UFUNCTION(BlueprintPure, Category = "Gameplay|Earth Collection")
	float GetEarthCollectionRemainingTime() const;

	/** Returns whether Earth resources may currently be collected. */
	UFUNCTION(BlueprintPure, Category = "Gameplay|Earth Collection")
	bool IsEarthCollectionActive() const;

	UFUNCTION(BlueprintPure, Category = "Gameplay")
	bool IsWaitingToStart() const;

	/** Returns whether the Earth resource collection phase has ended. */
	UFUNCTION(BlueprintPure, Category = "Gameplay|Earth Collection")
	bool IsEarthCollectionFinished() const;

	UFUNCTION(BlueprintPure, Category = "Gameplay|Launch")
	bool IsLaunching() const;

	UFUNCTION(BlueprintPure, Category = "Gameplay|Outcome")
	bool IsEarthCaptureFailure() const;

	UFUNCTION(BlueprintPure, Category = "Gameplay|Outcome")
	bool IsMoonArrivalSuccess() const;

	UFUNCTION(BlueprintPure, Category = "Gameplay|Moon")
	bool IsMoonExploration() const;

	UFUNCTION(BlueprintPure, Category = "Gameplay|Outcome")
	bool IsSuccessfulOutcome() const;

	UFUNCTION(BlueprintPure, Category = "Gameplay|Outcome")
	EJTSFailureReason GetFailureReason() const;

	/** Broadcast when the active gameplay phase changes. */
	UPROPERTY(BlueprintAssignable, Category = "Gameplay")
	FOnJTSGameplayPhaseChanged OnGameplayPhaseChanged;

	/** Broadcast when the remaining Earth collection time changes. */
	UPROPERTY(BlueprintAssignable, Category = "Gameplay|Earth Collection")
	FOnJTSEarthCollectionTimeChanged OnEarthCollectionTimeChanged;

private:
	friend class AJTSEarthGameMode;
	friend class AJTSMoonGameMode;

	void SetGameplayPhase(EJTSGameplayPhase NewGameplayPhase);
	void SetFailureReason(EJTSFailureReason NewFailureReason);
	void SetEarthCollectionEndTime(double NewEndTimeSeconds);
	void SetEarthCollectionRemainingTime(float NewRemainingTime);
	void RefreshCachedRemainingTime();

	/** The current authoritative phase for this level. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gameplay", meta = (AllowPrivateAccess = "true"))
	EJTSGameplayPhase GameplayPhase = EJTSGameplayPhase::WaitingToStart;

	/** Minimal reason used by the result presentation for a failed attempt. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gameplay|Outcome", meta = (AllowPrivateAccess = "true"))
	EJTSFailureReason FailureReason = EJTSFailureReason::None;

	/** Absolute world-game-time deadline for the Earth collection phase. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gameplay|Earth Collection", meta = (AllowPrivateAccess = "true"))
	double EarthCollectionEndTimeSeconds = 0.0;

	/** The remaining Earth collection time, clamped to zero after the phase ends. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gameplay|Earth Collection", meta = (AllowPrivateAccess = "true"))
	float EarthCollectionRemainingTime = 0.0f;
};
