// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Modes/JTSEarthGameMode.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/SoftObjectPath.h"
#include "space/Core/JTSGameInstance.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/World/JTSResourceSpawnArea.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/UI/JTSPrototypeHUD.h"

AJTSEarthGameMode::AJTSEarthGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	DefaultPawnClass = AJTSCharacter::StaticClass();
	PlayerControllerClass = AJTSPlayerController::StaticClass();
	GameStateClass = AJTSGameState::StaticClass();
	HUDClass = AJTSPrototypeHUD::StaticClass();
}

float AJTSEarthGameMode::GetEarthCollectionDuration() const
{
	return FMath::Max(0.0f, EarthCollectionDuration);
}

void AJTSEarthGameMode::BeginPlay()
{
	Super::BeginPlay();

	bEarthCollectionStarted = false;
	bEarthCollectionFinished = false;
	bLaunchSequenceStarted = false;
	bLaunchOutcomeResolved = false;
	bMoonTravelScheduled = false;

	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EarthCollectionTimerHandle);
		World->GetTimerManager().ClearTimer(LaunchSequenceTimerHandle);
		World->GetTimerManager().ClearTimer(MoonTransitionTimerHandle);
	}

	if (AJTSGameState* const JTSGameState = GetJTSGameState())
	{
		JTSGameState->SetFailureReason(EJTSFailureReason::None);
		JTSGameState->SetEarthCollectionEndTime(0.0);
		JTSGameState->SetGameplayPhase(EJTSGameplayPhase::WaitingToStart);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Jump to Space could not enter Waiting To Start because its GameState is unavailable."));
	}
}

void AJTSEarthGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EarthCollectionTimerHandle);
		World->GetTimerManager().ClearTimer(LaunchSequenceTimerHandle);
		World->GetTimerManager().ClearTimer(MoonTransitionTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AJTSEarthGameMode::StartEarthCollection()
{
	if (bEarthCollectionStarted || bEarthCollectionFinished || bLaunchSequenceStarted || bLaunchOutcomeResolved)
	{
		return;
	}

	AJTSGameState* const JTSGameState = GetJTSGameState();
	UWorld* const World = GetWorld();
	if (!IsValid(JTSGameState) || World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Jump to Space could not start Earth collection because its GameState is unavailable."));
		return;
	}

	if (!JTSGameState->IsWaitingToStart())
	{
		return;
	}

	if (UJTSGameInstance* const GameInstance = World->GetGameInstance<UJTSGameInstance>())
	{
		GameInstance->ClearPersistedSpacecraftStorage();
	}

	bEarthCollectionStarted = true;
	JTSGameState->SetFailureReason(EJTSFailureReason::None);

	const float CollectionDuration = GetEarthCollectionDuration();
	JTSGameState->SetEarthCollectionEndTime(
		static_cast<double>(World->GetTimeSeconds()) + static_cast<double>(CollectionDuration));
	JTSGameState->SetGameplayPhase(EJTSGameplayPhase::EarthCollection);

	AJTSResourceSpawnArea* ResourceSpawnArea = nullptr;
	int32 ResourceSpawnAreaCount = 0;
	for (TActorIterator<AJTSResourceSpawnArea> It(World); It; ++It)
	{
		if (!IsValid(*It))
		{
			continue;
		}

		++ResourceSpawnAreaCount;
		if (ResourceSpawnArea == nullptr)
		{
			ResourceSpawnArea = *It;
		}
	}

	if (ResourceSpawnArea == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Jump to Space found no JTSResourceSpawnArea. Earth collection will start without automatically generated resources."));
	}
	else
	{
		if (ResourceSpawnAreaCount > 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("Jump to Space found %d JTSResourceSpawnArea actors; using the first valid area only."), ResourceSpawnAreaCount);
		}

		ResourceSpawnArea->ApplyEarthSpawnSettings(ResourceSpawnSettings);
		ResourceSpawnArea->GenerateResources();
	}

	const float RemainingDuration = JTSGameState->GetEarthCollectionRemainingTime();

	if (RemainingDuration > 0.0f)
	{
		World->GetTimerManager().SetTimer(
			EarthCollectionTimerHandle,
			this,
			&AJTSEarthGameMode::FinishEarthCollection,
			RemainingDuration,
			false);
	}

	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(World->GetFirstPlayerController()))
	{
		PlayerController->ApplyEarthCollectionInputMode();
	}

	if (RemainingDuration <= 0.0f)
	{
		FinishEarthCollection();
	}

}

bool AJTSEarthGameMode::IsEarthCollectionActive() const
{
	const AJTSGameState* const JTSGameState = GetJTSGameState();
	return IsValid(JTSGameState) && JTSGameState->IsEarthCollectionActive();
}

float AJTSEarthGameMode::GetMinimumFuelRequired() const
{
	return FMath::Max(0.0f, MinimumFuelRequired);
}

