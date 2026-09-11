// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSPlanetLandingSite.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetLandingManager.h"
#include "space/World/JTSSpaceWorldManager.h"

AJTSPlanetLandingSite::AJTSPlanetLandingSite()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	LandingVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("LandingVolume"));
	LandingVolume->SetupAttachment(SceneRoot);
	LandingVolume->InitBoxExtent(FVector(500.0f, 500.0f, 300.0f));
	LandingVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LandingVolume->SetGenerateOverlapEvents(false);
	LandingVolume->SetCanEverAffectNavigation(false);

	LandingTarget = CreateDefaultSubobject<USceneComponent>(TEXT("LandingTarget"));
	LandingTarget->SetupAttachment(SceneRoot);
}

void AJTSPlanetLandingSite::BeginPlay()
{
	Super::BeginPlay();

	if (AJTSPlanetLandingManager* const LandingManager = AJTSPlanetLandingManager::FindPlanetLandingManager(this))
	{
		LandingManager->RegisterLandingSite(this);
	}

	if (bDebugDrawLandingSite)
	{
		DrawDebugLandingSite(-1.0f);
	}
}

void AJTSPlanetLandingSite::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AJTSPlanetLandingManager* const LandingManager = AJTSPlanetLandingManager::FindPlanetLandingManager(this))
	{
		LandingManager->UnregisterLandingSite(this);
	}

	Super::EndPlay(EndPlayReason);
}

AJTSPlanetAnchor* AJTSPlanetLandingSite::GetPlanetAnchor() const
{
	if (IsValid(PlanetAnchor))
	{
		return PlanetAnchor.Get();
	}

	if (PlanetId.IsNone())
	{
		return nullptr;
	}

	if (AJTSSpaceWorldManager* const SpaceWorldManager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
	{
		return SpaceWorldManager->FindPlanetById(PlanetId);
	}

	return nullptr;
}

FName AJTSPlanetLandingSite::GetPlanetId() const
{
	if (const AJTSPlanetAnchor* const Planet = PlanetAnchor.Get(); IsValid(Planet))
	{
		return Planet->GetPlanetId();
	}

	return PlanetId;
}

bool AJTSPlanetLandingSite::IsLandingEnabled() const
{
	return bEnabled;
}

bool AJTSPlanetLandingSite::IsLocationInsideLandingArea(const FVector& Location) const
{
	if (!bEnabled)
	{
		return false;
	}

	TArray<UShapeComponent*> Volumes;
	GetLandingVolumes(Volumes);
	for (const UShapeComponent* const Volume : Volumes)
	{
		if (IsPointInsideVolume(Volume, Location))
		{
			return true;
		}
	}

	return false;
}

bool AJTSPlanetLandingSite::FindClosestPointInLandingArea(const FVector& Location, FVector& OutLocation) const
{
	if (!bEnabled)
	{
		return false;
	}

	TArray<UShapeComponent*> Volumes;
	GetLandingVolumes(Volumes);
	bool bFoundPoint = false;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	for (const UShapeComponent* const Volume : Volumes)
	{
		FVector Candidate;
		if (!FindClosestPointInVolume(Volume, Location, Candidate))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(Location, Candidate);
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			OutLocation = Candidate;
			bFoundPoint = true;
		}
	}

	return bFoundPoint;
}

bool AJTSPlanetLandingSite::FindRandomValidRespawnPoint(
	const FVector& NearLocation,
	float SearchRadius,
	FVector& OutLocation,
	int32 MaxAttempts) const
{
	if (!bEnabled)
	{
		return false;
	}

	TArray<UShapeComponent*> Volumes;
	GetLandingVolumes(Volumes);
	if (Volumes.IsEmpty())
	{
		return false;
	}

	const float SafeSearchRadius = FMath::Max(0.0f, SearchRadius);
	const float SearchRadiusSquared = FMath::Square(SafeSearchRadius);
	const int32 SafeAttemptCount = FMath::Max(1, MaxAttempts);
	for (int32 AttemptIndex = 0; AttemptIndex < SafeAttemptCount; ++AttemptIndex)
	{
		UShapeComponent* const Volume = Volumes[FMath::RandRange(0, Volumes.Num() - 1)];
		const FVector Candidate = MakeRandomCandidateInVolumeBounds(Volume);
		if (!IsPointInsideVolume(Volume, Candidate)
			|| (SafeSearchRadius > 0.0f && FVector::DistSquared(Candidate, NearLocation) > SearchRadiusSquared))
		{
			continue;
		}

		OutLocation = Candidate;
		return true;
	}

	FVector NearestLocation;
	if (!FindClosestPointInLandingArea(NearLocation, NearestLocation))
	{
		return false;
	}

	if (SafeSearchRadius > 0.0f && FVector::DistSquared(NearestLocation, NearLocation) > SearchRadiusSquared)
	{
		return false;
	}

	OutLocation = NearestLocation;
	return true;
}

