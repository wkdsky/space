// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Modes/JTSSpaceWorldGameMode.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "space/Core/JTSGameState.h"
#include "space/Components/JTSSpacecraftFlightMovementComponent.h"
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

	// L_SpaceWorld begins as an active flight chapter. MoonExploration is set only after
	// assisted landing has spawned the surface character and initialized the controller.
	if (AJTSGameState* const JTSGameState = World->GetGameState<AJTSGameState>())
	{
		JTSGameState->SetFailureReason(EJTSFailureReason::None);
		JTSGameState->SetGameplayPhase(EJTSGameplayPhase::SpaceFlight);
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
	Manager->InitializeCurrentPlanet();
	Manager->OnInitialSurfaceLevelReady().AddUObject(this, &AJTSSpaceWorldGameMode::HandleInitialSurfaceLevelReady);
	Manager->OnLandingRequested().AddUObject(this, &AJTSSpaceWorldGameMode::HandleLandingRequested);
	TryStartSpaceFlight();
}

void AJTSSpaceWorldGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SpaceWorldManager.IsValid())
	{
		SpaceWorldManager->OnInitialSurfaceLevelReady().RemoveAll(this);
		SpaceWorldManager->OnLandingRequested().RemoveAll(this);
	}
	if (AJTSSpacecraftActor* const Spacecraft = PersistentSpacecraft.Get())
	{
		if (UJTSSpacecraftFlightMovementComponent* const FlightMovement = Spacecraft->GetFlightMovementComponent())
		{
			FlightMovement->OnAssistedLandingCompleted.RemoveAll(this);
		}
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
	TryStartSpaceFlight();
}

void AJTSSpaceWorldGameMode::TryStartSpaceFlight()
{
	if (bFlightStarted || !SpaceWorldManager.IsValid())
	{
		return;
	}

	UWorld* const World = GetWorld();
	APlayerController* const PlayerController = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	AJTSPlanetAnchor* const Planet = SpaceWorldManager->GetCurrentPlanet();
	if (!IsValid(World) || !IsValid(PlayerController) || !IsValid(Planet))
	{
		if (World != nullptr)
		{
			World->GetTimerManager().SetTimer(ArrivalRetryTimerHandle, this, &AJTSSpaceWorldGameMode::TryStartSpaceFlight, 0.05f, false);
		}
		return;
	}

	AJTSSpacecraftActor* const Spacecraft = CreateOrAdoptFlightSpacecraft();
	if (!IsValid(Spacecraft))
	{
		World->GetTimerManager().SetTimer(ArrivalRetryTimerHandle, this, &AJTSSpaceWorldGameMode::TryStartSpaceFlight, 0.05f, false);
		return;
	}

	if (APawn* const ExistingPawn = PlayerController->GetPawn())
	{
		PlayerController->UnPossess();
		if (ExistingPawn != Spacecraft)
		{
			ExistingPawn->Destroy();
		}
	}

	Spacecraft->SetActorTransform(Planet->GetApproachEntryTransform(), false, nullptr, ETeleportType::TeleportPhysics);
	Spacecraft->SetFlightTargetPlanet(Planet);
	if (UJTSSpacecraftFlightMovementComponent* const FlightMovement = Spacecraft->GetFlightMovementComponent())
	{
		FlightMovement->OnAssistedLandingCompleted.RemoveAll(this);
		FlightMovement->OnAssistedLandingCompleted.AddUObject(this, &AJTSSpaceWorldGameMode::HandleAssistedLandingCompleted);
	}

	PlayerController->Possess(Spacecraft);
	SpaceWorldManager->SetTravelState(EJTSSpaceTravelState::SpaceFlight);
	bFlightStarted = true;
	UE_LOG(LogTemp, Log, TEXT("SpaceWorld Flight Started: Planet=%s EntryAltitude=%.1f"),
		*Planet->GetPlanetId().ToString(),
		Planet->GetExteriorAltitude(Spacecraft->GetActorLocation()));
	if (AJTSPlayerController* const JTSPlayerController = Cast<AJTSPlayerController>(PlayerController))
	{
		JTSPlayerController->ApplySpaceWorldInputMode();
	}
}

void AJTSSpaceWorldGameMode::HandleLandingRequested(AJTSPlanetAnchor* Planet)
{
	if (bLandingInProgress || !SpaceWorldManager.IsValid() || Planet != SpaceWorldManager->GetCurrentPlanet())
	{
		return;
	}

	AJTSSpacecraftActor* const Spacecraft = PersistentSpacecraft.Get();
	if (!IsValid(Spacecraft))
	{
		UE_LOG(LogTemp, Error, TEXT("SpaceWorld cannot begin assisted landing because its persistent spacecraft is missing."));
		return;
	}
	UJTSSpacecraftFlightMovementComponent* const FlightMovement = Spacecraft->GetFlightMovementComponent();
	if (!IsValid(FlightMovement))
	{
		UE_LOG(LogTemp, Error, TEXT("SpaceWorld cannot begin assisted landing because the spacecraft flight movement component is missing."));
		return;
	}

	if (!Spacecraft->BeginAssistedLanding(Planet->GetLandingTransform(), AssistedLandingDuration))
	{
		UE_LOG(LogTemp, Error, TEXT("SpaceWorld assisted landing could not start; returning to Approach."));
		SpaceWorldManager->SetTravelState(EJTSSpaceTravelState::Approach);
		return;
	}

	bLandingInProgress = true;
	UE_LOG(LogTemp, Log, TEXT("SpaceWorld Assisted Landing Started: Planet=%s Altitude=%.1f Duration=%.1fs"),
		*Planet->GetPlanetId().ToString(),
		Planet->GetExteriorAltitude(Spacecraft->GetActorLocation()),
		AssistedLandingDuration);
}

