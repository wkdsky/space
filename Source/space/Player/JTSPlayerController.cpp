// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "space/Core/JTSGameState.h"
#include "space/Modes/JTSGameplayGameModeBase.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Components/JTSPlayerEquipmentComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Systems/JTSOnlineSessionSubsystem.h"
#include "space/Core/JTSMapPaths.h"
#include "space/UI/JTSPreLaunchLobbyWidget.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "space/UI/JTSPrototypeHUD.h"
#include "space/UI/JTSPrototypeHUDWidget.h"
#include "space/UI/JTSShopWidget.h"
#include "space/World/JTSSpaceWorldManager.h"
#include "TimerManager.h"

AJTSPlayerController::AJTSPlayerController()
{
	bShouldPerformFullTickWhenPaused = true;
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

bool AJTSPlayerController::IsLookYAxisInverted() const
{
	return bInvertLookYAxis;
}

void AJTSPlayerController::BeginPlayingState()
{
	Super::BeginPlayingState();
	BindGameState();
	ScheduleGameStateBind();
	if (const AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
		IsValid(Manager) && Manager->IsSurfaceGameplayReady())
	{
		ApplySpaceWorldInputMode();
	}

	if (IsLocalController())
	{
		UE_LOG(LogTemp, Log, TEXT("Jump to Space PlayerController initialized."));
	}
}

void AJTSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	BindGameState();
	ScheduleGameStateBind();
	if (const AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
		IsValid(Manager) && Manager->IsSurfaceGameplayReady())
	{
		ApplySpaceWorldInputMode();
	}
}

void AJTSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bGameplayInputModeActive = false;
	CloseSpaceShop();
	CloseMoonShop();
	HideLobby();
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GameStateBindingRetryTimer);
	}

	if (AJTSGameState* const GameState = BoundGameState.Get())
	{
		GameState->OnGameplayPhaseChanged.RemoveDynamic(this, &AJTSPlayerController::HandleGameplayPhaseChanged);
	}
	BoundGameState.Reset();

	Super::EndPlay(EndPlayReason);
}

void AJTSPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	RefreshGameplayInputAfterPossess();
}

void AJTSPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
	Super::ClientRestart_Implementation(NewPawn);
	RefreshGameplayInputAfterPossess();
}

void AJTSPlayerController::StartGame()
{
	if (!IsLocalController())
	{
		return;
	}

	ServerRequestStartExpedition();
}

void AJTSPlayerController::RequestSetReady()
{
	if (IsLocalController())
	{
		if (const AJTSPlayerState* const State = GetPlayerState<AJTSPlayerState>())
		{
			ServerSetReady(!State->IsReady());
		}
	}
}

void AJTSPlayerController::RequestAvatarColor(EJTSAvatarColor NewColor)
{
	if (IsLocalController())
	{
		ServerSetAvatarColor(NewColor);
	}
}

void AJTSPlayerController::RequestCloseJoining()
{
	if (IsLocalController())
	{
		ServerRequestCloseJoining();
	}
}

void AJTSPlayerController::RequestKickPlayer(AJTSPlayerState* TargetPlayerState)
{
	if (IsLocalController() && TargetPlayerState != nullptr)
	{
		ServerRequestKickPlayer(TargetPlayerState);
	}
}

void AJTSPlayerController::ServerSetReady_Implementation(bool bReady)
{
	if (AJTSPlayerState* const State = GetPlayerState<AJTSPlayerState>())
	{
		const AJTSGameState* const GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
		if (GameState != nullptr && GameState->IsWaitingToStart())
		{
			State->SetReady(bReady);
		}
	}
}

void AJTSPlayerController::ServerSetAvatarColor_Implementation(EJTSAvatarColor NewColor)
{
	const uint8 ColorIndex = static_cast<uint8>(NewColor);
	const AJTSGameState* const GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
	if (ColorIndex <= static_cast<uint8>(EJTSAvatarColor::Purple)
		&& GameState != nullptr
		&& GameState->IsWaitingToStart())
	{
		if (AJTSPlayerState* const State = GetPlayerState<AJTSPlayerState>())
		{
			State->SetAvatarColor(NewColor);
		}
	}
}

