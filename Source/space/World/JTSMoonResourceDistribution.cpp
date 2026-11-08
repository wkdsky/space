#include "space/World/JTSMoonResourceDistribution.h"

namespace
{
	float PatchNoise(const FVector& Point, float Size, int32 Seed, const FVector& Offset)
	{
		const uint32 Hash = GetTypeHash(Seed);
		const FVector SeedOffset(Hash & 255, (Hash >> 8) & 255, (Hash >> 16) & 255);
		return FMath::Clamp(2.0f * FMath::PerlinNoise3D(Point / FMath::Max(100.0f, Size) + SeedOffset + Offset), -1.0f, 1.0f);
	}
}

float FJTSMoonResourcePatchSettings::Acceptance(const FVector& Point, int32 Seed) const
{
	if (!bEnabled) return 1;
	const float Noise = PatchNoise(Point, PatchSize, Seed, FVector(17.3, 8.1, 2.7));
	const float Richness = FMath::Square(0.5f + 0.5f * Noise);
	return FMath::Lerp(1.0f, 0.1f + 0.9f * Richness, FMath::Clamp(DensityContrast, 0.0f, 1.0f));
}

float FJTSMoonResourcePatchSettings::KindWeight(EJTSMoonResourceKind Kind, const FVector& Point, int32 Seed) const
{
	if (!bEnabled) return 1;
	const float Ore = PatchNoise(Point, PatchSize, Seed, FVector(6.2, 29.4, 13.8));
	const bool bRock = Kind <= EJTSMoonResourceKind::LargeRock;
	const bool bCrystal = Kind == EJTSMoonResourceKind::MediumCrystalRock || Kind == EJTSMoonResourceKind::LargeCrystalRock;
	const float Affinity = bRock ? -Ore : bCrystal
		? 0.5f * (Ore + PatchNoise(Point, PatchSize, Seed, FVector(41.1, 3.6, 27.2))) : Ore;
	return FMath::Exp(Affinity * FMath::Clamp(CompositionContrast, 0.0f, 4.0f));
}

bool FJTSMoonResourceKinds::IsEnabled(EJTSMoonResourceKind Kind) const
{
	const bool Values[] = { bSmallRock, bMediumRock, bLargeRock, bMediumMetalRock,
		bLargeMetalRock, bMediumCrystalRock, bLargeCrystalRock };
	const int32 Index = static_cast<int32>(Kind);
	return Index >= 0 && Index < UE_ARRAY_COUNT(Values) && Values[Index];
}

float FJTSMoonResourceDistribution::GetPercentage(EJTSMoonResourceKind Kind) const
{
	const float Values[] = { SmallRock, MediumRock, LargeRock, MediumMetalRock,
		LargeMetalRock, MediumCrystalRock, LargeCrystalRock };
	const int32 Index = static_cast<int32>(Kind);
	return Index >= 0 && Index < UE_ARRAY_COUNT(Values) && EnabledKinds.IsEnabled(Kind) ? Values[Index] : 0;
}

float FJTSMoonResourceDistribution::GetTotalPercentage() const
{
	float Total = 0;
	for (int32 Index = 0; Index < static_cast<int32>(EJTSMoonResourceKind::Count); ++Index)
	{
		Total += GetPercentage(static_cast<EJTSMoonResourceKind>(Index));
	}
	return Total;
}

void FJTSMoonResourceDistribution::Normalize(FName EditedProperty)
{
	float* Values[] = { &SmallRock, &MediumRock, &LargeRock, &MediumMetalRock,
		&LargeMetalRock, &MediumCrystalRock, &LargeCrystalRock };
	const FName Names[] = { TEXT("SmallRock"), TEXT("MediumRock"), TEXT("LargeRock"), TEXT("MediumMetalRock"),
		TEXT("LargeMetalRock"), TEXT("MediumCrystalRock"), TEXT("LargeCrystalRock") };
	TArray<int32, TInlineAllocator<7>> Enabled;
	int32 Locked = INDEX_NONE;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Values); ++Index)
	{
		*Values[Index] = FMath::IsFinite(*Values[Index]) ? FMath::Clamp(*Values[Index], 0.0f, 100.0f) : 0;
		if (EnabledKinds.IsEnabled(static_cast<EJTSMoonResourceKind>(Index)))
		{
			Enabled.Add(Index);
			if (Names[Index] == EditedProperty) Locked = Index;
		}
	}
	if (Enabled.IsEmpty()) return;
	if (Enabled.Num() == 1) { *Values[Enabled[0]] = 100; return; }
	const double Budget = Locked == INDEX_NONE ? 100.0 : 100.0 - *Values[Locked];
	double Others = 0;
	for (int32 Index : Enabled) if (Index != Locked) Others += *Values[Index];
	const int32 AdjustableCount = Enabled.Num() - (Locked != INDEX_NONE ? 1 : 0);
	float Assigned = 0;
	int32 LastAdjusted = INDEX_NONE;
	for (int32 Index : Enabled)
	{
		if (Index == Locked) continue;
		*Values[Index] = static_cast<float>(Others > 0 ? Budget * *Values[Index] / Others : Budget / AdjustableCount);
		Assigned += *Values[Index];
		LastAdjusted = Index;
	}
	// Correct rounding in the remainder, without moving the value being edited.
	*Values[LastAdjusted] = FMath::Clamp(*Values[LastAdjusted] + static_cast<float>(Budget) - Assigned, 0.0f, 100.0f);
}

void FJTSMoonResourceDistribution::SetEnabledKinds(const FJTSMoonResourceKinds& Kinds)
{
	EnabledKinds = Kinds;
	bEnableSmallRock = Kinds.bSmallRock;
	bEnableMediumRock = Kinds.bMediumRock;
	bEnableLargeRock = Kinds.bLargeRock;
	bEnableMediumMetalRock = Kinds.bMediumMetalRock;
	bEnableLargeMetalRock = Kinds.bLargeMetalRock;
	bEnableMediumCrystalRock = Kinds.bMediumCrystalRock;
	bEnableLargeCrystalRock = Kinds.bLargeCrystalRock;
}
