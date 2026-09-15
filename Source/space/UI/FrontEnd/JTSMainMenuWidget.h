// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "JTSMainMenuWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnJTSFrontEndPageRequested, FName);

/** Native front-end landing page. Art styling remains Blueprint-overridable. */
UCLASS()
class SPACE_API UJTSMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FOnJTSFrontEndPageRequested OnPageRequested;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();
	UFUNCTION()
	void RequestPlay();
	UFUNCTION()
	void RequestJoin();
	UFUNCTION()
	void RequestSettings();
	UFUNCTION()
	void RequestCredits();
	UFUNCTION()
	void RequestQuit();
	UFUNCTION()
	void HandleOperationFinished(bool bSucceeded, const FString& Message);

	UPROPERTY(Transient)
	TObjectPtr<UButton> PlayButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> JoinButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> SettingsButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> QuitButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> CreditsButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
};
