// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "JTSLobbyWidget.generated.h"

class UButton;
class UTextBlock;
class UJTSSessionDetailsWidget;
class AJTSPlayerState;

/** Local lobby presentation backed entirely by replicated GameState/PlayerState data. */
UCLASS()
class SPACE_API UJTSLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildWidgetTree();
	UFUNCTION() void ToggleReady();
	UFUNCTION() void CycleAvatarColor();
	UFUNCTION() void RequestStart();
	UFUNCTION() void CloseJoining();
	UFUNCTION() void Leave();
	UFUNCTION() void KickSlotOne();
	UFUNCTION() void KickSlotTwo();
	UFUNCTION() void KickSlotThree();
	UFUNCTION() void KickSlotFour();
	UFUNCTION() void MuteSlotOne();
	UFUNCTION() void MuteSlotTwo();
	UFUNCTION() void MuteSlotThree();
	UFUNCTION() void MuteSlotFour();
	void KickSlot(int32 SlotIndex);
	void MuteSlot(int32 SlotIndex);
	void Refresh();

	float RefreshAccumulator = 0.0f;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PlayerListText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UButton> ReadyButton;
	UPROPERTY(Transient) TObjectPtr<UButton> AvatarButton;
	UPROPERTY(Transient) TObjectPtr<UButton> StartButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CloseJoiningButton;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> KickButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> MuteButtons;
	UPROPERTY(Transient) TObjectPtr<UJTSSessionDetailsWidget> SessionDetails;
	TArray<TWeakObjectPtr<AJTSPlayerState>> SlotPlayers;
};
