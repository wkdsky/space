#include "space/World/JTSMoonResourceCoverage.h"

#include "Math/RandomStream.h"
#include "space/World/JTSMoonResourceSpawner.h"
#include "space/World/JTSPlanetAnchor.h"

namespace
{
	bool SameDistribution(const AJTSMoonResourceSpawner* A, const AJTSMoonResourceSpawner* B)
	{
		if (A->ResourceDensity != B->ResourceDensity) return false;
		for (int32 Index = 0; Index < static_cast<int32>(EJTSMoonResourceKind::Count); ++Index)
		{
			const auto Kind = static_cast<EJTSMoonResourceKind>(Index);
			if (A->ResourceRatios.GetPercentage(Kind) != B->ResourceRatios.GetPercentage(Kind))
			{
				return false;
			}
		}
		return true;
	}
}

FJTSMoonResourceCoverage::FJTSMoonResourceCoverage(const AJTSPlanetAnchor* InPlanet,
	const TArray<AJTSMoonResourceSpawner*>& Spawners, int32 TieSeed)
	: Planet(InPlanet)
{
	if (!IsValid(Planet)) return;
	TArray<AJTSMoonResourceSpawner*> Ordered = Spawners;
	Ordered.Sort([](const AJTSMoonResourceSpawner& A, const AJTSMoonResourceSpawner& B)
	{
		return A.GetPathName() < B.GetPathName();
	});
	FRandomStream Random(TieSeed);
	for (AJTSMoonResourceSpawner* Spawner : Ordered)
	{
		if (!IsValid(Spawner) || Spawner->Radius <= 0) continue;
		FRegion& Region = Regions.AddDefaulted_GetRef();
		Region.Spawner = Spawner;
		Region.CenterDirection = Planet->GetRadialUpVector(Spawner->GetActorLocation());
		Region.Angle = FMath::Min(UE_DOUBLE_PI, static_cast<double>(Spawner->Radius) / Planet->GetApproximateRadius());
		Region.CosAngle = FMath::Cos(Region.Angle);
		Region.TieRank = Random.GetUnsignedInt();
	}
}

AJTSMoonResourceSpawner* FJTSMoonResourceCoverage::ResolveOwner(const FVector& SurfaceLocation) const
{
	if (!IsValid(Planet)) return nullptr;
	const FVector Direction = Planet->GetRadialUpVector(SurfaceLocation);
	TArray<const FRegion*, TInlineAllocator<8>> Covering;
	const FRegion* WinningConfiguration = nullptr;
	for (const FRegion& Region : Regions)
	{
		if (!Region.Spawner.IsValid() || Region.Spawner->IsActorBeingDestroyed()
			|| FVector::DotProduct(Direction, Region.CenterDirection) + 1.e-10 < Region.CosAngle) continue;
		Covering.Add(&Region);
		if (!WinningConfiguration || Region.Spawner->OverlapPriority < WinningConfiguration->Spawner->OverlapPriority
			|| (Region.Spawner->OverlapPriority == WinningConfiguration->Spawner->OverlapPriority
				&& Region.TieRank < WinningConfiguration->TieRank))
		{
			WinningConfiguration = &Region;
		}
	}
	if (!WinningConfiguration) return nullptr;
	// Only identical ratios AND density form one layer; priority does not choose its representative.
	const FRegion* Owner = WinningConfiguration;
	for (const FRegion* Region : Covering)
	{
		if (!SameDistribution(Region->Spawner.Get(), WinningConfiguration->Spawner.Get())) continue;
		if (Region->TieRank < Owner->TieRank)
		{
			Owner = Region;
		}
	}
	return Owner->Spawner.Get();
}