void AJTSPlayerController::ServerRequestStartExpedition_Implementation()
{
	if (AJTSGameplayGameModeBase* const GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AJTSGameplayGameModeBase>() : nullptr)
	{
		GameMode->RequestStartExpedition(this);
	}
}

void AJTSPlayerController::ServerRequestCloseJoining_Implementation()
{
	if (AJTSGameplayGameModeBase* const GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AJTSGameplayGameModeBase>() : nullptr)
	{
		GameMode->RequestCloseJoining(this);
	}
}

void AJTSPlayerController::ServerRequestKickPlayer_Implementation(AJTSPlayerState* TargetPlayerState)
{
	if (AJTSGameplayGameModeBase* const GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AJTSGameplayGameModeBase>() : nullptr)
	{
		GameMode->RequestKickPlayer(this, TargetPlayerState);
	}
}

void AJTSPlayerController::ClientHostLeftExpedition_Implementation()
{
	if (!IsLocalController())
	{
		return;
	}

	CloseGameMenu();
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>())
	{
		Online->HandleHostLeftExpedition();
	}
}

void AJTSPlayerController::ServerRequestBoardSpacecraft_Implementation(AJTSSpacecraftActor* Spacecraft)
{
	APawn* const ControlledPawn = GetPawn();
	if (IsValid(Spacecraft) && IsValid(ControlledPawn) && Spacecraft->IsPawnInBoardingRange(ControlledPawn))
	{
		Spacecraft->TryBoardPlayer(ControlledPawn);
	}
}

void AJTSPlayerController::ServerRequestDisembarkSpacecraft_Implementation(AJTSSpacecraftActor* Spacecraft)
{
	if (IsValid(Spacecraft))
	{
		// This RPC also serves hidden passengers, whose current pawn is their character rather than
		// the spacecraft. Resolve from this controller's PlayerState on the server instead of relying
		// on whichever pawn possession is currently replicating.
		Spacecraft->TryDisembarkPlayerForController(this);
	}
}

void AJTSPlayerController::ServerRequestCraft_Implementation(EJTSEquipmentType EquipmentType, AJTSSpacecraftActor* Spacecraft)
{
	// Retained as a harmless RPC symbol for old Blueprint/UI assets. New purchases
	// are validated through the nearby shared spacecraft.
	static_cast<void>(EquipmentType);
	static_cast<void>(Spacecraft);
	UE_LOG(LogTemp, Verbose, TEXT("JumpToSpace: ignored retired Moon workshop request from %s."), *GetNameSafe(this));
}

void AJTSPlayerController::ServerRequestShopPurchase_Implementation(AJTSSpacecraftActor* Spacecraft, EJTSItemId ItemId)
{
	AJTSCharacter* const ControlledCharacter = Cast<AJTSCharacter>(GetPawn());
	const EJTSShopPurchaseResult Result = IsValid(ControlledCharacter) && IsValid(Spacecraft)
		? Spacecraft->TryPurchase(ControlledCharacter, ItemId)
		: EJTSShopPurchaseResult::DeliveryFailed;
	ClientReceiveShopPurchaseResult(Result);
}

void AJTSPlayerController::ClientReceiveShopPurchaseResult_Implementation(EJTSShopPurchaseResult Result)
{
	if (IsValid(SpaceShopWidget))
	{
		SpaceShopWidget->NotifyPurchaseResult(Result);
	}
}

void AJTSPlayerController::ClientOpenSpaceShop_Implementation(AJTSSpacecraftActor* Spacecraft)
{
	OpenSpaceShop(Spacecraft);
}

void AJTSPlayerController::RestartCurrentLevel()
{
	if (!IsLocalController())
	{
		return;
	}

	// A connected expedition must never use OpenLevel on one peer. Leaving is the explicit restart path.
	ReturnToMainMenu();
}

void AJTSPlayerController::QuitGame()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UGameInstance* const GameInstance = GetGameInstance())
	{
		if (UJTSOnlineSessionSubsystem* const Online = GameInstance->GetSubsystem<UJTSOnlineSessionSubsystem>())
		{
			Online->RequestApplicationQuit();
			return;
		}
	}

	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void AJTSPlayerController::ReturnToMainMenu()
{
	if (!IsLocalController())
	{
		return;
	}

	CloseGameMenu();
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>())
	{
		Online->LeaveExpedition();
	}
}

