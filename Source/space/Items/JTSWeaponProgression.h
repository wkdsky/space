// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "space/Items/JTSItemTypes.h"

class UJTSItemDefinition;

/**
 * Everything a normal-weapon skill point can change. The table below maps each weapon's six skills onto
 * these channels, so the combat code reads one resolved number per channel instead of per-weapon switches.
 */
enum class EJTSWeaponStat : uint8
{
	Damage,
	FireRate,
	Reach,
	Arc,
	ExtraTargets,
	ChargeTime,
	Stagger,
	GuardWindow,
	CounterDamage,
	StaminaCost,
	BlastRadius,
	ArmorBreak,
	Recoil,
	Spread,
	Magazine,
	ReloadTime,
	WeakPoint,
	Pierce,
	AimTime,
	FalloffStart,
	Pellets,
	Knockback,
	EnergyCapacity,
	RechargeRate,
	ProjectileSpeed,
	Fragments,
	ChainTargets,
	ChainRange,
	ChainFalloff,
	DischargeTime,
	BoltTime,
	BreathCost,
	Count
};

/** One of a weapon's six skills. Value = PerPoint * (Divisor > 0 ? floor(points / Divisor) : points). */
struct FJTSWeaponSkillDef
{
	const TCHAR* Name = TEXT("");
	EJTSWeaponStat Stat = EJTSWeaponStat::Damage;
	/** In the stat's natural unit: a fraction for percentages, centimetres for lengths, degrees for angles, a count otherwise. */
	double PerPoint = 0.0;
	int32 Divisor = 0;
	int32 Decimals = 0;
	/** Multiplies the value for display only (100 turns 0.05 into 5%). */
	double DisplayScale = 1.0;
	/** "{0}" is replaced with the formatted total for the invested points. */
	const TCHAR* Format = TEXT("{0}");
};

/** The aggregate of one weapon instance's body level and skill points. */
struct SPACE_API FJTSResolvedWeaponStats
{
	/** 1 + 12% per body level above the first (2.08 at level ten). */
	float LevelMultiplier = 1.0f;
	float Bonus[static_cast<int32>(EJTSWeaponStat::Count)] = {};

	float Get(EJTSWeaponStat Stat) const { return Bonus[static_cast<int32>(Stat)]; }
	float ScaleDamage(float Base) const { return Base * LevelMultiplier * (1.0f + Get(EJTSWeaponStat::Damage)); }
	/** Interval shrinks as fire rate rises. */
	float ScaleInterval(float Base) const { return Base / (1.0f + Get(EJTSWeaponStat::FireRate)); }
	/**
	 * Recoil only reduces the camera kick. At least a fifth of it always remains so a maxed weapon still
	 * reads as firing, and hip/aim spread is a separate channel.
	 */
	float ScaleViewKick(float Base) const
	{
		return Base * (1.0f - FMath::Clamp(Get(EJTSWeaponStat::Recoil), 0.0f, 0.8f));
	}
	float ScaleSpread(float Base) const { return Base * (1.0f - FMath::Clamp(Get(EJTSWeaponStat::Spread), 0.0f, 0.6f)); }
};

/** Effective ranged numbers for one weapon instance: definition mechanics with the upgrade bonuses folded in. */
struct SPACE_API FJTSRangedWeaponStats
{
	int32 MagazineSize = 0;
	float ReloadSeconds = 0.0f;
	float FireInterval = 0.0f;
	int32 Pellets = 1;
	float DamagePerPellet = 0.0f;
	float FalloffStartCm = 0.0f;
	float FalloffEndCm = 0.0f;
	float MinFalloffFraction = 1.0f;
	int32 PierceCount = 0;
	int32 ChainTargets = 0;
	float ChainRangeCm = 0.0f;
	float ChainFalloff = 0.3f;
	float EnergyCapacity = 0.0f;
	float EnergyPerShot = 1.0f;
	float RechargePerSecond = 0.0f;
	float ResumeFraction = 0.3f;
	float ProjectileSpeed = 0.0f;
	float ProjectileGravityScale = 1.0f;
	float BlastRadiusCm = 0.0f;
	int32 Fragments = 0;
	float FragmentDamage = 0.0f;

	bool HasMagazine() const { return MagazineSize > 0; }
	bool UsesEnergy() const { return EnergyCapacity > 0.0f; }
	bool IsProjectile() const { return ProjectileSpeed > 0.0f; }
	/** 1 inside the falloff start, easing down to MinFalloffFraction at the end distance. */
	float FalloffMultiplier(float DistanceCm) const;
};

/**
 * Pure rules for normal-weapon growth. No state: the server, UI and tests all evaluate the same numbers.
 * Per-instance state lives on FJTSItemInstance (WeaponBodyLevel, WeaponPoints).
 */
struct SPACE_API FJTSWeaponProgression
{
	static constexpr int32 MaxBodyLevel = 10;
	static constexpr int32 SkillCount = 6;
	static constexpr int32 MaxPointsPerSkill = 10;
	static constexpr int32 PointsPerLevel = 4;

	static bool IsUpgradeable(EJTSItemId ItemId);
	/** Six skill definitions for an upgradeable weapon, or null. */
	static const FJTSWeaponSkillDef* FindSkillDefs(EJTSItemId ItemId);

	static int32 GetPointBudget(int32 BodyLevel);
	static int32 SumPoints(const TArray<uint8>& Points);
	/** Pads to six entries and clamps each to 0..10. */
	static void NormalizePoints(TArray<uint8>& Points);
	/** True when every entry is within range and the total fits the body-level budget. */
	static bool IsValidAllocation(const TArray<uint8>& Points, int32 BodyLevel);

	static FJTSResolvedWeaponStats Resolve(const FJTSItemInstance& Item);

	static double EvaluateSkill(const FJTSWeaponSkillDef& Def, int32 Points);
	static FString FormatSkill(const FJTSWeaponSkillDef& Def, int32 Points);
	/** Points still needed for the next whole step of an integer skill, or negative for continuous skills. */
	static int32 PointsToNextStep(const FJTSWeaponSkillDef& Def, int32 Points);
	/** Whether combat code already consumes this channel. The UI marks the others as not yet active. */
	static bool IsStatImplemented(EJTSItemId ItemId, EJTSWeaponStat Stat);
	/** Folds a held weapon instance's upgrades into the definition's ranged mechanics. */
	static FJTSRangedWeaponStats ResolveRanged(const UJTSItemDefinition* Definition, const FJTSItemInstance& Item);

	/**
	 * Cost to raise a body from FromLevel to FromLevel + 1 (FromLevel 1..9), in Rock, Ore and Organic only.
	 * Scaled by the definition's UpgradeCostMultiplier. Returns false at the level cap.
	 */
	static bool GetUpgradeCost(const UJTSItemDefinition* Definition, int32 FromLevel, TArray<FJTSItemCost>& OutCost);
};
