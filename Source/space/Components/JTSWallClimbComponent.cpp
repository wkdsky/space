// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSWallClimbComponent.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSPlanetGravityComponent.h"
#include "space/Items/JTSItemTypes.h"
#include "space/Player/JTSCharacter.h"

UJTSWallClimbComponent::UJTSWallClimbComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void UJTSWallClimbComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSWallClimbComponent, bClimbing);
	DOREPLIFETIME(UJTSWallClimbComponent, SurfaceNormal);
	DOREPLIFETIME(UJTSWallClimbComponent, StepDirection);
	DOREPLIFETIME(UJTSWallClimbComponent, bLeadHandLeft);
	DOREPLIFETIME(UJTSWallClimbComponent, StepStartWorldTime);
}

void UJTSWallClimbComponent::OnRep_Climbing()
{
	if (!bClimbing)
	{
		bStepActive = false;
		SwingAlpha = 0.0f;
	}
}

bool UJTSWallClimbComponent::HasIceAxesInHand() const
{
	const AActor* const Owner = GetOwner();
	const UJTSInventoryComponent* const Inventory = Owner != nullptr
		? Owner->FindComponentByClass<UJTSInventoryComponent>()
		: nullptr;
	return IsValid(Inventory) && Inventory->GetActiveItemId() == EJTSItemId::IceAxe;
}

FVector UJTSWallClimbComponent::GetGravityUp() const
{
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* const Movement = Character != nullptr
		? Character->GetCharacterMovement()
		: nullptr;
	if (!IsValid(Movement))
	{
		return FVector::UpVector;
	}
	const FVector Gravity = Movement->GetGravityDirection();
	return Gravity.IsNearlyZero() ? FVector::UpVector : (-Gravity).GetSafeNormal();
}

bool UJTSWallClimbComponent::IsSurfaceTooSteepToStand(const FVector& Normal) const
{
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* const Movement = Character != nullptr
		? Character->GetCharacterMovement()
		: nullptr;
	const float WalkableZ = IsValid(Movement) ? Movement->GetWalkableFloorZ() : FMath::Cos(FMath::DegreesToRadians(50.0f));
	return FVector::DotProduct(Normal.GetSafeNormal(), GetGravityUp()) < WalkableZ - KINDA_SMALL_NUMBER;
}

bool UJTSWallClimbComponent::ProbeClimbSurface(FHitResult& OutHit) const
{
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	UWorld* const World = GetWorld();
	if (!IsValid(Character) || World == nullptr || !HasIceAxesInHand() || Character->GetAttachParentActor() != nullptr)
	{
		return false;
	}

	const FVector Up = GetGravityUp();
	const FVector Forward = FVector::VectorPlaneProject(Character->GetActorForwardVector(), Up).GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		return false;
	}

	const UCapsuleComponent* const Capsule = Character->GetCapsuleComponent();
	const float HalfHeight = IsValid(Capsule) ? Capsule->GetScaledCapsuleHalfHeight() : 96.0f;
	const FVector Start = Character->GetActorLocation() + Up * (HalfHeight * 0.15f);
	const FVector End = Start + Forward * ProbeDistance;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JTSWallClimb), false, Character);
	if (!World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params) || !OutHit.bBlockingHit)
	{
		return false;
	}
	return IsSurfaceTooSteepToStand(OutHit.ImpactNormal);
}

