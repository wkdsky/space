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
	/** Result of the irreversible server-side ability confirmation. */
	void NotifyAbilityAllocationResult(bool bSucceeded);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildWidgetTree();
	void RefreshAll();
	void RefreshCatalog();
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
	UFUNCTION() void HandleDebugResourcesClicked();
	UFUNCTION() void HandleDebugLevelsClicked();
	UFUNCTION() void HandleSupplyTabClicked();
	UFUNCTION() void HandleAbilityTabClicked();
	UFUNCTION() void HandleInventorySlotsDecrease();
	UFUNCTION() void HandleInventorySlotsIncrease();
	UFUNCTION() void HandleStackLimitDecrease();
	UFUNCTION() void HandleStackLimitIncrease();
	UFUNCTION() void HandleRunSpeedDecrease();
	UFUNCTION() void HandleRunSpeedIncrease();
	UFUNCTION() void HandleConfirmAbilitiesClicked();
	UFUNCTION() void HandleResetAbilitiesClicked();
	UFUNCTION() void HandleCloseClicked();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient) TObjectPtr<UBorder> ShopFrame;
	UPROPERTY(Transient) TObjectPtr<UBorder> CatalogPanel;
	UPROPERTY(Transient) TObjectPtr<UBorder> AbilityPanel;
	UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> CatalogGrid;
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
	bool bAbilityCommitPending = false;
	bool bShopOpen = false;
	float RefreshAccumulator = 0.0f;
};
