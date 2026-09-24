// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "JTSPlayerProgressionTypes.generated.h"

/** The three permanent player abilities currently available from the ship ability terminal. */
UENUM(BlueprintType)
enum class EJTSPlayerAbility : uint8
{
	InventorySlots UMETA(DisplayName = "Cargo Bays"),
	StackLimit UMETA(DisplayName = "Stack Compression"),
	RunSpeed UMETA(DisplayName = "Running Thrusters")
};

/** Client-submitted, server-validated pending allocation. Every rank costs one ability point. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSAbilityAllocation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (ClampMin = "0"))
	int32 InventorySlotRanks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (ClampMin = "0"))
	int32 StackLimitRanks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (ClampMin = "0"))
	int32 RunSpeedRanks = 0;

	int32 GetTotalPointCost() const
	{
		return FMath::Max(0, InventorySlotRanks)
			+ FMath::Max(0, StackLimitRanks)
			+ FMath::Max(0, RunSpeedRanks);
	}
};

/**
 * Immutable early-game progression tuning.  Keeping the tables in one native rules type makes
 * the PlayerState authoritative while Blueprint remains free to style the terminal presentation.
 */
struct SPACE_API FJTSPlayerProgressionRules
{
	static constexpr int32 MaximumLevel = 101;
	static constexpr int32 MaximumAbilityRank = 5;

	/** Experience needed to advance from the supplied one-based level. Zero means the level cap. */
	static int32 GetExperienceRequiredForNextLevel(int32 CurrentLevel);
	static int32 GetInventorySlotBonus(int32 AbilityRank);
	static int32 GetStackLimit(int32 AbilityRank);
	static int32 GetRunSpeedBonusPercent(int32 AbilityRank);
	static float GetRunSpeedMultiplier(int32 AbilityRank);
};
