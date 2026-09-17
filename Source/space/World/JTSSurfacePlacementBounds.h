#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;

/**
 * Physical visual bounds projected onto one local surface-up direction.
 *
 * The projections are measured relative to the owning Actor root, so the support value remains
 * correct when a visual component has a Blueprint-authored relative offset, rotation, or scale.
 */
struct FJTSSurfaceVisualProjectionBounds
{
	bool bIsValid = false;
	float LowestProjectionFromRoot = 0.0f;
	float HighestProjectionFromRoot = 0.0f;
	FVector LowestPoint = FVector::ZeroVector;
	FVector HighestPoint = FVector::ZeroVector;

	/** Signed root offset that places the visual's lowest projected point on a surface. */
	float GetRootToLowestSupport() const
	{
		return -LowestProjectionFromRoot;
	}

	/** Visual thickness along the supplied SurfaceUp, never a three-dimensional bounds diagonal. */
	float GetSurfaceThickness() const
	{
		return bIsValid ? FMath::Max(0.0f, HighestProjectionFromRoot - LowestProjectionFromRoot) : 0.0f;
	}
};

/** Small physical clearance shared by real-planet visual placement paths. */
namespace JTSSurfacePlacementBounds
{
	constexpr float DefaultSurfaceClearance = 2.0f;

	/**
	 * Adds one primitive's uninflated physical local bounds to a SurfaceUp projection.
	 * BoundsScale is deliberately removed because it represents render culling expansion, not geometry.
	 */
	SPACE_API bool AccumulateVisualProjectionBounds(
		const UPrimitiveComponent* VisualComponent,
		const FVector& ActorRootLocation,
		const FVector& SurfaceUp,
		FJTSSurfaceVisualProjectionBounds& InOutBounds);
}
