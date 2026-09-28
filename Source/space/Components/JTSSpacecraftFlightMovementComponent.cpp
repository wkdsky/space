// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSSpacecraftFlightMovementComponent.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Math/RotationMatrix.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSpaceWorldManager.h"

namespace
{
	FVector BlendUnitDirections(const FVector& From, const FVector& To, float Alpha)
	{
		const FVector SafeFrom = From.GetSafeNormal();
		const FVector SafeTo = To.GetSafeNormal();
		if (SafeFrom.IsNearlyZero())
		{
			return SafeTo.IsNearlyZero() ? FVector::UpVector : SafeTo;
		}
		if (SafeTo.IsNearlyZero())
		{
			return SafeFrom;
		}

		const float SafeAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
		if (SafeAlpha <= KINDA_SMALL_NUMBER)
		{
			return SafeFrom;
		}
		if (SafeAlpha >= 1.0f - KINDA_SMALL_NUMBER)
		{
			return SafeTo;
		}

		const FQuat DeltaRotation = FQuat::FindBetweenNormals(SafeFrom, SafeTo);
		return FQuat::Slerp(FQuat::Identity, DeltaRotation, SafeAlpha)
			.RotateVector(SafeFrom)
			.GetSafeNormal();
	}
}

UJTSSpacecraftFlightMovementComponent::UJTSSpacecraftFlightMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bAutoActivate = true;
}

void UJTSSpacecraftFlightMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	EffectiveStats = BaseStats;
	if (const APawn* const OwningPawn = GetPawnOwner())
	{
		const FVector ActorUp = OwningPawn->GetActorUpVector().GetSafeNormal();
		InertialReferenceUp = ActorUp.IsNearlyZero() ? FVector::UpVector : ActorUp;
		CurrentReferenceUp = InertialReferenceUp;
		bInertialReferenceUpInitialized = true;
	}
}

void UJTSSpacecraftFlightMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetOwner() == nullptr || ShouldSkipUpdate(DeltaTime) || !IsValid(UpdatedComponent))
	{
		return;
	}

	// Clients do not simulate authoritative movement, but they maintain the same inexpensive,
	// throttled real-surface sample so their camera/HUD blend matches the server's flight frame.
	RefreshSurfaceProximity(DeltaTime);
	UpdateReferenceFrame();
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (bAssistedLanding)
	{
		TickAssistedLanding(DeltaTime);
	}
	else
	{
		TickFlight(DeltaTime);
	}

	SubmitExteriorAltitude(DeltaTime);
}

void UJTSSpacecraftFlightMovementComponent::SetMoveInput(const FVector2D& Value)
{
	if (bAssistedLanding)
	{
		return;
	}
	MoveInput = Value.GetClampedToMaxSize(1.0f);
}