float AJTSEarthGameMode::GetLaunchSequenceDuration() const
{
	return FMath::Max(0.0f, LaunchSequenceDuration);
}

float AJTSEarthGameMode::GetMoonTransitionDelay() const
{
	return FMath::Max(0.0f, MoonTransitionDelay);
}

void AJTSEarthGameMode::FinishEarthCollection()
{
	if (!bEarthCollectionStarted || bEarthCollectionFinished || bLaunchSequenceStarted || bLaunchOutcomeResolved)
	{
		return;
	}

	bEarthCollectionFinished = true;

	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EarthCollectionTimerHandle);
	}

	AJTSSpacecraftActor* Spacecraft = nullptr;
	int32 SpacecraftCount = 0;
	APawn* PlayerPawn = nullptr;
	if (UWorld* const World = GetWorld())
	{
		PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
		for (TActorIterator<AJTSSpacecraftActor> It(World); It; ++It)
		{
			if (!IsValid(*It))
			{
				continue;
			}

			++SpacecraftCount;
			if (Spacecraft == nullptr)
			{
				Spacecraft = *It;
			}
		}
	}
	if (SpacecraftCount > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Jump to Space found %d spacecraft actors at the Earth collection deadline; using the first valid spacecraft only."), SpacecraftCount);
	}

	const bool bPlayerBoardedAtDeadline = IsValid(Spacecraft)
		&& IsValid(PlayerPawn)
		&& Spacecraft->IsPlayerBoarded(PlayerPawn);

	if (AJTSGameState* const JTSGameState = GetJTSGameState())
	{
		JTSGameState->SetGameplayPhase(EJTSGameplayPhase::EarthCollectionFinished);
		OnEarthCollectionFinished.Broadcast();

		if (!bPlayerBoardedAtDeadline)
		{
			JTSGameState->SetFailureReason(IsValid(Spacecraft)
				? EJTSFailureReason::NoTimelyBoarding
				: EJTSFailureReason::NoSpacecraft);
			JTSGameState->SetGameplayPhase(EJTSGameplayPhase::EarthCaptureFailure);
			return;
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Jump to Space could not finish Earth collection because its GameState is unavailable."));
	}

	StartLaunchSequence();
}

void AJTSEarthGameMode::StartLaunchSequence()
{
	if (!bEarthCollectionFinished || bLaunchSequenceStarted || bLaunchOutcomeResolved)
	{
		return;
	}

	AJTSGameState* const JTSGameState = GetJTSGameState();
	UWorld* const World = GetWorld();
	if (!IsValid(JTSGameState) || World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Jump to Space could not start the launch sequence because its GameState or World is unavailable."));
		return;
	}

	bLaunchSequenceStarted = true;
	JTSGameState->SetGameplayPhase(EJTSGameplayPhase::Launching);

	const float SequenceDuration = GetLaunchSequenceDuration();
	if (SequenceDuration <= 0.0f)
	{
		ResolveLaunchOutcome();
		return;
	}

	World->GetTimerManager().SetTimer(
		LaunchSequenceTimerHandle,
		this,
		&AJTSEarthGameMode::ResolveLaunchOutcome,
		SequenceDuration,
		false);
}

