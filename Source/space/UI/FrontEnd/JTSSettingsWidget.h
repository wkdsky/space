// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "JTSSettingsWidget.generated.h"

class UButton;
class UTextBlock;

/** Local settings page for input/output devices and microphone mute. */
UCLASS()
class SPACE_API UJTSSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

private:
	void BuildWidgetTree();
	UFUNCTION() void ToggleMute();
	UFUNCTION() void RefreshDevices();
	UFUNCTION() void SelectNextInputDevice();
	UFUNCTION() void SelectNextOutputDevice();
	UFUNCTION() void ReturnToMain();
	bool bMuted = false;
	int32 InputDeviceIndex = INDEX_NONE;
	int32 OutputDeviceIndex = INDEX_NONE;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MuteButtonText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DeviceText;
};