void UJTSSpacecraftFlightMovementComponent::SetVerticalInput(float Value)
{
	if (bAssistedLanding)
	{
		return;
	}
	VerticalInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void UJTSSpacecraftFlightMovementComponent::SetSteeringInput(const FVector2D& Value)
{
	if (bAssistedLanding)
	{
		return;
	}
	SteeringInput = FVector2D(
		FMath::Clamp(Value.X, -1.0f, 1.0f),
		FMath::Clamp(Value.Y, -1.0f, 1.0f));
}

void UJTSSpacecraftFlightMovementComponent::SetTurnAround(bool bNewTurnAround)
{
	if (bAssistedLanding)
	{
		return;
	}
	bTurnAround = bNewTurnAround;
}

void UJTSSpacecraftFlightMovementComponent::SetBoosting(bool bNewBoosting)
{
	SetBoostState(bNewBoosting && !bAssistedLanding);
}

void UJTSSpacecraftFlightMovementComponent::SetBraking(bool bNewBraking)
{
	bBraking = bNewBraking && !bAssistedLanding;
}

void UJTSSpacecraftFlightMovementComponent::ClearInput()
{
	MoveInput = FVector2D::ZeroVector;
	VerticalInput = 0.0f;
	SteeringInput = FVector2D::ZeroVector;
	bTurnAround = false;
	bTurnAroundLatched = false;
	TurnAroundTargetForward = FVector::ZeroVector;
	TurnAroundDeckUp = FVector::ZeroVector;
	CurrentFacingTurnSpeedRadians = 0.0f;
	bBraking = false;
	SetBoostState(false);
}

bool UJTSSpacecraftFlightMovementComponent::BeginAssistedLanding(
	AJTSPlanetAnchor* Planet,
	float LandingClearance,
	float DurationSeconds)
{
	if (!IsValid(UpdatedComponent) || !IsValid(Planet))
	{
		return false;
	}

	ClearInput();
	bAssistedLanding = true;
	Velocity = FVector::ZeroVector;
	TargetPlanet = Planet;
	SetAssistedLandingPhase(EJTSSpacecraftLandingAssistPhase::Aligning);
	AssistedLandingClearance = FMath::Max(0.0f, LandingClearance);
	AssistedLandingDescentSpeed = AssistedLandingMaximumDescentSpeed;
	if (AJTSSpacecraftActor* const Spacecraft = Cast<AJTSSpacecraftActor>(GetPawnOwner()))
	{
		const FJTSSpacecraftGroundInfo GroundInfo = Spacecraft->GetGroundInfo();
		if (GroundInfo.bHasGround)
		{
			const float DesiredDuration = FMath::Max(
				0.1f,
				DurationSeconds > 0.0f ? DurationSeconds : DefaultLandingDuration);
			const float HeightToLose = FMath::Max(0.0f, GroundInfo.DockingHeight - AssistedLandingClearance);
			AssistedLandingDescentSpeed = FMath::Clamp(
				HeightToLose / DesiredDuration,
				AssistedLandingMinimumDescentSpeed,
				AssistedLandingMaximumDescentSpeed);
		}
	}
	AssistedLandingElapsed = 0.0f;
	return true;
}

void UJTSSpacecraftFlightMovementComponent::CancelAssistedLanding()
{
	if (!bAssistedLanding)
	{
		return;
	}

	bAssistedLanding = false;
	ClearInput();
	Velocity = FVector::ZeroVector;
	AssistedLandingClearance = 0.0f;
	AssistedLandingDescentSpeed = 0.0f;
	SetAssistedLandingPhase(EJTSSpacecraftLandingAssistPhase::None);
}

bool UJTSSpacecraftFlightMovementComponent::IsBoosting() const
{
	return bBoosting;
}

bool UJTSSpacecraftFlightMovementComponent::IsAssistedLanding() const
{
	return bAssistedLanding;
}

bool UJTSSpacecraftFlightMovementComponent::IsUsingPlanetSurfaceFlightFrame() const
{
	return GetSurfaceFlightAssistAlpha() > KINDA_SMALL_NUMBER;
}

float UJTSSpacecraftFlightMovementComponent::GetSurfaceFlightAssistAlpha() const
{
	return bUsePlanetSurfaceFlightFrame ? FMath::Clamp(SurfaceFlightAssistAlpha, 0.0f, 1.0f) : 0.0f;
}

bool UJTSSpacecraftFlightMovementComponent::GetResolvedSurfaceAltitude(
	const AJTSPlanetAnchor* Planet,
	float& OutAltitude) const
{
	if (!bHasSurfaceProximity || !IsValid(Planet) || TargetPlanet.Get() != Planet)
	{
		return false;
	}

	OutAltitude = CachedSurfaceAltitude;
	return FMath::IsFinite(OutAltitude);
}

float UJTSSpacecraftFlightMovementComponent::GetCurrentSpeed() const
{
	return Velocity.Size();
}

float UJTSSpacecraftFlightMovementComponent::GetSpeedNormalized() const
{
	const float MaximumPlanarSpeed = EffectiveStats.MaxMoveSpeed * FMath::Max(1.0f, EffectiveStats.BoostMultiplier);
	const float MaximumSpeed = FMath::Max(1.0f, FMath::Max(MaximumPlanarSpeed, EffectiveStats.LiftSpeed));
	return FMath::Clamp(GetCurrentSpeed() / MaximumSpeed, 0.0f, 1.0f);
}

float UJTSSpacecraftFlightMovementComponent::GetThrottleNormalized() const
{
	return MoveInput.Y;
}

float UJTSSpacecraftFlightMovementComponent::GetVerticalInput() const
{
	return VerticalInput;
}

FJTSSpacecraftFlightStats UJTSSpacecraftFlightMovementComponent::GetBaseStats() const
{
	return BaseStats;
}

FJTSSpacecraftFlightStats UJTSSpacecraftFlightMovementComponent::GetEffectiveStats() const
{
	return EffectiveStats;
}

void UJTSSpacecraftFlightMovementComponent::SetEffectiveStats(const FJTSSpacecraftFlightStats& NewEffectiveStats)
{
	EffectiveStats = NewEffectiveStats;
}

void UJTSSpacecraftFlightMovementComponent::ResetEffectiveStats()
{
	EffectiveStats = BaseStats;
}

void UJTSSpacecraftFlightMovementComponent::SetTargetPlanet(AJTSPlanetAnchor* NewTargetPlanet)
{
	if (TargetPlanet.Get() == NewTargetPlanet)
	{
		return;
	}

	TargetPlanet = NewTargetPlanet;
	bHasSurfaceProximity = false;
	SurfaceFlightAssistAlpha = 0.0f;
	SurfaceProximityProbeElapsed = FMath::Max(0.0f, SurfaceProximityProbeInterval);
}

void UJTSSpacecraftFlightMovementComponent::CaptureInertialReferenceUp(const FVector& NewReferenceUp)
{
	const FVector SafeReferenceUp = NewReferenceUp.GetSafeNormal();
	if (SafeReferenceUp.IsNearlyZero())
	{
		return;
	}

	InertialReferenceUp = SafeReferenceUp;
	bInertialReferenceUpInitialized = true;
	if (GetSurfaceFlightAssistAlpha() <= KINDA_SMALL_NUMBER)
	{
		CurrentReferenceUp = InertialReferenceUp;
	}
}

void UJTSSpacecraftFlightMovementComponent::TickAssistedLanding(float DeltaTime)
{
	AJTSSpacecraftActor* const Spacecraft = Cast<AJTSSpacecraftActor>(GetPawnOwner());
	AJTSPlanetAnchor* const Planet = TargetPlanet.Get();
	if (!IsValid(Spacecraft) || !IsValid(Planet))
	{
		FailAssistedLanding(EJTSLandingValidationFailure::NoPlanet);
		return;
	}

	if (!Spacecraft->RefreshGroundInfo(Planet))
	{
		FailAssistedLanding(EJTSLandingValidationFailure::NoSurface);
		return;
	}

	const FJTSSpacecraftGroundInfo GroundInfo = Spacecraft->GetGroundInfo();
	const FVector SurfaceUp = GroundInfo.SurfaceNormal.GetSafeNormal();
	if (!GroundInfo.bHasGround || SurfaceUp.IsNearlyZero())
	{
		FailAssistedLanding(EJTSLandingValidationFailure::NoSurface);
		return;
	}

	FVector SurfaceForward = FVector::VectorPlaneProject(Spacecraft->GetActorForwardVector(), SurfaceUp).GetSafeNormal();
	if (SurfaceForward.IsNearlyZero())
	{
		SurfaceForward = GroundInfo.SurfaceTransform.GetUnitAxis(EAxis::X).GetSafeNormal();
	}
	if (SurfaceForward.IsNearlyZero())
	{
		FVector FallbackRight;
		SurfaceUp.FindBestAxisVectors(SurfaceForward, FallbackRight);
	}

	const FQuat DesiredRotation = FRotationMatrix::MakeFromXZ(SurfaceForward, SurfaceUp).ToQuat();
	const FQuat CurrentRotation = UpdatedComponent->GetComponentQuat();
	const float RotationAlpha = FMath::Clamp(
		1.0f - FMath::Exp(-FMath::Max(0.1f, AssistedLandingRotationInterpolationSpeed) * DeltaTime),
		0.0f,
		1.0f);
	const FQuat NewRotation = FQuat::Slerp(CurrentRotation, DesiredRotation, RotationAlpha).GetNormalized();
	const float NewAlignment = FVector::DotProduct(NewRotation.GetAxisZ().GetSafeNormal(), SurfaceUp);
	const float AlignmentCosine = FMath::Cos(FMath::DegreesToRadians(
		FMath::Clamp(AssistedLandingAlignmentToleranceDegrees, 0.1f, 45.0f)));
	if (AssistedLandingPhase == EJTSSpacecraftLandingAssistPhase::Aligning)
	{
		// Rotating a long hull directly above terrain can make its future collision box overlap the
		// mesh even though the current attitude is valid. Stage upward first, then align in place.
		const float RequiredAlignmentHeight = FMath::Max(
			AssistedLandingClearance,
			Spacecraft->GetLandingCollisionClearanceForRotation(DesiredRotation, SurfaceUp))
			+ FMath::Max(0.0f, AssistedLandingAlignmentClearance);
		const float AlignmentHeightShortfall = RequiredAlignmentHeight - GroundInfo.DockingHeight;
		if (AlignmentHeightShortfall > LandingContactTolerance)
		{
			const float LiftSpeed = FMath::Clamp(
				AlignmentHeightShortfall * 3.0f,
				FMath::Max(1.0f, AssistedLandingMinimumDescentSpeed),
				FMath::Max(AssistedLandingMinimumDescentSpeed, AssistedLandingMaximumDescentSpeed));
			Velocity = FMath::VInterpConstantTo(
				Velocity,
				SurfaceUp * LiftSpeed,
				DeltaTime,
				FMath::Max(1.0f, AssistedLandingVelocityResponse));
			FHitResult LiftHit;
			MoveWithCollisionSweep(Velocity * DeltaTime, CurrentRotation, LiftHit);
			if (LiftHit.IsValidBlockingHit())
			{
				Velocity = FVector::ZeroVector;
			}
		}
		else
		{
			Velocity = FVector::ZeroVector;
			FHitResult AlignmentHit;
			MoveWithCollisionSweep(FVector::ZeroVector, NewRotation, AlignmentHit);
			if (!AlignmentHit.IsValidBlockingHit() && NewAlignment >= AlignmentCosine)
			{
				SetAssistedLandingPhase(EJTSSpacecraftLandingAssistPhase::Descending);
			}
		}

		AssistedLandingElapsed += DeltaTime;
		if (AssistedLandingElapsed >= FMath::Max(1.0f, AssistedLandingTimeout))
		{
			FailAssistedLanding(EJTSLandingValidationFailure::CollisionBlocked);
		}
		return;
	}

	const float HeightError = GroundInfo.DockingHeight - AssistedLandingClearance;
	// The probe runs along radial gravity, but terrain normals need not be radial. Moving to
	// HitPoint + Normal * Clearance also introduces an unwanted sideways step at touchdown.
	const FVector TargetLocation = UpdatedComponent->GetComponentLocation() - SurfaceUp * HeightError;
	const float CompletionCosine = FMath::Cos(FMath::DegreesToRadians(
		FMath::Clamp(LandingCompletionAlignmentDegrees, 0.0f, 90.0f)));
	const bool bAtLandingHeight = FMath::Abs(HeightError) <= LandingContactTolerance;
	if (bAtLandingHeight && NewAlignment >= CompletionCosine)
	{
		FHitResult FinalMoveHit;
		MoveWithCollisionSweep(TargetLocation - UpdatedComponent->GetComponentLocation(), DesiredRotation, FinalMoveHit);
		if (FVector::DistSquared(UpdatedComponent->GetComponentLocation(), TargetLocation)
			<= FMath::Square(FMath::Max(2.0f, LandingContactTolerance))
			&& Spacecraft->CanOccupyLandingTransform(Spacecraft->GetActorTransform())
			&& Spacecraft->RefreshGroundInfo(Planet))
		{
			const FJTSSpacecraftGroundInfo FinalGroundInfo = Spacecraft->GetGroundInfo();
			const FVector FinalSurfaceUp = FinalGroundInfo.SurfaceNormal.GetSafeNormal();
			const bool bFinalHeightValid = FinalGroundInfo.bHasGround
				&& FMath::Abs(FinalGroundInfo.DockingHeight - AssistedLandingClearance) <= LandingContactTolerance;
			const bool bFinalAlignmentValid = !FinalSurfaceUp.IsNearlyZero()
				&& FVector::DotProduct(Spacecraft->GetActorUpVector(), FinalSurfaceUp) >= CompletionCosine;
			if (bFinalHeightValid && bFinalAlignmentValid)
			{
				CompleteAssistedLanding();
				return;
			}
		}
	}

	// Descend briskly while high, then ease into the last part of the approach. The proportional
	// cap avoids overshooting a moving/uneven mesh surface while the braking band prevents a hard snap.
	const float HeightAboveTouchdown = FMath::Max(0.0f, HeightError);
	const float BrakingAlpha = FMath::SmoothStep(
		0.0f,
		FMath::Max(1.0f, AssistedLandingBrakingDistance),
		HeightAboveTouchdown);
	const float CruiseSpeed = FMath::Clamp(
		FMath::Max(AssistedLandingDescentSpeed, HeightAboveTouchdown * 1.8f),
		AssistedLandingMinimumDescentSpeed,
		AssistedLandingMaximumDescentSpeed);
	const float DesiredNormalSpeed = FMath::Min(
		HeightAboveTouchdown * 3.0f,
		FMath::Lerp(AssistedLandingTouchdownSpeed, CruiseSpeed, BrakingAlpha));
	const FVector DesiredVelocity = -SurfaceUp * FMath::Max(0.0f, DesiredNormalSpeed);
	Velocity = FMath::VInterpConstantTo(
		Velocity,
		DesiredVelocity,
		DeltaTime,
		FMath::Max(1.0f, AssistedLandingVelocityResponse));

	FHitResult Hit;
	MoveWithCollisionSweep(Velocity * DeltaTime, NewRotation, Hit);
	if (Hit.IsValidBlockingHit())
	{
		Velocity = FVector::ZeroVector;
	}

	AssistedLandingElapsed += DeltaTime;
	if (AssistedLandingElapsed >= FMath::Max(1.0f, AssistedLandingTimeout))
	{
		FailAssistedLanding(EJTSLandingValidationFailure::CollisionBlocked);
	}
}

void UJTSSpacecraftFlightMovementComponent::TickFlight(float DeltaTime)
{
	const FVector ReferenceUp = GetReferenceUp();
	if (bTurnAround)
	{
		if (!bTurnAroundLatched && IsValid(GetPawnOwner()))
		{
			// Latch the deck the ship is already flying in. S yaws inside that plane.
			const FVector DeckUp = GetPawnOwner()->GetActorUpVector().GetSafeNormal();
			const FVector HullForward = GetPawnOwner()->GetActorForwardVector().GetSafeNormal();
			TurnAroundDeckUp = DeckUp;
			TurnAroundTargetForward = FVector::VectorPlaneProject(-HullForward, DeckUp).GetSafeNormal();
			bTurnAroundLatched = !TurnAroundDeckUp.IsNearlyZero() && !TurnAroundTargetForward.IsNearlyZero();
		}
	}
	else
	{
		bTurnAroundLatched = false;
		TurnAroundTargetForward = FVector::ZeroVector;
		TurnAroundDeckUp = FVector::ZeroVector;
	}

	const FVector TargetVelocity = BuildTargetVelocity(ReferenceUp);
	const float Rate = bBraking ? EffectiveStats.BrakeStrength : GetAccelerationRate();
	Velocity = FMath::VInterpConstantTo(Velocity, TargetVelocity, DeltaTime, FMath::Max(1.0f, Rate));
	ApplyPlanetGravity(DeltaTime);
	ApplySurfaceClearanceProtection();

	FHitResult Hit;
	MoveWithCollisionSweep(Velocity * DeltaTime, UpdateRotation(DeltaTime, ReferenceUp), Hit);
	if (Hit.IsValidBlockingHit())
	{
		Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal);
	}
}

