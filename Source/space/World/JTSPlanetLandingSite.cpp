// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSPlanetLandingSite.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetLandingManager.h"
#include "space/World/JTSSpaceWorldManager.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

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

	RuntimeLandingMarker = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RuntimeLandingMarker"));
	RuntimeLandingMarker->SetupAttachment(SceneRoot);
	RuntimeLandingMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RuntimeLandingMarker->SetGenerateOverlapEvents(false);
	RuntimeLandingMarker->SetCastShadow(false);
	RuntimeLandingMarker->SetCanEverAffectNavigation(false);
	RuntimeLandingMarker->bUseAsyncCooking = false;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MarkerMaterial(
		TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (MarkerMaterial.Succeeded())
	{
		RuntimeLandingMarker->SetMaterial(0, MarkerMaterial.Object);
	}

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

	BuildRuntimeLandingMarker();
}

void AJTSPlanetLandingSite::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AJTSPlanetLandingManager* const LandingManager = AJTSPlanetLandingManager::FindPlanetLandingManager(this))
	{
		LandingManager->UnregisterLandingSite(this);
	}

	Super::EndPlay(EndPlayReason);
}

bool AJTSPlanetLandingSite::ProjectMarkerPointToSurface(
	const FVector& SourcePoint,
	FVector& OutPoint,
	FVector& OutNormal) const
{
	AJTSPlanetAnchor* const Planet = GetPlanetAnchor();
	if (!IsValid(Planet))
	{
		return false;
	}

	FVector RadialUp = Planet->GetRadialUpVector(SourcePoint).GetSafeNormal();
	if (RadialUp.IsNearlyZero())
	{
		return false;
	}

	FJTSPlanetSurfaceHit SurfaceHit;
	if (!Planet->ProbeSurfaceAlongGravity(
		SourcePoint + RadialUp * FMath::Max(100.0f, RuntimeMarkerSurfaceProbeLift),
		FMath::Max(RuntimeMarkerSurfaceProbeDistance, RuntimeMarkerSurfaceProbeLift + 100.0f),
		SurfaceHit))
	{
		return false;
	}

	OutNormal = SurfaceHit.ImpactNormal.GetSafeNormal();
	if (OutNormal.IsNearlyZero())
	{
		OutNormal = RadialUp;
	}
	OutPoint = SurfaceHit.ImpactPoint + OutNormal * FMath::Max(0.0f, RuntimeMarkerSurfaceOffset);
	return true;
}

void AJTSPlanetLandingSite::AppendMarkerSegment(
	const FVector& Start,
	const FVector& End,
	const FVector& StartNormal,
	const FVector& EndNormal,
	TArray<FVector>& OutVertices,
	TArray<int32>& OutTriangles,
	TArray<FVector>& OutNormals,
	TArray<FVector2D>& OutUVs,
	TArray<FLinearColor>& OutColors,
	TArray<FProcMeshTangent>& OutTangents) const
{
	const FVector Direction = (End - Start).GetSafeNormal();
	const FVector AverageNormal = (StartNormal + EndNormal).GetSafeNormal();
	FVector Side = FVector::CrossProduct(AverageNormal, Direction).GetSafeNormal();
	if (Direction.IsNearlyZero() || Side.IsNearlyZero())
	{
		return;
	}

	Side *= FMath::Max(1.0f, RuntimeMarkerLineWidth) * 0.5f;
	const int32 BaseIndex = OutVertices.Num();
	OutVertices.Add(Start + Side);
	OutVertices.Add(Start - Side);
	OutVertices.Add(End + Side);
	OutVertices.Add(End - Side);
	OutTriangles.Append({ BaseIndex, BaseIndex + 2, BaseIndex + 1, BaseIndex + 1, BaseIndex + 2, BaseIndex + 3 });
	OutNormals.Append({ StartNormal, StartNormal, EndNormal, EndNormal });
	OutUVs.Append({ FVector2D(0.0f, 0.0f), FVector2D(0.0f, 1.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 1.0f) });
	OutColors.Append({ RuntimeMarkerColor, RuntimeMarkerColor, RuntimeMarkerColor, RuntimeMarkerColor });
	const FProcMeshTangent Tangent(Direction, false);
	OutTangents.Append({ Tangent, Tangent, Tangent, Tangent });
}

