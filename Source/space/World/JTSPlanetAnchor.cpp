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
	if (IsValid(GameplaySurfaceComponent)
		&& GameplaySurfaceComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
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
	return TraceRadialDirectionToSurface(
		RadialDirection,
		FVector::Distance(WorldPosition, PlanetCenter),
		OutSurfaceHit);
}

bool AJTSPlanetAnchor::ProjectPointToSurface(const FVector& WorldPosition, FJTSPlanetSurfaceHit& OutSurfaceHit) const
{
	return TraceToSurface(WorldPosition, OutSurfaceHit);
}

bool AJTSPlanetAnchor::ProbeSurfaceAlongGravity(
	const FVector& StartLocation,
	float MaxDistance,
	FJTSPlanetSurfaceHit& OutSurfaceHit) const
{
	UPrimitiveComponent* const SurfaceComponent = GetGameplaySurfaceComponent();
	const FVector GravityDirection = GetGravityDirection(StartLocation);
	const float SafeMaxDistance = FMath::Max(0.0f, MaxDistance);
	const FVector TraceEnd = StartLocation + GravityDirection * SafeMaxDistance;
	ResetSurfaceHit(OutSurfaceHit, StartLocation, TraceEnd);
	if (!IsValid(SurfaceComponent)
		|| SurfaceComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision
		|| GravityDirection.IsNearlyZero()
		|| SafeMaxDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	UWorld* const World = GetWorld();
	if (bDebugPlanetSurface && World != nullptr)
	{
		DrawDebugLine(World, StartLocation, TraceEnd, FColor::Orange, false, -1.0f, 0, 0.75f);
	}

	FHitResult TraceHit;
	if (!TraceGameplaySurfaceSegment(StartLocation, TraceEnd, TraceHit))
	{
		return false;
	}

	FVector ImpactNormal = TraceHit.ImpactNormal.GetSafeNormal();
	if (ImpactNormal.IsNearlyZero())
	{
		ImpactNormal = TraceHit.Normal.GetSafeNormal();
	}

	const FVector RadialUpAtImpact = GetRadialUpVector(TraceHit.ImpactPoint);
	if (!ImpactNormal.IsNearlyZero()
		&& !RadialUpAtImpact.IsNearlyZero()
		&& FVector::DotProduct(ImpactNormal, RadialUpAtImpact) < 0.0f)
	{
		ImpactNormal *= -1.0f;
	}

	OutSurfaceHit.bBlockingHit = true;
	OutSurfaceHit.ImpactPoint = TraceHit.ImpactPoint;
	OutSurfaceHit.ImpactNormal = ImpactNormal.IsNearlyZero() ? RadialUpAtImpact : ImpactNormal;
	OutSurfaceHit.Distance = FVector::Distance(StartLocation, TraceHit.ImpactPoint);
	OutSurfaceHit.HitActor = IsValid(TraceHit.GetActor()) ? TraceHit.GetActor() : SurfaceComponent->GetOwner();
	OutSurfaceHit.HitComponent = IsValid(TraceHit.GetComponent()) ? TraceHit.GetComponent() : SurfaceComponent;
	if (bDebugPlanetSurface && World != nullptr)
	{
		DrawDebugDirectionalArrow(
			World,
			TraceHit.ImpactPoint,
			TraceHit.ImpactPoint + OutSurfaceHit.ImpactNormal * 150.0f,
			24.0f,
			FColor::Yellow,
			false,
			-1.0f,
			0,
			1.0f);
	}

	return true;
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

bool AJTSPlanetAnchor::GetSurfaceFrameAlongGravity(
	const FVector& StartLocation,
	float MaxDistance,
	const FVector& PreferredForward,
	FJTSPlanetSurfaceFrame& OutSurfaceFrame) const
{
	FJTSPlanetSurfaceHit SurfaceHit;
	if (!ProbeSurfaceAlongGravity(StartLocation, MaxDistance, SurfaceHit))
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
		GetApproximateRadius(),
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
		+ GetActorUpVector().GetSafeNormal() * GetApproximateRadius();
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
	float CandidateDistance,
	FJTSPlanetSurfaceHit& OutSurfaceHit) const
{
	UPrimitiveComponent* const SurfaceComponent = GetGameplaySurfaceComponent();
	if (!IsValid(SurfaceComponent)
		|| SurfaceComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		ResetSurfaceHit(OutSurfaceHit, FVector::ZeroVector, FVector::ZeroVector);
		return false;
	}

	const FVector SafeRadialDirection = RadialDirection.IsNearlyZero()
		? GetActorUpVector().GetSafeNormal()
		: RadialDirection.GetSafeNormal();
	const FVector PlanetCenter = GetPlanetCenter();
	float BoundsOuterRadius = FVector::Distance(PlanetCenter, SurfaceComponent->Bounds.Origin)
		+ SurfaceComponent->Bounds.SphereRadius;
	if (AActor* const SurfaceActor = GetGameplaySurfaceActor(); IsValid(SurfaceActor))
	{
		TArray<UPrimitiveComponent*> SurfaceComponents;
		SurfaceActor->GetComponents<UPrimitiveComponent>(SurfaceComponents);
		for (const UPrimitiveComponent* const CandidateComponent : SurfaceComponents)
		{
			if (IsValid(CandidateComponent)
				&& CandidateComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				BoundsOuterRadius = FMath::Max(
					BoundsOuterRadius,
					FVector::Distance(PlanetCenter, CandidateComponent->Bounds.Origin)
						+ CandidateComponent->Bounds.SphereRadius);
			}
		}
	}
	const float SafeOuterTraceRadius = FMath::Max(
		FMath::Max(BoundsOuterRadius, GetApproximateRadius()),
		FMath::Max(0.0f, CandidateDistance))
		+ FMath::Max(1.0f, SurfaceTraceOuterPadding);
	const FVector TraceStart = PlanetCenter + SafeRadialDirection * SafeOuterTraceRadius;
	const FVector TraceEnd = PlanetCenter - SafeRadialDirection * SafeOuterTraceRadius;
	ResetSurfaceHit(OutSurfaceHit, TraceStart, TraceEnd);

	UWorld* const World = GetWorld();
	if (bDebugPlanetSurface && World != nullptr)
	{
		DrawDebugLine(World, TraceStart, TraceEnd, FColor::Cyan, false, -1.0f, 0, 0.75f);
	}

	FHitResult TraceHit;
	if (!TraceGameplaySurfaceSegment(TraceStart, TraceEnd, TraceHit))
	{
		return false;
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
	OutSurfaceHit.HitActor = IsValid(TraceHit.GetActor()) ? TraceHit.GetActor() : SurfaceComponent->GetOwner();
	OutSurfaceHit.HitComponent = IsValid(TraceHit.GetComponent()) ? TraceHit.GetComponent() : SurfaceComponent;
	if (bDebugPlanetSurface && World != nullptr)
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

bool AJTSPlanetAnchor::TraceGameplaySurfaceSegment(
	const FVector& TraceStart,
	const FVector& TraceEnd,
	FHitResult& OutTraceHit) const
{
	OutTraceHit = FHitResult();

	TArray<UPrimitiveComponent*> CandidateSurfaceComponents;
	const auto AddCandidateSurfaceComponent = [&CandidateSurfaceComponents](UPrimitiveComponent* CandidateComponent)
	{
		if (IsValid(CandidateComponent)
			&& CandidateComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision
			&& !CandidateSurfaceComponents.Contains(CandidateComponent))
		{
			CandidateSurfaceComponents.Add(CandidateComponent);
		}
	};

	// The Blueprint-selected component stays first. If it is a decorative sibling or has a stale
	// collision setup, the configured surface actor remains authoritative and its other collision
	// primitives are valid real-surface candidates.
	AddCandidateSurfaceComponent(GetGameplaySurfaceComponent());
	if (AActor* const SurfaceActor = GetGameplaySurfaceActor(); IsValid(SurfaceActor))
	{
		TArray<UPrimitiveComponent*> SurfaceComponents;
		SurfaceActor->GetComponents<UPrimitiveComponent>(SurfaceComponents);
		for (UPrimitiveComponent* const CandidateComponent : SurfaceComponents)
		{
			AddCandidateSurfaceComponent(CandidateComponent);
		}
	}

	if (CandidateSurfaceComponents.IsEmpty())
	{
		return false;
	}

	UWorld* const World = GetWorld();
	for (UPrimitiveComponent* const SurfaceComponent : CandidateSurfaceComponents)
	{
		FCollisionQueryParams ComponentTraceParameters(SCENE_QUERY_STAT(JTSPlanetConfiguredSurfaceTrace), true, this);
		ComponentTraceParameters.bTraceComplex = true;
		if (SurfaceComponent->LineTraceComponent(OutTraceHit, TraceStart, TraceEnd, ComponentTraceParameters)
			&& OutTraceHit.bBlockingHit)
		{
			return true;
		}

		// Some authored mesh collision is simple-only. Keep the trace tied to this configured
		// component before falling back to the physics-scene query below.
		OutTraceHit = FHitResult();
		ComponentTraceParameters.bTraceComplex = false;
		if (SurfaceComponent->LineTraceComponent(OutTraceHit, TraceStart, TraceEnd, ComponentTraceParameters)
			&& OutTraceHit.bBlockingHit)
		{
			return true;
		}

		if (!IsValid(World))
		{
			continue;
		}

		// Query the candidate component's actual object type instead of a broad visibility trace.
		// This avoids a nearby Pawn or spacecraft masking the configured planet mesh during startup.
		const auto TracePhysicsSceneForComponent = [World, SurfaceComponent, TraceStart, TraceEnd, this](bool bTraceComplex, FHitResult& OutPhysicsHit)
		{
			FCollisionQueryParams WorldTraceParameters(SCENE_QUERY_STAT(JTSPlanetSurfacePhysicsTrace), true, this);
			WorldTraceParameters.bTraceComplex = bTraceComplex;
			FCollisionObjectQueryParams ObjectQuery;
			ObjectQuery.AddObjectTypesToQuery(SurfaceComponent->GetCollisionObjectType());
			TArray<FHitResult> TraceHits;
			World->LineTraceMultiByObjectType(
				TraceHits,
				TraceStart,
				TraceEnd,
				ObjectQuery,
				WorldTraceParameters);

			for (const FHitResult& CandidateHit : TraceHits)
			{
				if (CandidateHit.bBlockingHit && CandidateHit.GetComponent() == SurfaceComponent)
				{
					OutPhysicsHit = CandidateHit;
					return true;
				}
			}

			return false;
		};

		OutTraceHit = FHitResult();
		if (TracePhysicsSceneForComponent(true, OutTraceHit)
			|| TracePhysicsSceneForComponent(false, OutTraceHit))
		{
			return true;
		}
	}

	return false;
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
