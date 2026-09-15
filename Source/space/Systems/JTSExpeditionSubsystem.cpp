// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Systems/JTSExpeditionSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"
#include "space/Components/JTSCarryComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Systems/JTSExpeditionSaveGame.h"

void UJTSExpeditionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UJTSExpeditionSubsystem::Deinitialize()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredSaveTimer);
	}
	Super::Deinitialize();
}

FString UJTSExpeditionSubsystem::GetSaveGameSlotName(int32 InSaveSlot) const
{
	return FString::Printf(TEXT("%s_%02d"), *SaveSlotPrefix, FMath::Clamp(InSaveSlot, 1, MaximumSaveSlots));
}

bool UJTSExpeditionSubsystem::HasSaveInSlot(int32 InSaveSlot) const
{
	if (InSaveSlot < 1 || InSaveSlot > MaximumSaveSlots)
	{
		return false;
	}

	return UGameplayStatics::DoesSaveGameExist(GetSaveGameSlotName(InSaveSlot), 0)
		|| (InSaveSlot == 1 && UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0));
}

bool UJTSExpeditionSubsystem::HasSavedSnapshot() const
{
	for (int32 SlotIndex = 1; SlotIndex <= MaximumSaveSlots; ++SlotIndex)
	{
		if (HasSaveInSlot(SlotIndex))
		{
			return true;
		}
	}
	return false;
}

int32 UJTSExpeditionSubsystem::FindFirstAvailableSaveSlot() const
{
	for (int32 SlotIndex = 1; SlotIndex <= MaximumSaveSlots; ++SlotIndex)
	{
		if (!UGameplayStatics::DoesSaveGameExist(GetSaveGameSlotName(SlotIndex), 0))
		{
			return SlotIndex;
		}
	}
	return 1;
}

void UJTSExpeditionSubsystem::NormalizeSnapshotMetadata(int32 PreferredSaveSlot)
{
	if (PreferredSaveSlot >= 1 && PreferredSaveSlot <= MaximumSaveSlots)
	{
		Snapshot.SaveSlot = PreferredSaveSlot;
	}
	else if (Snapshot.SaveSlot < 1 || Snapshot.SaveSlot > MaximumSaveSlots)
	{
		Snapshot.SaveSlot = ActiveSaveSlot >= 1 && ActiveSaveSlot <= MaximumSaveSlots
			? ActiveSaveSlot
			: FindFirstAvailableSaveSlot();
	}

	if (Snapshot.DisplayName.TrimStartAndEnd().IsEmpty())
	{
		Snapshot.DisplayName = FString::Printf(TEXT("Expedition %02d"), Snapshot.SaveSlot);
	}
	if (Snapshot.CurrentCheckpoint.IsEmpty())
	{
		Snapshot.CurrentCheckpoint = Snapshot.GameplayPhase == EJTSGameplayPhase::WaitingToStart
			? TEXT("Pre-Launch")
			: TEXT("In Progress");
	}
	if (Snapshot.LastPlayedUtcTicks <= 0)
	{
		Snapshot.LastPlayedUtcTicks = Snapshot.SavedUtcTicks;
	}
	Snapshot.PlaytimeSeconds = FMath::Max(0.0, Snapshot.PlaytimeSeconds);
	Snapshot.SaveVersion = FMath::Max(2, Snapshot.SaveVersion);
}

void UJTSExpeditionSubsystem::StartNewExpedition(const FString& InExpeditionId)
{
	const int32 SelectedSlot = ActiveSaveSlot >= 1 && ActiveSaveSlot <= MaximumSaveSlots
		? ActiveSaveSlot
		: FindFirstAvailableSaveSlot();
	Snapshot = FJTSExpeditionSnapshot();
	Snapshot.ExpeditionId = InExpeditionId.IsEmpty() ? FGuid::NewGuid().ToString(EGuidFormats::Digits) : InExpeditionId;
	Snapshot.SaveSlot = SelectedSlot;
	Snapshot.DisplayName = FString::Printf(TEXT("Expedition %02d"), SelectedSlot);
	Snapshot.CurrentCheckpoint = TEXT("Pre-Launch");
	Snapshot.SaveVersion = 2;
	ActiveSaveSlot = SelectedSlot;
	ActivePlaySegmentStartedUtc = FDateTime::UtcNow();
	JoinCode.Reset();
	JoinPasswordHash.Reset();
	bResumeRequested = false;
}

