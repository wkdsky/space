#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "space/World/JTSPopulationRecovery.h"
#include "space/World/JTSMoonResourceDistribution.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSPopulationRecoveryTest, "JTS.Ecology.DepletionRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSPopulationRecoveryTest::RunTest(const FString&)
{
	FJTSPopulationRecoverySettings Settings;
	Settings.Delay = 10;
	Settings.Duration = 120;
	Settings.BatchSize = 100;
	FJTSPopulationRecovery Recovery;
	Recovery.Reset(100);
	TestEqual(TEXT("An entire population can be depleted"), Recovery.Update(0, 100, 0, Settings), 0);
	TestEqual(TEXT("No immediate replacement during the cooldown"), Recovery.Update(0, 100, 10, Settings), 0);
	int32 Live = 0;
	int32 FirstQuarter = 0;
	int32 LastQuarter = 0;
	for (int32 Time = 11; Time <= 130; ++Time)
	{
		const int32 Added = Recovery.Update(Live, 100, Time, Settings);
		Recovery.Commit(Added);
		Live += Added;
		if (Time <= 40) FirstQuarter += Added;
		if (Time > 100) LastQuarter += Added;
	}
	TestTrue(TEXT("Recovery starts fast and slows towards capacity"), FirstQuarter > LastQuarter * 4);
	TestEqual(TEXT("The final unit returns within the finite period"), Live, 100);
	TestEqual(TEXT("Full population never earns extra replacements"), Recovery.Update(Live, 100, 10000, Settings), 0);

	Recovery.Reset(100);
	Recovery.Update(0, 100, 0, Settings);
	int32 Harvested = 0;
	for (int32 Time = 1; Time <= 260; ++Time)
	{
		const int32 Added = Recovery.Update(0, 100, Time, Settings);
		Recovery.Commit(Added);
		Harvested += Added; // Players immediately harvest every replacement, including the last unit.
	}
	TestTrue(TEXT("Sustained harvesting permits an empty world without restarting older cohorts"), Harvested > 100 && Harvested < 600);
	Live = 0;
	for (int32 Time = 261; Time <= 400; ++Time)
	{
		const int32 Added = Recovery.Update(Live, 100, Time, Settings);
		Live += Added;
		Recovery.Commit(Added);
	}
	TestEqual(TEXT("Stopping sustained harvesting eventually restores the original capacity"), Live, 100);

	Settings.BatchSize = 3;
	Recovery.Reset(100);
	Recovery.Update(0, 100, 0, Settings);
	TestEqual(TEXT("Deferred placement cannot cause a catch-up burst"), Recovery.Update(0, 100, 1000, Settings), 3);
	TestEqual(TEXT("Failed placement preserves the available budget"), Recovery.Update(0, 100, 1001, Settings), 3);
	Recovery.Commit(2);
	TestEqual(TEXT("Capacity reduction cannot overpopulate a disabled entrance"), Recovery.Update(2, 0, 1002, Settings), 0);
	Recovery.Reset(0);
	TestEqual(TEXT("A new activation cannot inherit the old credit"), Recovery.Update(0, 100, 2000, Settings), 0);
	int32 EarlyResidents = 0;
	for (int32 Seed = 0; Seed < 128; ++Seed)
	{
		FJTSPopulationRecovery SingleResident;
		SingleResident.Reset(1, Seed);
		SingleResident.Update(0, 1, 0, Settings);
		EarlyResidents += SingleResident.Update(0, 1, 40, Settings);
	}
	TestTrue(TEXT("Single-resident refuges recover at staggered times along the curve"), EarlyResidents > 30 && EarlyResidents < 110);

	FJTSMoonResourcePatchSettings Patches;
	Patches.bEnabled = true;
	float MinDensity = 1, MaxDensity = 0, MinOre = 100, MaxOre = 0;
	for (int32 X = -20; X <= 20; ++X)
	{
		const FVector Point(X * 400, 1500, 500);
		const float Density = Patches.Acceptance(Point, 42);
		MinDensity = FMath::Min(MinDensity, Density); MaxDensity = FMath::Max(MaxDensity, Density);
		const float Ore = Patches.KindWeight(EJTSMoonResourceKind::MediumMetalRock, Point, 42);
		MinOre = FMath::Min(MinOre, Ore); MaxOre = FMath::Max(MaxOre, Ore);
		TestTrue(TEXT("Nearby points belong to a continuous patch"), FMath::Abs(Density - Patches.Acceptance(Point + FVector(1, 0, 0), 42)) < 0.005f);
	}
	TestTrue(TEXT("The surface includes dense and sparse areas"), MaxDensity - MinDensity > 0.15f);
	TestTrue(TEXT("Regional ore affinity varies independently of per-node randomness"), MaxOre > MinOre * 3);
	return true;
}
#endif
