// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Items/JTSWeaponProgression.h"

#include "space/Items/JTSItemDefinition.h"

namespace
{
	using FDef = FJTSWeaponSkillDef;
	using EStat = EJTSWeaponStat;

	// Name, Stat, PerPoint, Divisor, Decimals, DisplayScale, Format. Rules follow design document chapter 5.
	// Lengths are centimetres, angles degrees, times seconds, percentages fractions (display scale 100).
	const FDef ShortBladeSkills[] = {
		{ TEXT("刃口伤害"), EStat::Damage, 0.05, 0, 0, 100.0, TEXT("伤害 +{0}%") },
		{ TEXT("挥刺频率"), EStat::FireRate, 0.025, 0, 1, 100.0, TEXT("挥刺速度 +{0}%") },
		{ TEXT("攻击距离"), EStat::Reach, 3.0, 0, 0, 1.0, TEXT("距离 +{0} 厘米") },
		{ TEXT("格挡窗口"), EStat::GuardWindow, 0.012, 0, 3, 1.0, TEXT("格挡窗口 +{0} 秒") },
		{ TEXT("反击伤害"), EStat::CounterDamage, 0.08, 0, 0, 100.0, TEXT("反击伤害 +{0}%") },
		{ TEXT("体力节约"), EStat::StaminaCost, 0.03, 0, 0, 100.0, TEXT("体力消耗 −{0}%") } };
	const FDef ShockPoleSkills[] = {
		{ TEXT("刃伤"), EStat::Damage, 0.05, 0, 0, 100.0, TEXT("伤害 +{0}%") },
		{ TEXT("攻击扇角"), EStat::Arc, 4.0, 0, 0, 1.0, TEXT("扇角 +{0}°") },
		{ TEXT("攻击距离"), EStat::Reach, 8.0, 0, 0, 1.0, TEXT("距离 +{0} 厘米") },
		{ TEXT("额外命中"), EStat::ExtraTargets, 1.0, 0, 0, 1.0, TEXT("额外命中 +{0} 个目标") },
		{ TEXT("蓄力速度"), EStat::ChargeTime, 0.03, 0, 0, 100.0, TEXT("蓄力时间 −{0}%") },
		{ TEXT("受击硬直"), EStat::Stagger, 0.05, 0, 0, 100.0, TEXT("硬直强度 +{0}%") } };
	const FDef PowerHammerSkills[] = {
		{ TEXT("直接伤害"), EStat::Damage, 0.05, 0, 0, 100.0, TEXT("伤害 +{0}%") },
		{ TEXT("冲击半径"), EStat::BlastRadius, 8.0, 0, 0, 1.0, TEXT("冲击波半径 +{0} 厘米") },
		{ TEXT("破甲"), EStat::ArmorBreak, 0.02, 0, 0, 100.0, TEXT("破甲 +{0}%") },
		{ TEXT("蓄力速度"), EStat::ChargeTime, 0.03, 0, 0, 100.0, TEXT("蓄力时间 −{0}%") },
		{ TEXT("打断强度"), EStat::Stagger, 0.06, 0, 0, 100.0, TEXT("打断值 +{0}%") },
		{ TEXT("体力节约"), EStat::StaminaCost, 0.03, 0, 0, 100.0, TEXT("体力消耗 −{0}%") } };
	const FDef RailPistolSkills[] = {
		{ TEXT("弹伤"), EStat::Damage, 0.05, 0, 0, 100.0, TEXT("伤害 +{0}%") },
		{ TEXT("后坐力控制"), EStat::Recoil, 0.10, 0, 0, 100.0, TEXT("镜头后坐力 −{0}%") },
		{ TEXT("精度"), EStat::Spread, 0.05, 0, 0, 100.0, TEXT("散布 −{0}%") },
		{ TEXT("弹匣扩容"), EStat::Magazine, 1.0, 2, 0, 1.0, TEXT("弹匣 +{0} 发") },
		{ TEXT("快速换弹"), EStat::ReloadTime, 0.03, 0, 0, 100.0, TEXT("换弹时间 −{0}%") },
		{ TEXT("弱点打击"), EStat::WeakPoint, 0.05, 0, 2, 1.0, TEXT("弱点倍率 +{0}") } };
	const FDef AssaultRifleSkills[] = {
		{ TEXT("弹伤"), EStat::Damage, 0.05, 0, 0, 100.0, TEXT("伤害 +{0}%") },
		{ TEXT("射速"), EStat::FireRate, 0.03, 0, 0, 100.0, TEXT("射速 +{0}%") },
		{ TEXT("后坐力控制"), EStat::Recoil, 0.10, 0, 0, 100.0, TEXT("镜头后坐力 −{0}%") },
		{ TEXT("精度"), EStat::Spread, 0.05, 0, 0, 100.0, TEXT("散布 −{0}%") },
		{ TEXT("弹匣扩容"), EStat::Magazine, 2.0, 0, 0, 1.0, TEXT("弹匣 +{0} 发") },
		{ TEXT("快速换弹"), EStat::ReloadTime, 0.03, 0, 0, 100.0, TEXT("换弹时间 −{0}%") } };
	const FDef ShotgunSkills[] = {
		{ TEXT("散射伤害"), EStat::Damage, 0.05, 0, 0, 100.0, TEXT("总伤害 +{0}%") },
		{ TEXT("有效射程"), EStat::FalloffStart, 30.0, 0, 0, 1.0, TEXT("衰减起点 +{0} 厘米") },
		{ TEXT("弹丸数"), EStat::Pellets, 1.0, 5, 0, 1.0, TEXT("弹丸 +{0}（总伤害不变）") },
		{ TEXT("快速装填"), EStat::ReloadTime, 0.03, 0, 0, 100.0, TEXT("装填时间 −{0}%") },
		{ TEXT("击退"), EStat::Knockback, 0.05, 0, 0, 100.0, TEXT("击退 +{0}%") },
		{ TEXT("弹仓扩容"), EStat::Magazine, 1.0, 2, 0, 1.0, TEXT("弹仓 +{0} 发") } };
	const FDef RailSniperSkills[] = {
		{ TEXT("弹伤"), EStat::Damage, 0.05, 0, 0, 100.0, TEXT("伤害 +{0}%") },
		{ TEXT("弱点扫描"), EStat::WeakPoint, 0.08, 0, 2, 1.0, TEXT("弱点倍率 +{0}") },
		{ TEXT("穿甲导轨"), EStat::Pierce, 1.0, 5, 0, 1.0, TEXT("额外穿透 +{0}") },
		{ TEXT("开镜速度"), EStat::AimTime, 0.04, 0, 0, 100.0, TEXT("开镜时间 −{0}%") },
		{ TEXT("快栓"), EStat::BoltTime, 0.03, 0, 0, 100.0, TEXT("拉栓时间 −{0}%") },
		{ TEXT("屏息"), EStat::BreathCost, 0.05, 0, 0, 100.0, TEXT("屏息体力消耗 −{0}%") } };
	const FDef HeavyMachineGunSkills[] = {
		{ TEXT("弹伤"), EStat::Damage, 0.04, 0, 0, 100.0, TEXT("伤害 +{0}%") },
		{ TEXT("射速"), EStat::FireRate, 0.02, 0, 0, 100.0, TEXT("射速 +{0}%") },
		{ TEXT("弹链扩容"), EStat::Magazine, 5.0, 0, 0, 1.0, TEXT("弹链 +{0} 发") },
		{ TEXT("快速换弹"), EStat::ReloadTime, 0.03, 0, 0, 100.0, TEXT("换弹时间 −{0}%") },
		{ TEXT("稳定性"), EStat::Recoil, 0.10, 0, 0, 100.0, TEXT("镜头后坐力 −{0}%") },
		{ TEXT("穿透链带"), EStat::Pierce, 1.0, 5, 0, 1.0, TEXT("额外穿透 +{0}") } };
	const FDef GrenadeLauncherSkills[] = {
		{ TEXT("爆心伤害"), EStat::Damage, 0.05, 0, 0, 100.0, TEXT("爆心伤害 +{0}%") },
		{ TEXT("爆炸半径"), EStat::BlastRadius, 8.0, 0, 0, 1.0, TEXT("半径 +{0} 厘米") },
		{ TEXT("快速装填"), EStat::ReloadTime, 0.03, 0, 0, 100.0, TEXT("装填时间 −{0}%") },
		{ TEXT("破片"), EStat::Fragments, 1.0, 2, 0, 1.0, TEXT("额外破片 +{0}（共享破片伤害预算）") },
		{ TEXT("弹速"), EStat::ProjectileSpeed, 0.04, 0, 0, 100.0, TEXT("弹速 +{0}%") },
		{ TEXT("弹仓扩容"), EStat::Magazine, 1.0, 5, 0, 1.0, TEXT("弹仓 +{0} 发") } };
	const FDef ArcGunSkills[] = {
		{ TEXT("主目标伤害"), EStat::Damage, 0.05, 0, 0, 100.0, TEXT("伤害 +{0}%") },
		{ TEXT("连锁目标"), EStat::ChainTargets, 1.0, 3, 0, 1.0, TEXT("连锁 +{0} 个目标") },
		{ TEXT("跳跃距离"), EStat::ChainRange, 12.0, 0, 0, 1.0, TEXT("跳跃距离 +{0} 厘米") },
		{ TEXT("衰减控制"), EStat::ChainFalloff, 0.01, 0, 0, 100.0, TEXT("连锁衰减 −{0} 个百分点") },
		{ TEXT("能量容量"), EStat::EnergyCapacity, 0.06, 0, 0, 100.0, TEXT("充能条容量 +{0}%") },
		{ TEXT("充能速度"), EStat::RechargeRate, 0.05, 0, 0, 100.0, TEXT("充能速度 +{0}%") } };

