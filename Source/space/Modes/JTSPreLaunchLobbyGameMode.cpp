// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Modes/JTSPreLaunchLobbyGameMode.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Core/JTSMapPaths.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/World/JTSPreLaunchLobbyStage.h"
#include "TimerManager.h"

AJTSPreLaunchLobbyGameMode::AJTSPreLaunchLobbyGameMode()
{
	HUDClass = nullptr;
	bUseSeamlessTravel = true;
}

void AJTSPreLaunchLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (GetWorld() == nullptr || GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}

	EnsureLobbyStage();
	if (AJTSGameState* const State = GetGameState<AJTSGameState>())
	{
		State->SetFailureReason(EJTSFailureReason::None);
		SetLobbyState();
		if (UJTSExpeditionSubsystem* const Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>())
		{
			State->SetExpeditionId(Expedition->GetSnapshot().ExpeditionId);
		}
	}
}

void AJTSPreLaunchLobbyGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	EnsureLobbyStage();
	ArrangeLobbyPlayers();
	if (NewPlayer != nullptr && LobbyStage != nullptr)
	{
		NewPlayer->SetViewTarget(LobbyStage);
	}
}

bool AJTSPreLaunchLobbyGameMode::RequestStartExpedition(AJTSPlayerController* RequestingController)
{
	if (GetWorld()->GetTimerManager().IsTimerActive(LaunchTimer)) return false;
	if (!Super::RequestStartExpedition(RequestingController))
	{
		return false;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	const FString Destination = ResolveLaunchDestination();
	UE_LOG(LogTemp, Log, TEXT("Jump to Space pre-launch lobby locked; travelling expedition to %s."), *Destination);
	if (auto* State=GetGameState<AJTSGameState>()) State->SetLaunchDeadline(State->GetSynchronizedServerTimeSeconds()+3.0);
	World->GetTimerManager().SetTimer(LaunchTimer,FTimerDelegate::CreateWeakLambda(this,[this,Destination]() { if (GetWorld()) GetWorld()->ServerTravel(Destination,true); }),3.f,false);
	return true;
}

void AJTSPreLaunchLobbyGameMode::EnsureLobbyStage()
{
	if (LobbyStage != nullptr || GetWorld() == nullptr)
	{
		return;
	}

	for (TActorIterator<AJTSPreLaunchLobbyStage> It(GetWorld()); It; ++It)
	{
		LobbyStage = *It;
		return;
	}
	LobbyStage = GetWorld()->SpawnActor<AJTSPreLaunchLobbyStage>(AJTSPreLaunchLobbyStage::StaticClass(), FTransform::Identity);
}

void AJTSPreLaunchLobbyGameMode::ArrangeLobbyPlayers()
{
	if (LobbyStage == nullptr)
	{
		return;
	}

	TArray<AJTSPlayerState*> Players = GetActivePlayerStates();
	Players.Sort([](const AJTSPlayerState& Left, const AJTSPlayerState& Right)
	{
		return Left.GetPlayerId() < Right.GetPlayerId();
	});

	for (int32 SlotIndex = 0; SlotIndex < Players.Num(); ++SlotIndex)
	{
		AJTSCharacter* const Character = Players[SlotIndex] != nullptr ? Cast<AJTSCharacter>(Players[SlotIndex]->GetPawn()) : nullptr;
		if (Character == nullptr)
		{
			continue;
		}
		Character->SetActorLocation(LobbyStage->GetPresentationPosition(SlotIndex));
		Character->SetActorRotation(LobbyStage->PresentationRotation);
		if (UCharacterMovementComponent* const Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			// Presentation pads have no planet gravity. Freeze the capsule so the gameplay
			// animation remains grounded instead of holding its falling/jump pose.
			Movement->DisableMovement();
		}
	}
}

FString AJTSPreLaunchLobbyGameMode::ResolveLaunchDestination()
{
	FString Destination = JTSMapPaths::Earth;
	if (UJTSExpeditionSubsystem* const Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>())
	{
		const FJTSExpeditionSnapshot& Snapshot = Expedition->GetSnapshot();
		if (Expedition->ConsumeResumeRequest()
			&& !Snapshot.CurrentMapPackage.IsEmpty()
			&& Snapshot.CurrentMapPackage != JTSMapPaths::FrontEnd
			&& Snapshot.CurrentMapPackage != JTSMapPaths::PreLaunchLobby)
		{
			Destination = Snapshot.CurrentMapPackage;
		}
	}
	return Destination;
}
