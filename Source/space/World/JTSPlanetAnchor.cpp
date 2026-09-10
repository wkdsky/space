// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSPlanetAnchor.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"
#include "space/World/JTSPlanetSurfaceAnchor.h"
#include "space/World/JTSSpaceWorldManager.h"

namespace
{
	void ResetSurfaceHit(FJTSPlanetSurfaceHit& SurfaceHit, const FVector& TraceStart, const FVector& TraceEnd)
	{
		SurfaceHit = FJTSPlanetSurfaceHit();
		SurfaceHit.TraceStart = TraceStart;
		SurfaceHit.TraceEnd = TraceEnd;
	}
}

AJTSPlanetAnchor::AJTSPlanetAnchor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AJTSPlanetAnchor::BeginPlay()
{
	Super::BeginPlay();

	if (AJTSSpaceWorldManager* const SpaceWorldManager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
	{
		SpaceWorldManager->RegisterPlanet(this);
	}
}

void AJTSPlanetAnchor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AJTSSpaceWorldManager* const SpaceWorldManager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
	{
		SpaceWorldManager->UnregisterPlanet(this);
	}

	Super::EndPlay(EndPlayReason);
}

FName AJTSPlanetAnchor::GetPlanetId() const
{
	return PlanetId;
}

FVector AJTSPlanetAnchor::GetPlanetCenter() const
{
	return IsValid(CenterActor) ? CenterActor->GetActorLocation() : GetActorLocation();
}

float AJTSPlanetAnchor::GetApproximateRadius() const
{
	return FMath::IsFinite(ApproximateRadius) ? FMath::Max(1.0f, ApproximateRadius) : 1.0f;
}

float AJTSPlanetAnchor::GetPlanetRadius() const
{
	return GetApproximateRadius();
}

AActor* AJTSPlanetAnchor::GetGameplaySurfaceActor() const
{
	if (IsValid(GameplaySurfaceActor))
	{
		return GameplaySurfaceActor.Get();
	}

	return IsValid(GameplaySurfaceComponent) ? GameplaySurfaceComponent->GetOwner() : nullptr;
}

UPrimitiveComponent* AJTSPlanetAnchor::GetGameplaySurfaceComponent() const
{
	if (IsValid(GameplaySurfaceComponent))
	{
		return GameplaySurfaceComponent.Get();
	}

	AActor* const SurfaceActor = GameplaySurfaceActor.Get();
	if (!IsValid(SurfaceActor))
	{
		return nullptr;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	SurfaceActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* const PrimitiveComponent : PrimitiveComponents)
	{
		if (IsValid(PrimitiveComponent)
			&& PrimitiveComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			return PrimitiveComponent;
		}
	}

	return nullptr;
}

bool AJTSPlanetAnchor::HasGameplaySurface() const
{
	return IsValid(GetGameplaySurfaceComponent());
}

bool AJTSPlanetAnchor::OwnsGameplaySurfaceActor(const AActor* Candidate) const
{
	const AActor* const SurfaceActor = GetGameplaySurfaceActor();
	if (!IsValid(Candidate) || !IsValid(SurfaceActor))
	{
		return false;
	}

	for (const AActor* CurrentActor = Candidate; IsValid(CurrentActor); CurrentActor = CurrentActor->GetAttachParentActor())
	{
		if (CurrentActor == SurfaceActor)
		{
			return true;
		}
	}

	return false;
}

bool AJTSPlanetAnchor::IsGravityEnabled() const
{
	return bEnableGravity && GetGravityStrength() > KINDA_SMALL_NUMBER;
}

float AJTSPlanetAnchor::GetGravityStrength() const
{
	return FMath::IsFinite(GravityStrength) ? FMath::Max(0.0f, GravityStrength) : 0.0f;
}

float AJTSPlanetAnchor::GetGravityInfluenceRange() const
{
	return FMath::IsFinite(GravityInfluenceRange) ? FMath::Max(0.0f, GravityInfluenceRange) : 0.0f;
}

float AJTSPlanetAnchor::GetSpaceExitRange() const
{
	return FMath::IsFinite(SpaceExitRange) ? FMath::Max(0.0f, SpaceExitRange) : 0.0f;
}

bool AJTSPlanetAnchor::IsWithinSpaceExitRange(const FVector& WorldPosition) const
{
	return GetApproximateAltitude(WorldPosition) <= GetSpaceExitRange();
}

