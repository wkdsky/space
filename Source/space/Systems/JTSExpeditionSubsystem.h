// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "space/Core/JTSExpeditionTypes.h"

#include "JTSExpeditionSubsystem.generated.h"

class AJTSGameState;
class AJTSCharacter;
class AJTSPlayerState;
class AJTSSpacecraftActor;

/**
 * Owns server-authoritative expedition state that must survive seamless/server travel.
 * It deliberately stores no local-client-only preferences.
 */
UCLASS(Config = Game)
class SPACE_API UJTSExpeditionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 MaximumSaveSlots = 4;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	const FJTSExpeditionSnapshot& GetSnapshot() const { return Snapshot; }
	FJTSExpeditionSnapshot& GetMutableSnapshot() { return Snapshot; }

	void StartNewExpedition(const FString& InExpeditionId);

	/** Creates the selected host-owned save slot in memory. The first durable write happens at normal save time. */
	UFUNCTION(BlueprintCallable, Category = "Expedition|Save")
	bool BeginNewExpeditionInSlot(int32 InSaveSlot, const FString& InDisplayName);

	/** Loads one of the four host save slots into the authoritative cross-level snapshot. */
	UFUNCTION(BlueprintCallable, Category = "Expedition|Save")
	bool LoadExpeditionSlot(int32 InSaveSlot);

	/** Fixed four-slot projection used by Expedition Select. Empty slots are returned explicitly. */
	UFUNCTION(BlueprintPure, Category = "Expedition|Save")
	TArray<FJTSExpeditionSaveSummary> GetSaveSlotSummaries() const;

	UFUNCTION(BlueprintPure, Category = "Expedition|Save")
	bool HasSaveInSlot(int32 InSaveSlot) const;

	/**
	 * Permanently removes one local host save slot. This is intentionally unavailable to
	 * connected clients and also clears the in-memory snapshot when it is the active slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "Expedition|Save")
	bool DeleteExpeditionSlot(int32 InSaveSlot);

	UFUNCTION(BlueprintPure, Category = "Expedition|Save")
	int32 GetActiveSaveSlot() const { return ActiveSaveSlot; }

	UFUNCTION(BlueprintPure, Category = "Expedition|Save")
	int32 GetMaximumSaveSlots() const { return MaximumSaveSlots; }

	/** Canonical physical slot name; retained here so UI never hard-codes SaveGame naming. */
	FString GetSaveGameSlotName(int32 InSaveSlot) const;

	void SetJoinCredentials(const FString& InJoinCode, const FString& PlaintextPassword);
	bool ValidateJoinCredentials(const FString& InJoinCode, const FString& PasswordHash) const;
	const FString& GetJoinCode() const { return JoinCode; }
	void CaptureWorldState(const AJTSGameState* GameState, const AJTSSpacecraftActor* Spacecraft);
	void RestoreSpacecraft(AJTSSpacecraftActor* Spacecraft) const;
	void RestorePlayerState(AJTSPlayerState* PlayerState, AJTSCharacter* Character) const;
	void SetSpacecraftSnapshot(TSubclassOf<AJTSSpacecraftActor> SpacecraftClass, const TMap<EJTSResourceType, int32>& Storage);
	void SetCurrentMap(const FString& MapPackageName);
	void SetCurrentPlanetId(const FString& PlanetId);
	void SetCurrentCheckpoint(const FString& Checkpoint);
	void RequestSave();
	bool SaveNow();
	bool LoadMostRecentSnapshot();
	void MarkResumeRequested();
	bool ConsumeResumeRequest();
	void ResetClientTransientState();

	UFUNCTION(BlueprintPure, Category = "Expedition")
	bool HasActiveExpedition() const { return !Snapshot.ExpeditionId.IsEmpty(); }

	/** True only when this machine has a host-owned snapshot it can offer from the front end. */
	UFUNCTION(BlueprintPure, Category = "Expedition")
	bool HasSavedSnapshot() const;

	UPROPERTY(Config, EditAnywhere, Category = "Save")
	FString SaveSlotPrefix = TEXT("JumpToSpaceExpedition");

	/** Legacy single-slot name retained solely to migrate pre-slot saves on first load. */
	UPROPERTY(Config, EditAnywhere, Category = "Save")
	FString SaveSlotName = TEXT("JumpToSpaceExpedition");

private:
	void FlushDeferredSave();
	void NormalizeSnapshotMetadata(int32 PreferredSaveSlot = INDEX_NONE);
	void UpdatePlaytime();
	bool LoadSnapshotFromSaveGameSlot(const FString& PhysicalSlotName, int32 InSaveSlot);
	FJTSExpeditionSaveSummary BuildSaveSummary(const FJTSExpeditionSnapshot& InSnapshot, int32 InSaveSlot) const;
	int32 FindFirstAvailableSaveSlot() const;
	static TArray<FJTSResourceAmount> ToResourceArray(const TMap<EJTSResourceType, int32>& Storage);
	static TMap<EJTSResourceType, int32> ToResourceMap(const TArray<FJTSResourceAmount>& Storage);

	FJTSExpeditionSnapshot Snapshot;
	FString JoinCode;
	FString JoinPasswordHash;
	int32 ActiveSaveSlot = INDEX_NONE;
	FDateTime ActivePlaySegmentStartedUtc;
	bool bResumeRequested = false;
	FTimerHandle DeferredSaveTimer;
};