void UJTSSpacecraftFlightMovementComponent::RefreshSurfaceProximity(float DeltaTime)
{
	APawn* const OwningPawn = GetPawnOwner();
	AJTSPlanetAnchor* const Planet = TargetPlanet.Get();
	if (!bUsePlanetSurfaceFlightFrame || !IsValid(OwningPawn) || !IsValid(Planet))
	{
		bHasSurfaceProximity = false;
		SurfaceFlightAssistAlpha = 0.0f;
		return;
	}

	SurfaceProximityProbeElapsed += FMath::Max(0.0f, DeltaTime);
	const float ProbeInterval = FMath::Max(0.0f, SurfaceProximityProbeInterval);
	const FVector CraftLocation = OwningPawn->GetActorLocation();
	const float ProbeDistance = FMath::Max(1.0f, SurfaceProximityProbeDistance);
	const bool bMovedBeyondCachedSample = FVector::DistSquared(CraftLocation, CachedSurfaceProbeLocation)
		>= FMath::Square(ProbeDistance);
	if (bHasSurfaceProximity
		&& !bMovedBeyondCachedSample
		&& ProbeInterval > 0.0f
		&& SurfaceProximityProbeElapsed < ProbeInterval)
	{
		return;
	}
	SurfaceProximityProbeElapsed = 0.0f;

	FJTSPlanetSurfaceHit SurfaceHit;
	if (!Planet->TraceToSurface(CraftLocation, SurfaceHit) || !SurfaceHit.bBlockingHit)
	{
		bHasSurfaceProximity = false;
		SurfaceFlightAssistAlpha = 0.0f;
		return;
	}

	const FVector RadialUp = Planet->GetRadialUpVector(CraftLocation).GetSafeNormal();
	const FVector SurfaceNormal = SurfaceHit.ImpactNormal.GetSafeNormal();
	if (RadialUp.IsNearlyZero() || SurfaceNormal.IsNearlyZero())
	{
		bHasSurfaceProximity = false;
		SurfaceFlightAssistAlpha = 0.0f;
		return;
	}

	const FVector SurfaceSeparation = CraftLocation - SurfaceHit.ImpactPoint;
	CachedSurfaceAltitude = FVector::DotProduct(SurfaceSeparation, RadialUp);
	CachedSurfaceDockingHeight = FVector::DotProduct(SurfaceSeparation, SurfaceNormal);
	CachedSurfaceNormal = SurfaceNormal;
	CachedSurfaceProbeLocation = CraftLocation;
	bHasSurfaceProximity = FMath::IsFinite(CachedSurfaceAltitude)
		&& FMath::IsFinite(CachedSurfaceDockingHeight);
	if (!bHasSurfaceProximity)
	{
		SurfaceFlightAssistAlpha = 0.0f;
		return;
	}

	const float TransitionAltitude = FMath::Max(0.0f, Planet->GetTakeoffTransitionAltitude());
	if (TransitionAltitude <= KINDA_SMALL_NUMBER || CachedSurfaceAltitude >= TransitionAltitude)
	{
		SurfaceFlightAssistAlpha = 0.0f;
		return;
	}

	const float FullStrengthAltitude = TransitionAltitude
		* FMath::Clamp(SurfaceAssistFullStrengthAltitudeRatio, 0.0f, 0.95f);
	const float SafeAltitude = FMath::Max(0.0f, CachedSurfaceAltitude);
	SurfaceFlightAssistAlpha = SafeAltitude <= FullStrengthAltitude
		? 1.0f
		: 1.0f - FMath::SmoothStep(FullStrengthAltitude, TransitionAltitude, SafeAltitude);
}

