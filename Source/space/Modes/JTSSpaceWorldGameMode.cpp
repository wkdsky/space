// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Modes/JTSSpaceWorldGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "space/Core/JTSGameInstance.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/UI/JTSPrototypeHUD.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSpaceWorldManager.h"

namespace
{
	constexpr float InitialSurfaceSpawnRetryInterval = 0.10f;
	constexpr int32 MaxInitialSurfaceSpawnRetries = 5;
}

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

	bInitialSurfaceCharacterSpawned = false;
	bInitialGroundedSpacecraftInitialized = false;
	InitialSurfaceSpawnRetryCount = 0;
	bInitialSurfaceSpawnRetryExhausted = false;
	bLoggedSurfaceSnapFailure = false;
	bLoggedLandingAnchorFailure = false;
	bLoggedMultipleSpacecraft = false;
	GetWorldTimerManager().ClearTimer(SurfaceSpawnRetryTimerHandle);

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
	if (bInitialSurfaceSpawnRetryExhausted
		|| (bInitialSurfaceCharacterSpawned && bInitialGroundedSpacecraftInitialized)
		|| !SpaceWorldManager.IsValid())
	{
		return;
	}

	UWorld* const World = GetWorld();
	APlayerController* const PlayerController = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	AJTSPlanetAnchor* const Planet = SpaceWorldManager->GetCurrentPlanet();
	if (IsValid(World) && IsValid(Planet) && Planet->HasGameplaySurface())
	{
		if (!bInitialSurfaceCharacterSpawned && IsValid(PlayerController))
		{
			bInitialSurfaceCharacterSpawned = SpawnAndSnapCharacter(PlayerController, Planet);
		}

		AJTSCharacter* const Character = IsValid(PlayerController)
			? Cast<AJTSCharacter>(PlayerController->GetPawn())
			: nullptr;
		if (bInitialSurfaceCharacterSpawned && !bInitialGroundedSpacecraftInitialized)
		{
			bInitialGroundedSpacecraftInitialized = TrySpawnInitialGroundedSpacecraft(Planet, Character);
		}

		if (bInitialSurfaceCharacterSpawned && bInitialGroundedSpacecraftInitialized)
		{
			SpaceWorldManager->SetSurfaceGameplayReady(true);
			if (AJTSPlayerController* const JTSPlayerController = Cast<AJTSPlayerController>(PlayerController))
			{
				JTSPlayerController->ApplySpaceWorldInputMode();
			}

			UE_LOG(LogTemp, Log, TEXT("SpaceWorld initial player and spacecraft are ready on real planet %s."), *Planet->GetPlanetId().ToString());
		}
	}

	if (bInitialSurfaceCharacterSpawned && bInitialGroundedSpacecraftInitialized)
	{
		if (World != nullptr)
		{
			World->GetTimerManager().ClearTimer(SurfaceSpawnRetryTimerHandle);
		}
		return;
	}

	ScheduleInitialSurfaceSpawnRetry(PlayerController, Planet);
}

void AJTSSpaceWorldGameMode::ScheduleInitialSurfaceSpawnRetry(APlayerController* PlayerController, AJTSPlanetAnchor* Planet)
{
	UWorld* const World = GetWorld();
	if (World == nullptr || bInitialSurfaceSpawnRetryExhausted)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (TimerManager.IsTimerActive(SurfaceSpawnRetryTimerHandle))
	{
		return;
	}

	if (InitialSurfaceSpawnRetryCount >= MaxInitialSurfaceSpawnRetries)
	{
		TimerManager.ClearTimer(SurfaceSpawnRetryTimerHandle);
		bInitialSurfaceSpawnRetryExhausted = true;
		LogInitialSurfaceInitializationFailure(PlayerController, Planet);
		return;
	}

	++InitialSurfaceSpawnRetryCount;
	TimerManager.SetTimer(
		SurfaceSpawnRetryTimerHandle,
		this,
		&AJTSSpaceWorldGameMode::TrySpawnInitialSurfaceCharacter,
		InitialSurfaceSpawnRetryInterval,
		false);
}

void AJTSSpaceWorldGameMode::LogInitialSurfaceInitializationFailure(
	APlayerController* PlayerController,
	AJTSPlanetAnchor* Planet)
{
	const FString PlanetId = IsValid(Planet) ? Planet->GetPlanetId().ToString() : TEXT("<unresolved>");
	const FString CharacterName = IsValid(PlayerController)
		? GetNameSafe(PlayerController->GetPawn())
		: TEXT("<unresolved>");
	const AActor* const PlayerStart = IsValid(PlayerController) ? FindPlayerStart(PlayerController) : nullptr;
	const FVector TraceReferenceLocation = IsValid(PlayerStart)
		? PlayerStart->GetActorLocation()
		: (IsValid(Planet) ? Planet->GetLandingTransform().GetLocation() : FVector::ZeroVector);
	const FString SurfaceActorName = IsValid(Planet) ? GetNameSafe(Planet->GetGameplaySurfaceActor()) : TEXT("<unresolved>");
	const FString SurfaceComponentName = IsValid(Planet) ? GetNameSafe(Planet->GetGameplaySurfaceComponent()) : TEXT("<unresolved>");

	if (!bInitialSurfaceCharacterSpawned && !bLoggedSurfaceSnapFailure)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("SpaceWorld initial character surface placement failed after %d retries. PlanetId=%s Character=%s TraceReferenceLocation=%s GameplaySurfaceActor=%s GameplaySurfaceComponent=%s."),
			MaxInitialSurfaceSpawnRetries,
			*PlanetId,
			*CharacterName,
			*TraceReferenceLocation.ToCompactString(),
			*SurfaceActorName,
			*SurfaceComponentName);
		bLoggedSurfaceSnapFailure = true;
	}

	if (!bInitialGroundedSpacecraftInitialized && !bLoggedLandingAnchorFailure)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("SpaceWorld initial spacecraft grounding failed after %d retries. PlanetId=%s GameplaySurfaceActor=%s GameplaySurfaceComponent=%s."),
			MaxInitialSurfaceSpawnRetries,
			*PlanetId,
			*SurfaceActorName,
			*SurfaceComponentName);
		bLoggedLandingAnchorFailure = true;
	}
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

