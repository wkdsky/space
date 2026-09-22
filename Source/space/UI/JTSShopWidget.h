// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "space/Items/JTSItemTypes.h"

#include "JTSShopWidget.generated.h"

class AJTSSpacecraftActor;
class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;
class UWrapBox;
class SWidget;

/**
 * Compact ship-owned supply screen. Definitions supply names, costs and descriptions; this widget
 * only presents one aligned buy cell per item and never owns expedition economy state.
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
	void NotifyResourceSupplyResult(bool bSucceeded);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildWidgetTree();
	void RefreshAll();
	void RefreshCatalog();
	/** Updates the compact wallet and reports whether any catalog cost resource changed. */
	bool RefreshWallet();
	bool CanAfford(EJTSItemId ItemId) const;
	FString FormatCosts(EJTSItemId ItemId) const;
	FString FormatMissingCosts(EJTSItemId ItemId) const;
	FText BuildItemTooltip(EJTSItemId ItemId) const;
	void RequestPurchase(EJTSItemId ItemId);
	void RequestResourceSupply();

	UFUNCTION() void HandlePickaxeBuy();
	UFUNCTION() void HandleKnifeBuy();
	UFUNCTION() void HandlePistolBuy();
	UFUNCTION() void HandleMachineGunBuy();
	UFUNCTION() void HandleSniperBuy();
	UFUNCTION() void HandleBackpackBuy();
	UFUNCTION() void HandleResourceSupplyClicked();
	UFUNCTION() void HandleCloseClicked();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient) TObjectPtr<UBorder> ShopFrame;
	UPROPERTY(Transient) TObjectPtr<UWrapBox> CatalogWrap;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WalletText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UButton> CloseButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ResourceSupplyButton;

	TWeakObjectPtr<AJTSSpacecraftActor> ActiveSpacecraft;
	TMap<EJTSResourceType, int32> LastDisplayedResourceAmounts;
	EJTSItemId LastRequestedItem = EJTSItemId::None;
	bool bShopOpen = false;
	float RefreshAccumulator = 0.0f;
};