bool AJTSPlanetAnchor::IsWithinGravityInfluence(const FVector& WorldPosition) const
{
	return IsGravityEnabled() && GetApproximateAltitude(WorldPosition) <= GetGravityInfluenceRange();
}

FVector AJTSPlanetAnchor::GetRadialUpVector(const FVector& WorldPosition) const
{
	const FVector RadialOffset = WorldPosition - GetPlanetCenter();
	if (!RadialOffset.IsNearlyZero())
	{
		return RadialOffset.GetSafeNormal();
	}

	const FVector ActorUp = GetActorUpVector().GetSafeNormal();
	return ActorUp.IsNearlyZero() ? FVector::UpVector : ActorUp;
}

FVector AJTSPlanetAnchor::GetGravityDirection(const FVector& WorldPosition) const
{
	return (GetPlanetCenter() - WorldPosition).GetSafeNormal();
}

float AJTSPlanetAnchor::GetApproximateAltitude(const FVector& WorldPosition) const
{
	return FVector::Distance(WorldPosition, GetPlanetCenter()) - GetApproximateRadius();
}

bool AJTSPlanetAnchor::TraceToSurface(const FVector& WorldPosition, FJTSPlanetSurfaceHit& OutSurfaceHit) const
{
	const FVector PlanetCenter = GetPlanetCenter();
	const FVector RadialDirection = GetRadialUpVector(WorldPosition);
	const float OuterTraceRadius = FMath::Max(
		FVector::Distance(WorldPosition, PlanetCenter) + FMath::Max(1.0f, SurfaceTraceOuterPadding),
		GetApproximateRadius() + FMath::Max(1.0f, SurfaceTraceOuterPadding));
	return TraceRadialDirectionToSurface(RadialDirection, OuterTraceRadius, OutSurfaceHit);
}

bool AJTSPlanetAnchor::ProjectPointToSurface(const FVector& WorldPosition, FJTSPlanetSurfaceHit& OutSurfaceHit) const
{
	return TraceToSurface(WorldPosition, OutSurfaceHit);
}

bool AJTSPlanetAnchor::GetSurfaceNormalAt(const FVector& WorldPosition, FVector& OutSurfaceNormal) const
{
	FJTSPlanetSurfaceHit SurfaceHit;
	if (!TraceToSurface(WorldPosition, SurfaceHit))
	{
		OutSurfaceNormal = GetRadialUpVector(WorldPosition);
		return false;
	}

	OutSurfaceNormal = SurfaceHit.ImpactNormal;
	return true;
}

bool AJTSPlanetAnchor::GetSurfaceFrameAt(
	const FVector& WorldPosition,
	const FVector& PreferredForward,
	FJTSPlanetSurfaceFrame& OutSurfaceFrame) const
{
	FJTSPlanetSurfaceHit SurfaceHit;
	if (!TraceToSurface(WorldPosition, SurfaceHit))
	{
		OutSurfaceFrame = FJTSPlanetSurfaceFrame();
		return false;
	}

	const FVector SurfaceUp = SurfaceHit.ImpactNormal.GetSafeNormal();
	FVector SurfaceForward = FVector::VectorPlaneProject(PreferredForward, SurfaceUp).GetSafeNormal();
	if (SurfaceForward.IsNearlyZero())
	{
		SurfaceForward = GetFallbackTangent(SurfaceUp);
	}

	FVector SurfaceRight = FVector::CrossProduct(SurfaceUp, SurfaceForward).GetSafeNormal();
	if (SurfaceRight.IsNearlyZero())
	{
		SurfaceForward = GetFallbackTangent(SurfaceUp);
		SurfaceRight = FVector::CrossProduct(SurfaceUp, SurfaceForward).GetSafeNormal();
	}
	SurfaceForward = FVector::CrossProduct(SurfaceRight, SurfaceUp).GetSafeNormal();

	OutSurfaceFrame.Location = SurfaceHit.ImpactPoint;
	OutSurfaceFrame.Up = SurfaceUp;
	OutSurfaceFrame.Forward = SurfaceForward;
	OutSurfaceFrame.Right = SurfaceRight;
	OutSurfaceFrame.Transform = FTransform(
		FRotationMatrix::MakeFromXZ(SurfaceForward, SurfaceUp).ToQuat(),
		SurfaceHit.ImpactPoint);
	return true;
}