	// Per-point step cost curve, shared by every weapon and scaled by the definition's UpgradeCostMultiplier.
	constexpr int32 BaseRockPerLevel = 2;
	constexpr int32 FirstOrganicLevel = 5;
}

bool FJTSWeaponProgression::IsUpgradeable(EJTSItemId ItemId)
{
	return FindSkillDefs(ItemId) != nullptr;
}

const FJTSWeaponSkillDef* FJTSWeaponProgression::FindSkillDefs(EJTSItemId ItemId)
{
	switch (ItemId)
	{
	case EJTSItemId::ShortBlade: return ShortBladeSkills;
	case EJTSItemId::ShockPole: return ShockPoleSkills;
	case EJTSItemId::PowerHammer: return PowerHammerSkills;
	case EJTSItemId::RailPistol: return RailPistolSkills;
	case EJTSItemId::AssaultRifle: return AssaultRifleSkills;
	case EJTSItemId::Shotgun: return ShotgunSkills;
	case EJTSItemId::RailSniper: return RailSniperSkills;
	case EJTSItemId::HeavyMachineGun: return HeavyMachineGunSkills;
	case EJTSItemId::GrenadeLauncher: return GrenadeLauncherSkills;
	case EJTSItemId::ArcGun: return ArcGunSkills;
	default: return nullptr;
	}
}

