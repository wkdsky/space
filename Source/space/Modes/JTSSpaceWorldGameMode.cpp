// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Modes/JTSSpaceWorldGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/UI/JTSPrototypeHUD.h"
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

	AJTSSpaceWorldManager* const Manager = FindOrCreateSpaceWorldManager();
	if (!IsValid(Manager))
	{
		UE_LOG(LogTemp, Error, TEXT("SpaceWorldGameMode could not create or find its SpaceWorldManager."));
		return;
	}

	SpaceWorldManager = Manager;
	Manager->InitializeCurrentPlanet();
	Manager->SetTravelState(EJTSSpaceTravelState::Surface);

	if (UWorld* const World = GetWorld())
	{
		if (AJTSGameState* const JTSGameState = World->GetGameState<AJTSGameState>())
		{
			JTSGameState->SetFailureReason(EJTSFailureReason::None);
			JTSGameState->SetGameplayPhase(EJTSGameplayPhase::MoonExploration);
		}
	}

	TrySpawnInitialSurfaceCharacter();
}

void AJTSSpaceWorldGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SurfaceSpawnRetryTimerHandle);
	SpaceWorldManager.Reset();

	Super::EndPlay(EndPlayReason);
}

void AJTSSpaceWorldGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	TrySpawnInitialSurfaceCharacter();
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

void AJTSSpaceWorldGameMode::TrySpawnInitialSurfaceCharacter()
{
	if (bInitialSurfaceCharacterSpawned || !SpaceWorldManager.IsValid())
	{
		return;
	}

	UWorld* const World = GetWorld();
	APlayerController* const PlayerController = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	AJTSPlanetAnchor* const Planet = SpaceWorldManager->GetCurrentPlanet();
	if (!IsValid(World) || !IsValid(PlayerController) || !IsValid(Planet) || !Planet->HasGameplaySurface())
	{
		if (World != nullptr)
		{
			World->GetTimerManager().SetTimer(SurfaceSpawnRetryTimerHandle, this, &AJTSSpaceWorldGameMode::TrySpawnInitialSurfaceCharacter, 0.05f, false);
		}
		return;
	}

	if (!bInitialGroundedSpacecraftInitialized)
	{
		bInitialGroundedSpacecraftInitialized = TrySpawnInitialGroundedSpacecraft(Planet);
	}

	if (!SpawnAndSnapCharacter(PlayerController, Planet))
	{
		if (!bLoggedSurfaceSnapFailure)
		{
			UE_LOG(LogTemp, Warning, TEXT("SpaceWorld initial character is waiting for a valid real mesh surface trace on planet %s."), *Planet->GetPlanetId().ToString());
			bLoggedSurfaceSnapFailure = true;
		}
		World->GetTimerManager().SetTimer(SurfaceSpawnRetryTimerHandle, this, &AJTSSpaceWorldGameMode::TrySpawnInitialSurfaceCharacter, 0.05f, false);
		return;
	}

	bInitialSurfaceCharacterSpawned = true;
	SpaceWorldManager->SetSurfaceGameplayReady(true);
	if (AJTSPlayerController* const JTSPlayerController = Cast<AJTSPlayerController>(PlayerController))
	{
		JTSPlayerController->ApplySpaceWorldInputMode();
	}

	UE_LOG(LogTemp, Log, TEXT("SpaceWorld initial surface character spawned on real planet %s."), *Planet->GetPlanetId().ToString());
}

