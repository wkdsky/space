// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "space/Core/JTSGameState.h"

#include "JTSPlayerController.generated.h"

class AJTSCharacter;
class AJTSSpacecraftActor;
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

	/** Applies the equivalent unpaused game-only mode for the persistent space-world prototype. */
	void ApplySpaceWorldInputMode();

	/** Explicitly uses the possessed spacecraft's FlightCamera rather than retaining the character view target. */
	UFUNCTION(BlueprintCallable, Category = "Spacecraft|Camera")
	void SetSpacecraftCameraViewTarget(AJTSSpacecraftActor* Spacecraft);

	/** Restores the character's own camera after a grounded spacecraft disembark. */
	UFUNCTION(BlueprintCallable, Category = "Spacecraft|Camera")
	void RestoreCharacterCameraViewTarget(AJTSCharacter* CharacterPawn);

	/** Shared look preference used by character and spacecraft mouse-pitch input. */
	UFUNCTION(BlueprintPure, Category = "Player|Input")
	bool IsLookYAxisInverted() const;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spacecraft|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float SpacecraftCameraBlendTime = 0.35f;

	/** Off by default: moving the mouse up looks/steers up, matching the usual first/third-person convention. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input", meta = (AllowPrivateAccess = "true"))
	bool bInvertLookYAxis = false;
};