void UJTSSpacecraftFlightMovementComponent::UpdateReferenceFrame()
{
	const APawn* const OwningPawn = GetPawnOwner();
	if (!bInertialReferenceUpInitialized && IsValid(OwningPawn))
	{
		const FVector ActorUp = OwningPawn->GetActorUpVector().GetSafeNormal();
		InertialReferenceUp = ActorUp.IsNearlyZero() ? FVector::UpVector : ActorUp;
		bInertialReferenceUpInitialized = true;
	}

	const AJTSPlanetAnchor* const Planet = TargetPlanet.Get();
	if (!IsValid(OwningPawn) || !IsValid(Planet) || GetSurfaceFlightAssistAlpha() <= KINDA_SMALL_NUMBER)
	{
		CurrentReferenceUp = InertialReferenceUp.GetSafeNormal();
		if (CurrentReferenceUp.IsNearlyZero())
		{
			CurrentReferenceUp = FVector::UpVector;
		}
		return;
	}

	const FVector RadialUp = Planet->GetRadialUpVector(OwningPawn->GetActorLocation()).GetSafeNormal();
	if (RadialUp.IsNearlyZero())
	{
		return;
	}

	const float AssistAlpha = GetSurfaceFlightAssistAlpha();
	CurrentReferenceUp = BlendUnitDirections(InertialReferenceUp, RadialUp, AssistAlpha);
}

