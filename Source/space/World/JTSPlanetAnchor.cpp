// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSPlanetAnchor.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"

AJTSPlanetAnchor::AJTSPlanetAnchor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

FName AJTSPlanetAnchor::GetPlanetId() const
{
	return PlanetId;
}

float AJTSPlanetAnchor::GetPlanetRadius() const
{
	return FMath::Max(1.0f, PlanetRadius);
}

FVector AJTSPlanetAnchor::GetExteriorCenter() const
{
	return ExteriorCenter;
}

float AJTSPlanetAnchor::GetExteriorAltitude(const FVector& WorldPosition) const
{
	return FVector::Distance(WorldPosition, GetExteriorCenter()) - GetPlanetRadius();
}

FVector AJTSPlanetAnchor::GetDirectionToPlanet(const FVector& WorldPosition) const
{
	const FVector Direction = GetExteriorCenter() - WorldPosition;
	return Direction.IsNearlyZero() ? -GetSurfaceUpVector() : Direction.GetSafeNormal();
}

AActor* AJTSPlanetAnchor::GetExteriorVisualActor() const
{
	return ExteriorVisualActor.Get();
}

FTransform AJTSPlanetAnchor::GetSurfaceFrameTransform() const
{
	return IsValid(SurfaceAnchorActor) ? SurfaceAnchorActor->GetActorTransform() : SurfaceAnchorTransform;
}

FTransform AJTSPlanetAnchor::GetLandingTransform() const
{
	return IsValid(LandingAnchorActor) ? LandingAnchorActor->GetActorTransform() : LandingAnchorTransform;
}

FTransform AJTSPlanetAnchor::GetApproachEntryTransform() const
{
	if (IsValid(ApproachEntryAnchorActor))
	{
		return ApproachEntryAnchorActor->GetActorTransform();
	}
	if (bUseApproachEntryTransform)
	{
		return ApproachEntryTransform;
	}

	const FVector SurfaceUp = GetSurfaceUpVector().GetSafeNormal();
	const FVector EntryLocation = GetExteriorCenter() + SurfaceUp * (GetPlanetRadius() + FMath::Max(0.0f, DefaultApproachEntryDistance));
	const FVector ForwardToPlanet = GetDirectionToPlanet(EntryLocation);
	return FTransform(FRotationMatrix::MakeFromX(ForwardToPlanet).ToQuat(), EntryLocation);
}

FVector AJTSPlanetAnchor::SurfaceLocalToWorld(const FVector& SurfaceLocalPosition) const
{
	return GetSurfaceFrameTransform().TransformPosition(SurfaceLocalPosition);
}

FVector AJTSPlanetAnchor::WorldToSurfaceLocal(const FVector& WorldPosition) const
{
	return GetSurfaceFrameTransform().InverseTransformPosition(WorldPosition);
}

FVector AJTSPlanetAnchor::GetSurfaceUpVector() const
{
	return GetSurfaceFrameTransform().GetUnitAxis(EAxis::Z);
}

float AJTSPlanetAnchor::GetSurfaceAltitude(const FVector& WorldPosition) const
{
	return WorldToSurfaceLocal(WorldPosition).Z;
}

FVector AJTSPlanetAnchor::GetRecommendedExteriorCenter() const
{
	return SurfaceLocalToWorld(FVector(0.0f, 0.0f, -GetPlanetRadius()));
}

void AJTSPlanetAnchor::SetExteriorCenterToRecommended()
{
	ExteriorCenter = GetRecommendedExteriorCenter();
}

bool AJTSPlanetAnchor::HasSurfaceLevel() const
{
	return !SurfaceLevel.IsNull();
}

const TSoftObjectPtr<UWorld>& AJTSPlanetAnchor::GetSurfaceLevel() const
{
	return SurfaceLevel;
}

float AJTSPlanetAnchor::GetTakeoffTransitionAltitude() const
{
	return FMath::Max(0.0f, TakeoffTransitionAltitude);
}

float AJTSPlanetAnchor::GetSpaceFlightAltitude() const
{
	return FMath::Max(0.0f, SpaceFlightAltitude);
}

float AJTSPlanetAnchor::GetSurfaceUnloadAltitude() const
{
	return FMath::Max(0.0f, SurfaceUnloadAltitude);
}

float AJTSPlanetAnchor::GetSurfaceLoadAltitude() const
{
	return FMath::Max(0.0f, SurfaceLoadAltitude);
}

float AJTSPlanetAnchor::GetApproachTransitionAltitude() const
{
	return FMath::Max(GetSurfaceLoadAltitude(), ApproachTransitionAltitude);
}

float AJTSPlanetAnchor::GetLandingApproachRange() const
{
	return FMath::Max(0.0f, LandingApproachRange);
}

float AJTSPlanetAnchor::GetLandingAssistAltitude() const
{
	return FMath::Max(0.0f, LandingAssistAltitude);
}

bool AJTSPlanetAnchor::IsActivePlanet() const
{
	return bIsActivePlanet;
}

void AJTSPlanetAnchor::SetActivePlanet(bool bInIsActivePlanet)
{
	bIsActivePlanet = bInIsActivePlanet;
}
