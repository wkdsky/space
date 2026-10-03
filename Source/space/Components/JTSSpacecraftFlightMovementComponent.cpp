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
		// Planet scale and surface-content visibility are local presentation. Clients
		// must observe the same mesh shrink before their HUD/field enters cruise.
		if (AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
		{
			Manager->UpdateCelestialPresentation(Cast<AJTSSpacecraftActor>(GetPawnOwner()));
		}
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

void UJTSSpacecraftFlightMovementComponent::SetRollInput(float Value)
{
	if (!bAssistedLanding)
	{
		RollInput = FMath::Clamp(Value, -1.0f, 1.0f);
	}
}

void UJTSSpacecraftFlightMovementComponent::SetFlightAssistEnabled(bool bEnabled)
{
	bFlightAssistEnabled = bEnabled;
}

void UJTSSpacecraftFlightMovementComponent::SetSpeedLimit(float NewLimit)
{
	SpeedLimit = FMath::Clamp(NewLimit, 0.1f, 1.0f);
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
	RollInput = 0.0f;
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
	ResetSurfaceNoseGuard();
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
	ResetSurfaceNoseGuard();
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
			// Latch the deck the ship is already flying in. T yaws inside that plane.
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
	if (bBraking || bFlightAssistEnabled)
	{
		Velocity = FMath::VInterpConstantTo(Velocity, TargetVelocity, DeltaTime, FMath::Max(1.0f, Rate));
	}
	else if (!TargetVelocity.IsNearlyZero())
	{
		const float MaximumSpeed = FMath::Max3(EffectiveStats.MaxMoveSpeed
			* (bBoosting ? FMath::Max(1.0f, EffectiveStats.BoostMultiplier) : 1.0f),
			EffectiveStats.LiftSpeed, EffectiveStats.StrafeSpeed) * SpeedLimit;
		const float ExistingSpeed = Velocity.Size();
		Velocity += TargetVelocity.GetSafeNormal() * FMath::Max(0.0f, Rate) * DeltaTime
			* FMath::Clamp(TargetVelocity.Size() / FMath::Max(1.0f, MaximumSpeed), 0.0f, 1.0f);
		Velocity = Velocity.GetClampedToMaxSize(FMath::Max(ExistingSpeed, MaximumSpeed));
	}
	ApplyPlanetGravity(DeltaTime);
	const FSurfaceAvoidance Avoidance = EvaluateSurfaceAvoidance(DeltaTime);
	ApplySurfaceClearanceProtection(DeltaTime, Avoidance);

	FHitResult Hit;
	MoveWithCollisionSweep(Velocity * DeltaTime, UpdateRotation(DeltaTime, Avoidance), Hit);
	if (Hit.IsValidBlockingHit())
	{
		Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal);
	}
}

void UJTSSpacecraftFlightMovementComponent::RefreshSurfaceProximity(float DeltaTime)
{
	APawn* const OwningPawn = GetPawnOwner();
	AJTSPlanetAnchor* const Planet = TargetPlanet.Get();
	if (!IsValid(OwningPawn) || !IsValid(Planet))
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
	if (RadialUp.IsNearlyZero())
	{
		bHasSurfaceProximity = false;
		SurfaceFlightAssistAlpha = 0.0f;
		return;
	}

	const FVector SurfaceSeparation = CraftLocation - SurfaceHit.ImpactPoint;
	CachedSurfaceAltitude = FVector::DotProduct(SurfaceSeparation, RadialUp);
	CachedSurfaceProbeLocation = CraftLocation;
	bHasSurfaceProximity = FMath::IsFinite(CachedSurfaceAltitude);
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

	// The planet binding now stays through the 18 km navigation handoff. Ease gravity
	// to zero before its influence edge so passing the old 14-15 km dial band cannot
	// change acceleration in a single frame.
	const float InfluenceRange = FMath::Max(1.0f, Planet->GetGravityInfluenceRange());
	const float Altitude = FMath::Max(0.0f, Planet->GetApproximateAltitude(OwningPawn->GetActorLocation()));
	const float InfluenceAlpha = 1.0f - FMath::SmoothStep(InfluenceRange * 0.5f, InfluenceRange, Altitude);
	Velocity += GravityDirection * Planet->GetGravityStrength() * PlanetGravityScale
		* InfluenceAlpha * DeltaTime;
}