void UJTSWallClimbComponent::BeginClimb(const FHitResult& Hit)
{
	ACharacter* const Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* const Movement = Character != nullptr ? Character->GetCharacterMovement() : nullptr;
	if (!IsValid(Character) || !IsValid(Movement))
	{
		return;
	}

	SurfaceNormal = Hit.ImpactNormal.GetSafeNormal();
	if (!bSavedGravityScale)
	{
		SavedGravityScale = Movement->GravityScale;
		bSavedGravityScale = true;
	}
	Movement->GravityScale = 0.0f;
	if (UJTSPlanetGravityComponent* const Gravity = Character->FindComponentByClass<UJTSPlanetGravityComponent>())
	{
		Gravity->SetSurfaceGravitySuspended(true);
	}
	Movement->StopMovementImmediately();
	Movement->SetMovementMode(MOVE_Flying);
	bClimbing = true;
	bLeadHandLeft = true;
	StepDirection = GetGravityUp();
	SwingAlpha = 0.0f;
	bStepActive = false;

	const FVector Up = GetGravityUp();
	const float Clearance = IsValid(Character->GetCapsuleComponent())
		? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + 6.0f
		: 48.0f;
	const FVector Planted = Hit.ImpactPoint + SurfaceNormal * Clearance - Up * (Character->GetCapsuleComponent()
		? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.15f
		: 0.0f);
	Character->SetActorLocation(Planted, true);
	FaceSurface();
}

void UJTSWallClimbComponent::EndClimb()
{
	ACharacter* const Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* const Movement = Character != nullptr ? Character->GetCharacterMovement() : nullptr;
	bClimbing = false;
	bStepActive = false;
	SwingAlpha = 0.0f;
	if (IsValid(Character))
	{
		if (UJTSPlanetGravityComponent* const Gravity = Character->FindComponentByClass<UJTSPlanetGravityComponent>())
		{
			Gravity->SetSurfaceGravitySuspended(false);
		}
	}
	if (IsValid(Movement))
	{
		if (bSavedGravityScale)
		{
			Movement->GravityScale = SavedGravityScale;
		}
		bSavedGravityScale = false;
		Movement->SetMovementMode(MOVE_Falling);
	}
}

void UJTSWallClimbComponent::FaceSurface()
{
	ACharacter* const Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(Character))
	{
		return;
	}
	const FVector Up = GetGravityUp();
	const FVector IntoWall = FVector::VectorPlaneProject(-SurfaceNormal, Up).GetSafeNormal();
	if (IntoWall.IsNearlyZero())
	{
		return;
	}
	Character->SetActorRotation(FRotationMatrix::MakeFromZX(Up, IntoWall).Rotator());
}

void UJTSWallClimbComponent::StartStep(const FVector& WorldStep)
{
	ACharacter* const Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(Character) || WorldStep.IsNearlyZero() || bStepActive)
	{
		return;
	}

	const FVector Up = GetGravityUp();
	const FVector Along = FVector::VectorPlaneProject(WorldStep, SurfaceNormal).GetSafeNormal();
	if (Along.IsNearlyZero())
	{
		return;
	}

	FHitResult NextHit;
	UWorld* const World = GetWorld();
	const FVector Start = Character->GetActorLocation() + Along * StepDistance - SurfaceNormal * 12.0f;
	const FVector End = Start - SurfaceNormal * (ProbeDistance + 40.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JTSWallClimbStep), false, Character);
	if (World == nullptr || !World->LineTraceSingleByChannel(NextHit, Start, End, ECC_Visibility, Params) || !NextHit.bBlockingHit
		|| !IsSurfaceTooSteepToStand(NextHit.ImpactNormal))
	{
		EndClimb();
		return;
	}

	const float Clearance = IsValid(Character->GetCapsuleComponent())
		? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + 6.0f
		: 48.0f;
	StepStart = Character->GetActorLocation();
	StepTarget = NextHit.ImpactPoint + NextHit.ImpactNormal.GetSafeNormal() * Clearance;
	SurfaceNormal = NextHit.ImpactNormal.GetSafeNormal();
	StepDirection = Along.GetSafeNormal();
	bLeadHandLeft = !bLeadHandLeft;
	StepStartWorldTime = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	StepElapsed = 0.0f;
	SwingAlpha = 0.0f;
	bStepActive = true;
	static_cast<void>(Up);
	FaceSurface();
}

