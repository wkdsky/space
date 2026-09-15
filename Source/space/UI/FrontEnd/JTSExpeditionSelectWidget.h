// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "JTSExpeditionSelectWidget.generated.h"

class UButton;
class UTextBlock;

/** Four-slot host-owned save selection; clients never see or mutate another host's saves. */
UCLASS()
class SPACE_API UJTSExpeditionSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void RefreshSlots();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();
	void ChooseSlot(int32 SlotIndex);
	UFUNCTION() void ChooseSlotOne();
	UFUNCTION() void ChooseSlotTwo();
	UFUNCTION() void ChooseSlotThree();
	UFUNCTION() void ChooseSlotFour();
	UFUNCTION() void Back();
	UFUNCTION() void HandleOperationFinished(bool bSucceeded, const FString& Message);

	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> SlotTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> SlotButtons;
};
