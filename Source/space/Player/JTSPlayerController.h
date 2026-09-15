// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "space/Core/JTSGameState.h"

#include "JTSPlayerController.generated.h"

class AJTSCharacter;
class AJTSPlayerState;
class AJTSSpacecraftActor;
class UJTSPreLaunchLobbyWidget;
enum class EJTSEquipmentType : uint8;
struct FInputKeyEventArgs;

/**
 * Native player controller for third-person Jump to Space gameplay.
 */
UCLASS(Config = Game)
class SPACE_API AJTSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AJTSPlayerController();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void StartGame();

	/** Toggles this local player's ready state through a validated server RPC. */
	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Lobby")
	void RequestSetReady();

	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Lobby")
	void RequestAvatarColor(EJTSAvatarColor NewColor);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Lobby")
	void RequestCloseJoining();

	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Lobby")
	void RequestKickPlayer(AJTSPlayerState* TargetPlayerState);

	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bReady);

	UFUNCTION(Server, Reliable)
	void ServerSetAvatarColor(EJTSAvatarColor NewColor);

	UFUNCTION(Server, Reliable)
	void ServerRequestStartExpedition();

	UFUNCTION(Server, Reliable)
	void ServerRequestCloseJoining();

	UFUNCTION(Server, Reliable)
	void ServerRequestKickPlayer(AJTSPlayerState* TargetPlayerState);

	/** Server notification used when a listen host terminates the expedition; no host migration is attempted. */
	UFUNCTION(Client, Reliable)
	void ClientHostLeftExpedition();

	UFUNCTION(Server, Reliable)
	void ServerRequestBoardSpacecraft(AJTSSpacecraftActor* Spacecraft);

	UFUNCTION(Server, Reliable)
	void ServerRequestDisembarkSpacecraft(AJTSSpacecraftActor* Spacecraft);

	UFUNCTION(Server, Reliable)
	void ServerRequestCraft(EJTSEquipmentType EquipmentType, AJTSSpacecraftActor* Spacecraft);

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
	virtual void OnRep_Pawn() override;
	virtual void ClientRestart_Implementation(APawn* NewPawn) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

private:
	void BindGameState();
	void RetryBindGameState();
	void ScheduleGameStateBind();
	void RefreshGameplayInputAfterPossess();
	void ApplyInputModeForPhase(EJTSGameplayPhase GameplayPhase);
	void ApplyPreLaunchLobbyInputMode();
	bool IsNormalGameplayPhase() const;
	bool IsPreLaunchLobbyWorld() const;
	void ShowLobby();
	void HideLobby();

	UFUNCTION()
	void HandleGameplayPhaseChanged(EJTSGameplayPhase NewGameplayPhase);

	TWeakObjectPtr<AJTSGameState> BoundGameState;

	UPROPERTY(Transient)
	TObjectPtr<UJTSPreLaunchLobbyWidget> LobbyWidget;

	/** Blueprint-owned lobby composition. Native widget remains a recoverable fallback. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSoftClassPtr<UJTSPreLaunchLobbyWidget> PreLaunchLobbyWidgetClass;

	FTimerHandle GameStateBindingRetryTimer;
	int32 GameStateBindingRetryCount = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spacecraft|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float SpacecraftCameraBlendTime = 0.35f;

	/** Off by default: moving the mouse up looks/steers up, matching the usual first/third-person convention. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input", meta = (AllowPrivateAccess = "true"))
	bool bInvertLookYAxis = false;
};
