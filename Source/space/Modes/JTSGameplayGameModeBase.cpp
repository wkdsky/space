// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Modes/JTSGameplayGameModeBase.h"

#include "CoreGlobals.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/Systems/JTSOnlineSessionSubsystem.h"

namespace
{
	bool IsApplicationShutdownInProgress(const UWorld* World)
	{
		if (IsEngineExitRequested() || World == nullptr || World->bIsTearingDown)
		{
			return true;
		}

		const UGameInstance* const GameInstance = World->GetGameInstance();
		const UJTSOnlineSessionSubsystem* const Online = GameInstance != nullptr
			? GameInstance->GetSubsystem<UJTSOnlineSessionSubsystem>()
			: nullptr;
		return Online != nullptr && Online->IsShuttingDown();
	}
}

AJTSGameplayGameModeBase::AJTSGameplayGameModeBase()
{
	bUseSeamlessTravel = true;
	DefaultPawnClass = AJTSCharacter::StaticClass();
	PlayerControllerClass = AJTSPlayerController::StaticClass();
	PlayerStateClass = AJTSPlayerState::StaticClass();
	GameStateClass = AJTSGameState::StaticClass();
}

void AJTSGameplayGameModeBase::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	const AJTSGameState* const State = GetGameState<AJTSGameState>();
	if (State != nullptr && !State->IsWaitingToStart())
	{
		ErrorMessage = TEXT("This expedition is already in progress. Mid-expedition joining is disabled.");
		return;
	}
	if (State != nullptr && !State->IsAcceptingNewPlayers())
	{
		ErrorMessage = TEXT("The host has closed joining for this expedition.");
		return;
	}
	if (GetNumPlayers() >= GetMaximumPlayers())
	{
		ErrorMessage = TEXT("This expedition is full.");
		return;
	}

	const FString BuildVersion = UGameplayStatics::ParseOption(Options, TEXT("JTSBuild"));
	if (bRequireMatchingBuildVersion && BuildVersion != ExpectedBuildVersion)
	{
		ErrorMessage = TEXT("Client build version does not match this expedition.");
		return;
	}

	if (const UGameInstance* const Instance = GetGameInstance())
	{
		if (const UJTSExpeditionSubsystem* const Expedition = Instance->GetSubsystem<UJTSExpeditionSubsystem>())
		{
			const FString JoinCode = UGameplayStatics::ParseOption(Options, TEXT("JTSJoinCode"));
			const FString PasswordHash = UGameplayStatics::ParseOption(Options, TEXT("JTSPassword"));
			if (!Expedition->ValidateJoinCredentials(JoinCode, PasswordHash))
			{
				ErrorMessage = TEXT("Invalid expedition join credentials.");
			}
		}
	}
}

int32 AJTSGameplayGameModeBase::GetMaximumPlayers() const
{
	// The host setup is authoritative for this session.  A config default only applies
	// to direct/editor map launches that do not have an OnlineSubsystem session yet.
	if (const UJTSOnlineSessionSubsystem* const Online = GetGameInstance() != nullptr
		? GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>()
		: nullptr)
	{
		return FMath::Clamp(Online->GetCurrentMaximumPlayers(), 1, 4);
	}
	return FMath::Clamp(MaximumPlayers, 1, 4);
}

void AJTSGameplayGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	AJTSPlayerState* const PlayerState = NewPlayer != nullptr ? NewPlayer->GetPlayerState<AJTSPlayerState>() : nullptr;
	if (PlayerState == nullptr)
	{
		return;
	}

	const bool bNoHostExists = !GetActivePlayerStates().ContainsByPredicate([](const AJTSPlayerState* State)
	{
		return State != nullptr && State->IsExpeditionHost();
	});
	PlayerState->SetExpeditionHost(bNoHostExists);
	PlayerState->SetBoarded(false);
	const AJTSGameState* const ExistingState = GetGameState<AJTSGameState>();
	const bool bJoiningPreLaunchLobby = ExistingState == nullptr || ExistingState->IsWaitingToStart();
	PlayerState->SetReady(false);
	PlayerState->SetExpeditionStatus(bJoiningPreLaunchLobby
		? EJTSPlayerExpeditionStatus::InLobby
		: EJTSPlayerExpeditionStatus::Active);

	if (AJTSGameState* const State = GetGameState<AJTSGameState>())
	{
		if (UJTSExpeditionSubsystem* const Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>())
		{
			if (!Expedition->HasActiveExpedition())
			{
				Expedition->StartNewExpedition(FGuid::NewGuid().ToString(EGuidFormats::Digits));
			}
			State->SetExpeditionId(Expedition->GetSnapshot().ExpeditionId);
		}
	}
}