UJTSSpacecraftFlightMovementComponent::FSurfaceAvoidance
UJTSSpacecraftFlightMovementComponent::EvaluateSurfaceAvoidance(float DeltaTime)
{
	FSurfaceAvoidance Result;
	const AJTSPlanetAnchor* const Planet = TargetPlanet.Get();
	const AJTSSpacecraftActor* const Spacecraft = Cast<AJTSSpacecraftActor>(GetPawnOwner());
	if (!bHasSurfaceProximity || !IsValid(Planet) || !IsValid(Spacecraft)
		|| !IsValid(UpdatedComponent))
	{
		ResetSurfaceNoseGuard();
		return Result;
	}

	const FVector CraftLocation = UpdatedComponent->GetComponentLocation();
	const FVector RadialUp = Planet->GetRadialUpVector(CraftLocation).GetSafeNormal();
	const FVector Forward = UpdatedComponent->GetForwardVector().GetSafeNormal();
	FVector Heading = FVector::VectorPlaneProject(Forward, RadialUp).GetSafeNormal();
	if (Heading.IsNearlyZero())
	{
		// A vertical nose has no projected forward vector. Preserve the deck's last horizontal
		// direction, then use travel direction if the ship is also rolled onto its side.
		Heading = FVector::VectorPlaneProject(
			Forward.Dot(RadialUp) < 0.0f ? UpdatedComponent->GetUpVector()
				: -UpdatedComponent->GetUpVector(), RadialUp).GetSafeNormal();
		if (Heading.IsNearlyZero())
		{
			Heading = FVector::VectorPlaneProject(Velocity, RadialUp).GetSafeNormal();
		}
	}
	if (RadialUp.IsNearlyZero() || Heading.IsNearlyZero())
	{
		ResetSurfaceNoseGuard();
		return Result;
	}
	Result.RadialUp = RadialUp;
	Result.Heading = Heading;

	const float RequiredClearance = Spacecraft->GetLandingCollisionClearanceForRotation(
		UpdatedComponent->GetComponentQuat(), RadialUp) + FMath::Max(0.0f, SurfaceClearanceSafetyMargin);
	const float GuardDistance = FMath::Max(0.0f, SurfaceNosePitchGuardDistance);
	const float LookAheadTime = FMath::Max(0.1f, SurfaceTerrainAvoidanceLookAheadTime);
	const float UpSpeed = FVector::DotProduct(Velocity, RadialUp);
	const float ForwardSpeed = FMath::Max(0.0f, FVector::DotProduct(Velocity, Heading));
	const bool bApproaching = ForwardSpeed > 20.0f || UpSpeed < -20.0f
		|| MoveInput.Y > MovementDeadZone || VerticalInput < -MovementDeadZone;
	const float CenterClearance = CachedSurfaceAltitude - RequiredClearance;
	float ClosestPredictedClearance = FMath::Min(CenterClearance,
		CenterClearance + UpSpeed * LookAheadTime);
	if (ClosestPredictedClearance <= GuardDistance)
	{
		Result.MinimumUpSpeed = (RequiredClearance - CachedSurfaceAltitude) / LookAheadTime;
	}

	const UBoxComponent* const Hull = Spacecraft->FindComponentByClass<UBoxComponent>();
	const float HullLength = IsValid(Hull) ? Hull->GetScaledBoxExtent().X * 2.0f : 0.0f;
	const float BowDistance = FMath::Max(
		FMath::Max(1.0f, SurfaceNosePitchTerrainSampleDistance), HullLength);
	// Bounds provide a conservative skip in open space without assuming the authored planet radius
	// equals the collision mesh. A distant mountain can still enter the prediction horizon.
	float OuterSurfaceRadius = 0.0f;
	TArray<UPrimitiveComponent*> SurfaceComponents;
	if (const AActor* const SurfaceActor = Planet->GetGameplaySurfaceActor(); IsValid(SurfaceActor))
	{
		SurfaceActor->GetComponents<UPrimitiveComponent>(SurfaceComponents);
	}
	else if (UPrimitiveComponent* const SurfaceComponent = Planet->GetGameplaySurfaceComponent();
		IsValid(SurfaceComponent))
	{
		SurfaceComponents.Add(SurfaceComponent);
	}
	for (const UPrimitiveComponent* const SurfaceComponent : SurfaceComponents)
	{
		if (IsValid(SurfaceComponent) && SurfaceComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			OuterSurfaceRadius = FMath::Max(OuterSurfaceRadius,
				FVector::Distance(Planet->GetPlanetCenter(), SurfaceComponent->Bounds.Origin)
					+ SurfaceComponent->Bounds.SphereRadius);
		}
	}
	if (OuterSurfaceRadius > 0.0f
		&& FVector::Distance(CraftLocation, Planet->GetPlanetCenter()) - OuterSurfaceRadius
			> RequiredClearance + GuardDistance + BowDistance + Velocity.Size() * LookAheadTime)
	{
		ResetSurfaceNoseGuard();
		return Result;
	}

	const FVector CachedUp = Planet->GetRadialUpVector(CachedSurfaceProbeLocation).GetSafeNormal();
	const FVector CenterSurfacePoint = CachedSurfaceProbeLocation - CachedUp * CachedSurfaceAltitude;
	FVector PreviousSurfacePoint = CenterSurfacePoint;
	float PreviousDistance = FVector::DotProduct(CachedSurfaceProbeLocation - CraftLocation, Heading);
	float HighestGroundPitch = -HALF_PI;
	bool bHasAheadSurface = false;
	for (int32 SampleIndex = 0; SampleIndex < 3; ++SampleIndex)
	{
		const float Fraction = static_cast<float>(SampleIndex) * 0.5f;
		const float TimeAhead = LookAheadTime * Fraction;
		const float DistanceAhead = BowDistance + ForwardSpeed * TimeAhead;
		if (SampleIndex > 0 && DistanceAhead - PreviousDistance < 1.0f)
		{
			continue;
		}
		const FVector SampleLocation = CraftLocation + Heading * DistanceAhead;
		FJTSPlanetSurfaceHit SampleHit;
		if (!Planet->TraceToSurface(SampleLocation, SampleHit) || !SampleHit.bBlockingHit)
		{
			continue;
		}
		bHasAheadSurface = true;
		const FVector SampleUp = Planet->GetRadialUpVector(SampleLocation).GetSafeNormal();
		const float SampleAltitude = FVector::DotProduct(
			SampleLocation - SampleHit.ImpactPoint, SampleUp);
		const float PredictedClearance = SampleAltitude + UpSpeed * TimeAhead - RequiredClearance;
		ClosestPredictedClearance = FMath::Min(ClosestPredictedClearance, PredictedClearance);
		if (PredictedClearance <= GuardDistance)
		{
			const float RecoveryTime = FMath::Max(0.2f, TimeAhead);
			Result.MinimumUpSpeed = FMath::Max(Result.MinimumUpSpeed,
				(RequiredClearance - SampleAltitude) / RecoveryTime);
		}

		const FVector GroundStep = SampleHit.ImpactPoint - PreviousSurfacePoint;
		const float ForwardSpan = FVector::DotProduct(GroundStep, Heading);
		if (ForwardSpan > (DistanceAhead - PreviousDistance) * 0.25f)
		{
			HighestGroundPitch = FMath::Max(HighestGroundPitch,
				FMath::Atan2(FVector::DotProduct(GroundStep, RadialUp), ForwardSpan));
		}
		PreviousSurfacePoint = SampleHit.ImpactPoint;
		PreviousDistance = DistanceAhead;
	}

	const bool bThreat = ClosestPredictedClearance <= GuardDistance;
	const bool bWasGuardActive = bSurfaceNoseGuardActive;
	if (bThreat && (CenterClearance <= GuardDistance || bApproaching))
	{
		bSurfaceNoseGuardActive = true;
		SurfaceNosePitchClearElapsed = 0.0f;
	}
	else if (bSurfaceNoseGuardActive)
	{
		const float ReleaseDistance = FMath::Max(0.0f, SurfaceNosePitchReleaseDistance);
		if (ClosestPredictedClearance > GuardDistance + 3.0f * ReleaseDistance)
		{
			// A genuine climb has cleared the envelope; only the narrow edge needs dwell.
			ResetSurfaceNoseGuard();
		}
		else if (ClosestPredictedClearance > GuardDistance + ReleaseDistance)
		{
			SurfaceNosePitchClearElapsed += FMath::Max(0.0f, DeltaTime);
			if (SurfaceNosePitchClearElapsed >= FMath::Max(0.0f, SurfaceNosePitchReleaseDelay))
			{
				ResetSurfaceNoseGuard();
			}
		}
		else
		{
			SurfaceNosePitchClearElapsed = 0.0f;
		}
	}
	Result.bConstrainPitch = bSurfaceNoseGuardActive;
	Result.bAssistVelocity = bThreat && bApproaching;
	if (Result.bConstrainPitch)
	{
		const float GroundPitch = bHasAheadSurface && HighestGroundPitch > -HALF_PI
			? HighestGroundPitch : 0.0f;
		const float SafePitch = FMath::Min(FMath::DegreesToRadians(80.0f),
			GroundPitch + FMath::DegreesToRadians(FMath::Clamp(
				SurfaceTerrainAvoidancePitchMarginDegrees, 0.0f, 20.0f)));
		if (!bWasGuardActive)
		{
			SmoothedSurfaceMinimumPitch = SafePitch;
		}
		else
		{
			const float ResponseDegrees = SafePitch > SmoothedSurfaceMinimumPitch
				? (ClosestPredictedClearance <= 0.0f
					? 360.0f : SurfaceTerrainPitchAttackDegreesPerSecond)
				: SurfaceTerrainPitchRelaxDegreesPerSecond;
			SmoothedSurfaceMinimumPitch = FMath::FInterpConstantTo(
				SmoothedSurfaceMinimumPitch, SafePitch, FMath::Max(0.0f, DeltaTime),
				FMath::DegreesToRadians(FMath::Max(1.0f, ResponseDegrees)));
		}
		Result.MinimumPitch = SmoothedSurfaceMinimumPitch;
	}
	const float CurrentPitch = FMath::Atan2(
		FVector::DotProduct(Forward, RadialUp), FVector::DotProduct(Forward, Heading));
	Result.bRaiseNose = Result.bConstrainPitch && bHasAheadSurface && !bTurnAround
		&& CurrentPitch < Result.MinimumPitch - FMath::DegreesToRadians(1.0f);
	return Result;
}