void UJTSSpacecraftFlightMovementComponent::ApplyPlanetGravity(float DeltaTime)
{
	if (PlanetGravityScale <= 0.0f || DeltaTime <= 0.0f)
	{
		return;
	}

	AJTSPlanetAnchor* const Planet = TargetPlanet.Get();
	APawn* const OwningPawn = GetPawnOwner();
	if (!IsValid(Planet) || !IsValid(OwningPawn)
		|| !Planet->IsGravityEnabled()
		|| !Planet->IsWithinGravityInfluence(OwningPawn->GetActorLocation()))
	{
		return;
	}

	const FVector GravityDirection = Planet->GetGravityDirection(OwningPawn->GetActorLocation());
	if (GravityDirection.IsNearlyZero())
	{
		return;
	}

	Velocity += GravityDirection * Planet->GetGravityStrength() * PlanetGravityScale * DeltaTime;
}

void UJTSSpacecraftFlightMovementComponent::ApplySurfaceClearanceProtection()
{
	const APawn* const OwningPawn = GetPawnOwner();
	const AJTSPlanetAnchor* const Planet = TargetPlanet.Get();
	const AJTSSpacecraftActor* const Spacecraft = Cast<AJTSSpacecraftActor>(OwningPawn);
	if (!bUsePlanetSurfaceFlightFrame || !bHasSurfaceProximity || !IsValid(Planet)
		|| !IsValid(Spacecraft) || CachedSurfaceAltitude > Planet->GetTakeoffTransitionAltitude())
	{
		return;
	}

	const FVector SurfaceUp = CachedSurfaceNormal.GetSafeNormal();
	if (SurfaceUp.IsNearlyZero())
	{
		return;
	}

	const float HullClearance = Spacecraft->GetLandingCollisionClearanceForRotation(
		UpdatedComponent->GetComponentQuat(),
		SurfaceUp);
	const float RequiredClearance = FMath::Max(0.0f, HullClearance + SurfaceClearanceSafetyMargin);
	const float AvailableClearance = CachedSurfaceDockingHeight - RequiredClearance;
	const float LookAheadTime = FMath::Max(0.1f, SurfaceClearanceLookAheadTime);
	float MinimumNormalSpeed = -FMath::Max(0.0f, AvailableClearance) / LookAheadTime;
	if (AvailableClearance < 0.0f)
	{
		MinimumNormalSpeed = FMath::Min(
			FMath::Max(0.0f, SurfaceClearanceRecoverySpeed),
			-AvailableClearance / LookAheadTime);
	}

	const float CurrentNormalSpeed = FVector::DotProduct(Velocity, SurfaceUp);
	if (CurrentNormalSpeed < MinimumNormalSpeed)
	{
		// Only remove the unsafe component. Tangential control and intentional upward thrust remain
		// untouched, so this behaves like a proximity envelope rather than an invisible rail.
		Velocity += SurfaceUp * (MinimumNormalSpeed - CurrentNormalSpeed);
	}
}

FVector UJTSSpacecraftFlightMovementComponent::GetReferenceUp() const
{
	const FVector SafeReferenceUp = CurrentReferenceUp.GetSafeNormal();
	return SafeReferenceUp.IsNearlyZero() ? FVector::UpVector : SafeReferenceUp;
}

void UJTSSpacecraftFlightMovementComponent::GetHullBasis(
	const FVector& ReferenceUp,
	FVector& OutForward,
	FVector& OutRight) const
{
	const APawn* const OwningPawn = GetPawnOwner();
	OutForward = IsValid(OwningPawn) ? OwningPawn->GetActorForwardVector().GetSafeNormal() : FVector::ForwardVector;
	if (OutForward.IsNearlyZero())
	{
		OutForward = FVector::ForwardVector;
	}

	FVector PlanarForward = FVector::VectorPlaneProject(OutForward, ReferenceUp).GetSafeNormal();
	if (PlanarForward.IsNearlyZero())
	{
		FVector FallbackRight;
		ReferenceUp.FindBestAxisVectors(PlanarForward, FallbackRight);
	}

	OutRight = FVector::CrossProduct(ReferenceUp, PlanarForward).GetSafeNormal();
	if (OutRight.IsNearlyZero())
	{
		FVector FallbackForward;
		ReferenceUp.FindBestAxisVectors(FallbackForward, OutRight);
	}
}