void AJTSPlayerController::ApplyEarthCollectionInputMode()
{
	if (!IsLocalController())
	{
		return;
	}
	if (bGameplayInputModeActive)
	{
		if (AJTSCharacter* const CharacterPawn = Cast<AJTSCharacter>(GetPawn()))
		{
			CharacterPawn->EnsureGameplayInputMapping();
		}
		return;
	}

	// The lobby uses UI-only focus and pushes both Ignore flags. Merely collapsing its widget leaves
	// Slate focus and input-stack state alive across seamless travel, which is why attack could still
	// fire while movement/look were ignored. Tear the widget down and restore every input owner here.
	HideLobby();
	FInputModeGameOnly InputMode;
	InputMode.SetConsumeCaptureMouseDown(true);
	SetInputMode(InputMode);
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	ResetIgnoreInputFlags();
	if (PlayerInput != nullptr)
	{
		PlayerInput->FlushPressedKeys();
	}
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
		FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
	}
	if (AJTSCharacter* const CharacterPawn = Cast<AJTSCharacter>(GetPawn()))
	{
		CharacterPawn->EnsureGameplayInputMapping();
	}
	bGameplayInputModeActive = true;
}

void AJTSPlayerController::ApplySpaceWorldInputMode()
{
	ApplyEarthCollectionInputMode();
}

void AJTSPlayerController::ApplyModalUIInputMode(UUserWidget* FocusWidget)
{
	if (!IsLocalController())
	{
		return;
	}
	bGameplayInputModeActive = false;

	FInputModeUIOnly InputMode;
	if (IsValid(FocusWidget))
	{
		InputMode.SetWidgetToFocus(FocusWidget->TakeWidget());
	}
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	// Input-mode changes can be replayed after possession or replication. Reset first so repeated
	// modal applications do not accumulate IgnoreInput stack entries.
	ResetIgnoreInputFlags();
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
}

void AJTSPlayerController::ApplyPreLaunchLobbyInputMode()
{
	if (!IsLocalController() || LobbyWidget == nullptr)
	{
		return;
	}

	ApplyModalUIInputMode(LobbyWidget);
}

bool AJTSPlayerController::IsPreLaunchLobbyWorld() const
{
	const UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	const FString PackageName = World->GetPackage() != nullptr ? World->GetPackage()->GetName() : FString();
	return PackageName == JTSMapPaths::PreLaunchLobby
		|| PackageName.EndsWith(TEXT("L_PreLaunchLobby"))
		|| World->GetMapName().Contains(TEXT("L_PreLaunchLobby"));
}

void AJTSPlayerController::SetSpacecraftCameraViewTarget(AJTSSpacecraftActor* Spacecraft)
{
	if (!IsLocalController() || !IsValid(Spacecraft))
	{
		return;
	}

	Spacecraft->ActivateFlightCameraThirdPerson();
	SetViewTargetWithBlend(
		Spacecraft,
		FMath::Max(0.0f, SpacecraftCameraBlendTime),
		VTBlend_Cubic,
		1.0f,
		false);
}

void AJTSPlayerController::RestoreCharacterCameraViewTarget(AJTSCharacter* CharacterPawn)
{
	if (!IsLocalController() || !IsValid(CharacterPawn))
	{
		return;
	}

	SetViewTargetWithBlend(
		CharacterPawn,
		FMath::Max(0.0f, SpacecraftCameraBlendTime),
		VTBlend_Cubic,
		1.0f,
		false);
}

