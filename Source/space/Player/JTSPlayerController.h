// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "space/Core/JTSGameState.h"

#include "JTSPlayerController.generated.h"

class AJTSCharacter;
struct FInputKeyEventArgs;

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

	/** Returns to the configured project GameDefaultMap, which contains the existing start flow. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ReturnToMainMenu();

	/** Applies the unpaused game-only input mode used by Earth collection. */
	void ApplyEarthCollectionInputMode();

	/** Opens the local Moon workshop and switches input to a click-capable modal mode. */
	void OpenMoonShop(AJTSCharacter* InPlayer);
	void CloseMoonShop();
	bool IsMoonShopOpen() const;

	/** Opens the gameplay pause menu and applies paused UI-only input. */
	void OpenGameMenu();
	void CloseGameMenu();
	bool IsGameMenuOpen() const;

protected:
	virtual void BeginPlay() override;
	virtual void BeginPlayingState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

private:
	void BindGameState();
	void ApplyInputModeForPhase(EJTSGameplayPhase GameplayPhase);
	bool IsNormalGameplayPhase() const;

	UFUNCTION()
	void HandleGameplayPhaseChanged(EJTSGameplayPhase NewGameplayPhase);

	TWeakObjectPtr<AJTSGameState> BoundGameState;
};
