// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Events.h"
#include "space/Items/JTSItemTypes.h"

#include "JTSInventoryQuantityDialogWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UEditableTextBox;
class UTextBlock;
class SWidget;

/**
 * Local modal quantity selector. It never mutates inventory locally: confirmation is sent to the
 * owning PlayerController, which asks the server to validate the current replicated stack again.
 */
UCLASS()
class SPACE_API UJTSInventoryQuantityDialogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	bool OpenForItem(int32 SlotIndex, const FJTSItemInstance& Item, bool bInDestroyMode);
	void CloseDialog();
	bool IsDialogOpen() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void BuildWidgetTree();
	void RefreshText();
	void AdjustQuantity(int32 Delta);
	void ConfirmSelection();

	UFUNCTION() void HandleDecreaseClicked();
	UFUNCTION() void HandleIncreaseClicked();
	UFUNCTION() void HandleConfirmClicked();
	UFUNCTION() void HandleCancelClicked();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient) TObjectPtr<UBorder> DialogFrame;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ItemText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HintText;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> QuantityInput;
	UPROPERTY(Transient) TObjectPtr<UButton> DecreaseButton;
	UPROPERTY(Transient) TObjectPtr<UButton> IncreaseButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ConfirmButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CancelButton;

	int32 ActiveSlotIndex = INDEX_NONE;
	int32 MaximumQuantity = 0;
	bool bDestroyMode = false;
	bool bDialogOpen = false;
	FJTSItemInstance ActiveItem;
};
