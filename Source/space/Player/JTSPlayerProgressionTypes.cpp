// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Player/JTSPlayerProgressionTypes.h"

namespace
{
	// L1->L2 through L9->L10.  The shallow early ramp gets a new expedition moving quickly while
	// the later values retain a meaningful long-term chase without an exponential grind wall.
	constexpr int32 ExperienceRequirements[] = { 100, 150, 225, 325, 450, 600, 775, 975, 1200 };

	// Constant additive gains per point. Speed is in cm/s, health/defense are damage units.
	// At the current 10-health baseline, max health/defense give 20 HP and 2.5 flat defense:
	// a 6-damage attack needs six hits to defeat the player; a 12-damage attack needs three.
	constexpr int32 InventorySlotsPerRank = 2;
	constexpr float RunSpeedPerRank = 20.0f;
	constexpr float StaminaPerRank = 20.0f;
	constexpr float HealthPerRank = 2.0f;
	constexpr float DefensePerRank = 0.5f;
	constexpr int32 CriticalChancePerRank = 5;

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
	return InventorySlotsPerRank * ClampRank(AbilityRank);
}

float FJTSPlayerProgressionRules::GetRunSpeedBonus(const int32 AbilityRank)
{
	return RunSpeedPerRank * ClampRank(AbilityRank);
}

float FJTSPlayerProgressionRules::GetMaxStamina(const int32 AbilityRank)
{
	return 100.0f + StaminaPerRank * ClampRank(AbilityRank);
}

int32 FJTSPlayerProgressionRules::GetCriticalChancePercent(const int32 AbilityRank)
{
	return CriticalChancePerRank * ClampRank(AbilityRank);
}

float FJTSPlayerProgressionRules::GetHealthBonus(const int32 AbilityRank)
{
	return HealthPerRank * ClampRank(AbilityRank);
}

float FJTSPlayerProgressionRules::GetDefense(const int32 AbilityRank)
{
	return DefensePerRank * ClampRank(AbilityRank);
}
