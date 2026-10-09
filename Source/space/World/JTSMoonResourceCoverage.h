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
	void RecordPlacement(const FVector& SurfaceLocation, float MinimumSpacing);

private:
	struct FRegion
	{
		AJTSMoonResourceSpawner* Spawner = nullptr;
		FVector CenterDirection;
		double Angle = 0;
		double CosAngle = 1;
		uint32 TieRank = 0;
	};
	struct FPlacement
	{
		FVector Location;
		float MinimumSpacing = 0;
	};

	const AJTSPlanetAnchor* Planet = nullptr;
	TArray<FRegion> Regions;
	TArray<FPlacement> Placements;
};
