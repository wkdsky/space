// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSSpacecraftFlightMovementComponent.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
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

bool UJTSSpacecraftFlightMovementComponent::BeginAssistedLanding(const FTransform& TargetTransform, float DurationSeconds)
{
	if (!IsValid(UpdatedComponent))
	{
		return false;
	}

	ClearInput();
	bAssistedLanding = true;
	Velocity = FVector::ZeroVector;
	AssistedLandingStart = UpdatedComponent->GetComponentTransform();
	AssistedLandingTarget = TargetTransform;
	AssistedLandingDuration = FMath::Max(0.1f, DurationSeconds > 0.0f ? DurationSeconds : DefaultLandingDuration);
	AssistedLandingElapsed = 0.0f;
	return true;
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
	AssistedLandingElapsed += DeltaTime;
	const float Progress = FMath::Clamp(AssistedLandingElapsed / FMath::Max(0.1f, AssistedLandingDuration), 0.0f, 1.0f);
	const float SmoothedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, Progress, 2.0f);
	const FTransform CurrentTransform = UpdatedComponent->GetComponentTransform();
	const FVector TargetLocation = FMath::Lerp(AssistedLandingStart.GetLocation(), AssistedLandingTarget.GetLocation(), SmoothedProgress);
	const FQuat TargetRotation = FQuat::Slerp(AssistedLandingStart.GetRotation(), AssistedLandingTarget.GetRotation(), SmoothedProgress);
	const FVector Delta = TargetLocation - CurrentTransform.GetLocation();
	FHitResult Hit;
	MoveWithCollisionSweep(Delta, TargetRotation, Hit);
	Velocity = (UpdatedComponent->GetComponentLocation() - CurrentTransform.GetLocation())
		/ FMath::Max(DeltaTime, KINDA_SMALL_NUMBER);

	if (Progress >= 1.0f)
	{
		CompleteAssistedLanding();
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

	FHitResult Hit;
	MoveWithCollisionSweep(Velocity * DeltaTime, UpdatedComponent->GetComponentQuat(), Hit);
	if (Hit.IsValidBlockingHit())
	{
		Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal);
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
	const float PitchDelta = -PendingPitchInput * MouseLookSensitivity * EffectiveStats.PitchRate * TurnMultiplier * DeltaTime;
	const float RollTarget = RollInput * 75.0f;
	const float CurrentRoll = FRotator::NormalizeAxis(OwningPawn->GetActorRotation().Roll);
	const float DesiredRoll = FMath::FInterpTo(CurrentRoll, RollTarget, DeltaTime, FMath::Max(0.1f, RollReturnRate));
	const float RollDelta = FMath::Clamp(DesiredRoll - CurrentRoll, -EffectiveStats.RollRate * DeltaTime, EffectiveStats.RollRate * DeltaTime);
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
	OnAssistedLandingCompleted.Broadcast();
}

bool UJTSSpacecraftFlightMovementComponent::MoveWithCollisionSweep(const FVector& Delta, const FQuat& NewRotation, FHitResult& OutHit)
{
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
