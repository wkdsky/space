// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "JTSPlayerCardWidget.generated.h"

class AJTSPlayerState;
class UBorder;
class UButton;
class UTextBlock;
class UJTSPreLaunchLobbyWidget;

/** One visual seat in the four-player pre-launch roster. It only presents replicated state and forwards local intent. */
UCLASS()
class SPACE_API UJTSPlayerCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSlot(int32 InSlotIndex, AJTSPlayerState* InPlayerState, bool bInIsLocalPlayer, bool bInHostCanManage, UJTSPreLaunchLobbyWidget* InLobbyOwner);

	AJTSPlayerState* GetAssignedPlayerState() const { return AssignedPlayerState.Get(); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildWidgetTree();
	void Refresh();
	UFUNCTION() void OpenCustomize();
	UFUNCTION() void OpenContextMenu();

	int32 SlotIndex = 0;
	bool bIsLocalPlayer = false;
	bool bHostCanManage = false;
	float RefreshAccumulator = 0.0f;
	TWeakObjectPtr<AJTSPlayerState> AssignedPlayerState;
	TWeakObjectPtr<UJTSPreLaunchLobbyWidget> LobbyOwner;

	UPROPERTY(Transient) TObjectPtr<UBorder> CardBorder;
	UPROPERTY(Transient) TObjectPtr<UBorder> AvatarPreview;
	UPROPERTY(Transient) TObjectPtr<UBorder> ReadyPill;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SlotText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DisplayNameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HostText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ReadyText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> VoiceIconText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ConnectionText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ColorText;
	UPROPERTY(Transient) TObjectPtr<UButton> CustomizeButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ContextButton;
};