FTransform AJTSPlanetLandingSite::GetLandingTargetTransform() const
{
	return IsValid(LandingTarget) ? LandingTarget->GetComponentTransform() : GetActorTransform();
}

FJTSPlanetLandingValidationData AJTSPlanetLandingSite::GetLandingValidationData() const
{
	return LandingValidationData;
}

void AJTSPlanetLandingSite::DrawDebugLandingSite(float Duration) const
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FColor Color = bEnabled ? FColor::Green : FColor::Red;
	TArray<UShapeComponent*> Volumes;
	GetLandingVolumes(Volumes);
	for (const UShapeComponent* const Volume : Volumes)
	{
		if (!IsValid(Volume))
		{
			continue;
		}

		if (const UBoxComponent* const Box = Cast<UBoxComponent>(Volume))
		{
			DrawDebugBox(World, Box->GetComponentLocation(), Box->GetScaledBoxExtent(), Box->GetComponentQuat(), Color, false, Duration, 0, 2.0f);
		}
		else if (const USphereComponent* const Sphere = Cast<USphereComponent>(Volume))
		{
			DrawDebugSphere(World, Sphere->GetComponentLocation(), Sphere->GetScaledSphereRadius(), 20, Color, false, Duration, 0, 2.0f);
		}
		else if (const UCapsuleComponent* const Capsule = Cast<UCapsuleComponent>(Volume))
		{
			DrawDebugCapsule(
				World,
				Capsule->GetComponentLocation(),
				Capsule->GetScaledCapsuleHalfHeight(),
				Capsule->GetScaledCapsuleRadius(),
				Capsule->GetComponentQuat(),
				Color,
				false,
				Duration,
				0,
				2.0f);
		}
		else
		{
			const FBoxSphereBounds& Bounds = Volume->Bounds;
			DrawDebugBox(World, Bounds.Origin, Bounds.BoxExtent, Color, false, Duration, 0, 1.0f);
		}
	}

	const FTransform TargetTransform = GetLandingTargetTransform();
	DrawDebugCoordinateSystem(World, TargetTransform.GetLocation(), TargetTransform.Rotator(), 180.0f, false, Duration, 0, 2.0f);
}

void AJTSPlanetLandingSite::GetLandingVolumes(TArray<UShapeComponent*>& OutVolumes) const
{
	OutVolumes.Reset();
	GetComponents<UShapeComponent>(OutVolumes);
}

bool AJTSPlanetLandingSite::IsPointInsideVolume(const UShapeComponent* Volume, const FVector& Location) const
{
	if (!IsValid(Volume))
	{
		return false;
	}

	if (const UBoxComponent* const Box = Cast<UBoxComponent>(Volume))
	{
		const FVector LocalLocation = Box->GetComponentTransform().InverseTransformPosition(Location);
		const FVector Extent = Box->GetUnscaledBoxExtent();
		return FMath::Abs(LocalLocation.X) <= Extent.X
			&& FMath::Abs(LocalLocation.Y) <= Extent.Y
			&& FMath::Abs(LocalLocation.Z) <= Extent.Z;
	}

	if (const USphereComponent* const Sphere = Cast<USphereComponent>(Volume))
	{
		return FVector::DistSquared(Location, Sphere->GetComponentLocation())
			<= FMath::Square(Sphere->GetScaledSphereRadius());
	}

	if (const UCapsuleComponent* const Capsule = Cast<UCapsuleComponent>(Volume))
	{
		const FVector LocalLocation = Capsule->GetComponentTransform().InverseTransformPosition(Location);
		const float Radius = Capsule->GetUnscaledCapsuleRadius();
		const float SegmentHalfHeight = FMath::Max(0.0f, Capsule->GetUnscaledCapsuleHalfHeight() - Radius);
		const FVector ClosestSegmentPoint(0.0f, 0.0f, FMath::Clamp(LocalLocation.Z, -SegmentHalfHeight, SegmentHalfHeight));
		return FVector::DistSquared(LocalLocation, ClosestSegmentPoint) <= FMath::Square(Radius);
	}

	return Volume->Bounds.GetBox().IsInsideOrOn(Location);
}

