// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Player/JTSPlayerProgressionTypes.h"

namespace
{
	// L1->L2 through L9->L10.  The shallow early ramp gets a new expedition moving quickly while
	// the later values retain a meaningful long-term chase without an exponential grind wall.
	constexpr int32 ExperienceRequirements[] = { 100, 150, 225, 325, 450, 600, 775, 975, 1200 };

	// Cumulative effects by ability rank.  Cargo reaches its first complete nine-key row at rank 4;
	// speed uses small, predictable percentage steps and deliberately tops out at a safe 20%.
	constexpr int32 InventorySlotBonuses[] = { 0, 1, 3, 5, 7, 10 };
	constexpr int32 StackLimits[] = { 1, 2, 4, 7, 11, 16 };
	constexpr int32 RunSpeedBonusPercents[] = { 0, 4, 8, 12, 16, 20 };

	int32 ClampRank(const int32 Rank)
	{
		return FMath::Clamp(Rank, 0, FJTSPlayerProgressionRules::MaximumAbilityRank);
	}
}

int32 FJTSPlayerProgressionRules::GetExperienceRequiredForNextLevel(const int32 CurrentLevel)
{
	const int32 RequirementIndex = CurrentLevel - 1;
	return ExperienceRequirements[RequirementIndex >= 0 && RequirementIndex < UE_ARRAY_COUNT(ExperienceRequirements)
		? RequirementIndex
		: UE_ARRAY_COUNT(ExperienceRequirements) - 1];
}

int32 FJTSPlayerProgressionRules::GetInventorySlotBonus(const int32 AbilityRank)
{
	return InventorySlotBonuses[ClampRank(AbilityRank)];
}

int32 FJTSPlayerProgressionRules::GetStackLimit(const int32 AbilityRank)
{
	return StackLimits[ClampRank(AbilityRank)];
}

int32 FJTSPlayerProgressionRules::GetRunSpeedBonusPercent(const int32 AbilityRank)
{
	return RunSpeedBonusPercents[ClampRank(AbilityRank)];
}

float FJTSPlayerProgressionRules::GetRunSpeedMultiplier(const int32 AbilityRank)
{
	return 1.0f + static_cast<float>(GetRunSpeedBonusPercent(AbilityRank)) / 100.0f;
}
