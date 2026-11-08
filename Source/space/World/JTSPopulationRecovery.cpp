#include "space/World/JTSPopulationRecovery.h"

double FJTSPopulationRecoverySettings::Fraction(double Age) const
{
	const double T = FMath::Clamp((Age - FMath::Max(0.0f, Delay)) / FMath::Max(1.0f, Duration), 0.0, 1.0);
	const double Shape = FMath::Clamp(FrontLoad, 0.0f, 8.0f);
	return Shape < 1.e-4 ? T : (1.0 - FMath::Exp(-Shape * T)) / (1.0 - FMath::Exp(-Shape));
}

void FJTSPopulationRecovery::Reset(int32 LiveCount, int32 Seed)
{
	Cohorts.Reset();
	Credit = 0;
	PreviousLive = FMath::Max(0, LiveCount);
	Random.Initialize(Seed);
}

int32 FJTSPopulationRecovery::Update(int32 LiveCount, int32 Capacity, double Now,
	const FJTSPopulationRecoverySettings& Settings)
{
	LiveCount = FMath::Max(0, LiveCount);
	const int32 Lost = FMath::Max(0, PreviousLive - LiveCount);
	if (Lost > 0) Cohorts.Add({Now, Lost, 0, Random.FRand()});
	PreviousLive = LiveCount;
	for (FCohort& Cohort : Cohorts)
	{
		// A stable random rounding phase also gives one-resident burrows the same recovery curve
		// across the population, rather than forcing every single resident to return at the very end.
		const int32 Released = FMath::Min(Cohort.Count, FMath::FloorToInt(
			Settings.Fraction(Now - Cohort.Start) * Cohort.Count + Cohort.RoundingPhase));
		Credit += FMath::Max(0, Released - Cohort.Released);
		Cohort.Released = Released;
	}
	Cohorts.RemoveAll([](const FCohort& Cohort) { return Cohort.Released >= Cohort.Count; });
	Credit = FMath::Clamp(Credit, 0.0, static_cast<double>(FMath::Max(0, Capacity - LiveCount)));
	// Failed/blocked placement retains credit, but never bypasses the per-check spawn budget.
	return FMath::Min(FMath::Max(1, Settings.BatchSize), FMath::FloorToInt(Credit + 1.e-6));
}

void FJTSPopulationRecovery::Commit(int32 Spawned)
{
	Credit = FMath::Max(0.0, Credit - Spawned);
	PreviousLive += FMath::Max(0, Spawned);
}
