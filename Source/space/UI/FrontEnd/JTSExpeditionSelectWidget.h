// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "JTSExpeditionSelectWidget.generated.h"

class UBorder;
class UButton;
class UOverlay;
class USizeBox;
class UTextBlock;

/** Four-slot host-owned save selection; clients never see or mutate another host's saves. */
UCLASS()
class SPACE_API UJTSExpeditionSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void RefreshSlots();
	/** Lets the parent front-end treat the delete confirmation as a real modal when Escape is pressed. */
	bool IsDeleteConfirmationOpen() const;
	void CancelPendingDeleteConfirmation();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();
	void ChooseSlot(int32 SlotIndex);
	void RequestDeleteSlot(int32 SlotIndex);
	void ShowDeleteConfirmation(int32 SlotIndex, const FString& DisplayName);
	void HideDeleteConfirmation();
	UFUNCTION() void ChooseSlotOne();
	UFUNCTION() void ChooseSlotTwo();
	UFUNCTION() void ChooseSlotThree();
	UFUNCTION() void ChooseSlotFour();
	UFUNCTION() void RequestDeleteSlotOne();
	UFUNCTION() void RequestDeleteSlotTwo();
	UFUNCTION() void RequestDeleteSlotThree();
	UFUNCTION() void RequestDeleteSlotFour();
	UFUNCTION() void ConfirmDelete();
	UFUNCTION() void CancelDelete();
	UFUNCTION() void Back();
	UFUNCTION() void HandleOperationFinished(bool bSucceeded, const FString& Message);

	UPROPERTY(Transient) TObjectPtr<UOverlay> RootOverlay;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> SlotTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> SlotButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> DeleteSlotButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<USizeBox>> DeleteSlotContainers;
	UPROPERTY(Transient) TObjectPtr<UButton> DeleteConfirmationBackdrop;
	UPROPERTY(Transient) TObjectPtr<UBorder> DeleteConfirmationPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DeleteConfirmationText;
	int32 PendingDeleteSlot = INDEX_NONE;
};