void AJTSPlanetLandingSite::BuildRuntimeLandingMarker()
{
	if (!IsValid(RuntimeLandingMarker))
	{
		return;
	}

	RuntimeLandingMarker->ClearAllMeshSections();
	if (!bShowRuntimeLandingMarker || !bEnabled)
	{
		return;
	}

	AJTSPlanetAnchor* const Planet = GetPlanetAnchor();
	if (!IsValid(Planet))
	{
		if (++RuntimeMarkerBuildAttempts <= 5)
		{
			GetWorld()->GetTimerManager().SetTimerForNextTick(this, &AJTSPlanetLandingSite::BuildRuntimeLandingMarker);
		}
		return;
	}

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	const auto AppendProjectedLoop = [this, &Vertices, &Triangles, &Normals, &UVs, &Colors, &Tangents](const TArray<FVector>& SourcePoints)
	{
		TArray<FVector> SurfacePoints;
		TArray<FVector> SurfaceNormals;
		for (const FVector& SourcePoint : SourcePoints)
		{
			FVector SurfacePoint;
			FVector SurfaceNormal;
			if (ProjectMarkerPointToSurface(SourcePoint, SurfacePoint, SurfaceNormal))
			{
				SurfacePoints.Add(SurfacePoint);
				SurfaceNormals.Add(SurfaceNormal);
			}
		}

		if (SurfacePoints.Num() < 2)
		{
			return;
		}
		for (int32 PointIndex = 0; PointIndex < SurfacePoints.Num(); ++PointIndex)
		{
			const int32 NextIndex = (PointIndex + 1) % SurfacePoints.Num();
			AppendMarkerSegment(
				SurfacePoints[PointIndex],
				SurfacePoints[NextIndex],
				SurfaceNormals[PointIndex],
				SurfaceNormals[NextIndex],
				Vertices,
				Triangles,
				Normals,
				UVs,
				Colors,
				Tangents);
		}
	};

	TArray<UShapeComponent*> Volumes;
	GetLandingVolumes(Volumes);
	for (const UShapeComponent* const Volume : Volumes)
	{
		if (!IsValid(Volume))
		{
			continue;
		}

		TArray<FVector> SourcePoints;
		const FTransform VolumeTransform = Volume->GetComponentTransform();
		if (const UBoxComponent* const Box = Cast<UBoxComponent>(Volume))
		{
			const FVector Extent = Box->GetUnscaledBoxExtent();
			SourcePoints = {
				VolumeTransform.TransformPosition(FVector(-Extent.X, -Extent.Y, 0.0f)),
				VolumeTransform.TransformPosition(FVector(Extent.X, -Extent.Y, 0.0f)),
				VolumeTransform.TransformPosition(FVector(Extent.X, Extent.Y, 0.0f)),
				VolumeTransform.TransformPosition(FVector(-Extent.X, Extent.Y, 0.0f)) };
		}
		else if (const USphereComponent* const Sphere = Cast<USphereComponent>(Volume))
		{
			constexpr int32 CircleSegments = 32;
			const float Radius = Sphere->GetUnscaledSphereRadius();
			for (int32 SegmentIndex = 0; SegmentIndex < CircleSegments; ++SegmentIndex)
			{
				const float Angle = 2.0f * PI * static_cast<float>(SegmentIndex) / static_cast<float>(CircleSegments);
				SourcePoints.Add(VolumeTransform.TransformPosition(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f)));
			}
		}
		else if (const UCapsuleComponent* const Capsule = Cast<UCapsuleComponent>(Volume))
		{
			constexpr int32 CircleSegments = 32;
			const float Radius = Capsule->GetUnscaledCapsuleRadius();
			for (int32 SegmentIndex = 0; SegmentIndex < CircleSegments; ++SegmentIndex)
			{
				const float Angle = 2.0f * PI * static_cast<float>(SegmentIndex) / static_cast<float>(CircleSegments);
				SourcePoints.Add(VolumeTransform.TransformPosition(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f)));
			}
		}
		else
		{
			const FBox Bounds = Volume->Bounds.GetBox();
			const float BoundsCenterZ = Bounds.GetCenter().Z;
			SourcePoints = {
				FVector(Bounds.Min.X, Bounds.Min.Y, BoundsCenterZ),
				FVector(Bounds.Max.X, Bounds.Min.Y, BoundsCenterZ),
				FVector(Bounds.Max.X, Bounds.Max.Y, BoundsCenterZ),
				FVector(Bounds.Min.X, Bounds.Max.Y, BoundsCenterZ) };
		}
		AppendProjectedLoop(SourcePoints);
	}

	const FVector TargetSourcePoint = GetLandingTargetTransform().GetLocation();
	FVector TargetPoint;
	FVector TargetNormal;
	if (ProjectMarkerPointToSurface(TargetSourcePoint, TargetPoint, TargetNormal))
	{
		FVector TargetForward = Planet->ProjectDirectionToSurfaceTangent(
			GetLandingTargetTransform().GetUnitAxis(EAxis::X), TargetPoint).GetSafeNormal();
		if (TargetForward.IsNearlyZero())
		{
			FVector UnusedRight;
			TargetNormal.FindBestAxisVectors(TargetForward, UnusedRight);
		}
		const FVector TargetRight = FVector::CrossProduct(TargetNormal, TargetForward).GetSafeNormal();
		const float TargetHalfLength = FMath::Max(90.0f, RuntimeMarkerLineWidth * 4.0f);
		AppendMarkerSegment(TargetPoint - TargetForward * TargetHalfLength, TargetPoint + TargetForward * TargetHalfLength, TargetNormal, TargetNormal, Vertices, Triangles, Normals, UVs, Colors, Tangents);
		AppendMarkerSegment(TargetPoint - TargetRight * TargetHalfLength, TargetPoint + TargetRight * TargetHalfLength, TargetNormal, TargetNormal, Vertices, Triangles, Normals, UVs, Colors, Tangents);
	}

	if (!Vertices.IsEmpty())
	{
		// Surface projection produces world-space points. A procedural mesh component consumes local
		// vertices, so feeding these world coordinates directly applied the landing site's transform a
		// second time and lifted the cyan outline into the sky.
		const FTransform MarkerTransform = RuntimeLandingMarker->GetComponentTransform();
		for (int32 VertexIndex = 0; VertexIndex < Vertices.Num(); ++VertexIndex)
		{
			Vertices[VertexIndex] = MarkerTransform.InverseTransformPosition(Vertices[VertexIndex]);
			Normals[VertexIndex] = MarkerTransform.InverseTransformVectorNoScale(Normals[VertexIndex]).GetSafeNormal();
			const FProcMeshTangent& WorldTangent = Tangents[VertexIndex];
			Tangents[VertexIndex] = FProcMeshTangent(
				MarkerTransform.InverseTransformVectorNoScale(WorldTangent.TangentX).GetSafeNormal(),
				WorldTangent.bFlipTangentY);
		}
		RuntimeLandingMarker->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
	}
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