bool UJTSExpeditionSubsystem::BeginNewExpeditionInSlot(int32 InSaveSlot, const FString& InDisplayName)
{
	if (InSaveSlot < 1 || InSaveSlot > MaximumSaveSlots)
	{
		return false;
	}

	ActiveSaveSlot = InSaveSlot;
	Snapshot = FJTSExpeditionSnapshot();
	Snapshot.ExpeditionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Snapshot.SaveSlot = InSaveSlot;
	Snapshot.DisplayName = InDisplayName.TrimStartAndEnd();
	Snapshot.CurrentCheckpoint = TEXT("Pre-Launch");
	Snapshot.SaveVersion = 2;
	NormalizeSnapshotMetadata(InSaveSlot);
	ActivePlaySegmentStartedUtc = FDateTime::UtcNow();
	JoinCode.Reset();
	JoinPasswordHash.Reset();
	bResumeRequested = false;
	return true;
}

FJTSExpeditionSaveSummary UJTSExpeditionSubsystem::BuildSaveSummary(const FJTSExpeditionSnapshot& InSnapshot, int32 InSaveSlot) const
{
	FJTSExpeditionSaveSummary Result;
	Result.bOccupied = !InSnapshot.ExpeditionId.IsEmpty();
	Result.SaveSlot = InSaveSlot;
	Result.ExpeditionId = InSnapshot.ExpeditionId;
	Result.DisplayName = InSnapshot.DisplayName.IsEmpty()
		? FString::Printf(TEXT("Expedition %02d"), InSaveSlot)
		: InSnapshot.DisplayName;
	Result.CurrentPlanet = InSnapshot.CurrentPlanetId.IsEmpty() ? TEXT("Earth") : InSnapshot.CurrentPlanetId;
	Result.CurrentCheckpoint = InSnapshot.CurrentCheckpoint.IsEmpty() ? TEXT("Pre-Launch") : InSnapshot.CurrentCheckpoint;
	Result.CurrentPhase = InSnapshot.GameplayPhase;
	Result.LastPlayedUtcTicks = InSnapshot.LastPlayedUtcTicks > 0 ? InSnapshot.LastPlayedUtcTicks : InSnapshot.SavedUtcTicks;
	Result.PlaytimeSeconds = FMath::Max(0.0, InSnapshot.PlaytimeSeconds);
	Result.SaveVersion = InSnapshot.SaveVersion;
	return Result;
}

TArray<FJTSExpeditionSaveSummary> UJTSExpeditionSubsystem::GetSaveSlotSummaries() const
{
	TArray<FJTSExpeditionSaveSummary> Result;
	Result.Reserve(MaximumSaveSlots);
	for (int32 SlotIndex = 1; SlotIndex <= MaximumSaveSlots; ++SlotIndex)
	{
		FJTSExpeditionSaveSummary Summary;
		Summary.SaveSlot = SlotIndex;
		if (const UJTSExpeditionSaveGame* const SaveGame = Cast<UJTSExpeditionSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSaveGameSlotName(SlotIndex), 0)))
		{
			Summary = BuildSaveSummary(SaveGame->Snapshot, SlotIndex);
		}
		else if (SlotIndex == 1)
		{
			// One-time compatibility projection for saves created before the four-slot front end.
			if (const UJTSExpeditionSaveGame* const LegacySave = Cast<UJTSExpeditionSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0)))
			{
				Summary = BuildSaveSummary(LegacySave->Snapshot, SlotIndex);
			}
		}
		Result.Add(Summary);
	}
	return Result;
}

