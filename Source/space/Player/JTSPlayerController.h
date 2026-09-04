// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "space/Core/JTSGameState.h"

#include "JTSPlayerController.generated.h"

/**
 * Native player controller for third-person Jump to Space gameplay.
 */
UCLASS()
class SPACE_API AJTSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AJTSPlayerController();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void StartGame();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void RestartCurrentLevel();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void QuitGame();

	/** Applies the unpaused game-only input mode used by Earth collection. */
	void ApplyEarthCollectionInputMode();

protected:
	virtual void BeginPlay() override;
	virtual void BeginPlayingState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindGameState();
	void ApplyInputModeForPhase(EJTSGameplayPhase GameplayPhase);

	UFUNCTION()
	void HandleGameplayPhaseChanged(EJTSGameplayPhase NewGameplayPhase);

	TWeakObjectPtr<AJTSGameState> BoundGameState;
};