void AJTSSpaceWorldGameMode::HandleAssistedLandingCompleted()
{
	if (!bLandingInProgress || !SpaceWorldManager.IsValid())
	{
		return;
	}

	bLandingInProgress = false;
	SpaceWorldManager->SetTravelState(EJTSSpaceTravelState::Surface);
	UE_LOG(LogTemp, Log, TEXT("SpaceWorld Assisted Landing Completed."));
	TryCompleteSurfaceArrival();
}

void AJTSSpaceWorldGameMode::TryCompleteSurfaceArrival()
{
	if (bSurfaceArrivalCompleted || !SpaceWorldManager.IsValid())
	{
		return;
	}

	UWorld* const World = GetWorld();
	AJTSPlanetAnchor* const Planet = SpaceWorldManager->GetCurrentPlanet();
	if (!IsValid(World) || !IsValid(Planet)
		|| !SpaceWorldManager->IsSurfaceLevelLoaded(Planet)
		|| !SpaceWorldManager->IsSurfaceLevelVisible(Planet))
	{
		if (World != nullptr)
		{
			World->GetTimerManager().SetTimer(ArrivalRetryTimerHandle, this, &AJTSSpaceWorldGameMode::TryCompleteSurfaceArrival, 0.05f, false);
		}
		return;
	}

	AJTSMoonSurfaceController* const SurfaceController = SpaceWorldManager->EnsureCurrentSurfaceController();
	if (!IsValid(SurfaceController) || !BindPersistentSpacecraftToSurface(SurfaceController))
	{
		World->GetTimerManager().SetTimer(ArrivalRetryTimerHandle, this, &AJTSSpaceWorldGameMode::TryCompleteSurfaceArrival, 0.05f, false);
		return;
	}

	if (!SpawnOrMovePlayer(SurfaceController))
	{
		World->GetTimerManager().SetTimer(ArrivalRetryTimerHandle, this, &AJTSSpaceWorldGameMode::TryCompleteSurfaceArrival, 0.05f, false);
		return;
	}

	bSurfaceArrivalCompleted = true;
	PendingSurfaceController = SurfaceController;
	UE_LOG(LogTemp, Log, TEXT("SpaceWorld Surface Arrival: Persistent spacecraft retained; spawning surface character."));
	SurfaceController->RequestSurfaceGameplayInitialization();
	PollSurfaceGameplayReady();
}

AJTSSpacecraftActor* AJTSSpaceWorldGameMode::CreateOrAdoptFlightSpacecraft()
{
	bool bPersistentSpacecraftConflict = false;
	AJTSSpacecraftActor* Spacecraft = FindPersistentSpacecraft(bPersistentSpacecraftConflict);
	if (bPersistentSpacecraftConflict)
	{
		return nullptr;
	}
	if (IsValid(Spacecraft))
	{
		PersistentSpacecraft = Spacecraft;
		Spacecraft->RestorePersistentStorage();
		return Spacecraft;
	}

	UWorld* const World = GetWorld();
	AJTSPlanetAnchor* const Planet = SpaceWorldManager.IsValid() ? SpaceWorldManager->GetCurrentPlanet() : nullptr;
	TSubclassOf<AJTSSpacecraftActor> SpawnClass = SpacecraftClass;
	if (SpawnClass == nullptr)
	{
		SpawnClass = AJTSSpacecraftActor::StaticClass();
	}
	if (!IsValid(World) || !IsValid(Planet) || SpawnClass == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = TEXT("JTSPersistentSpacecraft");
	SpawnParameters.OverrideLevel = World->PersistentLevel;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Spacecraft = World->SpawnActor<AJTSSpacecraftActor>(SpawnClass, Planet->GetApproachEntryTransform(), SpawnParameters);
	if (IsValid(Spacecraft))
	{
		PersistentSpacecraft = Spacecraft;
		Spacecraft->RestorePersistentStorage();
	}
	return Spacecraft;
}

bool AJTSSpaceWorldGameMode::BindPersistentSpacecraftToSurface(AJTSMoonSurfaceController* SurfaceController)
{
	AJTSSpacecraftActor* const Spacecraft = PersistentSpacecraft.Get();
	if (!IsValid(SurfaceController) || !IsValid(Spacecraft))
	{
		return false;
	}

	AJTSSpacecraftActor* const SurfaceSpacecraft = SurfaceController->GetSpacecraft();
	if (IsValid(SurfaceSpacecraft) && SurfaceSpacecraft != Spacecraft)
	{
		UE_LOG(LogTemp, Error, TEXT("MoonSurface contains a second spacecraft. Remove it so L_SpaceWorld can retain its single persistent spacecraft."));
		return false;
	}

	SurfaceController->SetSurfaceSpacecraft(Spacecraft);
	return true;
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

AJTSSpacecraftActor* AJTSSpaceWorldGameMode::FindPersistentSpacecraft(bool& bOutConflict) const
{
	bOutConflict = false;
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
			bOutConflict = true;
			UE_LOG(LogTemp, Error, TEXT("L_SpaceWorld has more than one persistent spacecraft actor; SpaceWorld cannot choose a unique flight pawn."));
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
	bool bPersistentSpacecraftConflict = false;
	AJTSSpacecraftActor* const ExistingPersistentSpacecraft = FindPersistentSpacecraft(bPersistentSpacecraftConflict);
	if (bPersistentSpacecraftConflict)
	{
		return nullptr;
	}
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
	Spacecraft->RestorePersistentStorage();
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
		if (ExistingPawn != PersistentSpacecraft.Get())
		{
			ExistingPawn->Destroy();
		}
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
