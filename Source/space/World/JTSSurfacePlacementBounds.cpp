#include "space/World/JTSSurfacePlacementBounds.h"

#include "Components/PrimitiveComponent.h"

bool JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(
	const UPrimitiveComponent* VisualComponent,
	const FVector& ActorRootLocation,
	const FVector& SurfaceUp,
	FJTSSurfaceVisualProjectionBounds& InOutBounds)
{
	if (!IsValid(VisualComponent))
	{
		return false;
	}

	const FVector SafeSurfaceUp = SurfaceUp.GetSafeNormal();
	if (SafeSurfaceUp.IsNearlyZero())
	{
		return false;
	}

	// CalcBounds(identity) gives the component's local primitive bounds. Transforming its eight
	// corners through the final ComponentToWorld preserves the visual's relative transform, scale,
	// and the actor's already surface-aligned rotation. Using Bounds.BoxExtent directly would first
	// turn this into a world AABB and incorrectly let tangent length increase vertical support.
	const FBoxSphereBounds LocalBounds = VisualComponent->CalcBounds(FTransform::Identity);
	const float BoundsScale = FMath::Max(FMath::Abs(VisualComponent->BoundsScale), KINDA_SMALL_NUMBER);
	const FVector PhysicalLocalExtent = LocalBounds.BoxExtent.GetAbs() / BoundsScale;
	if (PhysicalLocalExtent.IsNearlyZero())
	{
		return false;
	}

	const FTransform ComponentTransform = VisualComponent->GetComponentTransform();
	bool bAddedProjection = false;
	for (int32 XSign = -1; XSign <= 1; XSign += 2)
	{
		for (int32 YSign = -1; YSign <= 1; YSign += 2)
		{
			for (int32 ZSign = -1; ZSign <= 1; ZSign += 2)
			{
				const FVector LocalCorner = LocalBounds.Origin + FVector(
					PhysicalLocalExtent.X * static_cast<float>(XSign),
					PhysicalLocalExtent.Y * static_cast<float>(YSign),
					PhysicalLocalExtent.Z * static_cast<float>(ZSign));
				const FVector WorldCorner = ComponentTransform.TransformPosition(LocalCorner);
				const float ProjectionFromRoot = FVector::DotProduct(WorldCorner - ActorRootLocation, SafeSurfaceUp);
				if (!FMath::IsFinite(ProjectionFromRoot))
				{
					continue;
				}

				if (!InOutBounds.bIsValid || ProjectionFromRoot < InOutBounds.LowestProjectionFromRoot)
				{
					InOutBounds.LowestProjectionFromRoot = ProjectionFromRoot;
					InOutBounds.LowestPoint = WorldCorner;
				}
				if (!InOutBounds.bIsValid || ProjectionFromRoot > InOutBounds.HighestProjectionFromRoot)
				{
					InOutBounds.HighestProjectionFromRoot = ProjectionFromRoot;
					InOutBounds.HighestPoint = WorldCorner;
				}
				InOutBounds.bIsValid = true;
				bAddedProjection = true;
			}
		}
	}

	return bAddedProjection;
}