bool UJTSExpeditionSubsystem::LoadSnapshotFromSaveGameSlot(const FString& PhysicalSlotName, int32 InSaveSlot)
{
	if (!UGameplayStatics::DoesSaveGameExist(PhysicalSlotName, 0))
	{
		return false;
	}

	const UJTSExpeditionSaveGame* const SaveGame = Cast<UJTSExpeditionSaveGame>(UGameplayStatics::LoadGameFromSlot(PhysicalSlotName, 0));
	if (SaveGame == nullptr || SaveGame->Snapshot.ExpeditionId.IsEmpty())
	{
		return false;
	}

	Snapshot = SaveGame->Snapshot;
	ActiveSaveSlot = InSaveSlot;
	NormalizeSnapshotMetadata(InSaveSlot);
	ActivePlaySegmentStartedUtc = FDateTime::UtcNow();
	bResumeRequested = false;
	return true;
}

bool UJTSExpeditionSubsystem::LoadExpeditionSlot(int32 InSaveSlot)
{
	if (InSaveSlot < 1 || InSaveSlot > MaximumSaveSlots)
	{
		return false;
	}

	if (LoadSnapshotFromSaveGameSlot(GetSaveGameSlotName(InSaveSlot), InSaveSlot))
	{
		return true;
	}

	return InSaveSlot == 1 && LoadSnapshotFromSaveGameSlot(SaveSlotName, InSaveSlot);
}

void UJTSExpeditionSubsystem::SetJoinCredentials(const FString& InJoinCode, const FString& PlaintextPassword)
{
	JoinCode = InJoinCode;
	JoinPasswordHash = PlaintextPassword.IsEmpty() ? FString() : FMD5::HashAnsiString(*PlaintextPassword);
}

bool UJTSExpeditionSubsystem::ValidateJoinCredentials(const FString& InJoinCode, const FString& PasswordHash) const
{
	return JoinCode == InJoinCode
		&& (JoinPasswordHash.IsEmpty() || JoinPasswordHash == PasswordHash);
}

void UJTSExpeditionSubsystem::CaptureWorldState(const AJTSGameState* GameState, const AJTSSpacecraftActor* Spacecraft)
{
	if (GameState != nullptr)
	{
		Snapshot.GameplayPhase = GameState->GetGameplayPhase();
		Snapshot.CurrentPlanetId = GameState->GetCurrentPlanetId();
		if (const UWorld* const World = GameState->GetWorld())
		{
			Snapshot.CurrentMapPackage = World->GetPackage()->GetName();
		}
	}
	if (Spacecraft != nullptr)
	{
		SetSpacecraftSnapshot(Spacecraft->GetClass(), Spacecraft->GetStorage());
	}
	Snapshot.Players.Reset();
	if (GameState != nullptr)
	{
		for (APlayerState* const RawPlayerState : GameState->PlayerArray)
		{
			const AJTSPlayerState* const PlayerState = Cast<AJTSPlayerState>(RawPlayerState);
			const AJTSCharacter* const Character = PlayerState != nullptr ? Cast<AJTSCharacter>(PlayerState->GetPawn()) : nullptr;
			if (PlayerState == nullptr)
			{
				continue;
			}

			FJTSPlayerSnapshot& PlayerSnapshot = Snapshot.Players.AddDefaulted_GetRef();
			PlayerSnapshot.PlayerId = PlayerState->GetOnlineIdentityString();
			PlayerSnapshot.DisplayName = PlayerState->GetPlayerName();
			PlayerSnapshot.AvatarColor = PlayerState->GetAvatarColor();
			if (const UJTSHealthComponent* const Health = Character != nullptr ? Character->GetHealthComponent() : nullptr)
			{
				PlayerSnapshot.Health = Health->GetHealth();
			}
			if (const UJTSCarryComponent* const Carry = Character != nullptr ? Character->FindComponentByClass<UJTSCarryComponent>() : nullptr)
			{
				PlayerSnapshot.Inventory = ToResourceArray(Carry->GetCarriedResources());
			}
		}
	}
}

