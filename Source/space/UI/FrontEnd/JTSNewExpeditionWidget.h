// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "space/Core/JTSExpeditionTypes.h"

#include "JTSNewExpeditionWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

/** Host setup form with explicit labels; it prepares the selected save then delegates session creation to OnlineSubsystem. */
UCLASS()
class SPACE_API UJTSNewExpeditionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSaveSlot(int32 InSaveSlot);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();
	void RefreshPresentation();
	UFUNCTION() void DecreasePlayers();
	UFUNCTION() void IncreasePlayers();
	UFUNCTION() void SelectPublic();
	UFUNCTION() void SelectPrivate();
	UFUNCTION() void ToggleAdvancedJoinCode();
	UFUNCTION() void CreateExpedition();
	UFUNCTION() void Back();
	UFUNCTION() void HandleOperationFinished(bool bSucceeded, const FString& Message);

	int32 SaveSlot = 1;
	int32 MaximumPlayers = 4;
	EJTSLobbyVisibility LobbyVisibility = EJTSLobbyVisibility::Public;
	FString SuggestedJoinCode;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> ExpeditionNameBox;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> PasswordBox;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> CustomJoinCodeBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SaveSlotText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MaximumPlayersText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> VisibilityText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SuggestedJoinCodeText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UButton> AdvancedJoinCodeButton;
};
