// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Items/JTSStellarProgression.h"

namespace
{
	using FDef = FJTSStellarSkillDef;
	// Name, Base, PerX, Divisor, Max, OffAtZero, Decimals, Format. Values follow the design document, chapter 8.
	const FDef JetSkills[] = {
		{ TEXT("热流伤害"), 1.0, 0.04, 0, 0, false, 2, TEXT("火伤 ×{0}") },
		{ TEXT("喷流宽度"), 50.0, 2.0, 0, 0, false, 0, TEXT("喷幅 {0}°") },
		{ TEXT("喷射距离"), 18.0, 0.6, 0, 0, false, 1, TEXT("射程 {0} 米") },
		{ TEXT("点燃效率"), 1.0, 0.1, 0, 0, false, 2, TEXT("热量累积 ×{0}") },
		{ TEXT("火种传播"), 0.0, 1.0, 2, 5, false, 0, TEXT("传播 {0} 个目标") },
		{ TEXT("热能回收"), 22.0, -0.66, 0, 0, false, 1, TEXT("消耗 {0} 能量/秒") } };
	const FDef ExplosionSkills[] = {
		{ TEXT("爆心威力"), 1.0, 0.05, 0, 0, false, 2, TEXT("爆炸伤害 ×{0}") },
		{ TEXT("爆炸半径"), 3.0, 0.15, 0, 0, false, 2, TEXT("半径 {0} 米") },
		{ TEXT("次级爆破"), 0.0, 1.0, 3, 0, false, 0, TEXT("额外爆点 {0} 个") },
		{ TEXT("余焰延续"), 1.5, 0.25, 0, 0, false, 2, TEXT("余焰 {0} 秒") },
		{ TEXT("熔甲冲击"), 10.0, 2.0, 0, 0, false, 0, TEXT("右键破甲 {0}%") },
		{ TEXT("核心冷却"), 8.0, -0.24, 0, 0, false, 2, TEXT("右键冷却 {0} 秒") } };
	const FDef HealingSkills[] = {
		{ TEXT("生命灌注"), 6.0, 0.25, 0, 0, false, 2, TEXT("每秒回复 {0}% 生命") },
		{ TEXT("体力循环"), 10.0, 1.0, 0, 0, false, 0, TEXT("每秒回复 {0}% 体力") },
		{ TEXT("治愈覆盖"), 3.0, 0.2, 0, 0, false, 1, TEXT("半径 {0} 米") },
		{ TEXT("溢出护膜"), 0.0, 2.0, 0, 20, false, 0, TEXT("过量治疗转盾 {0}% 生命") },
		{ TEXT("紧急回流"), 15.0, 1.0, 0, 0, false, 0, TEXT("低血追加治疗 {0}%") },
		{ TEXT("水流节能"), 18.0, -0.54, 0, 0, false, 1, TEXT("消耗 {0} 能量/秒") } };
	const FDef FreezingSkills[] = {
		{ TEXT("冰锥伤害"), 1.0, 0.04, 0, 0, false, 2, TEXT("冰伤 ×{0}") },
		{ TEXT("凝结效率"), 1.0, 0.1, 0, 0, false, 2, TEXT("寒冷累积 ×{0}") },
		{ TEXT("锥体穿透"), 1.0, 1.0, 2, 0, false, 0, TEXT("可穿透 {0} 个目标") },
		{ TEXT("冰雹覆盖"), 5.0, 0.25, 0, 0, false, 2, TEXT("冰雹半径 {0} 米") },
		{ TEXT("碎裂波及"), 3.0, 0.15, 0, 0, false, 2, TEXT("碎冰半径 {0} 米") },
		{ TEXT("低温循环"), 22.0, -0.66, 0, 0, false, 1, TEXT("冰锥消耗 {0} 能量/秒") } };
	const FDef FocusSkills[] = {
		{ TEXT("光弹强度"), 1.0, 0.04, 0, 0, false, 2, TEXT("伤害 ×{0}") },
		{ TEXT("贯穿层数"), 4.0, 1.0, 2, 0, false, 0, TEXT("可穿透 {0} 个目标") },
		{ TEXT("折射弹跳"), 1.0, 1.0, 2, 0, false, 0, TEXT("弹射 {0} 次") },
		{ TEXT("灼伤传递"), 0.0, 5.0, 0, 0, false, 0, TEXT("传递系数 {0}%") },
		{ TEXT("脉冲频率"), 8.0, 0.24, 0, 0, false, 2, TEXT("射速 {0} 发/秒") },
		{ TEXT("焦点锁定"), 0.0, 0.05, 0, 0, false, 2, TEXT("弱点倍率 +{0}") } };
	const FDef ShapingSkills[] = {
		{ TEXT("剑锋凝聚"), 1.0, 0.05, 0, 0, false, 2, TEXT("斩击伤害 ×{0}") },
		{ TEXT("剑弧展开"), 110.0, 6.0, 0, 0, false, 0, TEXT("扇角 {0}°") },
		{ TEXT("光刃延展"), 2.4, 0.12, 0, 0, false, 2, TEXT("距离 {0} 米") },
		{ TEXT("蓄力聚能"), 1.2, -0.04, 0, 0, false, 2, TEXT("圆斩蓄力 {0} 秒") },
		{ TEXT("回旋循环"), 6.0, -0.3, 0, 0, false, 1, TEXT("圆斩冷却 {0} 秒") },
		{ TEXT("瞬时招架"), 0.12, 0.012, 0, 0, false, 3, TEXT("招架窗口 {0} 秒") } };
	const FDef EffectSkills[] = {
		{ TEXT("辐照伤害"), 1.0, 0.05, 0, 0, false, 2, TEXT("光场伤害 ×{0}") },
		{ TEXT("光场范围"), 4.0, 0.2, 0, 0, false, 1, TEXT("半径 {0} 米") },
		{ TEXT("光压定身"), 0.6, 0.06, 0, 0, false, 2, TEXT("定身 {0} 秒") },
		{ TEXT("护盾容量"), 20.0, 2.0, 0, 0, false, 0, TEXT("护盾 {0}% 最大生命") },
		{ TEXT("护盾轮转"), 12.0, -0.36, 0, 0, false, 1, TEXT("护盾冷却 {0} 秒") },
		{ TEXT("辐照节能"), 26.0, -0.78, 0, 0, false, 1, TEXT("消耗 {0} 能量/秒") } };
	const FDef DiffusionSkills[] = {
		{ TEXT("暗弹浓度"), 1.0, 0.04, 0, 0, false, 2, TEXT("直接伤害 ×{0}") },
		{ TEXT("侵蚀强度"), 1.0, 0.06, 0, 0, false, 2, TEXT("腐蚀伤害 ×{0}") },
		{ TEXT("穿透扩张"), 1.0, 1.0, 2, 0, false, 0, TEXT("可穿透 {0} 个目标") },
		{ TEXT("碎片分流"), 0.0, 1.0, 3, 0, false, 0, TEXT("命中后分支 {0} 个") },
		{ TEXT("溶甲效能"), 10.0, 2.0, 0, 0, false, 0, TEXT("削甲 {0}%") },
		{ TEXT("残蚀传播"), 1.5, 0.15, 0, 0, true, 2, TEXT("死亡传播半径 {0} 米") } };
	const FDef ShadowSkills[] = {
		{ TEXT("影蚀强度"), 1.0, 0.05, 0, 0, false, 2, TEXT("腐蚀秒伤 ×{0}") },
		{ TEXT("影域面积"), 4.0, 0.3, 0, 0, false, 1, TEXT("半径 {0} 米") },
		{ TEXT("迟缓压制"), 20.0, 3.0, 0, 50, false, 0, TEXT("减速 {0}%") },
		{ TEXT("侵甲深度"), 10.0, 2.0, 0, 0, false, 0, TEXT("削甲 {0}%") },
		{ TEXT("暗盾厚度"), 15.0, 2.0, 0, 0, false, 0, TEXT("暗盾 {0}% 最大生命") },
		{ TEXT("影流节能"), 24.0, -0.72, 0, 0, false, 1, TEXT("消耗 {0} 能量/秒") } };
	const FDef BlackHoleSkills[] = {
		{ TEXT("潮汐伤害"), 1.0, 0.05, 0, 0, false, 2, TEXT("伤害 ×{0}") },
		{ TEXT("引力覆盖"), 9.0, 0.5, 0, 0, false, 2, TEXT("吸引半径 {0} 米") },
		{ TEXT("引力牵引"), 1.0, 0.1, 0, 0, false, 2, TEXT("拉拽强度 ×{0}") },
		{ TEXT("黑洞数量"), 1.0, 1.0, 2, 6, false, 0, TEXT("同时存在 {0} 个黑洞（满额替换最早一个）") },
		{ TEXT("斥力轮转"), 1.2, -0.05, 0, 0, false, 2, TEXT("斥力间隔 {0} 秒") },
		{ TEXT("场能回路"), 30.0, -0.9, 0, 0, false, 1, TEXT("释放消耗 {0} 能量/次；斥力消耗同步降低") } };
	const FDef InstanceSkills[] = {
		{ TEXT("实例配额"), 4.0, 1.0, 2, 9, false, 0, TEXT("同时 {0} 个机器人") },
		{ TEXT("作战指令"), 1.0, 0.04, 0, 0, false, 2, TEXT("单机伤害 ×{0}") },
		{ TEXT("机体完整"), 1.0, 0.08, 0, 0, false, 2, TEXT("机器人生命 ×{0}") },
		{ TEXT("存在维持"), 20.0, 2.0, 0, 0, false, 0, TEXT("存在 {0} 秒") },
		{ TEXT("自爆协议"), 2.0, 0.2, 0, 0, false, 1, TEXT("自爆半径 {0} 米") },
		{ TEXT("制造流水"), 2.0, -0.1, 0, 0, false, 2, TEXT("制造间隔 {0} 秒") } };
	const FDef DisassemblySkills[] = {
		{ TEXT("拆解强度"), 1.0, 0.05, 0, 0, false, 2, TEXT("分解预算 ×{0}") },
		{ TEXT("识别速度"), 0.8, -0.035, 0, 0, false, 3, TEXT("锁定 {0} 秒") },
		{ TEXT("分解分支"), 0.0, 1.0, 3, 0, false, 0, TEXT("额外目标 {0} 个") },
		{ TEXT("残骸冲击"), 1.5, 0.15, 0, 0, true, 2, TEXT("散架冲击半径 {0} 米") },
		{ TEXT("结构作业"), 1.5, -0.07, 0, 0, false, 2, TEXT("工程锁定 {0} 秒") },
		{ TEXT("能量回收"), 1.0, 0.4, 0, 0, false, 1, TEXT("击杀回能 {0}") } };
}

