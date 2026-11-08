#pragma once

#include "CoreMinimal.h"

class AJTSMoonResourceSpawner;
class AJTSPlanetAnchor;

/** One server-generation snapshot: spherical union ownership and shared placement spacing. */
class SPACE_API FJTSMoonResourceCoverage
{
public:
	FJTSMoonResourceCoverage(const AJTSPlanetAnchor* InPlanet,
		const TArray<AJTSMoonResourceSpawner*>& Spawners, int32 TieSeed);

	AJTSMoonResourceSpawner* ResolveOwner(const FVector& SurfaceLocation) const;
	float GetOwnedAreaSquareMeters(const AJTSMoonResourceSpawner* Spawner) const;
	bool CanPlace(const FVector& SurfaceLocation, float MinimumSpacing) const;
	void RecordPlacement(const FVector& SurfaceLocation, float MinimumSpacing, AActor* Resource = nullptr);
	void RemoveInvalidPlacements();

private:
	struct FRegion
	{
		TWeakObjectPtr<AJTSMoonResourceSpawner> Spawner;
		FVector CenterDirection;
		double Angle = 0;
		double CosAngle = 1;
		uint32 TieRank = 0;
	};
	struct FPlacement
	{
		FVector Location;
		float MinimumSpacing = 0;
		TWeakObjectPtr<AActor> Resource;
		bool bTracked = false;
	};
	FVector SpacingPoint(const FVector& Location) const;
	FIntVector CellAt(const FVector& Point) const;
	void RebuildSpacingGrid();

	const AJTSPlanetAnchor* Planet = nullptr;
	TArray<FRegion> Regions;
	TArray<FPlacement> Placements;
	TMap<FIntVector, TArray<int32>> SpacingGrid;
	double CellSize = 200;
};
