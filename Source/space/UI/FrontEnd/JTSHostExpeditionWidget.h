// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "JTSHostExpeditionWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

/** Host form; it only delegates to UJTSOnlineSessionSubsystem. */
UCLASS()
class SPACE_API UJTSHostExpeditionWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();
	UFUNCTION()
	void CreateExpedition();
	UFUNCTION()
	void ReturnToMain();
	UFUNCTION()
	void HandleOperationFinished(bool bSucceeded, const FString& Message);

	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> JoinCodeBox;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> PasswordBox;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> PlayerCountBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UButton> CreateButton;
};