void UJTSSpacecraftFlightMovementComponent::ResetSurfaceNoseGuard()
{
	bSurfaceNoseGuardActive = false;
	SurfaceNosePitchClearElapsed = 0.0f;
	SmoothedSurfaceMinimumPitch = 0.0f;
}

void UJTSSpacecraftFlightMovementComponent::ApplySurfaceClearanceProtection(
	float DeltaTime, const FSurfaceAvoidance& Avoidance)
{
	const APawn* const OwningPawn = GetPawnOwner();
	const AJTSPlanetAnchor* const Planet = TargetPlanet.Get();
	const AJTSSpacecraftActor* const Spacecraft = Cast<AJTSSpacecraftActor>(OwningPawn);
	if (!bHasSurfaceProximity || !IsValid(Planet) || !IsValid(Spacecraft))
	{
		return;
	}

	// Terrain normals can swing sharply across craters and ridges. The radial direction is stable;
	// the real mesh supplies the separation and the collision sweep remains authoritative.
	const FVector SurfaceUp = Planet->GetRadialUpVector(OwningPawn->GetActorLocation()).GetSafeNormal();
	if (SurfaceUp.IsNearlyZero())
	{
		return;
	}

	if (CachedSurfaceAltitude <= Planet->GetTakeoffTransitionAltitude())
	{
		const float HullClearance = Spacecraft->GetLandingCollisionClearanceForRotation(
			UpdatedComponent->GetComponentQuat(), SurfaceUp);
		const float RequiredClearance = FMath::Max(0.0f, HullClearance + SurfaceClearanceSafetyMargin);
		const float AvailableClearance = CachedSurfaceAltitude - RequiredClearance;
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
			Velocity += SurfaceUp * (MinimumNormalSpeed - CurrentNormalSpeed);
		}
	}

	if (!Avoidance.bAssistVelocity || Avoidance.MinimumUpSpeed <= -BIG_NUMBER * 0.5f)
	{
		return;
	}
	const float MaximumClimbSpeed = FMath::Max(1.0f, EffectiveStats.LiftSpeed * SpeedLimit);
	const float DesiredUpSpeed = FMath::Min(Avoidance.MinimumUpSpeed, MaximumClimbSpeed);
	const float CurrentUpSpeed = FVector::DotProduct(Velocity, SurfaceUp);
	if (DesiredUpSpeed > CurrentUpSpeed)
	{
		const float MaximumUpSpeedChange = FMath::Max(0.0f, EffectiveStats.Acceleration) * DeltaTime;
		Velocity += SurfaceUp * FMath::Min(DesiredUpSpeed - CurrentUpSpeed, MaximumUpSpeedChange);
	}
	if (Avoidance.MinimumUpSpeed > MaximumClimbSpeed)
	{
		const float ForwardSpeed = FMath::Max(0.0f, FVector::DotProduct(Velocity, Avoidance.Heading));
		const float SafeForwardSpeed = ForwardSpeed * MaximumClimbSpeed / Avoidance.MinimumUpSpeed;
		const float SpeedReduction = FMath::Min(ForwardSpeed - SafeForwardSpeed,
			FMath::Max(0.0f, EffectiveStats.BrakeStrength) * DeltaTime);
		Velocity -= Avoidance.Heading * SpeedReduction;
	}
}