void AJTSPlayerController::OpenMoonShop(AJTSCharacter* InPlayer)
{
	// Compatibility entry point only.  It intentionally cannot reopen the
	// retired Moon prototype store and does not change input state.
	static_cast<void>(InPlayer);
	UE_LOG(LogTemp, Verbose, TEXT("JumpToSpace: Moon workshop is retired; use the nearby spacecraft supply screen."));
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
		if (const AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
			IsValid(Manager) && Manager->IsSurfaceGameplayReady())
		{
			ApplySpaceWorldInputMode();
		}
		else if (const AJTSGameState* const GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr)
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

void AJTSPlayerController::OpenSpaceShop(AJTSSpacecraftActor* Spacecraft)
{
	if (!IsLocalController() || !IsValid(Spacecraft) || IsGameMenuOpen())
	{
		return;
	}
	CloseMoonShop();
	if (!IsValid(SpaceShopWidget))
	{
		TSubclassOf<UJTSShopWidget> ShopWidgetClass = SpaceShopWidgetClass.IsNull()
			? nullptr
			: SpaceShopWidgetClass.LoadSynchronous();
		SpaceShopWidget = CreateWidget<UJTSShopWidget>(
			this,
			ShopWidgetClass != nullptr ? ShopWidgetClass : TSubclassOf<UJTSShopWidget>(UJTSShopWidget::StaticClass()));
		if (IsValid(SpaceShopWidget))
		{
			SpaceShopWidget->AddToViewport(300);
		}
	}
	if (!IsValid(SpaceShopWidget) || !SpaceShopWidget->OpenForSpacecraft(Spacecraft))
	{
		return;
	}

	ApplyModalUIInputMode(SpaceShopWidget);
}

void AJTSPlayerController::CloseSpaceShop()
{
	if (IsValid(SpaceShopWidget))
	{
		SpaceShopWidget->CloseShop();
		SpaceShopWidget->RemoveFromParent();
		SpaceShopWidget = nullptr;
	}
	if (IsLocalController())
	{
		if (const AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
			IsValid(Manager) && Manager->IsSurfaceGameplayReady())
		{
			ApplySpaceWorldInputMode();
		}
		else if (const AJTSGameState* const GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr)
		{
			ApplyInputModeForPhase(GameState->GetGameplayPhase());
		}
	}
}

bool AJTSPlayerController::IsSpaceShopOpen() const
{
	return IsValid(SpaceShopWidget) && SpaceShopWidget->IsShopOpen();
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
	if (IsSpaceShopOpen())
	{
		CloseSpaceShop();
		return;
	}

	AJTSPrototypeHUD* const PrototypeHud = Cast<AJTSPrototypeHUD>(GetHUD());
	UJTSPrototypeHUDWidget* const PrototypeWidget = IsValid(PrototypeHud) ? PrototypeHud->GetPrototypeWidget() : nullptr;
	if (!IsValid(PrototypeWidget) || PrototypeWidget->IsGameMenuOpen())
	{
		return;
	}

	PrototypeWidget->OpenGameMenu();
	ApplyModalUIInputMode(PrototypeWidget);
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

	if (const AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
		IsValid(Manager) && Manager->IsSurfaceGameplayReady())
	{
		ApplySpaceWorldInputMode();
	}
	else if (const AJTSGameState* const GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr)
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
		if (IsSpaceShopOpen())
		{
			CloseSpaceShop();
			return true;
		}
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

	if (Params.Event == IE_Pressed && Params.Key == EKeys::E && (IsMoonShopOpen() || IsSpaceShopOpen()))
	{
		if (IsSpaceShopOpen()) { CloseSpaceShop(); } else { CloseMoonShop(); }
		return true;
	}

	return Super::InputKey(Params);
}

bool AJTSPlayerController::IsNormalGameplayPhase() const
{
	if (const AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
		IsValid(Manager) && Manager->IsSurfaceGameplayReady())
	{
		return true;
	}

	const AJTSGameState* const GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
	return IsValid(GameState) && (GameState->IsEarthCollectionActive() || GameState->IsMoonExploration() || GameState->IsSpaceFlight());
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
		if (const AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
			IsValid(Manager) && Manager->IsSurfaceGameplayReady())
		{
			ApplySpaceWorldInputMode();
		}
		else
		{
			ApplyInputModeForPhase(NewGameState->GetGameplayPhase());
		}
	}
}

void AJTSPlayerController::ScheduleGameStateBind()
{
	if (!IsLocalController() || GetWorld() == nullptr)
	{
		return;
	}
	GameStateBindingRetryCount = 0;
	GetWorld()->GetTimerManager().SetTimer(
		GameStateBindingRetryTimer,
		this,
		&AJTSPlayerController::RetryBindGameState,
		0.05f,
		false);
}