bool AJTSSpaceWorldGameMode::SpawnAndSnapCharacter(APlayerController* PlayerController, AJTSPlanetAnchor* Planet)
{
	if (!IsValid(PlayerController) || !IsValid(Planet))
	{
		return false;
	}

	AActor* const PlayerStart = FindPlayerStart(PlayerController);
	const FTransform SpawnReferenceTransform = IsValid(PlayerStart)
		? PlayerStart->GetActorTransform()
		: Planet->GetLandingTransform();

	AJTSCharacter* Character = Cast<AJTSCharacter>(PlayerController->GetPawn());
	if (!IsValid(Character))
	{
		if (APawn* const ExistingPawn = PlayerController->GetPawn())
		{
			PlayerController->UnPossess();
			ExistingPawn->Destroy();
		}

		RestartPlayerAtTransform(PlayerController, SpawnReferenceTransform);
		Character = Cast<AJTSCharacter>(PlayerController->GetPawn());
	}

	if (!IsValid(Character))
	{
		return false;
	}

	Character->SetGameplayPlanet(Planet);
	return Character->SnapToPlanetSurface(Planet, SpawnReferenceTransform.GetLocation());
}

bool AJTSSpaceWorldGameMode::TrySpawnInitialGroundedSpacecraft(AJTSPlanetAnchor* Planet)
{
	if (!IsValid(Planet))
	{
		return false;
	}

	FTransform LandingSurfaceTransform;
	if (!Planet->GetLandingSurfaceTransform(LandingSurfaceTransform))
	{
		if (!bLoggedLandingAnchorFailure)
		{
			UE_LOG(LogTemp, Warning, TEXT("SpaceWorld initial spacecraft is waiting for a valid real-surface LandingAnchorActor on planet %s."),
				*Planet->GetPlanetId().ToString());
			bLoggedLandingAnchorFailure = true;
		}
		return false;
	}

	AJTSSpacecraftActor* Spacecraft = FindExistingGameplaySpacecraft();
	bool bSpawnedSpacecraft = false;
	if (!IsValid(Spacecraft))
	{
		UWorld* const World = GetWorld();
		if (World == nullptr || World->PersistentLevel == nullptr)
		{
			return false;
		}

		TSubclassOf<AJTSSpacecraftActor> ShipClass = SpacecraftClass;
		if (ShipClass == nullptr)
		{
			ShipClass = AJTSSpacecraftActor::StaticClass();
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = TEXT("JTSGameplaySpacecraft");
		SpawnParameters.OverrideLevel = World->PersistentLevel;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Spacecraft = World->SpawnActor<AJTSSpacecraftActor>(ShipClass, LandingSurfaceTransform, SpawnParameters);
		bSpawnedSpacecraft = IsValid(Spacecraft);
	}

	if (!IsValid(Spacecraft))
	{
		UE_LOG(LogTemp, Warning, TEXT("SpaceWorld could not create its persistent gameplay spacecraft."));
		return false;
	}

	Spacecraft->RestorePersistentStorage();
	if (!Spacecraft->SnapSpacecraftToSurfaceTransform(Planet, LandingSurfaceTransform))
	{
		if (bSpawnedSpacecraft)
		{
			Spacecraft->Destroy();
		}
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("SpaceWorld persistent spacecraft grounded on real planet %s without possession."),
		*Planet->GetPlanetId().ToString());
	return true;
}

AJTSSpacecraftActor* AJTSSpaceWorldGameMode::FindExistingGameplaySpacecraft()
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	TArray<AJTSSpacecraftActor*> SpacecraftActors;
	for (TActorIterator<AJTSSpacecraftActor> It(World); It; ++It)
	{
		if (AJTSSpacecraftActor* const Candidate = *It; IsValid(Candidate))
		{
			SpacecraftActors.Add(Candidate);
		}
	}

	if (SpacecraftActors.IsEmpty())
	{
		return nullptr;
	}

	SpacecraftActors.Sort([](const AJTSSpacecraftActor& Left, const AJTSSpacecraftActor& Right)
	{
		return Left.GetPathName() < Right.GetPathName();
	});
	if (SpacecraftActors.Num() > 1 && !bLoggedMultipleSpacecraft)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpaceWorld found %d gameplay spacecraft actors; using %s and not spawning another."),
			SpacecraftActors.Num(), *GetNameSafe(SpacecraftActors[0]));
		bLoggedMultipleSpacecraft = true;
	}

	return SpacecraftActors[0];
}