bool AJTSPlanetLandingSite::GetSurfaceFootprintLocalLocation(
	const UShapeComponent* Volume,
	const FVector& Location,
	FVector& OutLocalLocation) const
{
	if (!IsValid(Volume))
	{
		return false;
	}

	const FTransform VolumeTransform = Volume->GetComponentTransform();
	FVector FootprintLocation = Location;
	// Runtime markers are authored from each shape's local XY plane and projected to the real
	// surface along local planet gravity.  Use the inverse mapping here so collision/terrain height
	// cannot silently crop the legal area to the volume's small local-Z thickness.
	if (const AJTSPlanetAnchor* const Planet = GetPlanetAnchor())
	{
		const FVector GravityDirection = Planet->GetGravityDirection(Location).GetSafeNormal();
		const FVector FootprintNormal = VolumeTransform.GetUnitAxis(EAxis::Z).GetSafeNormal();
		const float DirectionNormalDot = FVector::DotProduct(GravityDirection, FootprintNormal);
		if (!GravityDirection.IsNearlyZero()
			&& !FootprintNormal.IsNearlyZero()
			&& !FMath::IsNearlyZero(DirectionNormalDot))
		{
			const float DistanceAlongGravity = FVector::DotProduct(
				VolumeTransform.GetLocation() - Location,
				FootprintNormal) / DirectionNormalDot;
			FootprintLocation = Location + GravityDirection * DistanceAlongGravity;
		}
	}

	OutLocalLocation = VolumeTransform.InverseTransformPosition(FootprintLocation);
	return true;
}

