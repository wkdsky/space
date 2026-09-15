// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "JTSSessionDetailsWidget.generated.h"

class UButton;
class UTextBlock;

/** Small reusable details/copy-code panel for the lobby. */
UCLASS()
class SPACE_API UJTSSessionDetailsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetJoinCode(const FString& JoinCode);
	void SetSessionDetails(const FString& JoinCode, bool bPasswordProtected, int32 CurrentPlayers, int32 MaximumPlayers);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

private:
	void BuildWidgetTree();
	UFUNCTION() void CopyJoinCode();
	FString CurrentJoinCode;
	bool bCurrentPasswordProtected = false;
	int32 CurrentPlayerCount = 0;
	int32 CurrentMaximumPlayers = 4;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> JoinCodeText;
};
