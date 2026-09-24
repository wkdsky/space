// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "space/Items/JTSItemTypes.h"
#include "space/Items/JTSResourceTypes.h"

#include "JTSExpeditionTypes.generated.h"

class AJTSPlayerState;
class AJTSCharacter;
class AJTSSpacecraftActor;

/** The synchronized high-level phase of one shared expedition. */
UENUM(BlueprintType)
enum class EJTSGameplayPhase : uint8
{
	EarthCollection UMETA(DisplayName = "Earth Collection"),
	EarthCollectionFinished UMETA(DisplayName = "Earth Collection Finished"),
	Launching UMETA(DisplayName = "Launching"),
	EarthCaptureFailure UMETA(DisplayName = "Earth Capture Failure"),
	MoonArrivalSuccess UMETA(DisplayName = "Moon Arrival Success"),
	WaitingToStart UMETA(DisplayName = "Waiting To Start"),
	MoonExploration UMETA(DisplayName = "Moon Exploration"),
	SpaceFlight UMETA(DisplayName = "Space Flight")
};

UENUM(BlueprintType)
enum class EJTSFailureReason : uint8
{
	None UMETA(DisplayName = "None"),
	NoTimelyBoarding UMETA(DisplayName = "No Timely Boarding"),
	InsufficientFuel UMETA(DisplayName = "Insufficient Fuel"),
	NoSpacecraft UMETA(DisplayName = "No Spacecraft"),
	InvalidGameInstance UMETA(DisplayName = "Invalid Runtime State")
};

UENUM(BlueprintType)
enum class EJTSAvatarColor : uint8
{
	Blue UMETA(DisplayName = "Blue"),
	Orange UMETA(DisplayName = "Orange"),
	Green UMETA(DisplayName = "Green"),
	Purple UMETA(DisplayName = "Purple")
};

/** Discovery policy for a pre-launch lobby. Private expeditions remain joinable by code. */
UENUM(BlueprintType)
enum class EJTSLobbyVisibility : uint8
{
	Public UMETA(DisplayName = "Public"),
	Private UMETA(DisplayName = "Private")
};

UENUM(BlueprintType)
enum class EJTSPlayerExpeditionStatus : uint8
{
	InFrontEnd UMETA(DisplayName = "In Front End"),
	InLobby UMETA(DisplayName = "In Lobby"),
	Ready UMETA(DisplayName = "Ready"),
	Active UMETA(DisplayName = "Active"),
	Boarded UMETA(DisplayName = "Boarded"),
	Dead UMETA(DisplayName = "Dead"),
	Disconnected UMETA(DisplayName = "Disconnected")
};

UENUM(BlueprintType)
enum class EJTSSpacecraftSeatRole : uint8
{
	Passenger UMETA(DisplayName = "Passenger"),
	Driver UMETA(DisplayName = "Driver")
};

/** Replication-friendly resource inventory item. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSResourceAmount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources")
	EJTSResourceType ResourceType = EJTSResourceType::Fuel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources", meta = (ClampMin = "0"))
	int32 Amount = 0;
};

/** Server-authoritative third-person flight intent submitted by the current driver. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSSpacecraftInputState
{
	GENERATED_BODY()

	UPROPERTY()
	float MoveForward = 0.0f;

	UPROPERTY()
	float MoveRight = 0.0f;

	UPROPERTY()
	float Lift = 0.0f;

	/** Full camera forward vector; surface flight projects it tangentially while deep space keeps 3D pitch. */
	UPROPERTY()
	FVector_NetQuantizeNormal ViewForward = FVector::ForwardVector;

	UPROPERTY()
	bool bBoosting = false;

	UPROPERTY()
	bool bBraking = false;
};

/** A replicated seat assignment. The vehicle remains a single shared actor. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSSpacecraftOccupantState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ship")
	TObjectPtr<AJTSPlayerState> PlayerState = nullptr;

	/** Survives possession transfer: APawn::UnPossessed clears the character's PlayerState. */
	UPROPERTY(BlueprintReadOnly, Category = "Ship")
	TObjectPtr<AJTSCharacter> Character = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Ship")
	EJTSSpacecraftSeatRole SeatRole = EJTSSpacecraftSeatRole::Passenger;
};

