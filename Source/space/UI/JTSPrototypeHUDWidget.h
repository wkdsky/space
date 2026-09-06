// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "space/Core/JTSGameState.h"
#include "space/Items/JTSResourceTypes.h"

#include "JTSPrototypeHUDWidget.generated.h"

class AJTSCharacter;
class AJTSSpacecraftActor;
class UBorder;
class UButton;
class UCanvasPanel;
class UCanvasPanelSlot;
class UJTSCircularProgressWidget;
class UTextBlock;
class UWidget;
class SWidget;

/** Native C++ presentation for the Earth Base prototype and its menus. */
UCLASS()
class SPACE_API UJTSPrototypeHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildWidgetTree();
	void BindGameState();
	void RefreshPhaseView(EJTSGameplayPhase NewGameplayPhase);
	void RefreshGameplayHud();
	void RefreshResultView(EJTSGameplayPhase NewGameplayPhase);
	void RefreshAvatarSelection();
	void RefreshBoardingProgress();
	void ApplyLayerVisibility(UWidget* Layer, bool bVisible);
	void SetBoardingProgressVisible(bool bVisible);

	UFUNCTION()
	void HandleGameplayPhaseChanged(EJTSGameplayPhase NewGameplayPhase);

	UFUNCTION()
	void HandleStartMissionClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleBackSettingsClicked();

	UFUNCTION()
	void HandleBlueAvatarClicked();

	UFUNCTION()
	void HandleOrangeAvatarClicked();

	UFUNCTION()
	void HandleGreenAvatarClicked();

	UFUNCTION()
	void HandleRestartClicked();

	UFUNCTION()
	void HandleQuitClicked();

	AJTSSpacecraftActor* FindSpacecraft() const;
	AJTSCharacter* FindPlayerCharacter() const;
	static FString ResourceTypeToString(EJTSResourceType ResourceType);
	static FString FormatRemainingTime(float RemainingSeconds);

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> StartMenuLayer;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> SettingsLayer;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> GameplayLayer;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> LaunchingLayer;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> ResultLayer;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ResultBackground;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TimeText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CarryText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HoldingText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> EarthPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ShipResourcesText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> AvatarBlock;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RocketIconCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> RocketBody;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> RocketFlame;

	UPROPERTY(Transient)
	TObjectPtr<UJTSCircularProgressWidget> BoardingProgressWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BoardingRemainingText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BoardingLabelText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BlueAvatarButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> OrangeAvatarButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> GreenAvatarButton;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> SettingsPreviewBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultSubtitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultDetailText;

	TWeakObjectPtr<AJTSGameState> BoundGameState;
	EJTSGameplayPhase CachedGameplayPhase = EJTSGameplayPhase::WaitingToStart;
	bool bSettingsVisible = false;
};
