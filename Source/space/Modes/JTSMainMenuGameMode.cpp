// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Modes/JTSMainMenuGameMode.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "space/UI/FrontEnd/JTSFrontEndHUD.h"
#include "space/World/JTSEntryPresentationStage.h"

AJTSMainMenuGameMode::AJTSMainMenuGameMode()
{
	PlayerControllerClass = APlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = AJTSFrontEndHUD::StaticClass();
	// Front End players receive a controller/HUD only. No Pawn or PlayerStart is required.
	bStartPlayersAsSpectators = true;
}

void AJTSMainMenuGameMode::BeginPlay()
{
	Super::BeginPlay();
	EnsureEntryPresentation();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Jump to Space Front End active: GameMode=%s HUD=%s PlayerController=%s DefaultPawn=%s"),
		*GetClass()->GetName(),
		*GetNameSafe(HUDClass),
		*GetNameSafe(PlayerControllerClass),
		*GetNameSafe(DefaultPawnClass));
}

void AJTSMainMenuGameMode::EnsureEntryPresentation()
{
	if (GetWorld() == nullptr || GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}

	for (TActorIterator<AJTSEntryPresentationStage> It(GetWorld()); It; ++It)
	{
		EntryPresentation = *It;
		break;
	}
	if (EntryPresentation == nullptr)
	{
		EntryPresentation = GetWorld()->SpawnActor<AJTSEntryPresentationStage>(AJTSEntryPresentationStage::StaticClass(), FTransform::Identity);
	}

	if (EntryPresentation != nullptr)
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* const Controller = It->Get())
			{
				Controller->SetViewTarget(EntryPresentation);
			}
		}
	}
}
