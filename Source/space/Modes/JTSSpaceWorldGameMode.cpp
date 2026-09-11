// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Modes/JTSSpaceWorldGameMode.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/UI/JTSPrototypeHUD.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetLandingManager.h"
#include "space/World/JTSSpaceWorldManager.h"

AJTSSpaceWorldGameMode::AJTSSpaceWorldGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	DefaultPawnClass = AJTSCharacter::StaticClass();
	PlayerControllerClass = AJTSPlayerController::StaticClass();
	GameStateClass = AJTSGameState::StaticClass();
	HUDClass = AJTSPrototypeHUD::StaticClass();
	bStartPlayersAsSpectators = true;
	SpaceWorldManagerClass = AJTSSpaceWorldManager::StaticClass();
	PlanetLandingManagerClass = AJTSPlanetLandingManager::StaticClass();
	SpacecraftClass = AJTSSpacecraftActor::StaticClass();
}

void AJTSSpaceWorldGameMode::BeginPlay()
{
	Super::BeginPlay();
	StartedLandingSequences.Empty();

	AJTSSpaceWorldManager* const WorldManager = FindOrCreateSpaceWorldManager();
	if (!IsValid(WorldManager))
	{
		UE_LOG(LogTemp, Error, TEXT("SpaceWorldGameMode could not create or find its SpaceWorldManager."));
		return;
	}

	SpaceWorldManager = WorldManager;
	WorldManager->InitializeCurrentPlanet();
	WorldManager->SetTravelState(EJTSSpaceTravelState::Surface);

	if (UWorld* const World = GetWorld())
	{
		if (AJTSGameState* const JTSGameState = World->GetGameState<AJTSGameState>())
		{
			JTSGameState->SetFailureReason(EJTSFailureReason::None);
			JTSGameState->SetGameplayPhase(EJTSGameplayPhase::MoonExploration);
		}
	}

	AJTSPlanetLandingManager* const LandingManager = FindOrCreatePlanetLandingManager();
	if (!IsValid(LandingManager))
	{
		UE_LOG(LogTemp, Error, TEXT("SpaceWorldGameMode could not create or find its PlanetLandingManager."));
		return;
	}

	PlanetLandingManager = LandingManager;
	LandingManager->SetDefaultSpacecraftClass(SpacecraftClass);

	if (UWorld* const World = GetWorld())
	{
		StartInitialLandingSequence(World->GetFirstPlayerController());
	}
}

void AJTSSpaceWorldGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StartedLandingSequences.Empty();
	PlanetLandingManager.Reset();
	SpaceWorldManager.Reset();

	Super::EndPlay(EndPlayReason);
}

void AJTSSpaceWorldGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	StartInitialLandingSequence(NewPlayer);
}

void AJTSSpaceWorldGameMode::HandlePlayerCharacterDeath(AJTSCharacter* Character)
{
	APlayerController* const PlayerController = IsValid(Character)
		? Cast<APlayerController>(Character->GetController())
		: nullptr;
	AJTSPlanetLandingManager* const LandingManager = PlanetLandingManager.Get();
	if (!IsValid(PlayerController) || !IsValid(LandingManager))
	{
		return;
	}

	if (!LandingManager->RespawnPlayerAtLandedSpacecraft(PlayerController))
	{
		UE_LOG(LogTemp, Log, TEXT("Player death is waiting for a landed spacecraft: PlayerController=%s"),
			*GetNameSafe(PlayerController));
	}
}

AJTSSpaceWorldManager* AJTSSpaceWorldGameMode::FindOrCreateSpaceWorldManager()
{
	if (AJTSSpaceWorldManager* const ExistingManager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
	{
		return ExistingManager;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr)
	{
		return nullptr;
	}

	TSubclassOf<AJTSSpaceWorldManager> ManagerClass = SpaceWorldManagerClass;
	if (ManagerClass == nullptr)
	{
		ManagerClass = AJTSSpaceWorldManager::StaticClass();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = TEXT("JTSSpaceWorldManager");
	SpawnParameters.OverrideLevel = World->PersistentLevel;
	return World->SpawnActor<AJTSSpaceWorldManager>(ManagerClass, FTransform::Identity, SpawnParameters);
}

AJTSPlanetLandingManager* AJTSSpaceWorldGameMode::FindOrCreatePlanetLandingManager()
{
	if (AJTSPlanetLandingManager* const ExistingManager = AJTSPlanetLandingManager::FindPlanetLandingManager(this))
	{
		return ExistingManager;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr)
	{
		return nullptr;
	}

	TSubclassOf<AJTSPlanetLandingManager> ManagerClass = PlanetLandingManagerClass;
	if (ManagerClass == nullptr)
	{
		ManagerClass = AJTSPlanetLandingManager::StaticClass();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = TEXT("JTSPlanetLandingManager");
	SpawnParameters.OverrideLevel = World->PersistentLevel;
	return World->SpawnActor<AJTSPlanetLandingManager>(ManagerClass, FTransform::Identity, SpawnParameters);
}

void AJTSSpaceWorldGameMode::StartInitialLandingSequence(APlayerController* PlayerController)
{
	if (!IsValid(PlayerController) || StartedLandingSequences.Contains(PlayerController))
	{
		return;
	}

	AJTSSpaceWorldManager* const WorldManager = SpaceWorldManager.Get();
	AJTSPlanetLandingManager* const LandingManager = PlanetLandingManager.Get();
	if (!IsValid(WorldManager) || !IsValid(LandingManager))
	{
		return;
	}

	if (!LandingManager->StartLandingSequence(PlayerController, WorldManager->GetCurrentPlanet()))
	{
		UE_LOG(LogTemp, Error, TEXT("SpaceWorld initial landing sequence failed: PlayerController=%s Planet=%s"),
			*GetNameSafe(PlayerController),
			*GetNameSafe(WorldManager->GetCurrentPlanet()));
		return;
	}

	StartedLandingSequences.Add(PlayerController);
}