FQuat UJTSSpacecraftFlightMovementComponent::UpdateRotation(float DeltaTime, const FVector& ReferenceUp)
{
	if (!IsValid(UpdatedComponent))
	{
		return FQuat::Identity;
	}

	const FQuat CurrentRotation = UpdatedComponent->GetComponentQuat();
	const FQuat FreeFlightRotation = BuildFreeFlightDesiredRotation(CurrentRotation, ReferenceUp);
	const float AssistAlpha = GetSurfaceFlightAssistAlpha();
	// Surface leveling would tilt the deck while S is spinning the nose inside that deck's plane.
	if (AssistAlpha <= KINDA_SMALL_NUMBER || (bTurnAround && bTurnAroundLatched))
	{
		return InterpolateTowardRotation(DeltaTime, CurrentRotation, FreeFlightRotation);
	}

	const FQuat SurfaceFlightRotation = BuildSurfaceFlightDesiredRotation(
		CurrentRotation,
		ReferenceUp,
		AssistAlpha);
	const FQuat DesiredRotation = FQuat::Slerp(
		FreeFlightRotation,
		SurfaceFlightRotation,
		AssistAlpha).GetNormalized();
	return InterpolateTowardRotation(DeltaTime, CurrentRotation, DesiredRotation);
}

FQuat UJTSSpacecraftFlightMovementComponent::BuildSurfaceFlightDesiredRotation(
	const FQuat& CurrentRotation,
	const FVector& SurfaceUp,
	float AssistAlpha) const
{
	check(IsValid(UpdatedComponent));
	// The hull holds its own heading. Near terrain, a nose-down command is limited to the dive
	// envelope; releasing the pitch key levels the nose back onto the local horizon.
	const FVector HullForward = UpdatedComponent->GetForwardVector().GetSafeNormal();
	FVector DesiredForward = FVector::VectorPlaneProject(HullForward, SurfaceUp).GetSafeNormal();
	const float PitchDeadZone = FMath::Clamp(MovementDeadZone, 0.0f, 1.0f);
	if (SteeringInput.Y > PitchDeadZone)
	{
		DesiredForward = ConstrainForwardToSurfaceEnvelope(HullForward, SurfaceUp, 0.0f);
	}
	else if (SteeringInput.Y < -PitchDeadZone)
	{
		DesiredForward = ConstrainForwardToSurfaceEnvelope(HullForward, SurfaceUp, AssistAlpha);
	}
	if (DesiredForward.IsNearlyZero())
	{
		return CurrentRotation;
	}

	const FVector TangentForward = FVector::VectorPlaneProject(DesiredForward, SurfaceUp).GetSafeNormal();
	const FVector SurfaceRight = FVector::CrossProduct(SurfaceUp, TangentForward).GetSafeNormal();
	if (SurfaceRight.IsNearlyZero())
	{
		return CurrentRotation;
	}

	// X/Y construction preserves the requested positive pitch while keeping roll referenced to the
	// planet. MakeFromXZ would silently flatten DesiredForward and recreate the old nose-up lock.
	return FRotationMatrix::MakeFromXY(DesiredForward.GetSafeNormal(), SurfaceRight).ToQuat();
}

FQuat UJTSSpacecraftFlightMovementComponent::BuildFreeFlightDesiredRotation(
	const FQuat& CurrentRotation,
	const FVector& ReferenceUp) const
{
	check(IsValid(UpdatedComponent));
	const FVector SafeUp = ReferenceUp.GetSafeNormal();
	const FVector CurrentForward = UpdatedComponent->GetForwardVector().GetSafeNormal();
	if (SafeUp.IsNearlyZero() || CurrentForward.IsNearlyZero())
	{
		return CurrentRotation;
	}

	FVector PlanarForward = FVector::VectorPlaneProject(CurrentForward, SafeUp).GetSafeNormal();
	if (PlanarForward.IsNearlyZero())
	{
		FVector FallbackRight;
		SafeUp.FindBestAxisVectors(PlanarForward, FallbackRight);
	}

	const float DeadZone = FMath::Clamp(MovementDeadZone, 0.0f, 1.0f);
	const bool bYawing = FMath::Abs(SteeringInput.X) > DeadZone;
	const bool bPitching = FMath::Abs(SteeringInput.Y) > DeadZone;
	const bool bTurningAround = bTurnAround && bTurnAroundLatched
		&& !TurnAroundTargetForward.IsNearlyZero()
		&& !TurnAroundDeckUp.IsNearlyZero();
	if (!bYawing && !bPitching && !bTurningAround)
	{
		return CurrentRotation;
	}

	// S keeps the deck where it is and only swaps nose with tail inside that plane.
	if (bTurningAround)
	{
		const FVector DeckUp = TurnAroundDeckUp.GetSafeNormal();
		FVector DesiredForward = FVector::VectorPlaneProject(TurnAroundTargetForward, DeckUp).GetSafeNormal();
		if (DesiredForward.IsNearlyZero() || FVector::CrossProduct(DesiredForward, DeckUp).IsNearlyZero())
		{
			return CurrentRotation;
		}
		return FRotationMatrix::MakeFromXZ(DesiredForward, DeckUp).ToQuat();
	}

	const float Step = FMath::DegreesToRadians(12.0f);
	FVector DesiredPlanar = PlanarForward;
	if (bYawing)
	{
		DesiredPlanar = FQuat(SafeUp, SteeringInput.X * Step).RotateVector(PlanarForward).GetSafeNormal();
	}

	const float VerticalComponent = FVector::DotProduct(CurrentForward, SafeUp);
	const float TangentComponent = FVector::VectorPlaneProject(CurrentForward, SafeUp).Size();
	float CurrentPitch = FMath::Atan2(VerticalComponent, TangentComponent);
	if (bPitching)
	{
		CurrentPitch = FMath::Clamp(CurrentPitch + SteeringInput.Y * Step, -1.5f, 1.5f);
	}

	const FVector DesiredForward = (
		DesiredPlanar * FMath::Cos(CurrentPitch) + SafeUp * FMath::Sin(CurrentPitch)).GetSafeNormal();
	if (DesiredForward.IsNearlyZero())
	{
		return CurrentRotation;
	}

	const FQuat Alignment = FQuat::FindBetweenNormals(CurrentForward, DesiredForward);
	return (Alignment * CurrentRotation).GetNormalized();
}