void AJTSEarthGameMode::ResolveLaunchOutcome()
{
	if (!bLaunchSequenceStarted || bLaunchOutcomeResolved)
	{
		return;
	}

	bLaunchOutcomeResolved = true;

	UWorld* const World = GetWorld();
	if (World != nullptr)
	{
		World->GetTimerManager().ClearTimer(LaunchSequenceTimerHandle);
	}

	AJTSSpacecraftActor* Spacecraft = nullptr;
	int32 SpacecraftCount = 0;
	if (World != nullptr)
	{
		for (TActorIterator<AJTSSpacecraftActor> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				++SpacecraftCount;
				if (Spacecraft == nullptr)
				{
					Spacecraft = *It;
				}
			}
		}
	}
	if (SpacecraftCount > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Jump to Space found %d spacecraft actors; using the first valid spacecraft only."), SpacecraftCount);
	}

	const int32 FuelCount = IsValid(Spacecraft) ? Spacecraft->GetFuelCount() : 0;
	const int32 LaunchFuelCost = FMath::CeilToInt(GetMinimumFuelRequired());
	if (!IsValid(Spacecraft))
	{
		UE_LOG(LogTemp, Warning, TEXT("Jump to Space could not find a spacecraft during launch resolution; treating the launch as fuel insufficient."));
	}

	const bool bHasEnoughFuel = IsValid(Spacecraft) && FuelCount >= LaunchFuelCost;
	UE_LOG(
		LogTemp,
		Log,
		TEXT("JumpToSpace Launch Resolve: Fuel=%d Required=%d Enough=%s"),
		FuelCount,
		LaunchFuelCost,
		bHasEnoughFuel ? TEXT("true") : TEXT("false"));

	if (AJTSGameState* const JTSGameState = GetJTSGameState())
	{
		if (bHasEnoughFuel)
		{
			UJTSGameInstance* const GameInstance = World != nullptr
				? World->GetGameInstance<UJTSGameInstance>()
				: nullptr;
			if (!IsValid(GameInstance))
			{
				UE_LOG(LogTemp, Error, TEXT("Jump to Space could not preserve spacecraft Storage because the configured GameInstance is not UJTSGameInstance."));
				JTSGameState->SetFailureReason(EJTSFailureReason::InvalidGameInstance);
				JTSGameState->SetGameplayPhase(EJTSGameplayPhase::EarthCaptureFailure);
				return;
			}

			if (LaunchFuelCost > 0
				&& !Spacecraft->TryConsumeResource(EJTSResourceType::Fuel, LaunchFuelCost))
			{
				UE_LOG(LogTemp, Error, TEXT("Jump to Space could not consume %d fuel from the spacecraft during launch."), LaunchFuelCost);
				JTSGameState->SetFailureReason(EJTSFailureReason::InsufficientFuel);
				JTSGameState->SetGameplayPhase(EJTSGameplayPhase::EarthCaptureFailure);
				return;
			}

			UE_LOG(
				LogTemp,
				Log,
				TEXT("JumpToSpace Launch Fuel Consumed: Before=%d Cost=%d Remaining=%d"),
				FuelCount,
				LaunchFuelCost,
				Spacecraft->GetFuelCount());

			GameInstance->SetPersistedSpacecraftStorage(Spacecraft->GetStorage());
			UE_LOG(
				LogTemp,
				Log,
				TEXT("JumpToSpace EarthToMoon Storage Saved: Fuel=%d Water=%.1f Food=%.1f Rock=%d Ore=%d"),
				Spacecraft->GetResourceAmount(EJTSResourceType::Fuel),
				static_cast<float>(Spacecraft->GetResourceAmount(EJTSResourceType::Water)),
				static_cast<float>(Spacecraft->GetResourceAmount(EJTSResourceType::Food)),
				Spacecraft->GetResourceAmount(EJTSResourceType::Rock),
				Spacecraft->GetResourceAmount(EJTSResourceType::Ore));
			JTSGameState->SetFailureReason(EJTSFailureReason::None);
			JTSGameState->SetGameplayPhase(EJTSGameplayPhase::MoonArrivalSuccess);
			BeginMoonTravel();
		}
		else
		{
			JTSGameState->SetFailureReason(IsValid(Spacecraft)
				? EJTSFailureReason::InsufficientFuel
				: EJTSFailureReason::NoSpacecraft);
			JTSGameState->SetGameplayPhase(EJTSGameplayPhase::EarthCaptureFailure);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Jump to Space could not store the launch outcome because its GameState is unavailable."));
	}
}

void AJTSEarthGameMode::BeginMoonTravel()
{
	if (bMoonTravelScheduled)
	{
		return;
	}

	FString MoonLevelPackageName;
	if (!ResolveMoonLevelPackageName(MoonLevelPackageName))
	{
		UE_LOG(LogTemp, Error, TEXT("JumpToSpace EarthToMoon Travel Failed: MoonLevel is not configured."));
		return;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("JumpToSpace EarthToMoon Travel Failed: the current World is unavailable."));
		return;
	}

	bMoonTravelScheduled = true;
	const float TransitionDelay = GetMoonTransitionDelay();
	UE_LOG(
		LogTemp,
		Log,
		TEXT("JumpToSpace EarthToMoon Travel: Level=%s Delay=%.2f"),
		*MoonLevelPackageName,
		TransitionDelay);

	if (TransitionDelay <= 0.0f)
	{
		TravelToMoon();
		return;
	}

	World->GetTimerManager().SetTimer(
		MoonTransitionTimerHandle,
		this,
		&AJTSEarthGameMode::TravelToMoon,
		TransitionDelay,
		false);
}

void AJTSEarthGameMode::TravelToMoon()
{
	FString MoonLevelPackageName;
	if (!ResolveMoonLevelPackageName(MoonLevelPackageName))
	{
		bMoonTravelScheduled = false;
		UE_LOG(LogTemp, Error, TEXT("JumpToSpace EarthToMoon Travel Failed: MoonLevel is not configured."));
		return;
	}

	UGameplayStatics::OpenLevel(this, FName(*MoonLevelPackageName));
}

bool AJTSEarthGameMode::ResolveMoonLevelPackageName(FString& OutPackageName) const
{
	OutPackageName.Reset();

	const FSoftObjectPath MoonLevelPath = MoonLevel.ToSoftObjectPath();
	if (!MoonLevelPath.IsValid())
	{
		return false;
	}

	OutPackageName = MoonLevelPath.GetLongPackageName();
	return !OutPackageName.IsEmpty();
}

AJTSGameState* AJTSEarthGameMode::GetJTSGameState() const
{
	UWorld* const World = GetWorld();
	return World != nullptr ? World->GetGameState<AJTSGameState>() : nullptr;
}