int32 FJTSWeaponProgression::GetPointBudget(int32 BodyLevel)
{
	return FMath::Clamp(BodyLevel, 1, MaxBodyLevel) * PointsPerLevel;
}

int32 FJTSWeaponProgression::SumPoints(const TArray<uint8>& Points)
{
	int32 Total = 0;
	for (const uint8 Point : Points) Total += Point;
	return Total;
}

void FJTSWeaponProgression::NormalizePoints(TArray<uint8>& Points)
{
	Points.SetNumZeroed(SkillCount);
	for (uint8& Point : Points) Point = FMath::Min<uint8>(Point, MaxPointsPerSkill);
}

bool FJTSWeaponProgression::IsValidAllocation(const TArray<uint8>& Points, int32 BodyLevel)
{
	if (Points.Num() > SkillCount) return false;
	for (const uint8 Point : Points)
	{
		if (Point > MaxPointsPerSkill) return false;
	}
	return SumPoints(Points) <= GetPointBudget(BodyLevel);
}

FJTSResolvedWeaponStats FJTSWeaponProgression::Resolve(const FJTSItemInstance& Item)
{
	FJTSResolvedWeaponStats Result;
	const FJTSWeaponSkillDef* const Defs = FindSkillDefs(Item.ItemId);
	if (Defs == nullptr) return Result;
	const int32 Level = FMath::Clamp(Item.WeaponBodyLevel, 1, MaxBodyLevel);
	Result.LevelMultiplier = 1.0f + 0.12f * (Level - 1);
	TArray<uint8> Points = Item.WeaponPoints;
	NormalizePoints(Points);
	// A corrupt or tampered allocation above the budget earns nothing rather than free power.
	if (SumPoints(Points) > GetPointBudget(Level)) return Result;
	for (int32 Index = 0; Index < SkillCount; ++Index)
	{
		Result.Bonus[static_cast<int32>(Defs[Index].Stat)] += static_cast<float>(EvaluateSkill(Defs[Index], Points[Index]));
	}
	return Result;
}