FVector UJTSSpacecraftFlightMovementComponent::GetReferenceUp() const
{
	const FVector SafeReferenceUp = CurrentReferenceUp.GetSafeNormal();
	return SafeReferenceUp.IsNearlyZero() ? FVector::UpVector : SafeReferenceUp;
}

FQuat UJTSSpacecraftFlightMovementComponent::UpdateRotation(
	float DeltaTime, const FSurfaceAvoidance& Avoidance)
{
	if (!IsValid(UpdatedComponent))
	{
		return FQuat::Identity;
	}

	const FQuat CurrentRotation = UpdatedComponent->GetComponentQuat();
	FQuat DesiredRotation = BuildFreeFlightDesiredRotation(CurrentRotation, Avoidance);
	if (Avoidance.bRaiseNose)
	{
		const FVector DesiredForward = DesiredRotation.GetForwardVector();
		const float DesiredPitch = FMath::Atan2(
			FVector::DotProduct(DesiredForward, Avoidance.RadialUp),
			FVector::DotProduct(DesiredForward, Avoidance.Heading));
		const float SafePitch = Avoidance.MinimumPitch;
		if (DesiredPitch < SafePitch)
		{
			// Raise the complete hull around the local horizontal axis, leaving yaw and roll intact.
			// InterpolateTowardRotation enforces the normal angular acceleration and turn-rate limits.
			const FVector PitchAxis = FVector::CrossProduct(
				Avoidance.RadialUp, Avoidance.Heading).GetSafeNormal();
			DesiredRotation = (FQuat(PitchAxis, DesiredPitch - SafePitch) * DesiredRotation).GetNormalized();
		}
	}
	return InterpolateTowardRotation(DeltaTime, CurrentRotation, DesiredRotation);
}

