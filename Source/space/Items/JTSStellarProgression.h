// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** One of the six skills of a stellar attachment: value = Base + PerX * (Divisor > 0 ? floor(x / Divisor) : x). */
struct FJTSStellarSkillDef
{
	const TCHAR* Name = TEXT("");
	double Base = 0.0;
	double PerX = 0.0;
	/** Integer mechanics (pierce, bounces, minions) step every Divisor effective points. */
	int32 Divisor = 0;
	/** Zero disables the cap. */
	double MaxValue = 0.0;
	/** Mechanics that only exist once at least one point is invested. */
	bool bOffAtZero = false;
	int32 Decimals = 2;
	/** "{0}" is replaced with the formatted value. */
	const TCHAR* Format = TEXT("{0}");
};

/**
 * Pure rules for core budgets and attachment point scaling. Holds no state so the server, UI and tests
 * evaluate exactly the same numbers.
 */
struct SPACE_API FJTSStellarProgression
{
	static constexpr int32 SkillCount = 6;
	static constexpr int32 MaxPointsPerSkill = 10;
	static constexpr int32 MaxCoreLevel = 61;

	/** Level 1 gives zero points; level 61 gives the full 60. */
	static int32 GetCoreBudget(int32 CoreLevel);
	/** Firmware units to raise a core with this level by one level: 1 + floor(P / 10). */
	static int32 GetUpgradeUnitCost(int32 CoreLevel);
	static int32 SumPoints(const TArray<uint8>& Points);
	/** Pads to six entries and clamps each to 0..10. */
	static void NormalizePoints(TArray<uint8>& Points);
	/** x_i = a_i * min(1, P / S). Recorded points are never rewritten. */
	static void ComputeEffectiveLevels(const TArray<uint8>& Points, int32 Budget, TArray<double>& OutLevels);

	/** Six skill definitions for an attachment id, or null when the id is unknown. */
	static const FJTSStellarSkillDef* FindSkillDefs(FName AttachmentId);
	static double EvaluateSkill(const FJTSStellarSkillDef& Def, double EffectiveLevel);
	static FString FormatSkill(const FJTSStellarSkillDef& Def, double EffectiveLevel);
	/** Effective points still needed for the next integer step, or negative for continuous skills. */
	static double PointsToNextStep(const FJTSStellarSkillDef& Def, double EffectiveLevel);
};
