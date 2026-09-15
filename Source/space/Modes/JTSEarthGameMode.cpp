// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Modes/JTSEarthGameMode.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/SoftObjectPath.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/World/JTSResourceSpawnArea.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/Systems/JTSOnlineSessionSubsystem.h"
#include "space/UI/JTSPrototypeHUD.h"

AJTSEarthGameMode::AJTSEarthGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	HUDClass = AJTSPrototypeHUD::StaticClass();
	PostEarthSpaceWorldLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(JTSMapPaths::SpaceWorldAsset));
}

float AJTSEarthGameMode::GetEarthCollectionDuration() const
{
	return FMath::Max(0.0f, EarthCollectionDuration);
}

void AJTSEarthGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (GetWorld() == nullptr || GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}

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
		JTSGameState->SetEarthLaunchFuelRequirement(GetMinimumFuelRequired());
		JTSGameState->SetEarthCollectionEndTime(0.0);
		// L_EarthLaunchPrototype is never a ready room. Arrival from L_PreLaunchLobby begins
		// the server-authoritative collection chapter immediately, so no lobby widget/HUD state
		// can leak across this boundary.
		StartEarthCollection();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Jump to Space could not enter Waiting To Start because its GameState is unavailable."));
	}
}

bool AJTSEarthGameMode::RequestStartExpedition(AJTSPlayerController* RequestingController)
{
	// Launch is intentionally owned by AJTSPreLaunchLobbyGameMode. Earth has no secondary
	// WaitingToStart state that a client could use to re-open a lobby or restart a chapter.
	(void)RequestingController;
	return false;
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
	if (GetWorld() == nullptr || GetWorld()->GetNetMode() == NM_Client || bEarthCollectionStarted || bEarthCollectionFinished || bLaunchSequenceStarted || bLaunchOutcomeResolved)
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

	if (UJTSExpeditionSubsystem* const Expedition = World->GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>())
	{
		if (!Expedition->HasActiveExpedition())
		{
			Expedition->StartNewExpedition(FGuid::NewGuid().ToString(EGuidFormats::Digits));
		}
		Expedition->SetCurrentMap(World->GetPackage()->GetName());
		Expedition->SetCurrentCheckpoint(TEXT("Earth Collection"));
		JTSGameState->SetExpeditionId(Expedition->GetSnapshot().ExpeditionId);
	}

	bEarthCollectionStarted = true;
	JTSGameState->SetFailureReason(EJTSFailureReason::None);
	JTSGameState->SetEarthLaunchFuelRequirement(GetMinimumFuelRequired());

	const float CollectionDuration = GetEarthCollectionDuration();
	JTSGameState->SetEarthCollectionEndTime(
		JTSGameState->GetSynchronizedServerTimeSeconds() + static_cast<double>(CollectionDuration));
	JTSGameState->SetGameplayPhase(EJTSGameplayPhase::EarthCollection);
	if (UJTSOnlineSessionSubsystem* const Online = World->GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>())
	{
		Online->UpdateExpeditionPhase(EJTSGameplayPhase::EarthCollection);
	}
	MarkAllPlayersActive();
	// Lobby movement is blocked through its local UI input mode, never by keeping the pawn disabled.
	// Restore a valid server movement mode here as a defensive boundary for old seamless-travel pawns.
	for (AJTSPlayerState* const PlayerState : GetActivePlayerStates())
	{
		if (AJTSCharacter* const PlayerCharacter = PlayerState != nullptr ? Cast<AJTSCharacter>(PlayerState->GetPawn()) : nullptr)
		{
			if (UCharacterMovementComponent* const Movement = PlayerCharacter->GetCharacterMovement(); Movement != nullptr && Movement->MovementMode == MOVE_None)
			{
				Movement->SetMovementMode(MOVE_Walking);
			}
		}
	}

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
	if (GetWorld() == nullptr || GetWorld()->GetNetMode() == NM_Client || !bEarthCollectionStarted || bEarthCollectionFinished || bLaunchSequenceStarted || bLaunchOutcomeResolved)
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
	if (UWorld* const World = GetWorld())
	{
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

	bool bPlayerBoardedAtDeadline = bRequireAllPlayersBoarded ? IsValid(Spacecraft) : false;
	int32 BoardedPlayerCount = 0;
	for (AJTSPlayerState* const PlayerState : GetActivePlayerStates())
	{
		APawn* const PlayerPawn = PlayerState != nullptr ? PlayerState->GetPawn() : nullptr;
		const bool bThisPlayerBoarded = IsValid(Spacecraft) && IsValid(PlayerPawn) && Spacecraft->IsPlayerBoarded(PlayerPawn);
		BoardedPlayerCount += bThisPlayerBoarded ? 1 : 0;
		if (bRequireAllPlayersBoarded)
		{
			bPlayerBoardedAtDeadline &= bThisPlayerBoarded;
		}
		else
		{
			bPlayerBoardedAtDeadline |= bThisPlayerBoarded;
		}
	}

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
			UJTSExpeditionSubsystem* const Expedition = World != nullptr
				? World->GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>()
				: nullptr;
			if (!IsValid(Expedition))
			{
				UE_LOG(LogTemp, Error, TEXT("Jump to Space could not preserve spacecraft storage because the Expedition subsystem is unavailable."));
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

			Expedition->SetSpacecraftSnapshot(Spacecraft->GetClass(), Spacecraft->GetStorage());
			Expedition->CaptureWorldState(JTSGameState, Spacecraft);
			Expedition->RequestSave();
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

	FString SpaceWorldLevelPackageName;
	if (!ResolvePostEarthSpaceWorldLevelPackageName(SpaceWorldLevelPackageName))
	{
		UE_LOG(LogTemp, Error, TEXT("JumpToSpace Earth launch travel failed: PostEarthSpaceWorldLevel is not configured."));
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
		TEXT("JumpToSpace EarthToSpaceWorld Travel: Level=%s Delay=%.2f"),
		*SpaceWorldLevelPackageName,
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
	FString SpaceWorldLevelPackageName;
	if (!ResolvePostEarthSpaceWorldLevelPackageName(SpaceWorldLevelPackageName))
	{
		bMoonTravelScheduled = false;
		UE_LOG(LogTemp, Error, TEXT("JumpToSpace Earth launch travel failed: PostEarthSpaceWorldLevel is not configured."));
		return;
	}

	if (UWorld* const World = GetWorld())
	{
		World->ServerTravel(SpaceWorldLevelPackageName, true);
	}
}

bool AJTSEarthGameMode::ResolvePostEarthSpaceWorldLevelPackageName(FString& OutPackageName) const
{
	OutPackageName.Reset();

	const FSoftObjectPath SpaceWorldLevelPath = PostEarthSpaceWorldLevel.ToSoftObjectPath();
	if (SpaceWorldLevelPath.IsValid())
	{
		OutPackageName = SpaceWorldLevelPath.GetLongPackageName();
		if (OutPackageName == JTSMapPaths::LegacyMoonPrototype)
		{
			UE_LOG(LogTemp, Error, TEXT("JumpToSpace Earth launch rejected L_MoonPrototype_Tmp. Configure PostEarthSpaceWorldLevel with L_SpaceWorld."));
			OutPackageName.Reset();
			return false;
		}
		return !OutPackageName.IsEmpty();
	}

	UE_LOG(LogTemp, Warning, TEXT("JumpToSpace Earth launch has no PostEarthSpaceWorldLevel override; using the canonical L_SpaceWorld map."));
	OutPackageName = JTSMapPaths::SpaceWorld;
	return true;
}

AJTSGameState* AJTSEarthGameMode::GetJTSGameState() const
{
	UWorld* const World = GetWorld();
	return World != nullptr ? World->GetGameState<AJTSGameState>() : nullptr;
}