bool AJTSPlanetLandingSite::IsPointInsideVolume(const UShapeComponent* Volume, const FVector& Location) const
{
	FVector LocalLocation;
	if (!GetSurfaceFootprintLocalLocation(Volume, Location, LocalLocation))
	{
		return false;
	}

	if (const UBoxComponent* const Box = Cast<UBoxComponent>(Volume))
	{
		const FVector Extent = Box->GetUnscaledBoxExtent();
		return FMath::Abs(LocalLocation.X) <= Extent.X
			&& FMath::Abs(LocalLocation.Y) <= Extent.Y;
	}

	if (const USphereComponent* const Sphere = Cast<USphereComponent>(Volume))
	{
		const float Radius = Sphere->GetUnscaledSphereRadius();
		return FMath::Square(LocalLocation.X) + FMath::Square(LocalLocation.Y)
			<= FMath::Square(Radius);
	}

	if (const UCapsuleComponent* const Capsule = Cast<UCapsuleComponent>(Volume))
	{
		const float Radius = Capsule->GetUnscaledCapsuleRadius();
		return FMath::Square(LocalLocation.X) + FMath::Square(LocalLocation.Y)
			<= FMath::Square(Radius);
	}

	return Volume->Bounds.GetBox().IsInsideOrOn(Location);
}

bool AJTSPlanetLandingSite::FindClosestPointInVolume(
	const UShapeComponent* Volume,
	const FVector& Location,
	FVector& OutLocation) const
{
	FVector LocalLocation;
	if (!GetSurfaceFootprintLocalLocation(Volume, Location, LocalLocation))
	{
		return false;
	}

	if (const UBoxComponent* const Box = Cast<UBoxComponent>(Volume))
	{
		const FVector Extent = Box->GetUnscaledBoxExtent();
		const FVector ClampedLocalLocation(
			FMath::Clamp(LocalLocation.X, -Extent.X, Extent.X),
			FMath::Clamp(LocalLocation.Y, -Extent.Y, Extent.Y),
			0.0f);
		OutLocation = Box->GetComponentTransform().TransformPosition(ClampedLocalLocation);
		return true;
	}

	if (const USphereComponent* const Sphere = Cast<USphereComponent>(Volume))
	{
		const float Radius = Sphere->GetUnscaledSphereRadius();
		const FVector2D LocalPlanarLocation(LocalLocation.X, LocalLocation.Y);
		const FVector2D ClosestPlanarLocation = LocalPlanarLocation.SizeSquared() <= FMath::Square(Radius)
			? LocalPlanarLocation
			: LocalPlanarLocation.GetSafeNormal() * Radius;
		OutLocation = Sphere->GetComponentTransform().TransformPosition(
			FVector(ClosestPlanarLocation.X, ClosestPlanarLocation.Y, 0.0f));
		return true;
	}

	if (const UCapsuleComponent* const Capsule = Cast<UCapsuleComponent>(Volume))
	{
		const float Radius = Capsule->GetUnscaledCapsuleRadius();
		const FVector2D LocalPlanarLocation(LocalLocation.X, LocalLocation.Y);
		const FVector2D ClosestPlanarLocation = LocalPlanarLocation.SizeSquared() <= FMath::Square(Radius)
			? LocalPlanarLocation
			: LocalPlanarLocation.GetSafeNormal() * Radius;
		OutLocation = Capsule->GetComponentTransform().TransformPosition(
			FVector(ClosestPlanarLocation.X, ClosestPlanarLocation.Y, 0.0f));
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

	const FTransform VolumeTransform = Volume->GetComponentTransform();
	if (const UBoxComponent* const Box = Cast<UBoxComponent>(Volume))
	{
		const FVector Extent = Box->GetUnscaledBoxExtent();
		return VolumeTransform.TransformPosition(FVector(
			FMath::FRandRange(-Extent.X, Extent.X),
			FMath::FRandRange(-Extent.Y, Extent.Y),
			0.0f));
	}

	if (const USphereComponent* const Sphere = Cast<USphereComponent>(Volume))
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Radius = FMath::Sqrt(FMath::FRand()) * Sphere->GetUnscaledSphereRadius();
		return VolumeTransform.TransformPosition(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f));
	}

	if (const UCapsuleComponent* const Capsule = Cast<UCapsuleComponent>(Volume))
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Radius = FMath::Sqrt(FMath::FRand()) * Capsule->GetUnscaledCapsuleRadius();
		return VolumeTransform.TransformPosition(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f));
	}

	const FBox Bounds = Volume->Bounds.GetBox();
	return FVector(
		FMath::FRandRange(Bounds.Min.X, Bounds.Max.X),
		FMath::FRandRange(Bounds.Min.Y, Bounds.Max.Y),
		Bounds.GetCenter().Z);
}
