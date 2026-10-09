// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSSpacecraftFlightMovementComponent.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Math/RotationMatrix.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Components/JTSSpacecraftLandingSupportComponent.h"
#include "space/Components/JTSSpacecraftSurfaceEnvelopeComponent.h"
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
	if (!IsActive() || GetOwner() == nullptr || ShouldSkipUpdate(DeltaTime) || !IsValid(UpdatedComponent))
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
	const FTransform& LandingPose,
	float DurationSeconds)
{
	if (!IsValid(UpdatedComponent) || !IsValid(Planet))
	{
		return false;
	}

	ClearInput();
	bAssistedLanding = true;
	EnvelopeLandingRetryElapsed = 0;
	Velocity = FVector::ZeroVector;
	TargetPlanet = Planet;
	SetAssistedLandingPhase(EJTSSpacecraftLandingAssistPhase::Aligning);
	AssistedLandingPose = LandingPose;
	const FVector Up = LandingPose.GetUnitAxis(EAxis::Z);
	LandingSupportCheckElapsed = 0;
	AssistedLandingDescentSpeed = AssistedLandingMaximumDescentSpeed;
	if (AJTSSpacecraftActor* const Spacecraft = Cast<AJTSSpacecraftActor>(GetPawnOwner()))
	{
		const FJTSSpacecraftGroundInfo GroundInfo = Spacecraft->GetGroundInfo();
		if (GroundInfo.bHasGround)
		{
			const float DesiredDuration = FMath::Max(
				0.1f,
				DurationSeconds > 0.0f ? DurationSeconds : DefaultLandingDuration);
			const float HeightToLose = FMath::Max(0.0f,
				float(FVector::DotProduct(Spacecraft->GetActorLocation() - LandingPose.GetLocation(), Up)));
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
	if (auto* Ship = Cast<AJTSSpacecraftActor>(GetPawnOwner()); Ship && Ship->HasAuthority())
		Ship->GetSurfaceEnvelopeComponent()->Reset();
	EnvelopeLandingRetryElapsed = 0;
	bTakeoffClearance = false;
	TakeoffClearanceRadius = 0;
	bEnvelopeEscape = false;
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
	auto* Ship = Cast<AJTSSpacecraftActor>(GetPawnOwner());
	auto* Planet = TargetPlanet.Get();
	auto* Support = Ship ? Ship->GetLandingSupportComponent() : nullptr;
	if (!Ship || !IsValid(Planet) || !Support) { FailAssistedLanding(EJTSLandingValidationFailure::NoPlanet); return; }
	AssistedLandingElapsed += DeltaTime;
	LandingSupportCheckElapsed += DeltaTime;
	if (AssistedLandingElapsed >= FMath::Max(1.0f, AssistedLandingTimeout))
	{
		FailAssistedLanding(EJTSLandingValidationFailure::ApproachBlocked); return;
	}
	// The plan stays inside the original bounded correction. Never chase successive moving targets.
	if (LandingSupportCheckElapsed >= 0.2f)
	{
		LandingSupportCheckElapsed = 0;
		FJTSPlanetLandingValidationResult Check;
		if (!Support->ValidatePose(Planet, AssistedLandingPose, Check))
		{
			FailAssistedLanding(Check.Failure); return;
		}
		Support->SetContacts(Check.FootContacts);
	}
	const FVector Up = AssistedLandingPose.GetUnitAxis(EAxis::Z);
	const FVector Current = UpdatedComponent->GetComponentLocation();
	const FQuat DesiredRotation = AssistedLandingPose.GetRotation();
	const FQuat CurrentRotation = UpdatedComponent->GetComponentQuat();
	const float RotationAlpha = FMath::Clamp(1 - FMath::Exp(-AssistedLandingRotationInterpolationSpeed * DeltaTime), 0.0f, 1.0f);
	const FQuat NewRotation = FQuat::Slerp(CurrentRotation, DesiredRotation, RotationAlpha).GetNormalized();
	const float AlignmentDegrees = FMath::RadiansToDegrees(NewRotation.AngularDistance(DesiredRotation));
    const FVector Error = AssistedLandingPose.GetLocation() - Current;
    const float Height = FMath::Max(0.0f, -float(FVector::DotProduct(Error, Up)));
    const float Braking = FMath::SmoothStep(0.0f, FMath::Max(1.0f, AssistedLandingBrakingDistance), Height);
    const float CruiseSpeed = FMath::Clamp(FMath::Max(AssistedLandingDescentSpeed, Height * 1.8f),
        AssistedLandingMinimumDescentSpeed, AssistedLandingMaximumDescentSpeed);
    const float Speed = FMath::Min(float(Error.Size()) * 4.0f, FMath::Lerp(AssistedLandingTouchdownSpeed, CruiseSpeed, Braking));
    FVector Delta = FVector::VectorPlaneProject(Error, Up).GetClampedToMaxSize(AssistedLandingCorrectionSpeed * DeltaTime);
    float DownStep = FMath::Min(Height, FMath::Max(1.0f, Speed) * DeltaTime);
    // Feet must finish deploying before final contact, while the initial descent never waits for gear.
    if (!Ship->IsLandingGearDeployed()) DownStep = FMath::Min(DownStep, FMath::Max(0.0f, Height - 20.0f));
    Delta -= Up * DownStep;
    if (AlignmentDegrees <= AssistedLandingAlignmentToleranceDegrees && Ship->IsLandingGearDeployed())
        SetAssistedLandingPhase(EJTSSpacecraftLandingAssistPhase::Descending);
	if (!Ship->CanTraverseLandingSegment(Current, Current + Delta, NewRotation)
		|| !Ship->CanOccupyLandingTransform(FTransform(NewRotation, Current + Delta, Ship->GetActorScale3D())))
	{
		FailAssistedLanding(EJTSLandingValidationFailure::CollisionBlocked); return;
	}
	FHitResult Hit;
	MoveWithCollisionSweep(Delta, NewRotation, Hit);
	Velocity = Delta / FMath::Max(DeltaTime, SMALL_NUMBER);
	if (Hit.IsValidBlockingHit()) { FailAssistedLanding(EJTSLandingValidationFailure::CollisionBlocked); return; }
	if (Error.Size() <= FMath::Clamp(LandingContactTolerance, 0.25f, 2.0f)
		&& AlignmentDegrees <= LandingCompletionAlignmentDegrees && Ship->IsLandingGearDeployed())
	{
		// Finish the remaining centimetre with a sweep. A fully extended leg cannot support a
		// hull that stops just above its fitted stance, even when the navigation tolerance passes.
		if (!Ship->CanOccupyLandingTransform(AssistedLandingPose))
		{
			FailAssistedLanding(EJTSLandingValidationFailure::CollisionBlocked); return;
		}
		FHitResult FinalHit;
		MoveWithCollisionSweep(AssistedLandingPose.GetLocation() - UpdatedComponent->GetComponentLocation(),
			DesiredRotation, FinalHit);
		if (FinalHit.IsValidBlockingHit()) { FailAssistedLanding(EJTSLandingValidationFailure::CollisionBlocked); return; }
		FJTSPlanetLandingValidationResult Check;
		if (!Support->ValidatePose(Planet, Ship->GetActorTransform(), Check)) { FailAssistedLanding(Check.Failure); return; }
		Support->SetContacts(Check.FootContacts);
		CompleteAssistedLanding();
	}
}

void UJTSSpacecraftFlightMovementComponent::TickFlight(float DeltaTime)
{
	const auto* ShipOwner = Cast<AJTSSpacecraftActor>(GetPawnOwner());
	const auto* Shell = ShipOwner ? ShipOwner->GetSurfaceEnvelopeComponent() : nullptr;
	const FVector ReferenceUp = Shell && Shell->IsFollowing() ? Shell->GetFrame().RadialUp : GetReferenceUp();
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
	if (ApplySurfaceEnvelope(DeltaTime)) return;

	FHitResult Hit;
	FQuat Rotation = UpdateRotation(DeltaTime);
	FVector Delta = Velocity * DeltaTime;
	if (auto* Ship = Cast<AJTSSpacecraftActor>(GetPawnOwner()))
	{
		const FVector Start = Ship->GetActorLocation();
		// The authored movement proxy may be narrower than the visible hull. Guard both.
		if (!Ship->CanOccupyLandingTransform(Ship->GetActorTransform()))
		{
			// A time-zero sweep cannot resolve an existing overlap. Check both hulls before
			// committing a small, fixed-attitude step out, including reverse pilot thrust.
			const float RecoveryStep = FMath::Min(25.0f, FMath::Max(1.0f, SurfaceClearanceRecoverySpeed) * DeltaTime);
			FVector Recovery = Delta.GetClampedToMaxSize(RecoveryStep);
			bool bCanRecover = Ship->CanRecoverFlightPenetration(Recovery);
			if (!bCanRecover && TargetPlanet.IsValid())
			{
				// Forward intent into a slope must not cancel the shell's upward recovery.
				const FVector Up = TargetPlanet->GetRadialUpVector(Start);
				Recovery = Up * RecoveryStep;
				bCanRecover = Ship->CanRecoverFlightPenetration(Recovery);
			}
			if (bCanRecover)
			{
				MoveUpdatedComponent(Recovery, Ship->GetActorQuat(), false, nullptr, ETeleportType::None);
				Velocity = Recovery / FMath::Max(DeltaTime, SMALL_NUMBER);
			}
			else Velocity = FVector::ZeroVector;
			return;
		}
		else
		{
			if (!Ship->CanOccupyLandingTransform(FTransform(Rotation, Start, Ship->GetActorScale3D())))
				Rotation = Ship->GetActorQuat();
			if (!Ship->CanTraverseLandingSegment(Start, Start + Delta, Rotation))
			{
				float Safe = 0, Blocked = 1;
				for (int32 I = 0; I < 10; ++I)
				{
					const float Alpha = (Safe + Blocked) * 0.5f;
					if (Ship->CanTraverseLandingSegment(Start, Start + Delta * Alpha, Rotation)) Safe = Alpha;
					else Blocked = Alpha;
				}
				Delta *= Safe;
				Velocity = Delta / FMath::Max(DeltaTime, SMALL_NUMBER);
			}
		}
	}
	MoveWithCollisionSweep(Delta, Rotation, Hit);
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

void UJTSSpacecraftFlightMovementComponent::BeginEnvelopeEscape(bool bRequireTakeoffClearance)
{
    bEnvelopeEscape = true;
    bTakeoffClearance = bRequireTakeoffClearance;
    TakeoffClearanceRadius = 0;
    EnvelopeLandingRetryElapsed = 0;
    ClearInput();
    if (bTakeoffClearance)
    {
        Velocity = FVector::ZeroVector;
        if (auto* Ship = Cast<AJTSSpacecraftActor>(GetPawnOwner()))
            Ship->GetSurfaceEnvelopeComponent()->Reset();
    }
}

bool UJTSSpacecraftFlightMovementComponent::IsAtEnvelopeBoundary() const
{
    const auto* Ship = Cast<AJTSSpacecraftActor>(GetPawnOwner());
    const auto Frame = Ship ? Ship->GetSurfaceEnvelopeComponent()->GetFrame() : FJTSSurfaceEnvelopeFrame();
    return Frame.bValid && FVector::DotProduct(Ship->GetActorLocation() - Frame.Location, Frame.RadialUp) <= 1.0;
}

bool UJTSSpacecraftFlightMovementComponent::ApplySurfaceEnvelope(float DeltaTime)
{
    auto* Ship = Cast<AJTSSpacecraftActor>(GetPawnOwner());
    if (!Ship) return false;
    auto* Envelope = Ship->GetSurfaceEnvelopeComponent();
    auto* Planet = TargetPlanet.Get();
    Envelope->Refresh(Planet, Ship->GetActorLocation(), bTakeoffClearance ? FVector::ZeroVector : Velocity,
        bHasSurfaceProximity ? CachedSurfaceAltitude : BIG_NUMBER, DeltaTime,
        (VerticalInput > MovementDeadZone || bEnvelopeEscape) && !bTakeoffClearance, bTakeoffClearance);
    const auto Frame = Envelope->GetFrame();
    if (!Frame.bValid)
    {
        // Without a departure surface there is no authorised horizontal path out of the crater.
        if (bTakeoffClearance) Velocity = FVector::ZeroVector;
        return false;
    }
    const auto& Settings = Envelope->GetSettings();
    const FVector Current = Ship->GetActorLocation();
    const FVector Up = Frame.RadialUp;
    const float Height = FVector::DotProduct(Current - Frame.Location, Up);
    if (bTakeoffClearance)
    {
        double RequiredRadius;
        if (!Envelope->GetTakeoffClearanceRadius(RequiredRadius))
        {
            Velocity = FVector::ZeroVector;
            return false;
        }
        // Keep the highest departure target even if a later sample drops away. A brief Space
        // press completes the lift; forward/strafe/boost cannot escape below the crater rim.
        TakeoffClearanceRadius = FMath::Max(TakeoffClearanceRadius, RequiredRadius);
        const double Remaining = TakeoffClearanceRadius - FVector::Distance(Current, Planet->GetPlanetCenter());
        if (Remaining > 1.0)
        {
            const float LiftSpeed = FMath::Max(1.0f, EffectiveStats.LiftSpeed);
            const float RadialSpeed = FMath::Max(0.0f, float(FVector::DotProduct(Velocity, Up)));
            const float Speed = FMath::FInterpConstantTo(RadialSpeed, LiftSpeed, DeltaTime,
                FMath::Max(1.0f, EffectiveStats.Acceleration));
            Velocity = Up * FMath::Min(double(Speed), Remaining / FMath::Max(DeltaTime, SMALL_NUMBER));
            return false;
        }
        bTakeoffClearance = false;
        bEnvelopeEscape = false;
        // A teleport/correction well above departure clearance has already left surface flight.
        // Release the takeoff attitude latch immediately, including when no Space key is held.
        if (Remaining < -Settings.FollowingExitHeight)
            Envelope->Refresh(Planet, Current, Velocity, bHasSurfaceProximity ? CachedSurfaceAltitude : BIG_NUMBER,
                0, true);
    }
    if (bEnvelopeEscape && Height >= Settings.HoverOffset) bEnvelopeEscape = false;
    const bool bFollowing = Envelope->IsFollowing();
    // Follow a single tangent field. Pilot pitch/camera error never fights terrain pitch.
    if (bFollowing)
    {
        const float RadialSpeed = FVector::DotProduct(Velocity, Up);
        FVector Tangent = FVector::VectorPlaneProject(Velocity, Up);
        const float TangentSpeed = Tangent.Size();
        Tangent = FVector::VectorPlaneProject(Tangent, Frame.Normal).GetSafeNormal() * TangentSpeed;
        Velocity = Tangent;
        if (FMath::Abs(VerticalInput) > MovementDeadZone)
            Velocity += Up * (RadialSpeed - FVector::DotProduct(Tangent, Up));
        else
        {
            const float Response = (1 - FMath::Exp(-Settings.HeightResponse * DeltaTime)) / FMath::Max(DeltaTime, SMALL_NUMBER);
            Velocity += Up * FMath::Clamp((Settings.HoverOffset - Height) * Response, -EffectiveStats.LiftSpeed, SurfaceClearanceRecoverySpeed);
        }
    }
    FJTSSurfaceEnvelopeFrame Next;
    const FVector Proposed = Current + Velocity * DeltaTime;
    if (!Envelope->Evaluate(Planet, Proposed, Next)) return false;
    const float NextHeight = FVector::DotProduct(Proposed - Next.Location, Next.RadialUp);
    EnvelopeLandingRetryElapsed += DeltaTime;
    // Test the attempted crossing before committing it. An invalid stance cannot enter the shell.
    // A rising mountain is not a landing command; takeoff and Space abort can escape from below.
    if (!bEnvelopeEscape && VerticalInput < -MovementDeadZone && NextHeight < 0
        && EnvelopeLandingRetryElapsed >= 0.2f)
    {
        EnvelopeLandingRetryElapsed = 0;
        if (Ship->TryLandingAtEnvelopeBoundary()) return true;
    }
    const float FloorHeight = VerticalInput < -MovementDeadZone ? 0.0f : Settings.HoverOffset;
    if (NextHeight < FloorHeight && (!bEnvelopeEscape || VerticalInput <= 0))
    {
        const float RequiredLift = (FloorHeight - NextHeight) / FMath::Max(DeltaTime, SMALL_NUMBER);
        // Recover already displaced hulls gradually; ordinary flight clamps precisely at the shell.
        const float RecoveryCap = Height < 0
            ? FMath::Max(0.0f, SurfaceClearanceRecoverySpeed - float(FVector::DotProduct(Velocity, Up))) : BIG_NUMBER;
        Velocity += Up * FMath::Min(RequiredLift, RecoveryCap);
    }
    return false;
}

FVector UJTSSpacecraftFlightMovementComponent::GetReferenceUp() const
{
	const FVector SafeReferenceUp = CurrentReferenceUp.GetSafeNormal();
	return SafeReferenceUp.IsNearlyZero() ? FVector::UpVector : SafeReferenceUp;
}

FQuat UJTSSpacecraftFlightMovementComponent::UpdateRotation(float DeltaTime)
{
    if (!IsValid(UpdatedComponent)) return FQuat::Identity;
    const FQuat Current = UpdatedComponent->GetComponentQuat();
    const auto* Ship = Cast<AJTSSpacecraftActor>(GetPawnOwner());
    const auto* Envelope = Ship ? Ship->GetSurfaceEnvelopeComponent() : nullptr;
    FQuat Desired = BuildFreeFlightDesiredRotation(Current);
    if (Envelope && (Envelope->IsFollowing() || bTakeoffClearance) && Envelope->GetFrame().bValid)
    {
        const FVector Normal = Envelope->GetFrame().Normal;
        FVector Forward = FVector::VectorPlaneProject(Current.GetForwardVector(), Normal).GetSafeNormal();
        if (Forward.IsNearlyZero()) Forward = FVector::VectorPlaneProject(Current.GetUpVector(), Normal).GetSafeNormal();
        if (bTurnAroundLatched) Forward = FVector::VectorPlaneProject(TurnAroundTargetForward, Normal).GetSafeNormal();
        else Forward = FQuat(Normal, SteeringInput.X * FMath::DegreesToRadians(12.0f)).RotateVector(Forward);
        if (!Forward.IsNearlyZero()) Desired = FRotationMatrix::MakeFromXZ(Forward, Normal).ToQuat();
    }
    return InterpolateTowardRotation(DeltaTime, Current, Desired);
}

FQuat UJTSSpacecraftFlightMovementComponent::BuildFreeFlightDesiredRotation(
	const FQuat& CurrentRotation) const
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
		RequestedPitch);
	const FQuat Roll(CurrentRotation.GetForwardVector(), bRolling ? RollInput * Step : 0.0f);
	return (Roll * Pitch * Yaw * CurrentRotation).GetNormalized();
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
	AssistedLandingDescentSpeed = 0.0f;
	SetAssistedLandingPhase(EJTSSpacecraftLandingAssistPhase::Touchdown);
	OnAssistedLandingCompleted.Broadcast();
}

void UJTSSpacecraftFlightMovementComponent::FailAssistedLanding(EJTSLandingValidationFailure Failure)
{
	bAssistedLanding = false;
	ClearInput();
	Velocity = FVector::ZeroVector;
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
