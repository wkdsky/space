// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "JTSJoinExpeditionWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

/** Join/search form. It exposes session details and never treats a typed address as a trusted connection. */
UCLASS()
class SPACE_API UJTSJoinExpeditionWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();
	UFUNCTION() void Search();
	UFUNCTION() void JoinSelectedListing();
	UFUNCTION() void RefreshListings();
	UFUNCTION() void ReturnToMain();
	UFUNCTION() void HandleOperationFinished(bool bSucceeded, const FString& Message);
	UFUNCTION() void SelectListingOne();
	UFUNCTION() void SelectListingTwo();
	UFUNCTION() void SelectListingThree();
	UFUNCTION() void SelectListingFour();
	void SelectListing(int32 Index);

	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> JoinCodeBox;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> PasswordBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ListingText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> ListingButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ListingButtonTexts;
	int32 SelectedListingIndex = INDEX_NONE;
};