void AJTSGameplayGameModeBase::Logout(AController* Exiting)
{
	UWorld* const World = GetWorld();
	const bool bApplicationShutdownInProgress = IsApplicationShutdownInProgress(World);
	const AJTSPlayerState* const ExitingState = Exiting != nullptr ? Exiting->GetPlayerState<AJTSPlayerState>() : nullptr;
	const bool bHostLeft = ExitingState != nullptr && ExitingState->IsExpeditionHost();
	Super::Logout(Exiting);

	if (bApplicationShutdownInProgress)
	{
		UE_LOG(LogTemp, Log, TEXT("Jump to Space ignored host-leave travel because application shutdown is in progress."));
		return;
	}

	if (bHostLeft)
	{
		if (UGameInstance* const GameInstance = GetGameInstance())
		{
			if (UJTSExpeditionSubsystem* const Expedition = GameInstance->GetSubsystem<UJTSExpeditionSubsystem>())
			{
				AJTSGameState* const State = GetGameState<AJTSGameState>();
				Expedition->CaptureWorldState(State, State != nullptr ? State->GetActiveSpacecraft() : nullptr);
				Expedition->SaveNow();
			}
		}
	}

	if (!bHostLeft || World == nullptr)
	{
		return;
	}

	TArray<AJTSPlayerController*> RemainingPlayers;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AJTSPlayerController* const RemainingPlayer = Cast<AJTSPlayerController>(It->Get()); RemainingPlayer != nullptr && RemainingPlayer != Exiting)
		{
			RemainingPlayers.Add(RemainingPlayer);
		}
	}

	if (RemainingPlayers.IsEmpty())
	{
		return;
	}

	// Deliberately terminate rather than electing a new host: no host migration is supported.
	for (AJTSPlayerController* const RemainingPlayer : RemainingPlayers)
	{
		RemainingPlayer->ClientHostLeftExpedition();
	}
	World->ServerTravel(GetFrontEndMapPackage());
}

bool AJTSGameplayGameModeBase::RequestStartExpedition(AJTSPlayerController* RequestingController)
{
	if (!IsHostController(RequestingController) || !AreAllActivePlayersReady())
	{
		return false;
	}
	SetLobbyAdmission(false);
	return true;
}

bool AJTSGameplayGameModeBase::RequestCloseJoining(AJTSPlayerController* RequestingController)
{
	const AJTSGameState* const State = GetGameState<AJTSGameState>();
	if (!IsHostController(RequestingController) || State == nullptr || !State->IsWaitingToStart())
	{
		return false;
	}
	SetLobbyAdmission(false);
	return true;
}

bool AJTSGameplayGameModeBase::RequestKickPlayer(AJTSPlayerController* RequestingController, AJTSPlayerState* TargetPlayerState)
{
	const AJTSGameState* const State = GetGameState<AJTSGameState>();
	if (!IsHostController(RequestingController)
		|| TargetPlayerState == nullptr
		|| TargetPlayerState->IsExpeditionHost()
		|| State == nullptr
		|| !State->IsWaitingToStart())
	{
		return false;
	}

	APlayerController* TargetController = nullptr;
	if (UWorld* const World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* const Candidate = It->Get();
			if (Candidate != nullptr && Candidate->GetPlayerState<AJTSPlayerState>() == TargetPlayerState)
			{
				TargetController = Candidate;
				break;
			}
		}
	}

	return TargetController != nullptr
		&& GameSession != nullptr
		&& GameSession->KickPlayer(TargetController, FText::FromString(TEXT("You were removed from the expedition lobby by the host.")));
}

bool AJTSGameplayGameModeBase::IsHostController(const AJTSPlayerController* Controller) const
{
	const AJTSPlayerState* const State = Controller != nullptr ? Controller->GetPlayerState<AJTSPlayerState>() : nullptr;
	return State != nullptr && State->IsExpeditionHost();
}

bool AJTSGameplayGameModeBase::AreAllActivePlayersReady() const
{
	const TArray<AJTSPlayerState*> Players = GetActivePlayerStates();
	return !Players.IsEmpty() && !Players.ContainsByPredicate([](const AJTSPlayerState* State)
	{
		return State == nullptr || !State->IsReady();
	});
}

TArray<AJTSPlayerState*> AJTSGameplayGameModeBase::GetActivePlayerStates() const
{
	TArray<AJTSPlayerState*> Result;
	if (const AJTSGameState* const State = GetGameState<AJTSGameState>())
	{
		for (APlayerState* PlayerState : State->PlayerArray)
		{
			if (AJTSPlayerState* const JTSState = Cast<AJTSPlayerState>(PlayerState))
			{
				Result.Add(JTSState);
			}
		}
	}
	return Result;
}

void AJTSGameplayGameModeBase::MarkAllPlayersActive()
{
	for (AJTSPlayerState* const PlayerState : GetActivePlayerStates())
	{
		PlayerState->SetReady(false);
		PlayerState->SetBoarded(false);
		PlayerState->SetExpeditionStatus(EJTSPlayerExpeditionStatus::Active);
	}
}

void AJTSGameplayGameModeBase::SetLobbyState()
{
	if (AJTSGameState* const State = GetGameState<AJTSGameState>())
	{
		State->SetGameplayPhase(EJTSGameplayPhase::WaitingToStart);
	}
	SetLobbyAdmission(true);
}

void AJTSGameplayGameModeBase::SetLobbyAdmission(bool bAcceptingNewPlayers)
{
	if (AJTSGameState* const State = GetGameState<AJTSGameState>())
	{
		State->SetAcceptingNewPlayers(bAcceptingNewPlayers);
	}
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>())
	{
		Online->UpdateLobbyAdmission(bAcceptingNewPlayers);
	}
}

FString AJTSGameplayGameModeBase::GetFrontEndMapPackage() const
{
	return FrontEndMapPath.IsEmpty() ? JTSMapPaths::FrontEnd : FrontEndMapPath;
}
