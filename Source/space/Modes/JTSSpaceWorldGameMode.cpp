// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Modes/JTSSpaceWorldGameMode.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/UI/JTSPrototypeHUD.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "space/World/JTSPlanetAnchor.h"
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
	SpacecraftClass = AJTSSpacecraftActor::StaticClass();
}

void AJTSSpaceWorldGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// A newly opened persistent world starts its GameState at WaitingToStart. Reuse the
	// existing unpaused arrival presentation while MoonSurface streams; otherwise the
	// legacy WaitingToStart input policy can pause the timer-driven arrival sequence.
	if (AJTSGameState* const JTSGameState = World->GetGameState<AJTSGameState>())
	{
		JTSGameState->SetFailureReason(EJTSFailureReason::None);
		JTSGameState->SetGameplayPhase(EJTSGameplayPhase::MoonArrivalSuccess);
	}

	AJTSSpaceWorldManager* Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
	if (!IsValid(Manager))
	{
		TSubclassOf<AJTSSpaceWorldManager> ManagerClass = SpaceWorldManagerClass;
		if (ManagerClass == nullptr)
		{
			ManagerClass = AJTSSpaceWorldManager::StaticClass();
		}
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = TEXT("JTSSpaceWorldManager");
		SpawnParameters.OverrideLevel = World->PersistentLevel;
		Manager = World->SpawnActor<AJTSSpaceWorldManager>(ManagerClass, FTransform::Identity, SpawnParameters);
	}

	if (!IsValid(Manager))
	{
		UE_LOG(LogTemp, Error, TEXT("SpaceWorldGameMode could not create its SpaceWorldManager."));
		return;
	}

	SpaceWorldManager = Manager;
	Manager->OnInitialSurfaceLevelReady().AddUObject(this, &AJTSSpaceWorldGameMode::HandleInitialSurfaceLevelReady);
	Manager->BeginInitialArrival();
	TryCompleteInitialSurfaceArrival();
}

void AJTSSpaceWorldGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SpaceWorldManager.IsValid())
	{
		SpaceWorldManager->OnInitialSurfaceLevelReady().RemoveAll(this);
	}
	GetWorldTimerManager().ClearTimer(ArrivalRetryTimerHandle);
	PendingSurfaceController.Reset();
	PersistentSpacecraft.Reset();
	SpaceWorldManager.Reset();

	Super::EndPlay(EndPlayReason);
}

void AJTSSpaceWorldGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	TryCompleteInitialSurfaceArrival();
}

void AJTSSpaceWorldGameMode::HandleInitialSurfaceLevelReady(AJTSMoonSurfaceController* SurfaceController)
{
	PendingSurfaceController = SurfaceController;
	TryCompleteInitialSurfaceArrival();
}

void AJTSSpaceWorldGameMode::TryCompleteInitialSurfaceArrival()
{
	if (bPersistentActorsPlaced)
	{
		PollSurfaceGameplayReady();
		return;
	}

	AJTSMoonSurfaceController* const SurfaceController = PendingSurfaceController.Get();
	UWorld* const World = GetWorld();
	APlayerController* const PlayerController = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	if (!IsValid(SurfaceController) || !IsValid(PlayerController) || !SpaceWorldManager.IsValid())
	{
		if (World != nullptr)
		{
			World->GetTimerManager().SetTimer(ArrivalRetryTimerHandle, this, &AJTSSpaceWorldGameMode::TryCompleteInitialSurfaceArrival, 0.05f, false);
		}
		return;
	}

	AJTSSpacecraftActor* const Spacecraft = CreateOrAdoptSurfaceSpacecraft(SurfaceController);
	if (!IsValid(Spacecraft))
	{
		World->GetTimerManager().SetTimer(ArrivalRetryTimerHandle, this, &AJTSSpaceWorldGameMode::TryCompleteInitialSurfaceArrival, 0.05f, false);
		return;
	}

	if (!SpawnOrMovePlayer(SurfaceController))
	{
		World->GetTimerManager().SetTimer(ArrivalRetryTimerHandle, this, &AJTSSpaceWorldGameMode::TryCompleteInitialSurfaceArrival, 0.05f, false);
		return;
	}

	bPersistentActorsPlaced = true;
	SurfaceController->RequestSurfaceGameplayInitialization();
	PollSurfaceGameplayReady();
}

void AJTSSpaceWorldGameMode::PollSurfaceGameplayReady()
{
	AJTSMoonSurfaceController* const SurfaceController = PendingSurfaceController.Get();
	UWorld* const World = GetWorld();
	if (!IsValid(SurfaceController) || World == nullptr)
	{
		return;
	}

	if (!SurfaceController->IsSurfaceGameplayInitialized())
	{
		World->GetTimerManager().SetTimer(ArrivalRetryTimerHandle, this, &AJTSSpaceWorldGameMode::PollSurfaceGameplayReady, 0.05f, false);
		return;
	}

	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(World->GetFirstPlayerController()))
	{
		PlayerController->ApplySpaceWorldInputMode();
	}
}

