// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSSpacecraftGroundProbeComponent.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "Math/RotationMatrix.h"
#include "space/World/JTSPlanetAnchor.h"

namespace
{
	FVector MakeSurfaceForward(const FVector& PreferredForward, const FVector& SurfaceUp)
	{
		FVector SurfaceForward = FVector::VectorPlaneProject(PreferredForward, SurfaceUp).GetSafeNormal();
		if (!SurfaceForward.IsNearlyZero())
		{
			return SurfaceForward;
		}

		FVector FallbackRight;
		SurfaceUp.FindBestAxisVectors(SurfaceForward, FallbackRight);
		return SurfaceForward.GetSafeNormal();
	}
}

UJTSSpacecraftGroundProbeComponent::UJTSSpacecraftGroundProbeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UJTSSpacecraftGroundProbeComponent::ProbeGround(
	AJTSPlanetAnchor* Planet,
	const FVector& PreferredForward,
	float MaxDistance)
{
	ClearGroundInfo();

	AActor* const Owner = GetOwner();
	if (!IsValid(Owner) || !IsValid(Planet))
	{
		return false;
	}

	const float ProbeDistance = FMath::Max(
		1.0f,
		MaxDistance > 0.0f ? MaxDistance : GroundProbeDistance);
	FJTSPlanetSurfaceHit SurfaceHit;
	if (!Planet->ProbeSurfaceAlongGravity(Owner->GetActorLocation(), ProbeDistance, SurfaceHit))
	{
		return false;
	}

	const FVector SurfaceUp = SurfaceHit.ImpactNormal.GetSafeNormal();
	if (SurfaceUp.IsNearlyZero())
	{
		return false;
	}

	const FVector SurfaceForward = MakeSurfaceForward(PreferredForward, SurfaceUp);
	if (SurfaceForward.IsNearlyZero())
	{
		return false;
	}

	CurrentGroundInfo.bHasGround = true;
	CurrentGroundInfo.Distance = SurfaceHit.Distance;
	CurrentGroundInfo.GroundLocation = SurfaceHit.ImpactPoint;
	CurrentGroundInfo.SurfaceNormal = SurfaceUp;
	CurrentGroundInfo.DockingHeight = FMath::Max(
		0.0f,
		FVector::DotProduct(Owner->GetActorLocation() - SurfaceHit.ImpactPoint, SurfaceUp));
	const FVector RadialUp = Planet->GetRadialUpVector(SurfaceHit.ImpactPoint);
	CurrentGroundInfo.SlopeDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
		FVector::DotProduct(SurfaceUp, RadialUp),
		-1.0f,
		1.0f)));
	CurrentGroundInfo.SurfaceTransform = FTransform(
		FRotationMatrix::MakeFromXZ(SurfaceForward, SurfaceUp).ToQuat(),
		SurfaceHit.ImpactPoint);

	if (bDebugDrawGroundProbe && GetWorld() != nullptr)
	{
		DrawDebugLine(
			GetWorld(),
			Owner->GetActorLocation(),
			SurfaceHit.ImpactPoint,
			FColor::Cyan,
			false,
			DebugDrawDuration,
			0,
			1.5f);
		DrawDebugDirectionalArrow(
			GetWorld(),
			SurfaceHit.ImpactPoint,
			SurfaceHit.ImpactPoint + SurfaceUp * 180.0f,
			30.0f,
			FColor::Green,
			false,
			DebugDrawDuration,
			0,
			2.0f);
	}

	return true;
}

FJTSSpacecraftGroundInfo UJTSSpacecraftGroundProbeComponent::GetGroundInfo() const
{
	return CurrentGroundInfo;
}

bool UJTSSpacecraftGroundProbeComponent::HasGround() const
{
	return CurrentGroundInfo.bHasGround;
}

void UJTSSpacecraftGroundProbeComponent::ClearGroundInfo()
{
	CurrentGroundInfo = FJTSSpacecraftGroundInfo();
}
