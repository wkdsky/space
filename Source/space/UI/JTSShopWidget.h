// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "space/Items/JTSItemTypes.h"

#include "JTSShopWidget.generated.h"

class AJTSShopTerminalActor;
class UBorder;
class UButton;
class UCanvasPanel;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UWrapBox;
class SWidget;

/** Large native SpaceWorld shop screen. It stays data-driven through UJTSItemDefinition. */
UCLASS()
class SPACE_API UJTSShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	bool OpenForTerminal(AJTSShopTerminalActor* Terminal);
	void CloseShop();
	bool IsShopOpen() const;
	void NotifyPurchaseResult(EJTSShopPurchaseResult Result);
	void NotifyDepositResult(bool bSucceeded);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildWidgetTree();
	void RefreshAll();
	void RefreshCatalog();
	void RefreshDetail();
	void RefreshWallet();
	void SelectItem(EJTSItemId ItemId);
	void SelectCategory(EJTSShopCategory Category);
	bool IsItemVisible(EJTSItemId ItemId) const;
	FString FormatCosts(EJTSItemId ItemId) const;
	FString FormatTags(EJTSItemId ItemId) const;
	FString FormatOwnership(EJTSItemId ItemId) const;
	FString FormatStats(EJTSItemId ItemId) const;
	void RequestPurchase();
	void RequestDeposit();

	UFUNCTION() void HandleAllCategory();
	UFUNCTION() void HandleWeaponsCategory();
	UFUNCTION() void HandleMiningCategory();
	UFUNCTION() void HandleUtilityCategory();
	UFUNCTION() void HandleResourcesCategory();
	UFUNCTION() void HandleWearablesCategory();
	UFUNCTION() void HandlePickaxeSelected();
	UFUNCTION() void HandleKnifeSelected();
	UFUNCTION() void HandlePistolSelected();
	UFUNCTION() void HandleMachineGunSelected();
	UFUNCTION() void HandleBackpackSelected();
	UFUNCTION() void HandlePurchaseClicked();
	UFUNCTION() void HandleDepositClicked();
	UFUNCTION() void HandleCloseClicked();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient) TObjectPtr<UBorder> ShopFrame;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> CategoryList;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> CatalogScroll;
	UPROPERTY(Transient) TObjectPtr<UWrapBox> CatalogWrap;
	UPROPERTY(Transient) TObjectPtr<UBorder> DetailPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WalletText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailNameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailTagsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailDescriptionText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailStatsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailCostText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DeliveryHintText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UButton> PurchaseButton;
	UPROPERTY(Transient) TObjectPtr<UButton> DepositButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CloseButton;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> CategoryButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> CatalogButtons;

	TWeakObjectPtr<AJTSShopTerminalActor> ActiveTerminal;
	EJTSShopCategory ActiveCategory = EJTSShopCategory::All;
	EJTSItemId SelectedItemId = EJTSItemId::Pickaxe;
	bool bShopOpen = false;
	float RefreshAccumulator = 0.0f;
};