AJTSSpacecraftActor* AJTSSpaceWorldGameMode::FindPersistentSpacecraft() const
{
	if (PersistentSpacecraft.IsValid())
	{
		return PersistentSpacecraft.Get();
	}

	const UWorld* const World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr)
	{
		return nullptr;
	}

	AJTSSpacecraftActor* Result = nullptr;
	for (AActor* const Actor : World->PersistentLevel->Actors)
	{
		AJTSSpacecraftActor* const Candidate = Cast<AJTSSpacecraftActor>(Actor);
		if (!IsValid(Candidate))
		{
			continue;
		}

		if (IsValid(Result))
		{
			UE_LOG(LogTemp, Error, TEXT("L_SpaceWorld has more than one persistent spacecraft actor; initial Moon arrival cannot choose one."));
			return nullptr;
		}
		Result = Candidate;
	}

	return Result;
}

AJTSSpacecraftActor* AJTSSpaceWorldGameMode::CreateOrAdoptSurfaceSpacecraft(AJTSMoonSurfaceController* SurfaceController)
{
	if (!IsValid(SurfaceController))
	{
		return nullptr;
	}

	AJTSSpacecraftActor* const SurfaceSpacecraft = SurfaceController->GetSpacecraft();
	AJTSSpacecraftActor* const ExistingPersistentSpacecraft = FindPersistentSpacecraft();
	if (IsValid(SurfaceSpacecraft) && IsValid(ExistingPersistentSpacecraft) && SurfaceSpacecraft != ExistingPersistentSpacecraft)
	{
		UE_LOG(LogTemp, Error, TEXT("Moon arrival found both a streamed-surface spacecraft and a persistent spacecraft. Remove one to preserve a single ship."));
		return nullptr;
	}

	AJTSSpacecraftActor* Spacecraft = IsValid(SurfaceSpacecraft) ? SurfaceSpacecraft : ExistingPersistentSpacecraft;
	if (!IsValid(Spacecraft))
	{
		UWorld* const World = GetWorld();
		AJTSPlanetAnchor* const Planet = SpaceWorldManager.IsValid() ? SpaceWorldManager->GetCurrentPlanet() : nullptr;
		TSubclassOf<AJTSSpacecraftActor> SpawnClass = SpacecraftClass;
		if (SpawnClass == nullptr)
		{
			SpawnClass = AJTSSpacecraftActor::StaticClass();
		}
		if (World == nullptr || !IsValid(Planet) || SpawnClass == nullptr)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = TEXT("JTSPersistentSpacecraft");
		SpawnParameters.OverrideLevel = World->PersistentLevel;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Spacecraft = World->SpawnActor<AJTSSpacecraftActor>(
			SpawnClass,
			SurfaceController->GetSurfaceSpacecraftSpawnTransform(Planet->GetLandingTransform()),
			SpawnParameters);
	}

	if (!IsValid(Spacecraft))
	{
		return nullptr;
	}

	PersistentSpacecraft = Spacecraft;
	SurfaceController->SetSurfaceSpacecraft(Spacecraft);
	Spacecraft->RestoreStorageForMoonTravel();
	return Spacecraft;
}

bool AJTSSpaceWorldGameMode::SpawnOrMovePlayer(AJTSMoonSurfaceController* SurfaceController)
{
	UWorld* const World = GetWorld();
	APlayerController* const PlayerController = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	AJTSPlanetAnchor* const Planet = SpaceWorldManager.IsValid() ? SpaceWorldManager->GetCurrentPlanet() : nullptr;
	if (!IsValid(SurfaceController) || !IsValid(PlayerController) || !IsValid(Planet))
	{
		return false;
	}

	if (APawn* const ExistingPawn = PlayerController->GetPawn())
	{
		PlayerController->UnPossess();
		ExistingPawn->Destroy();
	}

	RestartPlayerAtTransform(PlayerController, SurfaceController->GetSurfacePlayerSpawnTransform(Planet->GetLandingTransform()));
	APawn* const SpawnedPawn = PlayerController->GetPawn();
	if (!IsValid(SpawnedPawn))
	{
		UE_LOG(LogTemp, Error, TEXT("SpaceWorld Moon arrival could not spawn the player at the configured surface transform."));
		return false;
	}

	SurfaceController->RegisterSurfaceRuntimeActor(SpawnedPawn);
	return true;
}