double FJTSWeaponProgression::EvaluateSkill(const FJTSWeaponSkillDef& Def, int32 Points)
{
	const int32 Clamped = FMath::Clamp(Points, 0, MaxPointsPerSkill);
	const int32 Steps = Def.Divisor > 0 ? Clamped / Def.Divisor : Clamped;
	return Def.PerPoint * Steps;
}

FString FJTSWeaponProgression::FormatSkill(const FJTSWeaponSkillDef& Def, int32 Points)
{
	const FString Number = FString::Printf(TEXT("%.*f"), Def.Decimals, EvaluateSkill(Def, Points) * Def.DisplayScale);
	return FString(Def.Format).Replace(TEXT("{0}"), *Number);
}

int32 FJTSWeaponProgression::PointsToNextStep(const FJTSWeaponSkillDef& Def, int32 Points)
{
	if (Def.Divisor <= 0) return -1;
	const int32 Clamped = FMath::Clamp(Points, 0, MaxPointsPerSkill);
	return (Clamped / Def.Divisor + 1) * Def.Divisor - Clamped;
}

bool FJTSWeaponProgression::IsStatImplemented(EJTSItemId ItemId, EJTSWeaponStat Stat)
{
	if (Stat == EJTSWeaponStat::Damage || Stat == EJTSWeaponStat::FireRate) return true;
	const bool bRanged = ItemId == EJTSItemId::RailPistol || ItemId == EJTSItemId::AssaultRifle
		|| ItemId == EJTSItemId::Shotgun || ItemId == EJTSItemId::RailSniper
		|| ItemId == EJTSItemId::HeavyMachineGun || ItemId == EJTSItemId::GrenadeLauncher
		|| ItemId == EJTSItemId::ArcGun;
	// Melee skills other than damage and attack speed wait for the melee pass. Knockback, aim time and
	// breath control have no consumer yet.
	if (!bRanged) return false;
	switch (Stat)
	{
	case EJTSWeaponStat::Knockback:
	case EJTSWeaponStat::AimTime:
	case EJTSWeaponStat::BreathCost:
		return false;
	default:
		return true;
	}
}

float FJTSRangedWeaponStats::FalloffMultiplier(float DistanceCm) const
{
	if (FalloffEndCm <= FalloffStartCm || MinFalloffFraction >= 1.0f) return 1.0f;
	const float Alpha = FMath::Clamp((DistanceCm - FalloffStartCm) / (FalloffEndCm - FalloffStartCm), 0.0f, 1.0f);
	return FMath::Lerp(1.0f, MinFalloffFraction, Alpha);
}

