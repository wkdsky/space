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
}

void UJTSSpacecraftFlightMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || ShouldSkipUpdate(DeltaTime) || !IsValid(UpdatedComponent))
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

	SubmitExteriorAltitude();
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

void UJTSSpacecraftFlightMovementComponent::SetViewForward(const FVector& Value)
{
	if (Value.ContainsNaN())
	{
		return;
	}
	const FVector SafeForward = Value.GetSafeNormal();
	if (!SafeForward.IsNearlyZero())
	{
		ViewForward = SafeForward;
	}
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
	const APawn* const OwningPawn = GetPawnOwner();
	const AJTSPlanetAnchor* const Planet = TargetPlanet.Get();
	return bUsePlanetSurfaceFlightFrame
		&& IsValid(OwningPawn)
		&& IsValid(Planet)
		&& Planet->IsWithinSpaceExitRange(OwningPawn->GetActorLocation());
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
	TargetPlanet = NewTargetPlanet;
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
		// Do not trade attitude for altitude. The craft stabilizes in place first, then begins a
		// quick controlled descent; this makes the landing readable and prevents side-on touchdown.
		Velocity = FVector::ZeroVector;
		FHitResult AlignmentHit;
		MoveWithCollisionSweep(FVector::ZeroVector, NewRotation, AlignmentHit);
		if (NewAlignment >= AlignmentCosine)
		{
			SetAssistedLandingPhase(EJTSSpacecraftLandingAssistPhase::Descending);
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
	const FVector TargetVelocity = BuildTargetVelocity(ReferenceUp);
	const float Rate = bBraking ? EffectiveStats.BrakeStrength : GetAccelerationRate();
	Velocity = FMath::VInterpConstantTo(Velocity, TargetVelocity, DeltaTime, FMath::Max(1.0f, Rate));
	ApplyPlanetGravity(DeltaTime);

	FHitResult Hit;
	MoveWithCollisionSweep(Velocity * DeltaTime, UpdateRotation(DeltaTime, ReferenceUp), Hit);
	if (Hit.IsValidBlockingHit())
	{
		Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal);
	}
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

FVector UJTSSpacecraftFlightMovementComponent::GetReferenceUp() const
{
	const APawn* const OwningPawn = GetPawnOwner();
	const AJTSPlanetAnchor* const Planet = TargetPlanet.Get();
	if (IsUsingPlanetSurfaceFlightFrame() && IsValid(OwningPawn) && IsValid(Planet))
	{
		const FVector PlanetUp = Planet->GetRadialUpVector(OwningPawn->GetActorLocation()).GetSafeNormal();
		if (!PlanetUp.IsNearlyZero())
		{
			return PlanetUp;
		}
	}

	if (IsValid(OwningPawn))
	{
		const FVector ActorUp = OwningPawn->GetActorUpVector().GetSafeNormal();
		if (!ActorUp.IsNearlyZero())
		{
			return ActorUp;
		}
	}
	return FVector::UpVector;
}

void UJTSSpacecraftFlightMovementComponent::GetViewBasis(
	const FVector& ReferenceUp,
	FVector& OutForward,
	FVector& OutRight) const
{
	const APawn* const OwningPawn = GetPawnOwner();
	OutForward = ViewForward.GetSafeNormal();
	if (OutForward.IsNearlyZero() && IsValid(OwningPawn))
	{
		OutForward = OwningPawn->GetActorForwardVector().GetSafeNormal();
	}

	FVector PlanarForward = FVector::VectorPlaneProject(OutForward, ReferenceUp).GetSafeNormal();
	if (PlanarForward.IsNearlyZero() && IsValid(OwningPawn))
	{
		PlanarForward = FVector::VectorPlaneProject(OwningPawn->GetActorForwardVector(), ReferenceUp).GetSafeNormal();
	}
	if (PlanarForward.IsNearlyZero())
	{
		FVector FallbackRight;
		ReferenceUp.FindBestAxisVectors(PlanarForward, FallbackRight);
	}
	if (IsUsingPlanetSurfaceFlightFrame()
		|| OutForward.IsNearlyZero()
		|| FVector::VectorPlaneProject(OutForward, ReferenceUp).IsNearlyZero())
	{
		OutForward = PlanarForward;
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

	FVector ViewBasisForward;
	FVector ViewBasisRight;
	GetViewBasis(ReferenceUp, ViewBasisForward, ViewBasisRight);
	const FVector2D ClampedMoveInput = MoveInput.GetClampedToMaxSize(1.0f);
	FVector DesiredForward = ViewBasisForward;
	if (ClampedMoveInput.SizeSquared() <= FMath::Square(FMath::Clamp(MovementDeadZone, 0.0f, 1.0f)))
	{
		DesiredForward = FVector::VectorPlaneProject(UpdatedComponent->GetForwardVector(), ReferenceUp).GetSafeNormal();
	}
	if (DesiredForward.IsNearlyZero())
	{
		DesiredForward = ViewBasisForward;
	}

	const FQuat DesiredRotation = FRotationMatrix::MakeFromXZ(DesiredForward.GetSafeNormal(), ReferenceUp).ToQuat();
	const float TurnMultiplier = bBoosting ? FMath::Max(0.0f, EffectiveStats.BoostTurnMultiplier) : 1.0f;
	const float MaximumTurnSpeedRadians = FMath::DegreesToRadians(
		FMath::Max(0.0f, EffectiveStats.FacingTurnRate) * TurnMultiplier);
	const FQuat CurrentRotation = UpdatedComponent->GetComponentQuat();
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

void UJTSSpacecraftFlightMovementComponent::SubmitExteriorAltitude()
{
	AJTSPlanetAnchor* const Planet = TargetPlanet.Get();
	APawn* const OwningPawn = GetPawnOwner();
	if (!IsValid(Planet) || !IsValid(OwningPawn))
	{
		return;
	}

	if (AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
	{
		Manager->HandleFlightAltitude(Planet->GetExteriorAltitude(OwningPawn->GetActorLocation()));
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
	if (Delta.IsNearlyZero())
	{
		if (World->OverlapBlockingTestByChannel(Start, TargetHull.GetRotation(), ECC_Visibility, HullShape, QueryParams))
		{
			OutHit.bBlockingHit = true;
			OutHit.bStartPenetrating = true;
			return false;
		}
		MoveUpdatedComponent(Delta, NewRotation, false, nullptr, ETeleportType::None);
		return true;
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

	FVector ViewBasisForward;
	FVector ViewBasisRight;
	GetViewBasis(ReferenceUp, ViewBasisForward, ViewBasisRight);
	const FVector2D ClampedMoveInput = MoveInput.GetClampedToMaxSize(1.0f);
	const float MoveMagnitude = ClampedMoveInput.Size();
	FVector CameraRelativeDirection = ViewBasisForward * ClampedMoveInput.Y + ViewBasisRight * ClampedMoveInput.X;
	if (MoveMagnitude <= FMath::Clamp(MovementDeadZone, 0.0f, 1.0f))
	{
		CameraRelativeDirection = FVector::ZeroVector;
	}
	else
	{
		CameraRelativeDirection.Normalize();
	}

	const float MoveSpeed = EffectiveStats.MaxMoveSpeed
		* (bBoosting ? FMath::Max(1.0f, EffectiveStats.BoostMultiplier) : 1.0f);
	return CameraRelativeDirection * MoveSpeed * MoveMagnitude
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
