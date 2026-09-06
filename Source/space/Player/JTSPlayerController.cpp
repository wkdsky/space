// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSPlayerController.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "space/Modes/JTSEarthGameMode.h"

AJTSPlayerController::AJTSPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void AJTSPlayerController::BeginPlayingState()
{
	Super::BeginPlayingState();
	BindGameState();

	if (IsLocalController())
	{
		UE_LOG(LogTemp, Log, TEXT("Jump to Space PlayerController initialized."));
	}
}

void AJTSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	BindGameState();
}

void AJTSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AJTSGameState* const GameState = BoundGameState.Get())
	{
		GameState->OnGameplayPhaseChanged.RemoveDynamic(this, &AJTSPlayerController::HandleGameplayPhaseChanged);
	}
	BoundGameState.Reset();

	Super::EndPlay(EndPlayReason);
}

void AJTSPlayerController::StartGame()
{
	if (!IsLocalController())
	{
		return;
	}

	BindGameState();

	if (AJTSEarthGameMode* const GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AJTSEarthGameMode>() : nullptr)
	{
		GameMode->StartEarthCollection();
	}

	if (AJTSGameState* const GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr)
	{
		if (GameState->IsEarthCollectionActive())
		{
			ApplyEarthCollectionInputMode();
		}
	}
}

void AJTSPlayerController::RestartCurrentLevel()
{
	if (!IsLocalController())
	{
		return;
	}

	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	if (CurrentLevelName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Jump to Space could not restart because the current level name is empty."));
		return;
	}

	SetPause(false);
	UGameplayStatics::OpenLevel(this, FName(*CurrentLevelName));
}

void AJTSPlayerController::QuitGame()
{
	if (!IsLocalController())
	{
		return;
	}

	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void AJTSPlayerController::ApplyEarthCollectionInputMode()
{
	if (!IsLocalController())
	{
		return;
	}

	SetPause(false);
	FInputModeGameOnly InputMode;
	InputMode.SetConsumeCaptureMouseDown(true);
	SetInputMode(InputMode);
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
}

void AJTSPlayerController::BindGameState()
{
	if (!IsLocalController())
	{
		return;
	}

	AJTSGameState* const NewGameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
	if (BoundGameState.Get() != NewGameState)
	{
		if (AJTSGameState* const PreviousGameState = BoundGameState.Get())
		{
			PreviousGameState->OnGameplayPhaseChanged.RemoveDynamic(this, &AJTSPlayerController::HandleGameplayPhaseChanged);
		}

		BoundGameState = NewGameState;
		if (NewGameState != nullptr)
		{
			NewGameState->OnGameplayPhaseChanged.AddDynamic(this, &AJTSPlayerController::HandleGameplayPhaseChanged);
		}
	}

	if (NewGameState != nullptr)
	{
		ApplyInputModeForPhase(NewGameState->GetGameplayPhase());
	}
}

void AJTSPlayerController::ApplyInputModeForPhase(EJTSGameplayPhase GameplayPhase)
{
	if (!IsLocalController())
	{
		return;
	}

	switch (GameplayPhase)
	{
	case EJTSGameplayPhase::WaitingToStart:
	case EJTSGameplayPhase::EarthCaptureFailure:
	{
		SetPause(true);
		FInputModeUIOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		break;
	}

	case EJTSGameplayPhase::MoonArrivalSuccess:
	{
		// Keep the world running so the Earth GameMode transition timer can travel to the Moon.
		SetPause(false);
		FInputModeGameOnly InputMode;
		InputMode.SetConsumeCaptureMouseDown(true);
		SetInputMode(InputMode);
		bShowMouseCursor = false;
		bEnableClickEvents = false;
		bEnableMouseOverEvents = false;
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		break;
	}

	case EJTSGameplayPhase::Launching:
	{
		SetPause(false);
		FInputModeGameOnly InputMode;
		InputMode.SetConsumeCaptureMouseDown(true);
		SetInputMode(InputMode);
		bShowMouseCursor = false;
		bEnableClickEvents = false;
		bEnableMouseOverEvents = false;
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		break;
	}

	case EJTSGameplayPhase::EarthCollection:
		ApplyEarthCollectionInputMode();
		break;

	case EJTSGameplayPhase::MoonExploration:
		ApplyEarthCollectionInputMode();
		break;

	case EJTSGameplayPhase::EarthCollectionFinished:
	{
		SetPause(false);
		FInputModeGameOnly InputMode;
		InputMode.SetConsumeCaptureMouseDown(true);
		SetInputMode(InputMode);
		bShowMouseCursor = false;
		bEnableClickEvents = false;
		bEnableMouseOverEvents = false;
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		break;
	}

	default:
		break;
	}
}

void AJTSPlayerController::HandleGameplayPhaseChanged(EJTSGameplayPhase NewGameplayPhase)
{
	ApplyInputModeForPhase(NewGameplayPhase);
}
