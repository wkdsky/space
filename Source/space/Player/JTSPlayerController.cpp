// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSPlayerController.h"

AJTSPlayerController::AJTSPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void AJTSPlayerController::BeginPlayingState()
{
	Super::BeginPlayingState();

	if (IsLocalController())
	{
		FInputModeGameOnly InputMode;
		InputMode.SetConsumeCaptureMouseDown(true);
		SetInputMode(InputMode);
		bShowMouseCursor = false;

		UE_LOG(LogTemp, Log, TEXT("Jump to Space PlayerController initialized."));
	}
}