int32 FJTSStellarProgression::GetCoreBudget(int32 CoreLevel)
{
	return FMath::Clamp(CoreLevel, 1, MaxCoreLevel) - 1;
}

int32 FJTSStellarProgression::GetUpgradeUnitCost(int32 CoreLevel)
{
	return 1 + GetCoreBudget(CoreLevel) / 10;
}

int32 FJTSStellarProgression::SumPoints(const TArray<uint8>& Points)
{
	int32 Total = 0;
	for (const uint8 Point : Points) Total += Point;
	return Total;
}

void FJTSStellarProgression::NormalizePoints(TArray<uint8>& Points)
{
	Points.SetNumZeroed(SkillCount);
	for (uint8& Point : Points) Point = FMath::Min<uint8>(Point, MaxPointsPerSkill);
}

void FJTSStellarProgression::ComputeEffectiveLevels(const TArray<uint8>& Points, int32 Budget,
	TArray<double>& OutLevels)
{
	TArray<uint8> Recorded = Points;
	NormalizePoints(Recorded);
	const int32 Sum = SumPoints(Recorded);
	const double Scale = Sum <= 0 ? 0.0 : FMath::Min(1.0, static_cast<double>(FMath::Max(Budget, 0)) / Sum);
	OutLevels.SetNum(SkillCount);
	for (int32 Index = 0; Index < SkillCount; ++Index) OutLevels[Index] = Recorded[Index] * Scale;
}