bool AJTSPlanetLandingSite::FindClosestPointInVolume(
	const UShapeComponent* Volume,
	const FVector& Location,
	FVector& OutLocation) const
{
	if (!IsValid(Volume))
	{
		return false;
	}

	if (const UBoxComponent* const Box = Cast<UBoxComponent>(Volume))
	{
		const FVector Extent = Box->GetUnscaledBoxExtent();
		const FVector LocalLocation = Box->GetComponentTransform().InverseTransformPosition(Location);
		const FVector ClampedLocalLocation(
			FMath::Clamp(LocalLocation.X, -Extent.X, Extent.X),
			FMath::Clamp(LocalLocation.Y, -Extent.Y, Extent.Y),
			FMath::Clamp(LocalLocation.Z, -Extent.Z, Extent.Z));
		OutLocation = Box->GetComponentTransform().TransformPosition(ClampedLocalLocation);
		return true;
	}

	if (const USphereComponent* const Sphere = Cast<USphereComponent>(Volume))
	{
		const FVector Center = Sphere->GetComponentLocation();
		const float Radius = Sphere->GetScaledSphereRadius();
		const FVector Offset = Location - Center;
		OutLocation = Offset.SizeSquared() <= FMath::Square(Radius)
			? Location
			: Center + Offset.GetSafeNormal() * Radius;
		return true;
	}

	if (const UCapsuleComponent* const Capsule = Cast<UCapsuleComponent>(Volume))
	{
		const FTransform CapsuleTransform = Capsule->GetComponentTransform();
		const FVector LocalLocation = CapsuleTransform.InverseTransformPosition(Location);
		const float Radius = Capsule->GetUnscaledCapsuleRadius();
		const float SegmentHalfHeight = FMath::Max(0.0f, Capsule->GetUnscaledCapsuleHalfHeight() - Radius);
		const FVector ClosestSegmentPoint(0.0f, 0.0f, FMath::Clamp(LocalLocation.Z, -SegmentHalfHeight, SegmentHalfHeight));
		const FVector SegmentOffset = LocalLocation - ClosestSegmentPoint;
		const FVector ClosestLocalLocation = SegmentOffset.SizeSquared() <= FMath::Square(Radius)
			? LocalLocation
			: ClosestSegmentPoint + SegmentOffset.GetSafeNormal() * Radius;
		OutLocation = CapsuleTransform.TransformPosition(ClosestLocalLocation);
		return true;
	}

	const FBox Bounds = Volume->Bounds.GetBox();
	OutLocation = FVector(
		FMath::Clamp(Location.X, Bounds.Min.X, Bounds.Max.X),
		FMath::Clamp(Location.Y, Bounds.Min.Y, Bounds.Max.Y),
		FMath::Clamp(Location.Z, Bounds.Min.Z, Bounds.Max.Z));
	return true;
}

FVector AJTSPlanetLandingSite::MakeRandomCandidateInVolumeBounds(const UShapeComponent* Volume) const
{
	if (!IsValid(Volume))
	{
		return GetActorLocation();
	}

	const FBox Bounds = Volume->Bounds.GetBox();
	return FVector(
		FMath::FRandRange(Bounds.Min.X, Bounds.Max.X),
		FMath::FRandRange(Bounds.Min.Y, Bounds.Max.Y),
		FMath::FRandRange(Bounds.Min.Z, Bounds.Max.Z));
}
