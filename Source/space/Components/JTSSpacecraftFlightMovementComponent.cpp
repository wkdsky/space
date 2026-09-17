// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSSpacecraftFlightMovementComponent.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
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

void UJTSSpacecraftFlightMovementComponent::SetForwardInput(float Value)
{
	if (bAssistedLanding)
	{
		return;
	}
	InputVector.X = FMath::Clamp(Value, -1.0f, 1.0f);
}

void UJTSSpacecraftFlightMovementComponent::SetStrafeInput(float Value)
{
	if (bAssistedLanding)
	{
		return;
	}
	InputVector.Y = FMath::Clamp(Value, -1.0f, 1.0f);
}

void UJTSSpacecraftFlightMovementComponent::SetVerticalInput(float Value)
{
	if (bAssistedLanding)
	{
		return;
	}
	InputVector.Z = FMath::Clamp(Value, -1.0f, 1.0f);
}

void UJTSSpacecraftFlightMovementComponent::SetRollInput(float Value)
{
	if (bAssistedLanding)
	{
		return;
	}
	RollInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void UJTSSpacecraftFlightMovementComponent::AddYawInput(float Value)
{
	if (bAssistedLanding)
	{
		return;
	}
	PendingYawInput += Value;
}

void UJTSSpacecraftFlightMovementComponent::AddPitchInput(float Value)
{
	if (bAssistedLanding)
	{
		return;
	}
	PendingPitchInput += Value;
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
	InputVector = FVector::ZeroVector;
	RollInput = 0.0f;
	PendingYawInput = 0.0f;
	PendingPitchInput = 0.0f;
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

float UJTSSpacecraftFlightMovementComponent::GetCurrentSpeed() const
{
	return Velocity.Size();
}

float UJTSSpacecraftFlightMovementComponent::GetSpeedNormalized() const
{
	const float MaximumSpeed = FMath::Max(1.0f, EffectiveStats.MaxForwardSpeed * FMath::Max(1.0f, EffectiveStats.BoostMultiplier));
	return FMath::Clamp(GetCurrentSpeed() / MaximumSpeed, 0.0f, 1.0f);
}

float UJTSSpacecraftFlightMovementComponent::GetThrottleNormalized() const
{
	return InputVector.X;
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

	const FVector TargetLocation = GroundInfo.GroundLocation + SurfaceUp * AssistedLandingClearance;
	const float HeightError = GroundInfo.DockingHeight - AssistedLandingClearance;
	const float CompletionCosine = FMath::Cos(FMath::DegreesToRadians(
		FMath::Clamp(LandingCompletionAlignmentDegrees, 0.0f, 90.0f)));
	const bool bAtLandingHeight = FMath::Abs(HeightError) <= LandingContactTolerance;
	if (bAtLandingHeight && NewAlignment >= CompletionCosine)
	{
		FHitResult FinalMoveHit;
		MoveWithCollisionSweep(TargetLocation - UpdatedComponent->GetComponentLocation(), DesiredRotation, FinalMoveHit);
		if (FVector::DistSquared(UpdatedComponent->GetComponentLocation(), TargetLocation)
			<= FMath::Square(FMath::Max(2.0f, LandingContactTolerance * 1.5f)))
		{
			CompleteAssistedLanding();
			return;
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
	UpdateRotation(DeltaTime);
	const FVector TargetVelocity = BuildTargetVelocity();
	const float Rate = bBraking ? EffectiveStats.BrakeStrength : GetAccelerationRate();
	Velocity = FMath::VInterpConstantTo(Velocity, TargetVelocity, DeltaTime, FMath::Max(1.0f, Rate));
	if (InputVector.IsNearlyZero() && !bBraking)
	{
		Velocity = FMath::VInterpTo(Velocity, FVector::ZeroVector, DeltaTime, FMath::Max(0.0f, InertialDampeningRate));
	}
	ApplyPlanetGravity(DeltaTime);

	FHitResult Hit;
	MoveWithCollisionSweep(Velocity * DeltaTime, UpdatedComponent->GetComponentQuat(), Hit);
	if (Hit.IsValidBlockingHit())
	{
		Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal);
	}
}

void UJTSSpacecraftFlightMovementComponent::ApplyPlanetGravity(float DeltaTime)
{
	if (!bApplyPlanetaryGravity || DeltaTime <= 0.0f)
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

	Velocity += GravityDirection * Planet->GetGravityStrength() * DeltaTime;
	if (MaximumPlanetGravitySpeed > 0.0f)
	{
		Velocity = Velocity.GetClampedToMaxSize(MaximumPlanetGravitySpeed);
	}
}

void UJTSSpacecraftFlightMovementComponent::UpdateRotation(float DeltaTime)
{
	APawn* const OwningPawn = GetPawnOwner();
	if (!IsValid(OwningPawn))
	{
		return;
	}

	const float TurnMultiplier = bBoosting ? FMath::Max(0.0f, EffectiveStats.BoostTurnMultiplier) : 1.0f;
	// Mouse input is a per-frame delta, not a held axis. Multiplying it by DeltaTime made steering
	// frame-rate dependent and, together with the former [-1, 1] raw-mouse clamp, severely slowed it.
	// Keep upgrade turn-rate effects by normalizing against the Blueprint-configured base rates while
	// applying each delta exactly once.
	const float YawRateScale = FMath::Max(0.0f, EffectiveStats.YawRate) / FMath::Max(1.0f, BaseStats.YawRate);
	const float PitchRateScale = FMath::Max(0.0f, EffectiveStats.PitchRate) / FMath::Max(1.0f, BaseStats.PitchRate);
	const float YawDelta = PendingYawInput * MouseLookSensitivity * YawRateScale * TurnMultiplier;
	const float PitchDelta = PendingPitchInput * MouseLookSensitivity * PitchRateScale * TurnMultiplier;
	// Roll is an angular velocity about the spacecraft's local forward axis. Do not derive it from
	// FRotator::Roll or a finite target angle: those turn a held Q/E input into a bounded bank.
	const float RollDelta = RollInput * EffectiveStats.RollRate * DeltaTime;
	OwningPawn->AddActorLocalRotation(FRotator(PitchDelta, YawDelta, RollDelta));
	PendingYawInput = 0.0f;
	PendingPitchInput = 0.0f;
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
	if (World == nullptr || FlightCollision == nullptr || Delta.IsNearlyZero())
	{
		MoveUpdatedComponent(Delta, NewRotation, false, &OutHit, ETeleportType::None);
		return !OutHit.IsValidBlockingHit();
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JTSSpacecraftFlight), false, OwningPawn);
	QueryParams.AddIgnoredActor(OwningPawn);
	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FVector End = Start + Delta;
	const FCollisionShape HullShape = FCollisionShape::MakeBox(FlightCollision->GetScaledBoxExtent());
	const bool bHit = World->SweepSingleByChannel(
		OutHit,
		Start,
		End,
		NewRotation,
		ECC_Visibility,
		HullShape,
		QueryParams);
	const FVector AllowedDelta = bHit ? Delta * FMath::Clamp(OutHit.Time, 0.0f, 1.0f) : Delta;
	MoveUpdatedComponent(AllowedDelta, NewRotation, false, nullptr, ETeleportType::None);
	return !bHit;
}

FVector UJTSSpacecraftFlightMovementComponent::BuildTargetVelocity() const
{
	const APawn* const OwningPawn = GetPawnOwner();
	if (!IsValid(OwningPawn) || bBraking)
	{
		return FVector::ZeroVector;
	}

	const float ForwardSpeed = InputVector.X >= 0.0f
		? EffectiveStats.MaxForwardSpeed * (bBoosting ? EffectiveStats.BoostMultiplier : 1.0f)
		: EffectiveStats.MaxReverseSpeed;
	return OwningPawn->GetActorForwardVector() * InputVector.X * ForwardSpeed
		+ OwningPawn->GetActorRightVector() * InputVector.Y * EffectiveStats.StrafeSpeed
		+ OwningPawn->GetActorUpVector() * InputVector.Z * EffectiveStats.VerticalSpeed;
}

float UJTSSpacecraftFlightMovementComponent::GetAccelerationRate() const
{
	const bool bAcceleratingForward = InputVector.X > 0.0f;
	return bAcceleratingForward && bBoosting
		? EffectiveStats.Acceleration * FMath::Max(0.0f, EffectiveStats.BoostAccelerationMultiplier)
		: (InputVector.IsNearlyZero() ? EffectiveStats.Deceleration : EffectiveStats.Acceleration);
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