bool AJTSSpaceWorldGameMode::ResolveInitialSpacecraftLandingTransform(
	AJTSPlanetAnchor* Planet,
	const AJTSCharacter* Character,
	FTransform& OutLandingSurfaceTransform) const
{
	if (!IsValid(Planet)
		|| !IsValid(Character)
		|| !Planet->GetLandingSurfaceTransform(OutLandingSurfaceTransform))
	{
		return false;
	}

	const float MinimumPlayerDistance = FMath::Max(1.0f, InitialSpacecraftMinimumPlayerDistance);
	if (Planet->ApproximateSurfaceArcDistance(
		Character->GetActorLocation(),
		OutLandingSurfaceTransform.GetLocation()) >= MinimumPlayerDistance)
	{
		return true;
	}

	FJTSPlanetSurfaceFrame PlayerSurfaceFrame;
	const FVector LandingForward = OutLandingSurfaceTransform.GetUnitAxis(EAxis::X);
	if (!Planet->GetSurfaceFrameAt(Character->GetActorLocation(), LandingForward, PlayerSurfaceFrame))
	{
		return false;
	}

	const FVector SurfaceUp = PlayerSurfaceFrame.Up.GetSafeNormal();
	FVector PreferredDirection = FVector::VectorPlaneProject(LandingForward, SurfaceUp).GetSafeNormal();
	if (PreferredDirection.IsNearlyZero())
	{
		PreferredDirection = PlayerSurfaceFrame.Forward.GetSafeNormal();
	}
	FVector PerpendicularDirection = FVector::CrossProduct(SurfaceUp, PreferredDirection).GetSafeNormal();
	if (PerpendicularDirection.IsNearlyZero())
	{
		PerpendicularDirection = PlayerSurfaceFrame.Right.GetSafeNormal();
	}

	const FVector CandidateDirections[] = {
		PreferredDirection,
		PerpendicularDirection,
		-PreferredDirection,
		-PerpendicularDirection
	};
	for (const FVector& CandidateDirection : CandidateDirections)
	{
		if (CandidateDirection.IsNearlyZero())
		{
			continue;
		}

		const FVector PlayerRadialUp = Planet->GetRadialUpVector(PlayerSurfaceFrame.Location);
		const FVector CandidateTangentDirection = FVector::VectorPlaneProject(CandidateDirection, PlayerRadialUp).GetSafeNormal();
		const FVector RotationAxis = FVector::CrossProduct(PlayerRadialUp, CandidateTangentDirection).GetSafeNormal();
		if (CandidateTangentDirection.IsNearlyZero() || RotationAxis.IsNearlyZero())
		{
			continue;
		}

		// Move by an arc, not a straight tangent offset. A tangent offset always resolves to a
		// slightly shorter geodesic distance on a sphere and can therefore fail the safety test.
		const float CandidateArcDistance = MinimumPlayerDistance + FMath::Max(50.0f, MinimumPlayerDistance * 0.10f);
		const FVector CandidateRadialDirection = FQuat(
			RotationAxis,
			Planet->ArcDistanceToAngleRadians(CandidateArcDistance)).RotateVector(PlayerRadialUp).GetSafeNormal();
		const FVector CandidateReferenceLocation = Planet->GetPlanetCenter()
			+ CandidateRadialDirection * Planet->GetApproximateRadius();

		FJTSPlanetSurfaceFrame CandidateSurfaceFrame;
		if (!Planet->GetSurfaceFrameAt(CandidateReferenceLocation, PreferredDirection, CandidateSurfaceFrame)
			|| Planet->ApproximateSurfaceArcDistance(Character->GetActorLocation(), CandidateSurfaceFrame.Location) < MinimumPlayerDistance)
		{
			continue;
		}

		OutLandingSurfaceTransform = CandidateSurfaceFrame.Transform;
		return true;
	}

	return false;
}

bool AJTSSpaceWorldGameMode::TrySpawnInitialGroundedSpacecraft(AJTSPlanetAnchor* Planet, const AJTSCharacter* Character)
{
	if (!IsValid(Planet) || !IsValid(Character))
	{
		return false;
	}

	FTransform LandingSurfaceTransform;
	if (!ResolveInitialSpacecraftLandingTransform(Planet, Character, LandingSurfaceTransform))
	{
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

		TSubclassOf<AJTSSpacecraftActor> ShipClass;
		if (UJTSGameInstance* const GameInstance = World->GetGameInstance<UJTSGameInstance>();
			IsValid(GameInstance) && GameInstance->HasPersistedSpacecraftClass())
		{
			ShipClass = GameInstance->GetPersistedSpacecraftClass();
		}
		if (ShipClass == nullptr)
		{
			ShipClass = SpacecraftClass;
		}
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
