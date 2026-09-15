// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "space/Core/JTSExpeditionTypes.h"

#include "JTSGameState.generated.h"

class AJTSSpacecraftActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJTSGameplayPhaseChanged, EJTSGameplayPhase, NewGameplayPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJTSEarthCollectionTimeChanged, float, RemainingTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJTSExpeditionStateChanged);

/** Globally replicated game state. GameMode is the only writer during a live session. */
UCLASS()
class SPACE_API AJTSGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AJTSGameState();

	UFUNCTION(BlueprintPure, Category = "Gameplay")
	EJTSGameplayPhase GetGameplayPhase() const;

	UFUNCTION(BlueprintPure, Category = "Gameplay|Earth Collection")
	float GetEarthCollectionRemainingTime() const;

	/** Server-configured launch fuel target replicated for every Earth HUD. */
	UFUNCTION(BlueprintPure, Category = "Gameplay|Earth Collection")
	float GetEarthLaunchFuelRequirement() const { return EarthLaunchFuelRequirement; }

	UFUNCTION(BlueprintPure, Category = "Gameplay|Earth Collection")
	bool IsEarthCollectionActive() const;

	UFUNCTION(BlueprintPure, Category = "Gameplay")
	bool IsWaitingToStart() const;

	/** Replicated lobby admission state. A closed lobby remains visible to its current players. */
	UFUNCTION(BlueprintPure, Category = "Multiplayer")
	bool IsAcceptingNewPlayers() const { return bAcceptingNewPlayers; }

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

	UFUNCTION(BlueprintPure, Category = "Gameplay|Flight")
	bool IsSpaceFlight() const;

	UFUNCTION(BlueprintPure, Category = "Gameplay|Outcome")
	bool IsSuccessfulOutcome() const;

	UFUNCTION(BlueprintPure, Category = "Gameplay|Outcome")
	EJTSFailureReason GetFailureReason() const;

	UFUNCTION(BlueprintPure, Category = "Expedition")
	AJTSSpacecraftActor* GetActiveSpacecraft() const { return ActiveSpacecraft; }

	UFUNCTION(BlueprintPure, Category = "Expedition")
	FString GetCurrentPlanetId() const { return CurrentPlanetId; }

	UFUNCTION(BlueprintPure, Category = "Expedition")
	FString GetExpeditionId() const { return ExpeditionId; }

	/** Uses replicated server time on clients, avoiding a local timer as source of truth. */
	UFUNCTION(BlueprintPure, Category = "Gameplay")
	double GetSynchronizedServerTimeSeconds() const;

	/** Server-only mutation entry points used by the gameplay GameModes and surface controller. */
	void SetGameplayPhase(EJTSGameplayPhase NewGameplayPhase);
	void SetAcceptingNewPlayers(bool bNewAcceptingNewPlayers);
	void SetFailureReason(EJTSFailureReason NewFailureReason);
	void SetEarthCollectionEndTime(double NewEndTimeSeconds);
	void SetEarthCollectionRemainingTime(float NewRemainingTime);
	void SetEarthLaunchFuelRequirement(float NewRequirement);
	void SetActiveSpacecraft(AJTSSpacecraftActor* NewSpacecraft);
	void SetCurrentPlanetId(const FString& NewPlanetId);
	void SetExpeditionId(const FString& NewExpeditionId);

	UPROPERTY(BlueprintAssignable, Category = "Gameplay")
	FOnJTSGameplayPhaseChanged OnGameplayPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Gameplay|Earth Collection")
	FOnJTSEarthCollectionTimeChanged OnEarthCollectionTimeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Expedition")
	FOnJTSExpeditionStateChanged OnExpeditionStateChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void RefreshCachedRemainingTime();

	UFUNCTION()
	void OnRep_GameplayPhase();

	UFUNCTION()
	void OnRep_EarthCollectionDeadline();

	UFUNCTION()
	void OnRep_ExpeditionState();

	UPROPERTY(ReplicatedUsing = OnRep_GameplayPhase, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gameplay", meta = (AllowPrivateAccess = "true"))
	EJTSGameplayPhase GameplayPhase = EJTSGameplayPhase::WaitingToStart;

	UPROPERTY(ReplicatedUsing = OnRep_ExpeditionState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Multiplayer", meta = (AllowPrivateAccess = "true"))
	bool bAcceptingNewPlayers = true;

	UPROPERTY(ReplicatedUsing = OnRep_GameplayPhase, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gameplay|Outcome", meta = (AllowPrivateAccess = "true"))
	EJTSFailureReason FailureReason = EJTSFailureReason::None;

	UPROPERTY(ReplicatedUsing = OnRep_EarthCollectionDeadline, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gameplay|Earth Collection", meta = (AllowPrivateAccess = "true"))
	double EarthCollectionEndTimeSeconds = 0.0;

	UPROPERTY(ReplicatedUsing = OnRep_EarthCollectionDeadline, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gameplay|Earth Collection", meta = (AllowPrivateAccess = "true"))
	float EarthCollectionRemainingTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_ExpeditionState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gameplay|Earth Collection", meta = (AllowPrivateAccess = "true"))
	float EarthLaunchFuelRequirement = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_ExpeditionState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Expedition", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSSpacecraftActor> ActiveSpacecraft;

	UPROPERTY(ReplicatedUsing = OnRep_ExpeditionState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Expedition", meta = (AllowPrivateAccess = "true"))
	FString CurrentPlanetId;

	UPROPERTY(ReplicatedUsing = OnRep_ExpeditionState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Expedition", meta = (AllowPrivateAccess = "true"))
	FString ExpeditionId;
};
