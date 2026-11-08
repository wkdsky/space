// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "JTSPlayerProgressionTypes.generated.h"

/** Permanent player abilities available from the ship ability terminal. */
UENUM(BlueprintType)
enum class EJTSPlayerAbility : uint8
{
	InventorySlots UMETA(DisplayName = "背包容量"),
	StackLimit UMETA(Hidden), // Retain the serialized enum value for old saves/Blueprints.
	RunSpeed UMETA(DisplayName = "奔跑速度"),
	Stamina UMETA(DisplayName = "耐力"),
	CriticalChance UMETA(DisplayName = "暴击率"),
	Health UMETA(DisplayName = "血量"),
	Defense UMETA(DisplayName = "防御力")
};

/** Client-submitted, server-validated pending allocation. Every rank costs one ability point. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSAbilityAllocation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (ClampMin = "0"))
	int32 InventorySlotRanks = 0;

	/** Legacy request field. The server rejects nonzero values. */
	UPROPERTY()
	int32 StackLimitRanks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (ClampMin = "0"))
	int32 RunSpeedRanks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (ClampMin = "0"))
	int32 StaminaRanks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (ClampMin = "0"))
	int32 CriticalChanceRanks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (ClampMin = "0"))
	int32 HealthRanks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (ClampMin = "0"))
	int32 DefenseRanks = 0;

	int32 GetTotalPointCost() const
	{
		return FMath::Max(0, InventorySlotRanks)
			+ FMath::Max(0, RunSpeedRanks)
			+ FMath::Max(0, StaminaRanks)
			+ FMath::Max(0, CriticalChanceRanks)
			+ FMath::Max(0, HealthRanks)
			+ FMath::Max(0, DefenseRanks);
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
	static constexpr int32 ItemStackLimit = 99;

	/** Experience needed to advance from the supplied one-based level. Zero means the level cap. */
	static int32 GetExperienceRequiredForNextLevel(int32 CurrentLevel);
	static int32 GetInventorySlotBonus(int32 AbilityRank);
	/** Flat movement speed added to both normal movement and sprint, in cm/s. */
	static float GetRunSpeedBonus(int32 AbilityRank);
	static float GetMaxStamina(int32 AbilityRank);
	/** Percent chance on ordinary weapon hits. Authored weak-point hits always crit. */
	static int32 GetCriticalChancePercent(int32 AbilityRank);
	/** Flat health added to the character's Blueprint-authored base capacity. */
	static float GetHealthBonus(int32 AbilityRank);
	/** Flat damage blocked per non-bypassing hit; never a percentage reduction. */
	static float GetDefense(int32 AbilityRank);
	static constexpr float CriticalDamageMultiplier = 1.75f;
};