void UJTSExpeditionSubsystem::RestoreSpacecraft(AJTSSpacecraftActor* Spacecraft) const
{
	if (Spacecraft != nullptr && !Snapshot.SpacecraftStorage.IsEmpty())
	{
		Spacecraft->RestoreStorageFromExpedition(ToResourceMap(Snapshot.SpacecraftStorage));
	}
}

void UJTSExpeditionSubsystem::RestorePlayerState(AJTSPlayerState* PlayerState, AJTSCharacter* Character) const
{
	if (PlayerState == nullptr || Character == nullptr || !Character->HasAuthority())
	{
		return;
	}

	const FJTSPlayerSnapshot* const SavedPlayer = Snapshot.Players.FindByPredicate([PlayerState](const FJTSPlayerSnapshot& Candidate)
	{
		return Candidate.PlayerId == PlayerState->GetOnlineIdentityString();
	});
	if (SavedPlayer == nullptr)
	{
		return;
	}

	PlayerState->SetAvatarColor(SavedPlayer->AvatarColor);
	if (UJTSHealthComponent* const Health = Character->GetHealthComponent())
	{
		Health->RestoreAuthoritativeHealth(SavedPlayer->Health);
	}
	if (UJTSCarryComponent* const Carry = Character->FindComponentByClass<UJTSCarryComponent>())
	{
		TArray<EJTSResourceType> RestoredItems;
		for (const FJTSResourceAmount& Resource : SavedPlayer->Inventory)
		{
			for (int32 Index = 0; Index < FMath::Max(0, Resource.Amount); ++Index)
			{
				RestoredItems.Add(Resource.ResourceType);
			}
		}
		Carry->RestoreCarriedItems(RestoredItems);
	}
}

void UJTSExpeditionSubsystem::SetSpacecraftSnapshot(TSubclassOf<AJTSSpacecraftActor> SpacecraftClass, const TMap<EJTSResourceType, int32>& Storage)
{
	Snapshot.SpacecraftClass = SpacecraftClass;
	Snapshot.SpacecraftStorage = ToResourceArray(Storage);
}

void UJTSExpeditionSubsystem::SetCurrentMap(const FString& MapPackageName)
{
	Snapshot.CurrentMapPackage = MapPackageName;
}

void UJTSExpeditionSubsystem::SetCurrentPlanetId(const FString& PlanetId)
{
	Snapshot.CurrentPlanetId = PlanetId;
}

void UJTSExpeditionSubsystem::SetCurrentCheckpoint(const FString& Checkpoint)
{
	Snapshot.CurrentCheckpoint = Checkpoint;
}

void UJTSExpeditionSubsystem::RequestSave()
{
	UWorld* const World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_Client)
	{
		return;
	}
	World->GetTimerManager().SetTimer(DeferredSaveTimer, this, &UJTSExpeditionSubsystem::FlushDeferredSave, 1.0f, false);
}

void UJTSExpeditionSubsystem::UpdatePlaytime()
{
	if (ActivePlaySegmentStartedUtc.GetTicks() <= 0)
	{
		ActivePlaySegmentStartedUtc = FDateTime::UtcNow();
		return;
	}

	const FDateTime Now = FDateTime::UtcNow();
	Snapshot.PlaytimeSeconds += FMath::Max(0.0, (Now - ActivePlaySegmentStartedUtc).GetTotalSeconds());
	ActivePlaySegmentStartedUtc = Now;
}