void UJTSWallClimbComponent::AdvanceStep(float DeltaTime)
{
	ACharacter* const Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(Character) || !bStepActive)
	{
		return;
	}

	StepElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(StepElapsed / FMath::Max(0.05f, StepSeconds), 0.0f, 1.0f);
	const float MoveAlpha = FMath::SmoothStep(0.18f, 0.82f, Alpha);
	SwingAlpha = Alpha < 0.45f ? Alpha / 0.45f : 1.0f - (Alpha - 0.45f) / 0.55f;
	Character->SetActorLocation(FMath::Lerp(StepStart, StepTarget, MoveAlpha), false, nullptr, ETeleportType::TeleportPhysics);
	FaceSurface();
	if (Alpha >= 1.0f)
	{
		bStepActive = false;
		SwingAlpha = 0.0f;
		Character->SetActorLocation(StepTarget, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void UJTSWallClimbComponent::TryAttachFromApproach()
{
	if (bClimbing || !HasIceAxesInHand() || GetOwner() == nullptr)
	{
		return;
	}
	const UWorld* const World = GetWorld();
	const float Now = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	if (Now < NextAttachRequestSeconds)
	{
		return;
	}
	NextAttachRequestSeconds = Now + 0.35f;
	if (!GetOwner()->HasAuthority())
	{
		ServerTryAttach();
		return;
	}
	FHitResult Hit;
	if (ProbeClimbSurface(Hit))
	{
		BeginClimb(Hit);
	}
}

void UJTSWallClimbComponent::ServerTryAttach_Implementation()
{
	NextAttachRequestSeconds = -1.0f;
	TryAttachFromApproach();
}

void UJTSWallClimbComponent::SubmitClimbIntent(const FVector& WishDirection, bool bJump)
{
	const AActor* const Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}
	if (!Owner->HasAuthority())
	{
		ServerSubmitClimbIntent(WishDirection.GetSafeNormal(), bJump);
	}
	if (Owner->HasAuthority() || Owner->GetLocalRole() == ROLE_AutonomousProxy)
	{
		if (bJump && bClimbing)
		{
			if (Owner->HasAuthority())
			{
				EndClimb();
			}
			return;
		}
		if (!bClimbing || bStepActive || WishDirection.IsNearlyZero())
		{
			return;
		}
		if (Owner->HasAuthority())
		{
			StartStep(WishDirection);
		}
	}
}

void UJTSWallClimbComponent::ServerSubmitClimbIntent_Implementation(FVector_NetQuantizeNormal WishDirection, bool bJump)
{
	if (!HasIceAxesInHand())
	{
		if (bClimbing)
		{
			EndClimb();
		}
		return;
	}
	if (bJump)
	{
		if (bClimbing)
		{
			EndClimb();
		}
		return;
	}
	if (!bClimbing || bStepActive)
	{
		return;
	}
	StartStep(WishDirection);
}

void UJTSWallClimbComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* const Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(Character) || !Character->HasAuthority())
	{
		const UWorld* const World = GetWorld();
		const float Now = World != nullptr ? World->GetTimeSeconds() : 0.0f;
		if (bClimbing && StepStartWorldTime >= 0.0f)
		{
			const float Alpha = (Now - StepStartWorldTime) / FMath::Max(0.05f, StepSeconds);
			if (Alpha >= 0.0f && Alpha < 1.0f)
			{
				SwingAlpha = Alpha < 0.45f ? Alpha / 0.45f : 1.0f - (Alpha - 0.45f) / 0.55f;
			}
			else
			{
				SwingAlpha = 0.0f;
			}
		}
		return;
	}

	if (!HasIceAxesInHand() || Character->GetAttachParentActor() != nullptr)
	{
		if (bClimbing)
		{
			EndClimb();
		}
		return;
	}

	if (!bClimbing)
	{
		return;
	}

	if (bStepActive)
	{
		AdvanceStep(DeltaTime);
		return;
	}

	FHitResult StillThere;
	if (!ProbeClimbSurface(StillThere))
	{
		EndClimb();
		return;
	}
	SurfaceNormal = StillThere.ImpactNormal.GetSafeNormal();
	FaceSurface();
}
