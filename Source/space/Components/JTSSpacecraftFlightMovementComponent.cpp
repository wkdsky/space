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
	if (ShouldSkipUpdate(DeltaTime) || !IsValid(UpdatedComponent))
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
	const FVector TargetLocation = GroundInfo.GroundLocation + SurfaceUp * AssistedLandingClearance;
	const float HeightError = GroundInfo.DockingHeight - AssistedLandingClearance;
	const float CurrentAlignment = FMath::Clamp(
		FVector::DotProduct(Spacecraft->GetActorUpVector().GetSafeNormal(), SurfaceUp),
		-1.0f,
		1.0f);
	const float AlignmentFactor = FMath::Clamp((CurrentAlignment + 1.0f) * 0.5f, 0.15f, 1.0f);
	const float DesiredNormalSpeed = FMath::Clamp(
		HeightError * 2.0f,
		-AssistedLandingMaximumDescentSpeed * 0.5f,
		AssistedLandingDescentSpeed * AlignmentFactor);
	const FVector DesiredVelocity = -SurfaceUp * DesiredNormalSpeed;
	Velocity = FMath::VInterpConstantTo(
		Velocity,
		DesiredVelocity,
		DeltaTime,
		FMath::Max(1.0f, AssistedLandingVelocityResponse));

	const float CompletionCosine = FMath::Cos(FMath::DegreesToRadians(
		FMath::Clamp(LandingCompletionAlignmentDegrees, 0.0f, 90.0f)));
	const float NewAlignment = FVector::DotProduct(NewRotation.GetAxisZ().GetSafeNormal(), SurfaceUp);
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
	const float YawDelta = PendingYawInput * MouseLookSensitivity * EffectiveStats.YawRate * TurnMultiplier * DeltaTime;
	const float PitchDelta = PendingPitchInput * MouseLookSensitivity * EffectiveStats.PitchRate * TurnMultiplier * DeltaTime;
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
	OnAssistedLandingCompleted.Broadcast();
}

void UJTSSpacecraftFlightMovementComponent::FailAssistedLanding(EJTSLandingValidationFailure Failure)
{
	bAssistedLanding = false;
	ClearInput();
	Velocity = FVector::ZeroVector;
	AssistedLandingClearance = 0.0f;
	AssistedLandingDescentSpeed = 0.0f;
	OnAssistedLandingFailed.Broadcast(Failure);
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