bool UJTSExpeditionSubsystem::SaveNow()
{
	UWorld* const World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_Client || Snapshot.ExpeditionId.IsEmpty())
	{
		return false;
	}

	if (ActiveSaveSlot < 1 || ActiveSaveSlot > MaximumSaveSlots)
	{
		ActiveSaveSlot = Snapshot.SaveSlot >= 1 && Snapshot.SaveSlot <= MaximumSaveSlots
			? Snapshot.SaveSlot
			: FindFirstAvailableSaveSlot();
	}
	NormalizeSnapshotMetadata(ActiveSaveSlot);
	UpdatePlaytime();
	Snapshot.SavedUtcTicks = FDateTime::UtcNow().GetTicks();
	Snapshot.LastPlayedUtcTicks = Snapshot.SavedUtcTicks;

	UJTSExpeditionSaveGame* const SaveGame = Cast<UJTSExpeditionSaveGame>(UGameplayStatics::CreateSaveGameObject(UJTSExpeditionSaveGame::StaticClass()));
	if (SaveGame == nullptr)
	{
		return false;
	}
	SaveGame->Snapshot = Snapshot;
	return UGameplayStatics::SaveGameToSlot(SaveGame, GetSaveGameSlotName(ActiveSaveSlot), 0);
}

bool UJTSExpeditionSubsystem::LoadMostRecentSnapshot()
{
	int32 BestSlot = INDEX_NONE;
	int64 BestTicks = TNumericLimits<int64>::Lowest();
	for (int32 SlotIndex = 1; SlotIndex <= MaximumSaveSlots; ++SlotIndex)
	{
		FJTSExpeditionSnapshot Candidate;
		const UJTSExpeditionSaveGame* const SaveGame = Cast<UJTSExpeditionSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSaveGameSlotName(SlotIndex), 0));
		if (SaveGame != nullptr)
		{
			Candidate = SaveGame->Snapshot;
		}
		else if (SlotIndex == 1)
		{
			if (const UJTSExpeditionSaveGame* const LegacySave = Cast<UJTSExpeditionSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0)))
			{
				Candidate = LegacySave->Snapshot;
			}
		}

		if (!Candidate.ExpeditionId.IsEmpty())
		{
			const int64 CandidateTicks = Candidate.LastPlayedUtcTicks > 0 ? Candidate.LastPlayedUtcTicks : Candidate.SavedUtcTicks;
			if (BestSlot == INDEX_NONE || CandidateTicks > BestTicks)
			{
				BestSlot = SlotIndex;
				BestTicks = CandidateTicks;
			}
		}
	}

	return BestSlot != INDEX_NONE && LoadExpeditionSlot(BestSlot);
}

void UJTSExpeditionSubsystem::MarkResumeRequested()
{
	bResumeRequested = HasActiveExpedition();
}

bool UJTSExpeditionSubsystem::ConsumeResumeRequest()
{
	const bool bWasRequested = bResumeRequested;
	bResumeRequested = false;
	return bWasRequested;
}

void UJTSExpeditionSubsystem::ResetClientTransientState()
{
	if (UWorld* const World = GetWorld(); World != nullptr && World->GetNetMode() == NM_Client)
	{
		Snapshot = FJTSExpeditionSnapshot();
		JoinCode.Reset();
		JoinPasswordHash.Reset();
		ActiveSaveSlot = INDEX_NONE;
		ActivePlaySegmentStartedUtc = FDateTime();
		bResumeRequested = false;
	}
}

void UJTSExpeditionSubsystem::FlushDeferredSave()
{
	SaveNow();
}

TArray<FJTSResourceAmount> UJTSExpeditionSubsystem::ToResourceArray(const TMap<EJTSResourceType, int32>& Storage)
{
	TArray<FJTSResourceAmount> Result;
	for (const TPair<EJTSResourceType, int32>& Pair : Storage)
	{
		if (Pair.Value > 0)
		{
			FJTSResourceAmount& Entry = Result.AddDefaulted_GetRef();
			Entry.ResourceType = Pair.Key;
			Entry.Amount = Pair.Value;
		}
	}
	return Result;
}

TMap<EJTSResourceType, int32> UJTSExpeditionSubsystem::ToResourceMap(const TArray<FJTSResourceAmount>& Storage)
{
	TMap<EJTSResourceType, int32> Result;
	for (const FJTSResourceAmount& Entry : Storage)
	{
		if (Entry.Amount > 0)
		{
			Result.FindOrAdd(Entry.ResourceType) += Entry.Amount;
		}
	}
	return Result;
}
