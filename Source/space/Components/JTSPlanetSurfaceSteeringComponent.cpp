#include "space/Components/JTSPlanetSurfaceSteeringComponent.h"

#include "Engine/World.h"
#include "space/World/JTSPlanetAnchor.h"

UJTSPlanetSurfaceSteeringComponent::UJTSPlanetSurfaceSteeringComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UJTSPlanetSurfaceSteeringComponent::IsWalkable(const AJTSPlanetAnchor* Planet, const FJTSPlanetSurfaceHit& Hit) const
{
	return IsValid(Planet) && Hit.bBlockingHit && FVector::DotProduct(Hit.ImpactNormal.GetSafeNormal(),
		Planet->GetRadialUpVector(Hit.ImpactPoint)) >= FMath::Cos(FMath::DegreesToRadians(MaxSlopeDegrees));
}

bool UJTSPlanetSurfaceSteeringComponent::Probe(AJTSPlanetAnchor* Planet, const FVector& Ground,
	const FVector& Heading, float Distance, FJTSPlanetSurfaceHit& OutHit) const
{
	// Sample the entire short segment: a distant valid point must not hop over an invalid wall or cliff.
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(Distance / 25.0f));
	FVector Previous = Ground;
	for (int32 I = 1; I <= Steps; ++I)
	{
		if (!Planet->ProjectPointToSurface(Ground + Heading * (Distance * I / Steps), OutHit)
			|| !IsWalkable(Planet, OutHit)) return false;
		const FVector Up = Planet->GetRadialUpVector(Previous);
		if (FMath::Abs(FVector::DotProduct(OutHit.ImpactPoint - Previous, Up))
			> Distance / Steps * FMath::Tan(FMath::DegreesToRadians(MaxSlopeDegrees)) + 3.0f) return false;
		Previous = OutHit.ImpactPoint;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PlanetCreatureTraversal), true, GetOwner());
	Params.AddIgnoredActor(Planet->GetGameplaySurfaceActor());
	FHitResult Obstacle;
	const FVector Up = Planet->GetRadialUpVector(Ground);
	return !GetWorld()->SweepSingleByObjectType(Obstacle, Ground + Up * (BodyRadius + 3),
		OutHit.ImpactPoint + Planet->GetRadialUpVector(OutHit.ImpactPoint) * (BodyRadius + 3),
		FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(BodyRadius), Params);
}

bool UJTSPlanetSurfaceSteeringComponent::Advance(AJTSPlanetAnchor* Planet, const FVector& Ground,
	const FVector& DesiredDirection, float Speed, float DeltaSeconds, FVector& OutGround, FVector& OutHeading)
{
	return AdvanceWithState(Planet, Ground, DesiredDirection, Speed, DeltaSeconds, StandaloneState, OutGround, OutHeading);
}

bool UJTSPlanetSurfaceSteeringComponent::ConstrainDisplacement(AJTSPlanetAnchor* Planet, const FVector& Ground,
	const FVector& Displacement, FVector& OutGround, FVector& OutHeading) const
{
	OutGround = Ground;
	OutHeading = FVector::ZeroVector;
	if (!IsValid(Planet) || !GetWorld()) return false;
	const FVector Tangent = FVector::VectorPlaneProject(Displacement, Planet->GetRadialUpVector(Ground));
	if (Tangent.IsNearlyZero()) return false;
	FJTSPlanetSurfaceHit Hit;
	if (!Probe(Planet, Ground, Tangent.GetSafeNormal(), Tangent.Size(), Hit)) return false;
	OutGround = Hit.ImpactPoint;
	OutHeading = Tangent.GetSafeNormal();
	return true;
}

bool UJTSPlanetSurfaceSteeringComponent::AdvanceWithState(AJTSPlanetAnchor* Planet, const FVector& Ground,
	const FVector& DesiredDirection, float Speed, float DeltaSeconds, FJTSPlanetSurfaceSteeringState& State,
	FVector& OutGround, FVector& OutHeading) const
{
	OutGround = Ground;
	if (!IsValid(Planet) || !GetWorld() || DeltaSeconds <= 0 || Speed <= 0) return false;
	const FVector Up = Planet->GetRadialUpVector(Ground);
	FVector Desired = FVector::VectorPlaneProject(DesiredDirection, Up).GetSafeNormal();
	if (Desired.IsNearlyZero()) return false;
	State.AvoidanceRemaining = FMath::Max(0.0f, State.AvoidanceRemaining - DeltaSeconds);
	FVector Heading = State.AvoidanceRemaining > 0
		? FVector::VectorPlaneProject(State.AvoidanceHeading, Up).GetSafeNormal() : Desired;
	if (State.AvoidanceRemaining <= 0 && !State.PreviousHeading.IsNearlyZero())
	{
		Heading = FMath::Lerp(FVector::VectorPlaneProject(State.PreviousHeading, Up).GetSafeNormal(), Desired,
			FMath::Clamp(DeltaSeconds * HeadingResponse, 0.0f, 1.0f)).GetSafeNormal();
		if (Heading.IsNearlyZero()) Heading = Desired;
	}
	const float Travel = Speed * DeltaSeconds;
	const float LookAhead = FMath::Max(ProbeDistance, Travel);
	FJTSPlanetSurfaceHit Hit;
	if (!Probe(Planet, Ground, Heading, LookAhead, Hit))
	{
		if (State.PreferredSide == 0) State.PreferredSide = FMath::RandBool() ? 1.0f : -1.0f;
		const float Angles[] = {45, 75, 100, 135, 170};
		bool bFound = false;
		// Hold the selected side, including at corners, rather than alternating left/right each frame.
		for (int32 Side = 0; Side < 2 && !bFound; ++Side)
		{
			const float Sign = Side == 0 ? State.PreferredSide : -State.PreferredSide;
			for (const float Angle : Angles)
			{
				const FVector Candidate = FQuat(Up, FMath::DegreesToRadians(Angle * Sign)).RotateVector(Heading);
				if (!Probe(Planet, Ground, Candidate, LookAhead, Hit)) continue;
				Heading = Candidate;
				State.PreferredSide = Sign;
				State.AvoidanceHeading = Heading;
				State.AvoidanceRemaining = AvoidanceCommitSeconds;
				bFound = true;
				break;
			}
		}
		if (!bFound) return false;
	}
	if (!Probe(Planet, Ground, Heading, Travel, Hit)) return false;
	OutGround = Hit.ImpactPoint;
	OutHeading = Heading;
	State.PreviousHeading = Heading;
	return true;
}