FVector UJTSSpacecraftFlightMovementComponent::ConstrainForwardToSurfaceEnvelope(
	const FVector& DesiredForward,
	const FVector& SurfaceUp,
	float AssistAlpha) const
{
	const FVector SafeUp = SurfaceUp.GetSafeNormal();
	const FVector SafeForward = DesiredForward.GetSafeNormal();
	if (SafeUp.IsNearlyZero() || SafeForward.IsNearlyZero())
	{
		return SafeForward;
	}

	FVector TangentForward = FVector::VectorPlaneProject(SafeForward, SafeUp).GetSafeNormal();
	if (TangentForward.IsNearlyZero() && IsValid(UpdatedComponent))
	{
		TangentForward = FVector::VectorPlaneProject(UpdatedComponent->GetForwardVector(), SafeUp).GetSafeNormal();
	}
	if (TangentForward.IsNearlyZero())
	{
		FVector FallbackRight;
		SafeUp.FindBestAxisVectors(TangentForward, FallbackRight);
	}

	const float VerticalComponent = FVector::DotProduct(SafeForward, SafeUp);
	const float TangentComponent = FVector::VectorPlaneProject(SafeForward, SafeUp).Size();
	const float RequestedPitch = FMath::Atan2(VerticalComponent, TangentComponent);
	if (RequestedPitch >= 0.0f)
	{
		// Departure is asymmetric by design: looking up and pressing W may always point the nose
		// away from terrain, even while the low-altitude safety envelope is fully active.
		return SafeForward;
	}

	const float SafeAssistAlpha = FMath::Clamp(AssistAlpha, 0.0f, 1.0f);
	// Dive authority relaxes more slowly than visual/horizon stabilization. Even in the middle of
	// the blend band the terrain-facing command remains conservative, then opens rapidly near space.
	const float DiveProtectionAlpha = 1.0f - FMath::Square(1.0f - SafeAssistAlpha);
	const float MaximumDiveAngle = FMath::DegreesToRadians(FMath::Lerp(
		89.0f,
		FMath::Clamp(MaximumSurfaceDiveAngleDegrees, 0.0f, 89.0f),
		DiveProtectionAlpha));
	const float ConstrainedPitch = FMath::Max(RequestedPitch, -MaximumDiveAngle);
	return (TangentForward * FMath::Cos(ConstrainedPitch) + SafeUp * FMath::Sin(ConstrainedPitch)).GetSafeNormal();
}

FQuat UJTSSpacecraftFlightMovementComponent::InterpolateTowardRotation(
	float DeltaTime,
	const FQuat& CurrentRotation,
	const FQuat& DesiredRotation)
{
	const float TurnMultiplier = bBoosting ? FMath::Max(0.0f, EffectiveStats.BoostTurnMultiplier) : 1.0f;
	const float MaximumTurnSpeedRadians = FMath::DegreesToRadians(
		FMath::Max(0.0f, EffectiveStats.FacingTurnRate) * TurnMultiplier);
	const float AngularDistance = DesiredRotation.AngularDistance(CurrentRotation);
	if (AngularDistance <= KINDA_SMALL_NUMBER)
	{
		CurrentFacingTurnSpeedRadians = 0.0f;
		return DesiredRotation;
	}
	if (MaximumTurnSpeedRadians <= 0.0f)
	{
		CurrentFacingTurnSpeedRadians = 0.0f;
		return CurrentRotation;
	}

	const float TurnAccelerationRadians = FMath::DegreesToRadians(
		FMath::Max(0.0f, EffectiveStats.FacingTurnAcceleration));
	if (TurnAccelerationRadians <= 0.0f)
	{
		return FMath::QInterpConstantTo(
			CurrentRotation,
			DesiredRotation,
			FMath::Max(0.0f, DeltaTime),
			MaximumTurnSpeedRadians).GetNormalized();
	}

	const float BrakingLimitedSpeed = FMath::Sqrt(2.0f * TurnAccelerationRadians * AngularDistance);
	const float TargetTurnSpeed = FMath::Min(MaximumTurnSpeedRadians, BrakingLimitedSpeed);
	CurrentFacingTurnSpeedRadians = FMath::FInterpConstantTo(
		CurrentFacingTurnSpeedRadians,
		TargetTurnSpeed,
		FMath::Max(0.0f, DeltaTime),
		TurnAccelerationRadians);
	const float TurnStep = FMath::Min(AngularDistance, CurrentFacingTurnSpeedRadians * FMath::Max(0.0f, DeltaTime));
	return FQuat::Slerp(CurrentRotation, DesiredRotation, TurnStep / AngularDistance).GetNormalized();
}

