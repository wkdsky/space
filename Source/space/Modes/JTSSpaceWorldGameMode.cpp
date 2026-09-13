// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Modes/JTSSpaceWorldGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/UI/JTSPrototypeHUD.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetLandingManager.h"
#include "space/World/JTSMoonSurfaceController.h"
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

	FJTSSurfaceGameplayControllerDefinition MoonSurfaceGameplay;
	MoonSurfaceGameplay.PlanetId = TEXT("Moon");
	MoonSurfaceGameplay.ControllerClass = AJTSMoonSurfaceController::StaticClass();
	SurfaceGameplayControllers.Add(MoonSurfaceGameplay);
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
			JTSGameState->SetGameplayPhase(EJTSGameplayPhase::WaitingToStart);
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
	LandingManager->OnInitialLandingSequenceCompleted().AddUObject(
		this,
		&AJTSSpaceWorldGameMode::HandleInitialLandingSequenceCompleted);

	if (UWorld* const World = GetWorld())
	{
		StartInitialLandingSequence(World->GetFirstPlayerController());
	}
}

void AJTSSpaceWorldGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AJTSPlanetLandingManager* const LandingManager = PlanetLandingManager.Get())
	{
		LandingManager->OnInitialLandingSequenceCompleted().RemoveAll(this);
	}

	for (TPair<FName, TWeakObjectPtr<AActor>>& Entry : ActiveSurfaceGameplayControllers)
	{
		if (AActor* const ControllerActor = Entry.Value.Get())
		{
			if (IJTSPlanetSurfaceGameplay* const SurfaceGameplay = Cast<IJTSPlanetSurfaceGameplay>(ControllerActor))
			{
				SurfaceGameplay->ShutdownSurfaceGameplay();
			}
		}
	}
	ActiveSurfaceGameplayControllers.Empty();
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

void AJTSSpaceWorldGameMode::HandleInitialLandingSequenceCompleted(
	APlayerController* PlayerController,
	AJTSPlanetAnchor* Planet,
	AJTSCharacter* Character,
	AJTSSpacecraftActor* Spacecraft)
{
	AJTSSpaceWorldManager* const WorldManager = SpaceWorldManager.Get();
	if (!IsValid(WorldManager) || !IsValid(Planet) || !IsValid(Character) || !IsValid(Spacecraft))
	{
		UE_LOG(LogTemp, Error, TEXT("SpaceWorld surface gameplay received an incomplete arrival context: Planet=%s Player=%s Spacecraft=%s."),
			*GetNameSafe(Planet), *GetNameSafe(Character), *GetNameSafe(Spacecraft));
		return;
	}

	const FJTSSurfaceGameplayControllerDefinition* const Definition = FindSurfaceGameplayDefinition(Planet);
	if (Definition == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("SpaceWorld has no surface gameplay definition for PlanetId=%s."), *Planet->GetPlanetId().ToString());
		WorldManager->SetSurfaceGameplayReady(false);
		return;
	}

	AActor* const ControllerActor = FindOrSpawnSurfaceGameplayController(*Definition, Planet);
	IJTSPlanetSurfaceGameplay* const SurfaceGameplay = Cast<IJTSPlanetSurfaceGameplay>(ControllerActor);
	if (!IsValid(ControllerActor) || SurfaceGameplay == nullptr || !SurfaceGameplay->SupportsPlanet(Planet))
	{
		UE_LOG(LogTemp, Error, TEXT("SpaceWorld surface gameplay controller is invalid or does not support PlanetId=%s. Controller=%s."),
			*Planet->GetPlanetId().ToString(), *GetNameSafe(ControllerActor));
		WorldManager->SetSurfaceGameplayReady(false);
		return;
	}

	FJTSSurfaceGameplayContext Context;
	Context.Planet = Planet;
	Context.Player = Character;
	Context.Spacecraft = Spacecraft;
	Context.GameplayData = Definition->GameplayData.IsNull() ? nullptr : Definition->GameplayData.LoadSynchronous();
	if (!Definition->GameplayData.IsNull() && !IsValid(Context.GameplayData))
	{
		UE_LOG(LogTemp, Error, TEXT("SpaceWorld could not load surface gameplay data for PlanetId=%s."), *Planet->GetPlanetId().ToString());
		WorldManager->SetSurfaceGameplayReady(false);
		return;
	}

	if (!SurfaceGameplay->InitializeSurfaceGameplay(Context))
	{
		UE_LOG(LogTemp, Error, TEXT("SpaceWorld surface gameplay initialization failed: PlanetId=%s Controller=%s."),
			*Planet->GetPlanetId().ToString(), *GetNameSafe(ControllerActor));
		WorldManager->SetSurfaceGameplayReady(false);
		return;
	}

	ActiveSurfaceGameplayControllers.Add(Planet->GetPlanetId(), ControllerActor);
	WorldManager->SetSurfaceGameplayReady(true);
	if (AJTSGameState* const JTSGameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr)
	{
		JTSGameState->SetFailureReason(EJTSFailureReason::None);
		JTSGameState->SetGameplayPhase(EJTSGameplayPhase::MoonExploration);
	}
	if (AJTSPlayerController* const JTSPlayerController = Cast<AJTSPlayerController>(PlayerController))
	{
		JTSPlayerController->ApplySpaceWorldInputMode();
	}

	UE_LOG(LogTemp, Log, TEXT("SpaceWorld surface gameplay ready: PlanetId=%s Controller=%s."),
		*Planet->GetPlanetId().ToString(), *GetNameSafe(ControllerActor));
}

const FJTSSurfaceGameplayControllerDefinition* AJTSSpaceWorldGameMode::FindSurfaceGameplayDefinition(
	const AJTSPlanetAnchor* Planet) const
{
	if (!IsValid(Planet))
	{
		return nullptr;
	}

	return SurfaceGameplayControllers.FindByPredicate([Planet](const FJTSSurfaceGameplayControllerDefinition& Definition)
	{
		return !Definition.PlanetId.IsNone() && Definition.PlanetId == Planet->GetPlanetId();
	});
}

AActor* AJTSSpaceWorldGameMode::FindOrSpawnSurfaceGameplayController(
	const FJTSSurfaceGameplayControllerDefinition& Definition,
	AJTSPlanetAnchor* Planet)
{
	if (!IsValid(Planet) || Definition.ControllerClass == nullptr)
	{
		return nullptr;
	}

	if (const TWeakObjectPtr<AActor>* const ActiveController = ActiveSurfaceGameplayControllers.Find(Planet->GetPlanetId()))
	{
		if (AActor* const ControllerActor = ActiveController->Get(); IsValid(ControllerActor))
		{
			return ControllerActor;
		}
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* const Candidate = *ActorIt;
		IJTSPlanetSurfaceGameplay* const SurfaceGameplay = Cast<IJTSPlanetSurfaceGameplay>(Candidate);
		if (IsValid(Candidate) && SurfaceGameplay != nullptr && SurfaceGameplay->SupportsPlanet(Planet))
		{
			return Candidate;
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.OverrideLevel = World->PersistentLevel;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AActor>(Definition.ControllerClass, FTransform::Identity, SpawnParameters);
}
