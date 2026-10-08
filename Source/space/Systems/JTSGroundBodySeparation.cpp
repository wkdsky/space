#include "space/Systems/JTSGroundBodySeparation.h"
#include "HAL/PlatformTime.h"

namespace
{
	struct FGroundGrid
	{
		FVector Origin, X, Y;
		float MaxRadius = 0, MaxHeight = 0, CellSize = 1;
		TMap<FIntPoint, TArray<int32>> Cells;
		FIntPoint Key(const FVector& P) const
		{
			const FVector Offset = P - Origin;
			return FIntPoint(FMath::FloorToInt(FVector::DotProduct(Offset, X) / CellSize),
				FMath::FloorToInt(FVector::DotProduct(Offset, Y) / CellSize));
		}
	};

	bool Participates(const FJTSGroundBody& B)
	{
		return B.bParticipating && B.Id != 0 && !B.Position.ContainsNaN() && !B.Center.ContainsNaN()
			&& FMath::IsFinite(B.Radius) && B.Radius > 0 && FMath::IsFinite(B.HalfHeight)
			&& B.HalfHeight >= 0 && FMath::IsFinite(B.InverseMass) && B.InverseMass >= 0;
	}

	void Accumulate(const TArray<FJTSGroundBody>& Bodies, const FJTSGroundBodySeparationSettings& Settings,
		TArray<FVector>* Corrections, FJTSGroundBodySeparationStats& Stats, bool bDiagnose)
	{
		TArray<int32> Sorted;
		for (int32 I = 0; I < Bodies.Num(); ++I) if (Participates(Bodies[I])) Sorted.Add(I);
		Sorted.Sort([&](int32 A, int32 B) { return Bodies[A].Id < Bodies[B].Id; });
		TMap<uint32, FGroundGrid> Grids;
		for (int32 I : Sorted)
		{
			const auto& B = Bodies[I];
			FGroundGrid* Grid = Grids.Find(B.Group);
			if (!Grid)
			{
				Grid = &Grids.Add(B.Group);
				Grid->Origin = B.Center;
				const FVector Up = (B.Position - B.Center).GetSafeNormal(UE_DOUBLE_SMALL_NUMBER, FVector::UpVector);
				Up.FindBestAxisVectors(Grid->X, Grid->Y);
				Grid->CellSize = FMath::Max(1.0f, Settings.CellSize);
			}
			Grid->MaxRadius = FMath::Max(Grid->MaxRadius, B.Radius);
			Grid->MaxHeight = FMath::Max(Grid->MaxHeight, B.HalfHeight);
			Grid->Cells.FindOrAdd(Grid->Key(B.Position)).Add(I);
		}
		for (int32 I : Sorted)
		{
			const auto& A = Bodies[I];
			const auto& Grid = Grids.FindChecked(A.Group);
			const FIntPoint Key = Grid.Key(A.Position);
			// Orthographic projection contracts 3D distances. Include vertical extent as well as radii
			// so this 2D tangent chart cannot miss a contact on a curved planet or across a floor offset.
			const double R = A.Radius + Grid.MaxRadius + FMath::Max(0.0f, Settings.Skin);
			const double H = A.HalfHeight + Grid.MaxHeight;
			const int32 Range = FMath::CeilToInt(FMath::Sqrt(R * R + H * H) / Grid.CellSize) + 1;
			auto Visit = [&](const TArray<int32>& Cell)
			{
				for (int32 J : Cell)
				{
					const auto& B = Bodies[J];
					if (A.Id >= B.Id || A.Layer != B.Layer) continue;
					++Stats.PairChecks;
					const FVector RadialA = A.Position - A.Center, RadialB = B.Position - B.Center;
					const FVector UpA = RadialA.GetSafeNormal(UE_DOUBLE_SMALL_NUMBER, FVector::UpVector);
					const FVector UpB = RadialB.GetSafeNormal(UE_DOUBLE_SMALL_NUMBER, FVector::UpVector);
					if (FVector::DotProduct(UpA, UpB) < 0.5
						|| FMath::Abs(RadialA.Size() - RadialB.Size()) > A.HalfHeight + B.HalfHeight) continue;
					const FVector Up = (UpA + UpB).GetSafeNormal(UE_DOUBLE_SMALL_NUMBER, UpA);
					const FVector Delta = FVector::VectorPlaneProject(A.Position - B.Position, Up);
					const double Distance = Delta.Size();
					const double Penetration = A.Radius + B.Radius + FMath::Max(0.0f, Settings.Skin) - Distance;
					if (Penetration <= FMath::Max(0.0f, Settings.Tolerance)) continue;
					if (bDiagnose)
					{
						++Stats.ResidualPairs;
						Stats.MaxPenetration = FMath::Max(Stats.MaxPenetration, float(Penetration));
					}
					if (!Corrections) continue;
					const double Sum = double(A.InverseMass) + B.InverseMass;
					if (Sum <= UE_DOUBLE_SMALL_NUMBER) continue;
					const FVector Normal = Distance > 0.001 ? Delta / Distance
						: FJTSGroundBodySeparation::CoincidentNormal(A.Id, B.Id, Up);
					const FVector Correction = Normal * Penetration * FMath::Clamp(Settings.Relaxation, 0.0f, 1.0f);
					(*Corrections)[I] += FVector::VectorPlaneProject(Correction * (A.InverseMass / Sum), UpA);
					(*Corrections)[J] -= FVector::VectorPlaneProject(Correction * (B.InverseMass / Sum), UpB);
				}
			};
			// Very small cells / oversized bodies must not produce billions of empty-cell lookups.
			if (double(2 * int64(Range) + 1) * (2 * int64(Range) + 1) > Grid.Cells.Num() * 4.0)
			{
				for (const auto& Cell : Grid.Cells)
					if (FMath::Abs(int64(Cell.Key.X) - Key.X) <= Range && FMath::Abs(int64(Cell.Key.Y) - Key.Y) <= Range)
						Visit(Cell.Value);
			}
			else
			{
				for (int32 X = -Range; X <= Range; ++X)
				for (int32 Y = -Range; Y <= Range; ++Y)
					if (const auto* Cell = Grid.Cells.Find(Key + FIntPoint(X, Y))) Visit(*Cell);
			}
		}
	}
}