void UJTSSpacecraftFlightMovementComponent::SubmitExteriorAltitude(float DeltaTime)
{
	AJTSSpacecraftActor* const Spacecraft = Cast<AJTSSpacecraftActor>(GetPawnOwner());
	if (!IsValid(Spacecraft))
	{
		return;
	}

	if (AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
	{
		// The persistent world, not a retained departure planet, owns travel-state transitions and
		// target handoff. This runs after the authoritative movement step so all players receive the
		// same departure/arrival decision.
		Manager->UpdateSpacecraftFlightState(Spacecraft);
		Manager->UpdateCelestialPresentation(Spacecraft);
	}
}

void UJTSSpacecraftFlightMovementComponent::CompleteAssistedLanding()
{
	bAssistedLanding = false;
	ClearInput();
	Velocity = FVector::ZeroVector;
	AssistedLandingClearance = 0.0f;
	AssistedLandingDescentSpeed = 0.0f;
	SetAssistedLandingPhase(EJTSSpacecraftLandingAssistPhase::Touchdown);
	OnAssistedLandingCompleted.Broadcast();
}

void UJTSSpacecraftFlightMovementComponent::FailAssistedLanding(EJTSLandingValidationFailure Failure)
{
	bAssistedLanding = false;
	ClearInput();
	Velocity = FVector::ZeroVector;
	AssistedLandingClearance = 0.0f;
	AssistedLandingDescentSpeed = 0.0f;
	SetAssistedLandingPhase(EJTSSpacecraftLandingAssistPhase::None);
	OnAssistedLandingFailed.Broadcast(Failure);
}

void UJTSSpacecraftFlightMovementComponent::SetAssistedLandingPhase(EJTSSpacecraftLandingAssistPhase NewPhase)
{
	if (AssistedLandingPhase == NewPhase)
	{
		return;
	}

	AssistedLandingPhase = NewPhase;
	OnAssistedLandingPhaseChanged.Broadcast(AssistedLandingPhase);
}

bool UJTSSpacecraftFlightMovementComponent::MoveWithCollisionSweep(const FVector& Delta, const FQuat& NewRotation, FHitResult& OutHit)
{
	OutHit = FHitResult();
	if (!IsValid(UpdatedComponent))
	{
		return false;
	}

	APawn* const OwningPawn = GetPawnOwner();
	UWorld* const World = GetWorld();
	UBoxComponent* const FlightCollision = IsValid(OwningPawn) ? OwningPawn->FindComponentByClass<UBoxComponent>() : nullptr;
	if (World == nullptr || FlightCollision == nullptr)
	{
		MoveUpdatedComponent(Delta, NewRotation, false, &OutHit, ETeleportType::None);
		return !OutHit.IsValidBlockingHit();
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JTSSpacecraftFlight), false, OwningPawn);
	QueryParams.AddIgnoredActor(OwningPawn);
	// The Blueprint may offset and rotate the hull relative to the root.
	const FTransform HullRelative = FlightCollision->GetComponentTransform().GetRelativeTransform(OwningPawn->GetActorTransform());
	const FTransform TargetRoot(NewRotation, UpdatedComponent->GetComponentLocation(), OwningPawn->GetActorScale3D());
	const FTransform TargetHull = HullRelative * TargetRoot;
	const FVector Start = TargetHull.GetLocation();
	const FVector End = Start + Delta;
	const FCollisionShape HullShape = FCollisionShape::MakeBox(FlightCollision->GetScaledBoxExtent());
	const bool bTargetRotationBlocked = World->OverlapBlockingTestByChannel(
		Start,
		TargetHull.GetRotation(),
		ECC_Visibility,
		HullShape,
		QueryParams);
	if (bTargetRotationBlocked && Delta.IsNearlyZero())
	{
		OutHit.bBlockingHit = true;
		OutHit.bStartPenetrating = true;
		return false;
	}
	if (Delta.IsNearlyZero())
	{
		MoveUpdatedComponent(Delta, NewRotation, false, nullptr, ETeleportType::None);
		return true;
	}
	if (bTargetRotationBlocked)
	{
		// A long hull may need altitude before a requested pitch/roll is physically possible. Keep
		// its current attitude while applying the safe translation (normally the clearance recovery
		// lift), then retry the rotation on a later frame instead of rotating into terrain.
		const FQuat CurrentRotation = UpdatedComponent->GetComponentQuat();
		const FTransform CurrentRoot(
			CurrentRotation,
			UpdatedComponent->GetComponentLocation(),
			OwningPawn->GetActorScale3D());
		const FTransform CurrentHull = HullRelative * CurrentRoot;
		FHitResult TranslationHit;
		const bool bTranslationHit = World->SweepSingleByChannel(
			TranslationHit,
			CurrentHull.GetLocation(),
			CurrentHull.GetLocation() + Delta,
			CurrentHull.GetRotation(),
			ECC_Visibility,
			HullShape,
			QueryParams);
		const FVector AllowedTranslation = bTranslationHit
			? Delta * FMath::Clamp(TranslationHit.Time, 0.0f, 1.0f)
			: Delta;
		MoveUpdatedComponent(AllowedTranslation, CurrentRotation, false, nullptr, ETeleportType::None);
		if (bTranslationHit)
		{
			OutHit = TranslationHit;
		}
		return false;
	}
	const bool bHit = World->SweepSingleByChannel(
		OutHit,
		Start,
		End,
		TargetHull.GetRotation(),
		ECC_Visibility,
		HullShape,
		QueryParams);
	const FVector AllowedDelta = bHit ? Delta * FMath::Clamp(OutHit.Time, 0.0f, 1.0f) : Delta;
	MoveUpdatedComponent(AllowedDelta, NewRotation, false, nullptr, ETeleportType::None);
	return !bHit;
}

FVector UJTSSpacecraftFlightMovementComponent::BuildTargetVelocity(const FVector& ReferenceUp) const
{
	if (bBraking)
	{
		return FVector::ZeroVector;
	}

	FVector HullForward;
	FVector UnusedRight;
	GetHullBasis(ReferenceUp, HullForward, UnusedRight);
	const float DeadZone = FMath::Clamp(MovementDeadZone, 0.0f, 1.0f);
	const float ForwardThrottle = FMath::Clamp(MoveInput.Y, 0.0f, 1.0f);
	const float AssistAlpha = GetSurfaceFlightAssistAlpha();
	FVector HullRelativeDirection = FVector::ZeroVector;
	if (ForwardThrottle > DeadZone)
	{
		HullRelativeDirection = ConstrainForwardToSurfaceEnvelope(HullForward, ReferenceUp, AssistAlpha);
	}

	const float MoveSpeed = EffectiveStats.MaxMoveSpeed
		* (bBoosting && ForwardThrottle > DeadZone ? FMath::Max(1.0f, EffectiveStats.BoostMultiplier) : 1.0f);
	return HullRelativeDirection * MoveSpeed * ForwardThrottle
		+ ReferenceUp * VerticalInput * EffectiveStats.LiftSpeed;
}

float UJTSSpacecraftFlightMovementComponent::GetAccelerationRate() const
{
	const bool bHasMoveInput = !MoveInput.IsNearlyZero() || !FMath::IsNearlyZero(VerticalInput);
	return bHasMoveInput && bBoosting
		? EffectiveStats.Acceleration * FMath::Max(0.0f, EffectiveStats.BoostAccelerationMultiplier)
		: (bHasMoveInput ? EffectiveStats.Acceleration : EffectiveStats.Deceleration);
}

void UJTSSpacecraftFlightMovementComponent::SetBoostState(bool bNewBoosting)
{
	if (bBoosting == bNewBoosting)
	{
		return;
	}

	bBoosting = bNewBoosting;
	OnBoostStateChanged.Broadcast(bBoosting);
}