FJTSRangedWeaponStats FJTSWeaponProgression::ResolveRanged(const UJTSItemDefinition* Definition, const FJTSItemInstance& Item)
{
	FJTSRangedWeaponStats Result;
	if (!IsValid(Definition)) return Result;
	const FJTSResolvedWeaponStats Stats = Resolve(Item);
	const FJTSRangedMechanics& Base = Definition->RangedMechanics;
	auto Rounded = [](float Value) { return FMath::Max(0, FMath::RoundToInt(Value)); };

	Result.MagazineSize = Base.MagazineSize > 0 ? Base.MagazineSize + Rounded(Stats.Get(EJTSWeaponStat::Magazine)) : 0;
	Result.ReloadSeconds = FMath::Max(0.2f, Base.ReloadSeconds * (1.0f - FMath::Clamp(Stats.Get(EJTSWeaponStat::ReloadTime), 0.0f, 0.6f)));
	// Fire rate, bolt time and discharge period all shorten the time between shots.
	Result.FireInterval = FMath::Max(0.05f, Stats.ScaleInterval(Definition->RangedFireInterval)
		* (1.0f - FMath::Clamp(Stats.Get(EJTSWeaponStat::BoltTime), 0.0f, 0.5f)));
	Result.Pellets = FMath::Max(1, Base.Pellets + Rounded(Stats.Get(EJTSWeaponStat::Pellets)));
	// Extra pellets never add damage: the shot's total stays the same and is split.
	Result.DamagePerPellet = Stats.ScaleDamage(Definition->RangedDamage) / static_cast<float>(Result.Pellets);
	const float FalloffShift = Stats.Get(EJTSWeaponStat::FalloffStart);
	Result.FalloffStartCm = Base.FalloffStartCm > 0.0f ? Base.FalloffStartCm + FalloffShift : 0.0f;
	Result.FalloffEndCm = Base.FalloffEndCm > 0.0f ? Base.FalloffEndCm + FalloffShift : 0.0f;
	Result.MinFalloffFraction = Base.MinFalloffFraction;
	Result.PierceCount = Base.PierceCount + Rounded(Stats.Get(EJTSWeaponStat::Pierce));
	Result.ChainTargets = Base.ChainTargets > 0 ? Base.ChainTargets + Rounded(Stats.Get(EJTSWeaponStat::ChainTargets)) : 0;
	Result.ChainRangeCm = Base.ChainRangeCm + (Base.ChainTargets > 0 ? Stats.Get(EJTSWeaponStat::ChainRange) : 0.0f);
	Result.ChainFalloff = FMath::Max(0.05f, Base.ChainFalloff - Stats.Get(EJTSWeaponStat::ChainFalloff));
	Result.EnergyCapacity = Base.EnergyCapacity > 0.0f ? Base.EnergyCapacity * (1.0f + Stats.Get(EJTSWeaponStat::EnergyCapacity)) : 0.0f;
	Result.EnergyPerShot = Base.EnergyPerShot;
	Result.RechargePerSecond = Base.RechargePerSecond * (1.0f + Stats.Get(EJTSWeaponStat::RechargeRate));
	Result.ResumeFraction = Base.ResumeFraction;
	Result.ProjectileSpeed = Base.ProjectileSpeed > 0.0f ? Base.ProjectileSpeed * (1.0f + Stats.Get(EJTSWeaponStat::ProjectileSpeed)) : 0.0f;
	Result.ProjectileGravityScale = Base.ProjectileGravityScale;
	Result.BlastRadiusCm = Base.BlastRadiusCm > 0.0f ? Base.BlastRadiusCm + Stats.Get(EJTSWeaponStat::BlastRadius) : 0.0f;
	Result.Fragments = Base.BlastRadiusCm > 0.0f ? Rounded(Stats.Get(EJTSWeaponStat::Fragments)) : 0;
	// Fragments share one damage budget, so more of them means a lighter hit each.
	Result.FragmentDamage = Result.Fragments > 0
		? Stats.ScaleDamage(Definition->RangedDamage) * Base.FragmentDamageShare / static_cast<float>(Result.Fragments) : 0.0f;
	return Result;
}

bool FJTSWeaponProgression::GetUpgradeCost(const UJTSItemDefinition* Definition, int32 FromLevel,
	TArray<FJTSItemCost>& OutCost)
{
	OutCost.Reset();
	if (!IsValid(Definition) || FromLevel < 1 || FromLevel >= MaxBodyLevel) return false;
	const float Multiplier = FMath::Max(0.1f, Definition->UpgradeCostMultiplier);
	auto AddCost = [&OutCost](EJTSResourceType Type, float Amount)
	{
		const int32 Rounded = FMath::CeilToInt(Amount);
		if (Rounded <= 0) return;
		FJTSItemCost& Cost = OutCost.AddDefaulted_GetRef();
		Cost.ResourceType = Type;
		Cost.Amount = Rounded;
	};
	AddCost(EJTSResourceType::Rock, (BaseRockPerLevel + FromLevel) * Multiplier);
	AddCost(EJTSResourceType::Ore, FromLevel * Multiplier);
	if (FromLevel >= FirstOrganicLevel)
	{
		AddCost(EJTSResourceType::Organic, (FromLevel - FirstOrganicLevel + 1) * Multiplier);
	}
	return true;
}