float FJTSMoonResourceCoverage::GetOwnedAreaSquareMeters(const AJTSMoonResourceSpawner* Spawner) const
{
	const FRegion* Region = Regions.FindByPredicate([Spawner](const FRegion& Candidate) { return Candidate.Spawner.Get() == Spawner; });
	if (!Region) return 0;
	const bool bHasOverlap = Regions.ContainsByPredicate([Region](const FRegion& Other)
	{
		return Other.Spawner != Region->Spawner && FMath::Acos(FMath::Clamp(
			FVector::DotProduct(Other.CenterDirection, Region->CenterDirection), -1.0, 1.0)) <= Other.Angle + Region->Angle;
	});
	if (!bHasOverlap) return Spawner->GetCoverageAreaSquareMeters();
	// Equal-area quadrature estimates the clipped region, so overlap rejection does not densify its remainder.
	constexpr int32 SampleCount = 512;
	constexpr double GoldenAngle = 2.399963229728653;
	FVector TangentX, TangentY;
	Region->CenterDirection.FindBestAxisVectors(TangentX, TangentY);
	int32 OwnedSamples = 0;
	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const double Cosine = FMath::Lerp(1.0, Region->CosAngle, (Index + 0.5) / SampleCount);
		const double Sine = FMath::Sqrt(FMath::Max(0.0, 1.0 - Cosine * Cosine));
		const double Azimuth = Index * GoldenAngle;
		const FVector Direction = Region->CenterDirection * Cosine
			+ (TangentX * FMath::Cos(Azimuth) + TangentY * FMath::Sin(Azimuth)) * Sine;
		const FVector Point = Planet->GetPlanetCenter() + Direction * Planet->GetApproximateRadius();
		if (ResolveOwner(Point) == Spawner) ++OwnedSamples;
	}
	return Spawner->GetCoverageAreaSquareMeters() * OwnedSamples / SampleCount;
}

bool FJTSMoonResourceCoverage::CanPlace(const FVector& SurfaceLocation, float MinimumSpacing) const
{
	if (!IsValid(Planet)) return false;
	const FVector Point = SpacingPoint(SurfaceLocation);
	const FIntVector Cell = CellAt(Point);
	const double Radius = Planet->GetApproximateRadius();
	const double QueryChord = 2 * Radius * FMath::Sin(FMath::Min(UE_DOUBLE_PI, MinimumSpacing / Radius) * 0.5);
	const int32 Range = FMath::Max(1, FMath::CeilToInt(QueryChord / CellSize));
	for (int32 X = -Range; X <= Range; ++X)
	for (int32 Y = -Range; Y <= Range; ++Y)
	for (int32 Z = -Range; Z <= Range; ++Z)
	{
		const TArray<int32>* Bucket = SpacingGrid.Find(Cell + FIntVector(X, Y, Z));
		if (!Bucket) continue;
		for (int32 Index : *Bucket)
		{
			const FPlacement& Placement = Placements[Index];
			if (Placement.bTracked && (!Placement.Resource.IsValid() || Placement.Resource->IsActorBeingDestroyed())) continue;
			const double Arc = FMath::Min(UE_DOUBLE_PI, FMath::Max(MinimumSpacing, Placement.MinimumSpacing) / Radius);
			const double Chord = 2 * Radius * FMath::Sin(Arc * 0.5);
			if (FVector::DistSquared(Point, Placement.Location) < Chord * Chord) return false;
		}
	}
	return true;
}

FVector FJTSMoonResourceCoverage::SpacingPoint(const FVector& Location) const
{
	return Planet->GetRadialUpVector(Location) * Planet->GetApproximateRadius();
}

FIntVector FJTSMoonResourceCoverage::CellAt(const FVector& Point) const
{
	return FIntVector(FMath::FloorToInt(Point.X / CellSize), FMath::FloorToInt(Point.Y / CellSize), FMath::FloorToInt(Point.Z / CellSize));
}

void FJTSMoonResourceCoverage::RebuildSpacingGrid()
{
	SpacingGrid.Reset();
	for (int32 Index = 0; Index < Placements.Num(); ++Index)
		SpacingGrid.FindOrAdd(CellAt(Placements[Index].Location)).Add(Index);
}

void FJTSMoonResourceCoverage::RemoveInvalidPlacements()
{
	if (Placements.RemoveAll([](const FPlacement& P)
		{ return P.bTracked && (!P.Resource.IsValid() || P.Resource->IsActorBeingDestroyed()); }) > 0) RebuildSpacingGrid();
}

void FJTSMoonResourceCoverage::RecordPlacement(const FVector& SurfaceLocation, float MinimumSpacing, AActor* Resource)
{
	if (!IsValid(Planet)) return;
	if (MinimumSpacing > CellSize)
	{
		CellSize = MinimumSpacing;
		RebuildSpacingGrid();
	}
	const int32 Index = Placements.Add({SpacingPoint(SurfaceLocation), MinimumSpacing, Resource, Resource != nullptr});
	SpacingGrid.FindOrAdd(CellAt(Placements[Index].Location)).Add(Index);
}
