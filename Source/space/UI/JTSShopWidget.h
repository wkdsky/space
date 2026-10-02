// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "space/Items/JTSItemTypes.h"
#include "space/Player/JTSPlayerProgressionTypes.h"

#include "JTSShopWidget.generated.h"

class AJTSSpacecraftActor;
class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;
class UUniformGridPanel;
class UImage;
class USizeBox;
class UTexture2D;
class SWidget;

/**
 * Ship terminal presentation for the shop and player abilities. Gameplay and economy state remain
 * on the authoritative spacecraft and PlayerState; this widget only submits player intent.
 */
UCLASS()
class SPACE_API UJTSShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	bool OpenForSpacecraft(AJTSSpacecraftActor* Spacecraft);
	void CloseShop();
	bool IsShopOpen() const;
	void NotifyPurchaseResult(EJTSShopPurchaseResult Result);
	void NotifyStellarRollResult(EJTSStellarRollResult Result, FName ItemId, int32 SlotIndex);
	void NotifyShipLockerActionResult(bool bSucceeded, bool bTakeAction);
	void NotifyShipLockerExchangeResult(bool bSucceeded);
	void NotifyCarriedItemActionResult(bool bSucceeded, bool bStored);
	void UpdateCarriedDragPreview(const FVector2D& ScreenPosition, const FString& ItemLabel, bool bVisible);
	void HandleCarriedItemDrop(const FVector2D& ScreenPosition, int32 CarriedSlotIndex, FGuid ExpectedInstanceId);
	/** Result of the irreversible server-side ability confirmation. */
	void NotifyAbilityAllocationResult(bool bSucceeded);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void BuildWidgetTree();
	void RefreshAll();
	void RefreshCatalog();
	void RefreshShipLocker();
	void RefreshStellarReel();
	void RefreshStellarOddsTooltip();
	void SetStatus(const FString& Message, bool bError);
	void FinishStellarRoll();
	bool CanFinishStellarRoll() const;
	void SetLeverPull(float Distance);
	FString GetStellarItemLabel(FName ItemId) const;
	FString FormatStellarCosts() const;
	void RefreshAbilities();
	void RefreshPageVisibility();
	void ToggleShopPage();
	void ResetPendingAbilityAllocation();
	bool AdjustPendingAbility(EJTSPlayerAbility Ability, int32 Delta);
	int32 GetPendingAbilityRank(EJTSPlayerAbility Ability) const;
	FString BuildAbilityDescription(EJTSPlayerAbility Ability) const;
	/** Shows only resources used by the catalog and reports whether affordability may have changed. */
	bool RefreshWallet();
	bool CanAfford(EJTSItemId ItemId) const;
	FString FormatCosts(EJTSItemId ItemId) const;
	FString FormatMissingCosts(EJTSItemId ItemId) const;
	FText BuildItemTooltip(EJTSItemId ItemId) const;
	void RequestPurchase(EJTSItemId ItemId);

	UFUNCTION() void HandlePickaxeBuy();
	UFUNCTION() void HandleKnifeBuy();
	UFUNCTION() void HandlePistolBuy();
	UFUNCTION() void HandleMachineGunBuy();
	UFUNCTION() void HandleSniperBuy();
	UFUNCTION() void HandleWaistLampBuy();
	UFUNCTION() void HandleIceAxeBuy();
	UFUNCTION() void HandleDebugResourcesClicked();
	UFUNCTION() void HandleDebugLevelsClicked();
	UFUNCTION() void HandleSupplyTabClicked();
	UFUNCTION() void HandleAbilityTabClicked();
	UFUNCTION() void HandleStellarTabClicked();
	UFUNCTION() void HandleStellarRollClicked();
	UFUNCTION() void HandleTakeLockerItemClicked();
	UFUNCTION() void HandleInventorySlotsDecrease();
	UFUNCTION() void HandleInventorySlotsIncrease();
	UFUNCTION() void HandleStackLimitDecrease();
	UFUNCTION() void HandleStackLimitIncrease();
	UFUNCTION() void HandleRunSpeedDecrease();
	UFUNCTION() void HandleRunSpeedIncrease();
	UFUNCTION() void HandleStaminaDecrease();
	UFUNCTION() void HandleStaminaIncrease();
	UFUNCTION() void HandleCriticalChanceDecrease();
	UFUNCTION() void HandleCriticalChanceIncrease();
	UFUNCTION() void HandleConfirmAbilitiesClicked();
	UFUNCTION() void HandleResetAbilitiesClicked();
	UFUNCTION() void HandleCloseClicked();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient) TObjectPtr<UBorder> ShopFrame;
	UPROPERTY(Transient) TObjectPtr<UBorder> CatalogPanel;
	UPROPERTY(Transient) TObjectPtr<UBorder> AbilityPanel;
	UPROPERTY(Transient) TObjectPtr<UBorder> StellarPanel;
	UPROPERTY(Transient) TObjectPtr<UBorder> LockerPanel;
	UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> CatalogGrid;
	UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> LockerGrid;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LockerCountText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ReelPreviousText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ReelCurrentText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ReelNextText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ReelCostText;
	UPROPERTY(Transient) TObjectPtr<UBorder> LeverStem;
	UPROPERTY(Transient) TObjectPtr<UBorder> LeverKnob;
	UPROPERTY(Transient) TObjectPtr<UImage> TrashImage;
	/** The Widget Blueprint selects the project art; C++ only supplies the drop target behavior. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop|Presentation", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> TrashIcon;
	UPROPERTY(Transient) TObjectPtr<UBorder> DragGhost;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DragGhostText;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> LockerSlotBorders;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> LockerSlotTexts;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WalletText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AbilityProgressText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AbilityPointsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AbilityTabLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AbilityStatusText;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> AbilityDetailTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> AbilityRankTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> AbilityDecreaseButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> AbilityIncreaseButtons;
	UPROPERTY(Transient) TObjectPtr<UButton> CloseButton;
	UPROPERTY(Transient) TObjectPtr<UButton> SupplyTabButton;
	UPROPERTY(Transient) TObjectPtr<UButton> AbilityTabButton;
	UPROPERTY(Transient) TObjectPtr<UButton> StellarTabButton;
	UPROPERTY(Transient) TObjectPtr<UButton> StellarRollButton;
	UPROPERTY(Transient) TObjectPtr<UButton> TakeLockerItemButton;
	UPROPERTY(Transient) TObjectPtr<UButton> DebugResourcesButton;
	UPROPERTY(Transient) TObjectPtr<UButton> DebugLevelsButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ConfirmAbilitiesButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ResetAbilitiesButton;

	TWeakObjectPtr<AJTSSpacecraftActor> ActiveSpacecraft;
	TMap<EJTSResourceType, int32> LastDisplayedResourceAmounts;
	EJTSItemId LastRequestedItem = EJTSItemId::None;
	FJTSAbilityAllocation PendingAbilityAllocation;
	int32 ObservedProgressionRevision = INDEX_NONE;
	bool bShowingAbilityPage = false;
	bool bShowingStellarPage = false;
	bool bRollAnimating = false;
	bool bRollResultReceived = false;
	FName RolledStellarItemId;
	int32 RolledLockerSlotIndex = INDEX_NONE;
	TArray<int32> LockerVisualStates;
	int32 SelectedLockerSlot = INDEX_NONE;
	int32 DraggedLockerSlot = INDEX_NONE;
	FGuid DraggedLockerToken;
	bool bLeverDragging = false;
	float LeverGrabY = 0.0f;
	float LeverPull = 0.0f;
	float RollElapsed = 0.0f;
	float ReelStepAccumulator = 0.0f;
	int32 ReelDisplayIndex = 0;
	bool bAbilityCommitPending = false;
	bool bShopOpen = false;
	float RefreshAccumulator = 0.0f;
};
