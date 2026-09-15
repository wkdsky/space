// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "space/Core/JTSExpeditionTypes.h"

#include "JTSPlayerCustomizeWidget.generated.h"

class UButton;
class UTextBlock;
class UJTSPreLaunchLobbyWidget;

/** Modal player setup shell. Colour is live today; the category layout reserves the rest for future avatar data. */
UCLASS()
class SPACE_API UJTSPlayerCustomizeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLobbyOwner(UJTSPreLaunchLobbyWidget* InOwner);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;

private:
	void BuildWidgetTree();
	void SelectColor(EJTSAvatarColor NewColor);
	UFUNCTION() void SelectBlue();
	UFUNCTION() void SelectOrange();
	UFUNCTION() void SelectGreen();
	UFUNCTION() void SelectPurple();
	UFUNCTION() void Close();

	TWeakObjectPtr<UJTSPreLaunchLobbyWidget> LobbyOwner;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CurrentColorText;
};
