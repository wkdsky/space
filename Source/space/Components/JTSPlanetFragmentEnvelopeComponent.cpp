#include "JTSPlanetFragmentEnvelopeComponent.h"

UJTSPlanetFragmentEnvelopeComponent::UJTSPlanetFragmentEnvelopeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

double UJTSPlanetFragmentEnvelopeComponent::GetRegionAltitude(const FVector& WorldPosition) const
{
	if (!HasGeometry() || !GetOwner()) return 0;
	const FVector P = GeometryRotation.UnrotateVector(WorldPosition - GetOwner()->GetActorLocation());
	const FVector R = Data->RegionRadii.ComponentMax(FVector(1));
	const FVector N = P / R;
	if (N.SizeSquared() <= 1) return 0;
	// Closest point on an ellipsoid: monotone Lagrange multiplier root outside.
	const FVector R2 = R * R;
	double Low = 0, High = R.GetMax() * P.Size();
	for (int32 I = 0; I < 48; ++I)
	{
		const double T = (Low + High) * 0.5;
		const FVector Q = R * P / (R2 + FVector(T));
		if (Q.SizeSquared() > 1) Low = T; else High = T;
	}
	const FVector Closest = R2 * P / (R2 + FVector(High));
	return FVector::Distance(P, Closest);
}

bool UJTSPlanetFragmentEnvelopeComponent::QuerySurface(const FVector& WorldPosition, double Padding,
	double SearchDistance, double SmoothWidth, double& OutHeight, FVector& OutNormal, FVector& OutGround) const
{
	if (!HasGeometry() || !GetOwner()) return false;
	const FQuat Rotation = GeometryRotation.Quaternion();
	const FVector P = Rotation.UnrotateVector(WorldPosition - GetOwner()->GetActorLocation());
	double Best = DBL_MAX;
	FVector Normal = FVector::ZeroVector, Ground = FVector::ZeroVector;
	const double Width = FMath::Max(1.0, SmoothWidth);
	for (const FJTSFragmentHull& Hull : Data->Fragments)
	{
		const double Limit = FMath::Min(SearchDistance + Padding + Width, Best + Padding + Width);
		if (FBox(Hull.BoundsMin, Hull.BoundsMax).ComputeSquaredDistanceToPoint(P) > FMath::Square(FMath::Max(0.0, Limit))) continue;
		double ClosestSquared = DBL_MAX, MaxPlane = -DBL_MAX;
		FVector Closest = FVector::ZeroVector, FaceNormal = FVector::ZeroVector;
		for (int32 I = 0; I + 2 < Hull.Triangles.Num(); I += 3)
		{
			const int32 IA = Hull.Triangles[I], IB = Hull.Triangles[I + 1], IC = Hull.Triangles[I + 2];
			if (!Hull.Vertices.IsValidIndex(IA) || !Hull.Vertices.IsValidIndex(IB) || !Hull.Vertices.IsValidIndex(IC)) continue;
			const FVector A = Hull.Vertices[IA], B = Hull.Vertices[IB], C = Hull.Vertices[IC];
			const FVector FN = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
			if (FN.IsNearlyZero()) continue;
			MaxPlane = FMath::Max(MaxPlane, FVector::DotProduct(P - A, FN));
			const FVector Q = FMath::ClosestPointOnTriangleToPoint(P, A, B, C);
			const double D2 = FVector::DistSquared(P, Q);
			if (D2 < ClosestSquared) { ClosestSquared = D2; Closest = Q; FaceNormal = FN; }
		}
		if (ClosestSquared == DBL_MAX) continue;
		const bool bInside = MaxPlane <= 0;
		const double Distance = FMath::Sqrt(ClosestSquared);
		const double Height = (bInside ? -Distance : Distance) - Padding;
		FVector N = bInside ? (Closest - P).GetSafeNormal() : (P - Closest).GetSafeNormal();
		if (N.IsNearlyZero()) N = FaceNormal;
		if (Best == DBL_MAX) { Best = Height; Normal = N; Ground = Closest; continue; }
		// Conservative smooth union: never cuts into either fragment's protective wrap.
		const double Blend = FMath::Clamp(0.5 + 0.5 * (Best - Height) / Width, 0.0, 1.0);
		Normal = FMath::Lerp(Normal, N, Blend).GetSafeNormal();
		Ground = FMath::Lerp(Ground, Closest, Blend);
		Best = FMath::Lerp(Best, Height, Blend) - Width * Blend * (1 - Blend);
	}
	if (Best == DBL_MAX || Best > SearchDistance) return false;
	OutHeight = Best;
	OutNormal = Rotation.RotateVector(Normal);
	OutGround = GetOwner()->GetActorLocation() + Rotation.RotateVector(Ground);
	return !OutNormal.IsNearlyZero();
}