FQuat UJTSSpacecraftFlightMovementComponent::BuildFreeFlightDesiredRotation(
	const FQuat& CurrentRotation, const FSurfaceAvoidance& Avoidance) const
{
	const float DeadZone = FMath::Clamp(MovementDeadZone, 0.0f, 1.0f);
	const bool bYawing = FMath::Abs(SteeringInput.X) > DeadZone;
	const bool bPitching = FMath::Abs(SteeringInput.Y) > DeadZone;
	const bool bRolling = FMath::Abs(RollInput) > DeadZone;
	const bool bTurningAround = bTurnAround && bTurnAroundLatched
		&& !TurnAroundTargetForward.IsNearlyZero()
		&& !TurnAroundDeckUp.IsNearlyZero();
	if (!bYawing && !bPitching && !bRolling && !bTurningAround)
	{
		return CurrentRotation;
	}

	// Turnaround keeps the deck where it is and swaps nose with tail inside that plane.
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

	// Increment around the hull's own axes. No inertial "up" is imposed in free space, so
	// inverted flight and continuous loops remain possible without a pole singularity.
	const float Step = FMath::DegreesToRadians(12.0f);
	const FQuat Yaw(CurrentRotation.GetUpVector(), bYawing ? SteeringInput.X * Step : 0.0f);
	const float RequestedPitch = bPitching ? -SteeringInput.Y * Step : 0.0f;
	const FQuat Pitch(CurrentRotation.GetRightVector(),
		ConstrainDownwardPitchNearSurface(CurrentRotation, RequestedPitch, Avoidance));
	const FQuat Roll(CurrentRotation.GetForwardVector(), bRolling ? RollInput * Step : 0.0f);
	return (Roll * Pitch * Yaw * CurrentRotation).GetNormalized();
}