/** Durable player state used by the SaveGame snapshot. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSPlayerSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	FString PlayerId;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	EJTSAvatarColor AvatarColor = EJTSAvatarColor::Blue;

	UPROPERTY()
	float Health = 100.0f;

	UPROPERTY()
	TArray<FJTSResourceAmount> Inventory;

	/** v4 unified item-system snapshot. Tools, weapons, resources, and former equipment share these slots. */
	UPROPERTY()
	TArray<FJTSItemInstance> ItemInventory;

	/** Legacy v3 migration input only. New captures never write equipment into a separate container. */
	UPROPERTY()
	TArray<FJTSItemInstance> Wearables;

	UPROPERTY()
	int32 SelectedQuickbarSlot = 0;

	UPROPERTY()
	int32 ProgressionLevel = 1;

	UPROPERTY()
	int32 ExperienceInCurrentLevel = 0;

	UPROPERTY()
	int32 UnspentAbilityPoints = 0;

	UPROPERTY()
	int32 InventorySlotAbilityRank = 0;

	UPROPERTY()
	int32 StackLimitAbilityRank = 0;

	UPROPERTY()
	int32 RunSpeedAbilityRank = 0;
};

/** Cross-level snapshot owned by the authoritative expedition subsystem, never by a client GameInstance. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSExpeditionSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	FString ExpeditionId;

	/** Host-owned display metadata. It lives with the expedition snapshot, not client preferences. */
	UPROPERTY()
	FString DisplayName;

	/** One-based UI slot number. Four slots are currently exposed by the front end. */
	UPROPERTY()
	int32 SaveSlot = INDEX_NONE;

	UPROPERTY()
	FString CurrentMapPackage;

	UPROPERTY()
	FString CurrentPlanetId;

	UPROPERTY()
	EJTSGameplayPhase GameplayPhase = EJTSGameplayPhase::WaitingToStart;

	/** Optional human-facing checkpoint label for save selection and future chapter resumes. */
	UPROPERTY()
	FString CurrentCheckpoint;

	UPROPERTY()
	TSoftClassPtr<AJTSSpacecraftActor> SpacecraftClass;

	UPROPERTY()
	TArray<FJTSResourceAmount> SpacecraftStorage;

	UPROPERTY()
	TArray<FJTSPlayerSnapshot> Players;

	/** UTC ticks written by the authoritative host whenever this snapshot is persisted. */
	UPROPERTY()
	int64 SavedUtcTicks = 0;

	/** Mirrors SavedUtcTicks with a UI-oriented name for old and new save readers. */
	UPROPERTY()
	int64 LastPlayedUtcTicks = 0;

	/** Accumulated host-authoritative expedition time. */
	UPROPERTY()
	double PlaytimeSeconds = 0.0;

	UPROPERTY()
	int32 SaveVersion = 4;
};

/** Read-only front-end projection of a host-owned expedition save. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSExpeditionSaveSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Expedition Save")
	bool bOccupied = false;

	UPROPERTY(BlueprintReadOnly, Category = "Expedition Save")
	int32 SaveSlot = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Expedition Save")
	FString ExpeditionId;

	UPROPERTY(BlueprintReadOnly, Category = "Expedition Save")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Expedition Save")
	FString CurrentPlanet;

	UPROPERTY(BlueprintReadOnly, Category = "Expedition Save")
	FString CurrentCheckpoint;

	UPROPERTY(BlueprintReadOnly, Category = "Expedition Save")
	EJTSGameplayPhase CurrentPhase = EJTSGameplayPhase::WaitingToStart;

	UPROPERTY(BlueprintReadOnly, Category = "Expedition Save")
	int64 LastPlayedUtcTicks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Expedition Save")
	double PlaytimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Expedition Save")
	int32 SaveVersion = 4;
};
