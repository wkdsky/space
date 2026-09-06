// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "space/Components/JTSPlayerEquipmentComponent.h"
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
class UProgressBar;
class UTextBlock;
class UWidget;
class SWidget;

/** Native C++ presentation for the Earth Base prototype and its menus. */
UCLASS()
class SPACE_API UJTSPrototypeHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Opens the Moon-only workshop for the supplied local player when they are inside the ship range. */
	bool OpenMoonShop(AJTSCharacter* Player);
	void CloseMoonShop();
	bool IsMoonShopOpen() const;

	/** Controller-owned pause menu state. The widget only presents the menu and its actions. */
	void OpenGameMenu();
	void CloseGameMenu();
	bool IsGameMenuOpen() const;

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
	void RefreshEarthCollectionDurationText();
	void RefreshFuelToMoonHud();
	void RefreshInventorySlots();
	void RefreshEquipmentSlots();
	void RefreshInteractionPrompt();
	void RefreshMoonShop();
	void RefreshWorkshopTabs();
	void RefreshWorkshopLayout();
	void RefreshSpacecraftNavigation(AJTSSpacecraftActor* Spacecraft);
	void SetSpacecraftNavigationVisibility(bool bShowWorldMarker, bool bShowEdgeIndicator);
	void BuildWorkshopPanel();
	UBorder* BuildWorkshopItemCard(
		UCanvasPanel* Parent,
		const FName& CardName,
		const FString& ItemName,
		const FString& ItemCategory,
		const FString& Description,
		TObjectPtr<UTextBlock>& OutCostText,
		TObjectPtr<UButton>& OutBuyButton);
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

	UFUNCTION()
	void HandleWorkshopToolsTabClicked();

	UFUNCTION()
	void HandleWorkshopEquipmentTabClicked();

	UFUNCTION()
	void HandleBuyPickaxeClicked();

	UFUNCTION()
	void HandleBuyBackpackClicked();

	UFUNCTION()
	void HandleCloseMoonShopClicked();

	UFUNCTION()
	void HandleResumeGameClicked();

	UFUNCTION()
	void HandleReturnToMainMenuClicked();

	UFUNCTION()
	void HandleGameMenuQuitClicked();

	AJTSSpacecraftActor* FindSpacecraft() const;
	AJTSCharacter* FindPlayerCharacter() const;
	static FString ResourceTypeToString(EJTSResourceType ResourceType);
	static FString EquipmentTypeToString(EJTSEquipmentType EquipmentType);
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
	TObjectPtr<UCanvasPanel> PauseMenuLayer;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ResultBackground;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TimeText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StartRulesText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> FuelToMoonPanel;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> FuelProgressBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FuelAmountText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FuelStatusText;

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
	TObjectPtr<UBorder> InventoryPanel;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> InventoryPanelSlot;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InteractionPromptText;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> InteractionPromptSlot;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CrosshairText;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> SpacecraftWorldMarker;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpacecraftWorldMarkerText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpacecraftWorldMarkerDistanceText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpacecraftWorldMarkerArrowText;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> SpacecraftWorldMarkerSlot;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> SpacecraftEdgeIndicator;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> SpacecraftEdgeIndicatorSlot;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpacecraftEdgeArrowText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpacecraftEdgeDistanceText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GameplayHelpText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> InventorySlotTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> InventorySlotBorders;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> EquipmentPanel;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> EquipmentSlotBorders;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> EquipmentSlotTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> EquipmentSlotKeyTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UJTSCircularProgressWidget>> EquipmentHoldProgressWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> MoonShopPanel;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> MoonShopPanelSlot;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ShopPickaxeCard;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ShopBackpackCard;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> ShopPickaxeCardSlot;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> ShopBackpackCardSlot;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ShopPickaxeCostText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ShopPickaxeBuyButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ShopBackpackCostText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ShopBackpackBuyButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ShopToolsTabButton;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> ShopToolsTabSlot;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ShopEquipmentTabButton;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> ShopEquipmentTabSlot;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ShopCloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> ShopCloseButtonSlot;

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

	UPROPERTY(Transient)
	TObjectPtr<UButton> RestartButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> QuitButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ResumeGameButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ReturnToMainMenuButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> GameMenuQuitButton;

	TWeakObjectPtr<AJTSGameState> BoundGameState;
	TWeakObjectPtr<AJTSCharacter> ShopPlayer;
	TWeakObjectPtr<AJTSSpacecraftActor> ShopSpacecraft;
	mutable TWeakObjectPtr<AJTSSpacecraftActor> CachedSpacecraft;
	FIntPoint CachedWorkshopViewportSize = FIntPoint::ZeroValue;
	EJTSGameplayPhase CachedGameplayPhase = EJTSGameplayPhase::WaitingToStart;
	bool bSettingsVisible = false;
	bool bMoonShopOpen = false;
	bool bGameMenuOpen = false;
	bool bWorkshopEquipmentTab = false;
	bool bSpacecraftWasOnScreen = false;
};
