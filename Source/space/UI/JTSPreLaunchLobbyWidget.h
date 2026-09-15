// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "JTSPreLaunchLobbyWidget.generated.h"

class AJTSPlayerState;
class UBorder;
class UButton;
class UCanvasPanel;
class UJTSPlayerCardWidget;
class UJTSPlayerCustomizeWidget;
class UTextBlock;

/**
 * Pre-launch-only roster UI. All live state comes from replicated GameState/PlayerState; UI forwards
 * intent through AJTSPlayerController and never mutates lobby authority directly.
 */
UCLASS(Config = Game)
class SPACE_API UJTSPreLaunchLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenPlayerCustomization();
	void ClosePlayerCustomization();
	void OpenPlayerContextMenu(AJTSPlayerState* TargetPlayerState);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildWidgetTree();
	void Refresh();
	void SetContextPanelVisible(bool bVisible);
	void SetHostOptionsVisible(bool bVisible);
	UFUNCTION() void ToggleReady();
	UFUNCTION() void LaunchExpedition();
	UFUNCTION() void LeaveLobby();
	UFUNCTION() void CopyJoinCode();
	UFUNCTION() void ToggleInputMute();
	UFUNCTION() void OpenHostOptions();
	UFUNCTION() void CloseHostOptions();
	UFUNCTION() void CloseJoining();
	UFUNCTION() void ToggleTargetMute();
	UFUNCTION() void KickTarget();
	UFUNCTION() void CloseContextMenu();
	TSubclassOf<UJTSPlayerCardWidget> ResolvePlayerCardClass() const;
	TSubclassOf<UJTSPlayerCustomizeWidget> ResolvePlayerCustomizeClass() const;

	/** Blueprint presentation hooks; blank values fall back to the native complete layout. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "UI") TSoftClassPtr<UJTSPlayerCardWidget> PlayerCardWidgetClass;
	UPROPERTY(EditDefaultsOnly, Config, Category = "UI") TSoftClassPtr<UJTSPlayerCustomizeWidget> PlayerCustomizeWidgetClass;

	float RefreshAccumulator = 0.0f;
	TWeakObjectPtr<AJTSPlayerState> ContextTarget;

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> ModalCanvas;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> JoinCodeText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PlayerCountText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> VisibilityText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LobbyStatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ReadyButtonText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LaunchButtonText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MicButtonText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ContextTargetText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ContextMuteText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HostOptionsStatusText;
	UPROPERTY(Transient) TObjectPtr<UButton> ReadyButton;
	UPROPERTY(Transient) TObjectPtr<UButton> LaunchButton;
	UPROPERTY(Transient) TObjectPtr<UButton> MicButton;
	UPROPERTY(Transient) TObjectPtr<UButton> HostOptionsButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CloseJoiningButton;
	UPROPERTY(Transient) TObjectPtr<UBorder> ContextPanel;
	UPROPERTY(Transient) TObjectPtr<UBorder> HostOptionsPanel;
	UPROPERTY(Transient) TObjectPtr<UJTSPlayerCustomizeWidget> CustomizeWidget;
	UPROPERTY(Transient) TArray<TObjectPtr<UJTSPlayerCardWidget>> PlayerCards;
};
