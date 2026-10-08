#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "space/Systems/JTSGroundBodySeparation.h"

namespace
{
	FJTSGroundBody Body(uint64 Id, const FVector& Offset, float Radius = 45, float Mass = 1)
	{
		FJTSGroundBody B; B.Id = Id; B.Position = FVector(0, 0, 10000000) + Offset;
		B.Radius = Radius; B.InverseMass = Mass; return B;
	}
	FJTSGroundBodySeparationStats Step(TArray<FJTSGroundBody>& Bodies,
		const FJTSGroundBodySeparationSettings& Settings, float Delta = 1.0f / 60)
	{
		FJTSGroundBodySeparationStats Stats;
		FJTSGroundBodySeparation::Solve(Bodies, Settings, Delta,
			[](int32, const FVector&, const FVector& Desired) { return Desired; }, Stats);
		return Stats;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSBodyCoincidentTest, "JTS.Moon.BodyCollision.CoincidentAndWeights",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSBodyCoincidentTest::RunTest(const FString&)
{
	FJTSGroundBodySeparationSettings Settings;
	TArray<FJTSGroundBody> Bodies{Body(7, FVector::ZeroVector), Body(42, FVector::ZeroVector)};
	const FVector Start = Bodies[0].Position;
	const FVector Normal = FJTSGroundBodySeparation::CoincidentNormal(7, 42, FVector::UpVector);
	TestTrue(TEXT("Swapping stable IDs reverses the deterministic normal"), Normal.Equals(-FJTSGroundBodySeparation::CoincidentNormal(42, 7, FVector::UpVector), 1e-9));
	FJTSGroundBodySeparationStats Stats;
	for (int32 I = 0; I < 90; ++I) Stats = Step(Bodies, Settings);
	TestEqual(TEXT("Coincident pair converges without residual overlap"), Stats.ResidualPairs, 0);
	TestFalse(TEXT("No NaN from zero distance"), Bodies[0].Position.ContainsNaN() || Bodies[1].Position.ContainsNaN());
	TestTrue(TEXT("Equal inverse mass shares correction equally"),
		FMath::Abs(FVector::Distance(Start, Bodies[0].Position) - FVector::Distance(Start, Bodies[1].Position)) < .001);
	Bodies = {Body(7, FVector::ZeroVector, 45, 0), Body(42, FVector::ZeroVector)};
	for (int32 I = 0; I < 90; ++I) Stats = Step(Bodies, Settings);
	TestTrue(TEXT("Zero mass body stays fixed"), Bodies[0].Position.Equals(Start));
	TestEqual(TEXT("Mobile body takes full separation"), Stats.ResidualPairs, 0);
	Bodies[1].Position = Start; Bodies[1].InverseMass = 0;
	Stats = Step(Bodies, Settings);
	TestEqual(TEXT("Two immovable bodies retain a diagnosable overlap"), Stats.ResidualPairs, 1);
	TestTrue(TEXT("Zero total inverse mass safely skips correction"), Bodies[1].Position.Equals(Start));
	Bodies = {Body(7, FVector::ZeroVector), Body(42, FVector::ZeroVector)};
	Stats = Step(Bodies, Settings, .25f);
	TestTrue(TEXT("A long frame cannot amplify separation beyond the absolute step cap"), Stats.MaxStepCorrection <= Settings.MaxCorrectionPerStep + .001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSBodyGridTest, "JTS.Moon.BodyCollision.RadiusHeightAndGrid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSBodyGridTest::RunTest(const FString&)
{
	FJTSGroundBodySeparationSettings Settings; Settings.CellSize = 16; Settings.MaxCorrectionSpeed = 600;
	TArray<FJTSGroundBody> Bodies{Body(1, FVector::ZeroVector, 20), Body(2, FVector(510, 0, 0), 500)};
	FJTSGroundBodySeparationStats Stats;
	for (int32 I = 0; I < 30; ++I) Stats = Step(Bodies, Settings);
	TestTrue(TEXT("Grid finds different-size bodies more than one neighbor cell apart"), FVector::Distance(Bodies[0].Position, Bodies[1].Position) > 520.7);
	Bodies = {Body(1, FVector::ZeroVector), Body(2, FVector(0,0,250))};
	Stats = Step(Bodies, Settings);
	TestEqual(TEXT("Different height regions do not push each other"), Stats.ResidualPairs, 0);
	Bodies[1].Position = Bodies[0].Position; Bodies[1].Layer = 1;
	Stats = Step(Bodies, Settings);
	TestEqual(TEXT("Independent collision layers remain independent"), Stats.ResidualPairs, 0);
	Bodies[1].Layer = 0; Bodies[1].bParticipating = false;
	Stats = Step(Bodies, Settings);
	TestEqual(TEXT("Disabled footprint excluded"), Stats.Participants, 1);
	Bodies.Reset();
	for (int32 I = 0; I < 200; ++I) Bodies.Add(Body(I + 1, FVector((I%20)*250, (I/20)*250, 0)));
	Settings.CellSize = 160;
	Stats = Step(Bodies, Settings);
	AddInfo(FString::Printf(TEXT("200 spaced footprints: %d candidate pair checks, %.3f ms"), Stats.PairChecks, Stats.Milliseconds));
	TestTrue(TEXT("Ordinary grid broadphase avoids all-pairs work"), Stats.PairChecks < 5000);
	// Rotate the complete scene onto the side of a planet; the solver must not assume world Z.
	Bodies = {Body(1, FVector::ZeroVector), Body(2, FVector::ZeroVector)};
	const FQuat Turn(FVector::YAxisVector, UE_PI / 2);
	for (auto& B : Bodies) B.Position = Turn.RotateVector(B.Position);
	for (int32 I = 0; I < 90; ++I) Stats = Step(Bodies, Settings);
	TestEqual(TEXT("Radial side-of-planet footprints separate"), Stats.ResidualPairs, 0);
	TestTrue(TEXT("Side-of-planet correction stays horizontal to gravity"), FMath::Abs(Bodies[0].Position.X - 10000000) < .01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSBodyDenseTest, "JTS.Moon.BodyCollision.DenseBoundedDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSBodyDenseTest::RunTest(const FString&)
{
	FJTSGroundBodySeparationSettings Settings;
	TArray<FJTSGroundBody> Bodies, Reversed;
	for (int32 I = 0; I < 200; ++I) Bodies.Add(Body(I + 1, FVector::ZeroVector));
	for (int32 I = 199; I >= 0; --I) Reversed.Add(Bodies[I]);
	FJTSGroundBodySeparationStats Stats;
	float Maximum = 0, TailMovement = 0, TotalMs = 0;
	for (int32 I = 0; I < 1200; ++I)
	{
		const FVector Last = Bodies[0].Position;
		Stats = Step(Bodies, Settings); TotalMs += Stats.Milliseconds;
		Maximum = FMath::Max(Maximum, Stats.MaxStepCorrection);
		if (I >= 1140) TailMovement = FMath::Max(TailMovement, float(FVector::Distance(Last, Bodies[0].Position)));
		if (I < 60)
		{
			Step(Reversed, Settings);
			for (int32 J = 0; J < Bodies.Num(); ++J)
				if (!Bodies[J].Position.Equals(Reversed[199-J].Position, .00001)) { AddError(TEXT("Result depends on input traversal order")); return false; }
		}
		for (const auto& B : Bodies) if (B.Position.ContainsNaN()) { AddError(TEXT("Dense solver produced NaN")); return false; }
	}
	AddInfo(FString::Printf(TEXT("200 coincident bodies: residual %d, max depth %.3f, max step %.3f cm, tail motion %.5f cm, mean %.3f ms"),
		Stats.ResidualPairs, Stats.MaxPenetration, Maximum, TailMovement, TotalMs/1200));
	TestEqual(TEXT("A finite number of simulation steps unfolds 200 coincident bodies"), Stats.ResidualPairs, 0);
	const float StepBudget = FMath::Min(Settings.MaxCorrectionPerStep, Settings.MaxCorrectionSpeed * (1.0f / 60.0f));
	TestTrue(TEXT("Dense displacement never exceeds per-step speed budget"), Maximum <= StepBudget + 0.001f);
	TestTrue(TEXT("Settled cluster has no continuing high-frequency displacement"), TailMovement < .01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSBodyHeadOnTest, "JTS.Moon.BodyCollision.HeadOnAndConstrainedResidual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSBodyHeadOnTest::RunTest(const FString&)
{
	FJTSGroundBodySeparationSettings Settings;
	TArray<FJTSGroundBody> Bodies{Body(1, FVector(-130,0,0)), Body(2, FVector(130,0,0))};
	for (int32 I = 0; I < 120; ++I)
	{
		Bodies[0].Position.X += 2; Bodies[1].Position.X -= 2;
		Step(Bodies, Settings);
		TestTrue(TEXT("Head-on movement retains independent space"), FVector::Distance(Bodies[0].Position, Bodies[1].Position) >= 90.7);
	}
	Bodies = {Body(1, FVector::ZeroVector), Body(2, FVector::ZeroVector)};
	FJTSGroundBodySeparationStats Stats;
	int32 Constraints = 0;
	FJTSGroundBodySeparation::Solve(Bodies, Settings, 1.f/60,
		[&](int32, const FVector& From, const FVector&) { ++Constraints; return From; }, Stats);
	TestEqual(TEXT("Environmental constraints feed actual positions into residual diagnostics"), Stats.ResidualPairs, 1);
	TestTrue(TEXT("No movement is invented in an unavailable space"), Bodies[0].Position.Equals(Bodies[1].Position));
	TestTrue(TEXT("Bounded solver terminates when environment blocks progress"), Stats.Iterations <= Settings.Iterations && Constraints <= Settings.Iterations*2);
	return true;
}
#endif