FVector FJTSGroundBodySeparation::CoincidentNormal(uint64 A, uint64 B, const FVector& Up)
{
	const uint64 Low = FMath::Min(A, B), High = FMath::Max(A, B);
	uint64 Hash = Low * 0x9e3779b97f4a7c15ull ^ High * 0xbf58476d1ce4e5b9ull;
	Hash ^= Hash >> 30; Hash *= 0xbf58476d1ce4e5b9ull; Hash ^= Hash >> 27;
	const double Angle = double(Hash & 0xffffff) / 16777216.0 * 2.0 * UE_PI;
	FVector X, Y; Up.FindBestAxisVectors(X, Y);
	return (X * FMath::Cos(Angle) + Y * FMath::Sin(Angle)) * (A < B ? 1.0 : -1.0);
}

void FJTSGroundBodySeparation::Solve(TArray<FJTSGroundBody>& Bodies,
	const FJTSGroundBodySeparationSettings& Settings, float DeltaSeconds,
	FConstraint Constrain, FJTSGroundBodySeparationStats& Stats)
{
	const double Started = FPlatformTime::Seconds();
	Stats = FJTSGroundBodySeparationStats();
	for (const auto& Body : Bodies) if (Participates(Body)) ++Stats.Participants;
	const int32 Iterations = FMath::Clamp(Settings.Iterations, 1, 12);
	const double Limit = FMath::Min(double(FMath::Max(0.0f, Settings.MaxCorrectionPerStep)),
		double(FMath::Max(0.0f, Settings.MaxCorrectionSpeed)) * FMath::Max(0.0f, DeltaSeconds));
	TArray<FVector> Corrections, Next;
	TArray<double> Travel; Travel.Init(0, Bodies.Num());
	Next.SetNumUninitialized(Bodies.Num());
	for (int32 Pass = 0; Pass < Iterations && Stats.Participants > 0; ++Pass)
	{
		Corrections.Init(FVector::ZeroVector, Bodies.Num());
		Accumulate(Bodies, Settings, &Corrections, Stats, false);
		++Stats.Iterations;
		bool bMoved = false;
		for (int32 I = 0; I < Bodies.Num(); ++I)
		{
			const FVector From = Bodies[I].Position;
			Next[I] = From;
			const double Remaining = FMath::Max(0.0, Limit - Travel[I]);
			const FVector Correction = Corrections[I].GetClampedToMaxSize(FMath::Min(Limit / Iterations, Remaining));
			if (!Correction.IsNearlyZero())
			{
				const FVector Actual = Constrain(I, From, From + Correction);
				if (!Actual.ContainsNaN() && FVector::Distance(From, Actual) <= FMath::Min(Correction.Size() + 0.5, Remaining + 0.00001))
				{
					Next[I] = Actual;
					Travel[I] += FVector::Distance(From, Actual);
					bMoved |= !Actual.Equals(From, 0.00001);
				}
			}
		}
		// No neighbor reads during this apply phase; next pass uses constrained positions.
		for (int32 I = 0; I < Bodies.Num(); ++I) Bodies[I].Position = Next[I];
		if (!bMoved) break;
	}
	Accumulate(Bodies, Settings, nullptr, Stats, true);
	for (double Distance : Travel) Stats.MaxStepCorrection = FMath::Max(Stats.MaxStepCorrection, float(Distance));
	Stats.Milliseconds = float((FPlatformTime::Seconds() - Started) * 1000);
}
