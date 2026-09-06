// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSPlayerController.h"

#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "space/Modes/JTSEarthGameMode.h"
#include "space/Player/JTSCharacter.h"
#include "space/UI/JTSPrototypeHUD.h"
#include "space/UI/JTSPrototypeHUDWidget.h"

AJTSPlayerController::AJTSPlayerController()
{
	bShouldPerformFullTickWhenPaused = true;
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
	CloseMoonShop();

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

void AJTSPlayerController::ReturnToMainMenu()
{
	if (!IsLocalController())
	{
		return;
	}

	const FString DefaultMap = UGameMapsSettings::GetGameDefaultMap();
	if (DefaultMap.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Jump to Space cannot return to the main menu because GameDefaultMap is empty."));
		return;
	}

	CloseGameMenu();
	SetPause(false);
	UGameplayStatics::OpenLevel(this, FName(*DefaultMap));
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

void AJTSPlayerController::OpenMoonShop(AJTSCharacter* InPlayer)
{
	if (!IsLocalController() || !IsValid(InPlayer) || IsGameMenuOpen())
	{
		return;
	}

	const AJTSGameState* const GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
	if (!IsValid(GameState) || !GameState->IsMoonExploration())
	{
		return;
	}

	AJTSPrototypeHUD* const PrototypeHud = Cast<AJTSPrototypeHUD>(GetHUD());
	UJTSPrototypeHUDWidget* const PrototypeWidget = IsValid(PrototypeHud) ? PrototypeHud->GetPrototypeWidget() : nullptr;
	if (!IsValid(PrototypeWidget) || !PrototypeWidget->OpenMoonShop(InPlayer))
	{
		return;
	}

	SetPause(false);
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
}

void AJTSPlayerController::CloseMoonShop()
{
	AJTSPrototypeHUD* const PrototypeHud = Cast<AJTSPrototypeHUD>(GetHUD());
	if (IsValid(PrototypeHud))
	{
		if (UJTSPrototypeHUDWidget* const PrototypeWidget = PrototypeHud->GetPrototypeWidget())
		{
			PrototypeWidget->CloseMoonShop();
		}
	}

	if (IsLocalController())
	{
		const AJTSGameState* const GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
		if (IsValid(GameState))
		{
			ApplyInputModeForPhase(GameState->GetGameplayPhase());
		}
	}
}

bool AJTSPlayerController::IsMoonShopOpen() const
{
	const AJTSPrototypeHUD* const PrototypeHud = Cast<AJTSPrototypeHUD>(GetHUD());
	const UJTSPrototypeHUDWidget* const PrototypeWidget = IsValid(PrototypeHud) ? PrototypeHud->GetPrototypeWidget() : nullptr;
	return IsValid(PrototypeWidget) && PrototypeWidget->IsMoonShopOpen();
}

void AJTSPlayerController::OpenGameMenu()
{
	if (!IsLocalController() || !IsNormalGameplayPhase())
	{
		return;
	}

	if (IsMoonShopOpen())
	{
		CloseMoonShop();
		return;
	}

	AJTSPrototypeHUD* const PrototypeHud = Cast<AJTSPrototypeHUD>(GetHUD());
	UJTSPrototypeHUDWidget* const PrototypeWidget = IsValid(PrototypeHud) ? PrototypeHud->GetPrototypeWidget() : nullptr;
	if (!IsValid(PrototypeWidget) || PrototypeWidget->IsGameMenuOpen())
	{
		return;
	}

	PrototypeWidget->OpenGameMenu();
	SetPause(true);
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
}

void AJTSPlayerController::CloseGameMenu()
{
	AJTSPrototypeHUD* const PrototypeHud = Cast<AJTSPrototypeHUD>(GetHUD());
	if (IsValid(PrototypeHud))
	{
		if (UJTSPrototypeHUDWidget* const PrototypeWidget = PrototypeHud->GetPrototypeWidget())
		{
			PrototypeWidget->CloseGameMenu();
		}
	}

	if (!IsLocalController())
	{
		return;
	}

	SetPause(false);
	if (const AJTSGameState* const GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr)
	{
		ApplyInputModeForPhase(GameState->GetGameplayPhase());
	}
}

bool AJTSPlayerController::IsGameMenuOpen() const
{
	const AJTSPrototypeHUD* const PrototypeHud = Cast<AJTSPrototypeHUD>(GetHUD());
	const UJTSPrototypeHUDWidget* const PrototypeWidget = IsValid(PrototypeHud) ? PrototypeHud->GetPrototypeWidget() : nullptr;
	return IsValid(PrototypeWidget) && PrototypeWidget->IsGameMenuOpen();
}

bool AJTSPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (Params.Event == IE_Pressed && Params.Key == EKeys::Escape)
	{
		if (IsMoonShopOpen())
		{
			CloseMoonShop();
			return true;
		}
		if (IsGameMenuOpen())
		{
			CloseGameMenu();
			return true;
		}
		if (IsNormalGameplayPhase())
		{
			OpenGameMenu();
			return true;
		}
	}

	if (Params.Event == IE_Pressed && Params.Key == EKeys::E && IsMoonShopOpen())
	{
		CloseMoonShop();
		return true;
	}

	return Super::InputKey(Params);
}

bool AJTSPlayerController::IsNormalGameplayPhase() const
{
	const AJTSGameState* const GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
	return IsValid(GameState) && (GameState->IsEarthCollectionActive() || GameState->IsMoonExploration());
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

	if (IsGameMenuOpen())
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
	if (IsGameMenuOpen())
	{
		CloseGameMenu();
	}
	if (NewGameplayPhase != EJTSGameplayPhase::MoonExploration)
	{
		CloseMoonShop();
	}
	ApplyInputModeForPhase(NewGameplayPhase);
}