float UJTSSpacecraftFlightMovementComponent::ConstrainDownwardPitchNearSurface(
	const FQuat& CurrentRotation, float PitchRadians, const FSurfaceAvoidance& Avoidance) const
{
	if (FMath::IsNearlyZero(PitchRadians) || !Avoidance.bConstrainPitch)
	{
		return PitchRadians;
	}
	const FVector CurrentForward = CurrentRotation.GetForwardVector().GetSafeNormal();
	const FVector Right = CurrentRotation.GetRightVector();
	const auto PitchOfForward = [&Avoidance](const FVector& Forward)
	{
		return FMath::Atan2(
			FVector::DotProduct(Forward, Avoidance.RadialUp),
			FVector::DotProduct(Forward, Avoidance.Heading));
	};
	const float CurrentPitch = PitchOfForward(CurrentForward);
	const float RequestedWorldPitch = PitchOfForward(FQuat(Right, PitchRadians).RotateVector(CurrentForward));
	if (RequestedWorldPitch >= CurrentPitch || RequestedWorldPitch >= Avoidance.MinimumPitch)
	{
		return PitchRadians;
	}
	if (CurrentPitch <= Avoidance.MinimumPitch)
	{
		return 0.0f;
	}

	// Keep the largest pilot-requested step that remains parallel to or above the sampled surface.
	// This only clips the downward turn; it never lifts a previously pitched hull automatically.
	float AllowedStep = 0.0f;
	float BlockedStep = PitchRadians;
	for (int32 Iteration = 0; Iteration < 8; ++Iteration)
	{
		const float CandidateStep = (AllowedStep + BlockedStep) * 0.5f;
		const FVector CandidateForward = FQuat(Right, CandidateStep).RotateVector(CurrentForward);
		if (PitchOfForward(CandidateForward) >= Avoidance.MinimumPitch)
		{
			AllowedStep = CandidateStep;
		}
		else
		{
			BlockedStep = CandidateStep;
		}
	}
	return AllowedStep;
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
		// A correction or a forced spawn can leave the long hull already intersecting the real
		// surface. Sweeping from penetration returns time zero forever. Allow only outward radial
		// recovery against that surface, then resume ordinary swept motion once clear.
		const AJTSPlanetAnchor* const Planet = TargetPlanet.Get();
		if (bTranslationHit && TranslationHit.bStartPenetrating && bHasSurfaceProximity
			&& IsValid(Planet) && TranslationHit.GetActor() == Planet->GetGameplaySurfaceActor())
		{
			const FVector RecoveryUp = Planet->GetRadialUpVector(OwningPawn->GetActorLocation()).GetSafeNormal();
			const float OutwardStep = FVector::DotProduct(Delta, RecoveryUp);
			if (OutwardStep > KINDA_SMALL_NUMBER)
			{
				MoveUpdatedComponent(RecoveryUp * OutwardStep, CurrentRotation, false, nullptr, ETeleportType::None);
				return false;
			}
		}
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

	const APawn* const OwningPawn = GetPawnOwner();
	if (!IsValid(OwningPawn))
	{
		return FVector::ZeroVector;
	}
	const FVector HullForward = OwningPawn->GetActorForwardVector().GetSafeNormal();
	const FVector HullRight = OwningPawn->GetActorRightVector().GetSafeNormal();
	const float DeadZone = FMath::Clamp(MovementDeadZone, 0.0f, 1.0f);
	const float ForwardThrottle = FMath::Clamp(MoveInput.Y, -1.0f, 1.0f);
	const float AssistAlpha = GetSurfaceFlightAssistAlpha();
	const float SurfaceAxisAlpha = FMath::SmoothStep(0.0f, 0.4f, AssistAlpha);
	const FVector LiftDirection = FMath::Lerp(OwningPawn->GetActorUpVector(), ReferenceUp, SurfaceAxisAlpha).GetSafeNormal();

	const float MoveSpeed = EffectiveStats.MaxMoveSpeed
		* (bBoosting && ForwardThrottle > DeadZone ? FMath::Max(1.0f, EffectiveStats.BoostMultiplier) : 1.0f);
	return (HullForward * (ForwardThrottle >= 0.0f ? MoveSpeed : EffectiveStats.ReverseSpeed) * ForwardThrottle
		+ HullRight * MoveInput.X * EffectiveStats.StrafeSpeed
		+ LiftDirection * VerticalInput * EffectiveStats.LiftSpeed) * SpeedLimit;
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