void AJTSPlayerController::RetryBindGameState()
{
	if (!IsLocalController() || GetWorld() == nullptr)
	{
		return;
	}

	BindGameState();
	++GameStateBindingRetryCount;
	const AJTSGameState* const CurrentState = GetWorld()->GetGameState<AJTSGameState>();
	if ((CurrentState == nullptr || BoundGameState.Get() != CurrentState || GameStateBindingRetryCount < 8)
		&& GameStateBindingRetryCount < 40)
	{
		GetWorld()->GetTimerManager().SetTimer(
			GameStateBindingRetryTimer,
			this,
			&AJTSPlayerController::RetryBindGameState,
			0.10f,
			false);
	}
}

void AJTSPlayerController::RefreshGameplayInputAfterPossess()
{
	if (!IsLocalController())
	{
		return;
	}

	if (AJTSCharacter* const CharacterPawn = Cast<AJTSCharacter>(GetPawn()))
	{
		CharacterPawn->EnsureGameplayInputMapping();
	}
	BindGameState();
	ScheduleGameStateBind();
	if (const AJTSGameState* const State = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr)
	{
		ApplyInputModeForPhase(State->GetGameplayPhase());
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
	{
		if (IsPreLaunchLobbyWorld())
		{
			ShowLobby();
			ApplyPreLaunchLobbyInputMode();
		}
		else
		{
			// Earth may briefly expose its default replicated phase while the GameState arrives.
			// It is not a lobby; preserve game ownership of the viewport until its real phase lands.
			HideLobby();
			ApplyEarthCollectionInputMode();
		}
		break;
	}

	case EJTSGameplayPhase::EarthCaptureFailure:
	case EJTSGameplayPhase::MoonArrivalSuccess:
	{
		// The result card has active Restart/Quit controls. It must own the cursor and keyboard just
		// like the pause menu and store, while the world is still allowed to run behind it.
		HideLobby();
		AJTSPrototypeHUD* const PrototypeHud = Cast<AJTSPrototypeHUD>(GetHUD());
		UJTSPrototypeHUDWidget* const PrototypeWidget = IsValid(PrototypeHud) ? PrototypeHud->GetPrototypeWidget() : nullptr;
		ApplyModalUIInputMode(PrototypeWidget);
		break;
	}

	case EJTSGameplayPhase::Launching:
	{
		bGameplayInputModeActive = false;
		HideLobby();
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
		HideLobby();
		ApplyEarthCollectionInputMode();
		break;

	case EJTSGameplayPhase::MoonExploration:
		HideLobby();
		ApplyEarthCollectionInputMode();
		break;

	case EJTSGameplayPhase::SpaceFlight:
		HideLobby();
		ApplySpaceWorldInputMode();
		break;

	case EJTSGameplayPhase::EarthCollectionFinished:
	{
		bGameplayInputModeActive = false;
		HideLobby();
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
	if (const AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
		IsValid(Manager) && Manager->IsSurfaceGameplayReady())
	{
		ApplySpaceWorldInputMode();
		return;
	}

	ApplyInputModeForPhase(NewGameplayPhase);
}

void AJTSPlayerController::ShowLobby()
{
	if (!IsLocalController() || !IsPreLaunchLobbyWorld()) return;
	if (LobbyWidget == nullptr)
	{
		const TSubclassOf<UJTSPreLaunchLobbyWidget> WidgetClass = PreLaunchLobbyWidgetClass.IsNull()
			? UJTSPreLaunchLobbyWidget::StaticClass()
			: PreLaunchLobbyWidgetClass.LoadSynchronous();
		LobbyWidget = CreateWidget<UJTSPreLaunchLobbyWidget>(this, WidgetClass != nullptr ? WidgetClass : TSubclassOf<UJTSPreLaunchLobbyWidget>(UJTSPreLaunchLobbyWidget::StaticClass()));
		if (LobbyWidget != nullptr)
		{
			LobbyWidget->AddToViewport(90);
			UE_LOG(LogTemp, Log, TEXT("Jump to Space pre-launch lobby widget %s is visible for local controller %s."), *GetNameSafe(LobbyWidget->GetClass()), *GetName());
		}
	}
	if (LobbyWidget != nullptr) LobbyWidget->SetVisibility(ESlateVisibility::Visible);
}

void AJTSPlayerController::HideLobby()
{
	if (LobbyWidget != nullptr)
	{
		LobbyWidget->RemoveFromParent();
		LobbyWidget = nullptr;
	}
}