const FJTSStellarSkillDef* FJTSStellarProgression::FindSkillDefs(FName AttachmentId)
{
	static const TMap<FName, const FDef*> Catalog = {
		{ FName(TEXT("JetTube")), JetSkills }, { FName(TEXT("ExplosionTube")), ExplosionSkills },
		{ FName(TEXT("HealingTube")), HealingSkills }, { FName(TEXT("FreezingTube")), FreezingSkills },
		{ FName(TEXT("FocusTube")), FocusSkills }, { FName(TEXT("ShapingTube")), ShapingSkills },
		{ FName(TEXT("EffectTube")), EffectSkills }, { FName(TEXT("DiffusionTube")), DiffusionSkills },
		{ FName(TEXT("ShadowTube")), ShadowSkills }, { FName(TEXT("BlackHoleTube")), BlackHoleSkills },
		{ FName(TEXT("InstanceTube")), InstanceSkills }, { FName(TEXT("DisassemblyTube")), DisassemblySkills } };
	const FDef* const* Found = Catalog.Find(AttachmentId);
	return Found ? *Found : nullptr;
}

double FJTSStellarProgression::EvaluateSkill(const FJTSStellarSkillDef& Def, double EffectiveLevel)
{
	if (Def.bOffAtZero && EffectiveLevel <= 0.0) return 0.0;
	const double Steps = Def.Divisor > 0 ? FMath::FloorToDouble(EffectiveLevel / Def.Divisor) : EffectiveLevel;
	double Value = Def.Base + Def.PerX * Steps;
	if (Def.MaxValue > 0.0) Value = FMath::Min(Value, Def.MaxValue);
	return Value;
}

FString FJTSStellarProgression::FormatSkill(const FJTSStellarSkillDef& Def, double EffectiveLevel)
{
	if (Def.bOffAtZero && EffectiveLevel <= 0.0) return TEXT("未解锁（投入 1 点后生效）");
	const FString Number = FString::Printf(TEXT("%.*f"), Def.Decimals, EvaluateSkill(Def, EffectiveLevel));
	return FString(Def.Format).Replace(TEXT("{0}"), *Number);
}

double FJTSStellarProgression::PointsToNextStep(const FJTSStellarSkillDef& Def, double EffectiveLevel)
{
	if (Def.Divisor <= 0) return -1.0;
	return (FMath::FloorToDouble(EffectiveLevel / Def.Divisor) + 1.0) * Def.Divisor - EffectiveLevel;
}