FVector AJTSPlanetAnchor::ProjectDirectionToSurfaceTangent(const FVector& WorldDirection, const FVector& WorldPosition) const
{
	FVector SurfaceNormal;
	GetSurfaceNormalAt(WorldPosition, SurfaceNormal);
	FVector TangentDirection = FVector::VectorPlaneProject(WorldDirection, SurfaceNormal).GetSafeNormal();
	return TangentDirection.IsNearlyZero() ? GetFallbackTangent(SurfaceNormal) : TangentDirection;
}

FRotator AJTSPlanetAnchor::MakeSurfaceAlignedRotation(const FVector& WorldPosition, const FVector& PreferredForward) const
{
	FJTSPlanetSurfaceFrame SurfaceFrame;
	if (GetSurfaceFrameAt(WorldPosition, PreferredForward, SurfaceFrame))
	{
		return SurfaceFrame.Transform.Rotator();
	}

	const FVector SurfaceUp = GetRadialUpVector(WorldPosition);
	const FVector SurfaceForward = ProjectDirectionToSurfaceTangent(PreferredForward, WorldPosition);
	return FRotationMatrix::MakeFromXZ(SurfaceForward, SurfaceUp).Rotator();
}

bool AJTSPlanetAnchor::SnapActorToPlanetSurface(AActor* ActorToSnap, float SurfaceOffset)
{
	if (!IsValid(ActorToSnap))
	{
		return false;
	}

	FJTSPlanetSurfaceHit SurfaceHit;
	if (!TraceToSurface(ActorToSnap->GetActorLocation(), SurfaceHit))
	{
		return false;
	}

	const FTransform SurfaceTransform = BuildSurfaceTransform(SurfaceHit, ActorToSnap->GetActorForwardVector());
	const FVector SnappedLocation = SurfaceHit.ImpactPoint
		+ SurfaceHit.ImpactNormal * FMath::Max(0.0f, SurfaceOffset);
	ActorToSnap->SetActorLocationAndRotation(
		SnappedLocation,
		SurfaceTransform.GetRotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	return true;
}

FVector AJTSPlanetAnchor::RandomDirectionOnSphere() const
{
	return FMath::VRand();
}

float AJTSPlanetAnchor::ArcDistanceToAngleRadians(float ArcDistance) const
{
	return FMath::Clamp(FMath::Max(0.0f, ArcDistance) / GetApproximateRadius(), 0.0f, PI);
}

bool AJTSPlanetAnchor::RandomPointInSurfaceCap(
	const FVector& CenterDirection,
	float ArcDistance,
	FJTSPlanetSurfaceHit& OutSurfaceHit) const
{
	const FVector CapCenterDirection = CenterDirection.IsNearlyZero()
		? GetActorUpVector().GetSafeNormal()
		: CenterDirection.GetSafeNormal();
	const float MaximumAngle = ArcDistanceToAngleRadians(ArcDistance);
	const float CosineOfAngle = FMath::Lerp(1.0f, FMath::Cos(MaximumAngle), FMath::FRand());
	const float SineOfAngle = FMath::Sqrt(FMath::Max(0.0f, 1.0f - FMath::Square(CosineOfAngle)));
	const float Azimuth = FMath::FRandRange(0.0f, UE_TWO_PI);

	FVector TangentX;
	FVector TangentY;
	CapCenterDirection.FindBestAxisVectors(TangentX, TangentY);
	const FVector CandidateDirection = (
		CapCenterDirection * CosineOfAngle
		+ (TangentX * FMath::Cos(Azimuth) + TangentY * FMath::Sin(Azimuth)) * SineOfAngle).GetSafeNormal();
	return TraceRadialDirectionToSurface(
		CandidateDirection,
		GetApproximateRadius() + FMath::Max(1.0f, SurfaceTraceOuterPadding),
		OutSurfaceHit);
}

float AJTSPlanetAnchor::ApproximateSurfaceArcDistance(const FVector& WorldPositionA, const FVector& WorldPositionB) const
{
	const float Dot = FMath::Clamp(
		FVector::DotProduct(GetRadialUpVector(WorldPositionA), GetRadialUpVector(WorldPositionB)),
		-1.0f,
		1.0f);
	return FMath::Acos(Dot) * GetApproximateRadius();
}

FTransform AJTSPlanetAnchor::GetLandingTransform() const
{
	FTransform LandingSurfaceTransform;
	if (GetLandingSurfaceTransform(LandingSurfaceTransform))
	{
		return LandingSurfaceTransform;
	}
	if (bUseLegacyLandingAnchorTransform)
	{
		return LandingAnchorTransform;
	}

	const FVector LandingCandidate = GetPlanetCenter()
		+ GetActorUpVector().GetSafeNormal() * (GetApproximateRadius() + FMath::Max(1.0f, SurfaceTraceOuterPadding));
	FJTSPlanetSurfaceHit SurfaceHit;
	return TraceToSurface(LandingCandidate, SurfaceHit)
		? BuildSurfaceTransform(SurfaceHit, GetActorForwardVector())
		: FTransform(FRotationMatrix::MakeFromXZ(GetFallbackTangent(GetActorUpVector()), GetActorUpVector()).ToQuat(), LandingCandidate);
}

bool AJTSPlanetAnchor::GetLandingSurfaceTransform(FTransform& OutLandingTransform) const
{
	AActor* const LandingAnchor = LandingAnchorActor.Get();
	if (!IsValid(LandingAnchor) || !HasGameplaySurface())
	{
		return false;
	}

	if (AJTSPlanetSurfaceAnchor* const SurfaceAnchor = Cast<AJTSPlanetSurfaceAnchor>(LandingAnchor))
	{
		return SurfaceAnchor->GetPlanetAnchor() == this
			&& SurfaceAnchor->GetSurfaceTransform(OutLandingTransform);
	}

	FJTSPlanetSurfaceFrame SurfaceFrame;
	if (!GetSurfaceFrameAt(LandingAnchor->GetActorLocation(), LandingAnchor->GetActorForwardVector(), SurfaceFrame))
	{
		return false;
	}

	OutLandingTransform = SurfaceFrame.Transform;
	return true;
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

	const FVector ApproachDirection = GetActorUpVector().GetSafeNormal();
	const FVector EntryLocation = GetPlanetCenter()
		+ ApproachDirection * (GetApproximateRadius() + FMath::Max(0.0f, DefaultApproachEntryDistance));
	return FTransform(FRotationMatrix::MakeFromX(-ApproachDirection).ToQuat(), EntryLocation);
}

bool AJTSPlanetAnchor::HasPlanetContentLevel() const
{
	return !PlanetContentLevel.IsNull();
}

const TSoftObjectPtr<UWorld>& AJTSPlanetAnchor::GetPlanetContentLevel() const
{
	return PlanetContentLevel;
}

FTransform AJTSPlanetAnchor::GetPlanetContentTransform() const
{
	return PlanetContentTransform;
}

FVector AJTSPlanetAnchor::GetExteriorCenter() const
{
	return GetPlanetCenter();
}

float AJTSPlanetAnchor::GetExteriorAltitude(const FVector& WorldPosition) const
{
	return GetApproximateAltitude(WorldPosition);
}

FVector AJTSPlanetAnchor::GetDirectionToPlanet(const FVector& WorldPosition) const
{
	return GetGravityDirection(WorldPosition);
}

AActor* AJTSPlanetAnchor::GetExteriorVisualActor() const
{
	return ExteriorVisualActor.Get();
}

FTransform AJTSPlanetAnchor::GetSurfaceFrameTransform() const
{
	return IsValid(SurfaceAnchorActor) ? SurfaceAnchorActor->GetActorTransform() : SurfaceAnchorTransform;
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
	return GetApproximateAltitude(WorldPosition);
}

FVector AJTSPlanetAnchor::GetRecommendedExteriorCenter() const
{
	return GetPlanetCenter();
}

void AJTSPlanetAnchor::SetExteriorCenterToRecommended()
{
	ExteriorCenter = GetPlanetCenter();
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
	return FMath::Max(GetTakeoffTransitionAltitude(), SpaceFlightAltitude);
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
	return FMath::Max(0.0f, ApproachTransitionAltitude);
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

bool AJTSPlanetAnchor::TraceRadialDirectionToSurface(
	const FVector& RadialDirection,
	float OuterTraceRadius,
	FJTSPlanetSurfaceHit& OutSurfaceHit) const
{
	const FVector SafeRadialDirection = RadialDirection.IsNearlyZero()
		? GetActorUpVector().GetSafeNormal()
		: RadialDirection.GetSafeNormal();
	const FVector PlanetCenter = GetPlanetCenter();
	const float SafeOuterTraceRadius = FMath::Max(GetApproximateRadius() + 1.0f, OuterTraceRadius);
	const FVector TraceStart = PlanetCenter + SafeRadialDirection * SafeOuterTraceRadius;
	const FVector TraceEnd = PlanetCenter;
	ResetSurfaceHit(OutSurfaceHit, TraceStart, TraceEnd);

	if (!HasGameplaySurface())
	{
		return false;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}
	if (bDebugPlanetSurface)
	{
		DrawDebugLine(World, TraceStart, TraceEnd, FColor::Cyan, false, -1.0f, 0, 0.75f);
	}

	FCollisionQueryParams TraceParameters(SCENE_QUERY_STAT(JTSPlanetSurfaceTrace), true, this);
	TraceParameters.bTraceComplex = true;
	TraceParameters.AddIgnoredActor(this);

	TArray<FHitResult> TraceHits;
	if (!World->LineTraceMultiByChannel(
		TraceHits,
		TraceStart,
		TraceEnd,
		SurfaceTraceChannel,
		TraceParameters))
	{
		return false;
	}

	for (const FHitResult& TraceHit : TraceHits)
	{
		if (!TraceHit.bBlockingHit || !IsGameplaySurfaceComponent(TraceHit.GetComponent()))
		{
			continue;
		}

		FVector ImpactNormal = TraceHit.ImpactNormal.GetSafeNormal();
		if (ImpactNormal.IsNearlyZero())
		{
			ImpactNormal = TraceHit.Normal.GetSafeNormal();
		}
		const FVector RadialUpAtImpact = (TraceHit.ImpactPoint - PlanetCenter).GetSafeNormal();
		if (!ImpactNormal.IsNearlyZero() && !RadialUpAtImpact.IsNearlyZero()
			&& FVector::DotProduct(ImpactNormal, RadialUpAtImpact) < 0.0f)
		{
			ImpactNormal *= -1.0f;
		}

		OutSurfaceHit.bBlockingHit = true;
		OutSurfaceHit.ImpactPoint = TraceHit.ImpactPoint;
		OutSurfaceHit.ImpactNormal = ImpactNormal.IsNearlyZero() ? RadialUpAtImpact : ImpactNormal;
		OutSurfaceHit.Distance = FVector::Distance(TraceStart, TraceHit.ImpactPoint);
		OutSurfaceHit.HitActor = TraceHit.GetActor();
		OutSurfaceHit.HitComponent = TraceHit.GetComponent();
		if (bDebugPlanetSurface)
		{
			DrawDebugDirectionalArrow(
				World,
				TraceHit.ImpactPoint,
				TraceHit.ImpactPoint + OutSurfaceHit.ImpactNormal * 150.0f,
				24.0f,
				FColor::Green,
				false,
				-1.0f,
				0,
				1.0f);
		}
		return true;
	}

	return false;
}

bool AJTSPlanetAnchor::IsGameplaySurfaceComponent(const UPrimitiveComponent* Candidate) const
{
	if (!IsValid(Candidate) || Candidate->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		return false;
	}

	if (IsValid(GameplaySurfaceComponent))
	{
		return Candidate == GameplaySurfaceComponent.Get();
	}

	return IsValid(GameplaySurfaceActor) && Candidate->GetOwner() == GameplaySurfaceActor.Get();
}

FVector AJTSPlanetAnchor::GetFallbackTangent(const FVector& UpVector) const
{
	const FVector SafeUp = UpVector.GetSafeNormal();
	FVector TangentX;
	FVector TangentY;
	SafeUp.FindBestAxisVectors(TangentX, TangentY);
	return TangentX.GetSafeNormal();
}

FTransform AJTSPlanetAnchor::BuildSurfaceTransform(const FJTSPlanetSurfaceHit& SurfaceHit, const FVector& PreferredForward) const
{
	const FVector SurfaceUp = SurfaceHit.ImpactNormal.GetSafeNormal();
	FVector SurfaceForward = FVector::VectorPlaneProject(PreferredForward, SurfaceUp).GetSafeNormal();
	if (SurfaceForward.IsNearlyZero())
	{
		SurfaceForward = GetFallbackTangent(SurfaceUp);
	}
	return FTransform(FRotationMatrix::MakeFromXZ(SurfaceForward, SurfaceUp).ToQuat(), SurfaceHit.ImpactPoint);
}
