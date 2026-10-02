#include "space/Components/JTSWallClimbComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSPlanetGravityComponent.h"
#include "space/Components/JTSStaminaComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/World/JTSPlanetAnchor.h"

FJTSClimbMotionSample FJTSClimbMotionSample::Evaluate(float Phase, EJTSClimbMotion Motion)
{
	Phase = FMath::Clamp(Phase, 0.0f, 1.0f);
	const float Release = Motion == EJTSClimbMotion::Leap ? 0.18f
		: Motion == EJTSClimbMotion::Descend ? 0.08f : 0.13f;
	const float CatchStart = Motion == EJTSClimbMotion::Leap ? 0.76f : 0.78f;
	FJTSClimbMotionSample Result;
	const float Move = FMath::SmoothStep(Release, CatchStart, Phase);
	// Downward movement gathers speed under gravity before the hands arrest it.
	Result.Travel = Motion == EJTSClimbMotion::Descend ? FMath::Pow(Move, 1.3f) : Move;
	Result.Load = FMath::SmoothStep(0.0f, Release, Phase)
		* (1.0f - FMath::SmoothStep(Release, Release + 0.18f, Phase));
	Result.Reach = FMath::SmoothStep(Release * 0.35f, Release + 0.19f, Phase)
		* (1.0f - FMath::SmoothStep(CatchStart, 0.94f, Phase));
	Result.Catch = FMath::SmoothStep(CatchStart - 0.08f, CatchStart + 0.07f, Phase)
		* (1.0f - FMath::SmoothStep(0.93f, 1.0f, Phase));
	Result.Settle = FMath::SmoothStep(CatchStart, 0.90f, Phase)
		* (1.0f - FMath::SmoothStep(0.90f, 1.0f, Phase));
	return Result;
}

UJTSWallClimbComponent::UJTSWallClimbComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UJTSWallClimbComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSWallClimbComponent, bClimbing);
	DOREPLIFETIME(UJTSWallClimbComponent, SurfaceNormal);
	DOREPLIFETIME(UJTSWallClimbComponent, StepDirection);
	DOREPLIFETIME(UJTSWallClimbComponent, bLeadHandLeft);
	DOREPLIFETIME(UJTSWallClimbComponent, bLeaping);
	DOREPLIFETIME(UJTSWallClimbComponent, bMantling);
	DOREPLIFETIME(UJTSWallClimbComponent, StepStartWorldTime);
	DOREPLIFETIME(UJTSWallClimbComponent, ActiveStepDuration);
}

float UJTSWallClimbComponent::GetStepAlpha() const
{
	const UWorld* World = GetWorld();
	if (!bClimbing || StepStartWorldTime < 0.0f || ActiveStepDuration <= 0.0f || World == nullptr) return 0.0f;
	const AGameStateBase* GameState = World->GetGameState();
	const float Now = IsValid(GameState) ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
	return FMath::Clamp((Now - StepStartWorldTime) / ActiveStepDuration, 0.0f, 1.0f);
}

void UJTSWallClimbComponent::OnRep_Climbing()
{
	AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	UCharacterMovementComponent* Movement = IsValid(Character) ? Character->GetCharacterMovement() : nullptr;
	if (!IsValid(Movement)) return;
	if (bClimbing)
	{
		if (!bSavedGravityScale)
		{
			SavedGravityScale = Movement->GravityScale;
			bSavedGravityScale = true;
		}
		Movement->GravityScale = 0.0f;
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Flying);
		LockBodyFacingToSurface();
		FaceSurface();
	}
	else
	{
		RestoreBodyFacingSettings();
		Movement->GravityScale = bSavedGravityScale ? SavedGravityScale : 1.0f;
		bSavedGravityScale = false;
		bStepActive = false;
		if (Movement->MovementMode == MOVE_Flying) Movement->SetMovementMode(MOVE_Falling);
	}
	if (UJTSPlanetGravityComponent* Gravity = Character->FindComponentByClass<UJTSPlanetGravityComponent>())
	{
		Gravity->SetSurfaceGravitySuspended(bClimbing);
	}
	Character->RefreshClimbEquipmentPresentation();
}

FVector UJTSWallClimbComponent::GetGravityUp() const
{
	const AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	const AJTSPlanetAnchor* Planet = IsValid(Character) ? Character->GetGameplayPlanet() : nullptr;
	if (IsValid(Planet)) return Planet->GetRadialUpVector(Character->GetActorLocation());
	const UCharacterMovementComponent* Movement = IsValid(Character) ? Character->GetCharacterMovement() : nullptr;
	const FVector Down = IsValid(Movement) ? Movement->GetGravityDirection() : FVector::DownVector;
	return Down.IsNearlyZero() ? FVector::UpVector : -Down.GetSafeNormal();
}

bool UJTSWallClimbComponent::IsValidSurfaceNormal(const FVector& Normal) const
{
	const float UpDot = FVector::DotProduct(Normal.GetSafeNormal(), GetGravityUp());
	return UpDot < FMath::Cos(FMath::DegreesToRadians(MinimumSlopeDegrees)) - KINDA_SMALL_NUMBER
		&& UpDot > FMath::Cos(FMath::DegreesToRadians(MaximumSlopeDegrees)) + KINDA_SMALL_NUMBER;
}

bool UJTSWallClimbComponent::CanClimbNow() const
{
	const AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	if (!IsValid(Character) || Character->IsBoarded() || Character->GetAttachParentActor() != nullptr) return false;
	const UJTSHealthComponent* Health = Character->GetHealthComponent();
	const UJTSStaminaComponent* Stamina = Character->GetStaminaComponent();
	return (!IsValid(Health) || !Health->IsDead()) && IsValid(Stamina) && Stamina->CanClimb();
}

bool UJTSWallClimbComponent::TraceClimbSurface(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	const AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	const UWorld* World = GetWorld();
	if (!IsValid(Character) || World == nullptr) return false;
	if (const AJTSPlanetAnchor* Planet = Character->GetGameplayPlanet(); IsValid(Planet) && Planet->HasGameplaySurface())
	{
		if (Planet->TraceGameplaySurfaceSegment(Start, End, OutHit)) return true;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JTSClimbSurface), true, Character);
	return World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params) && OutHit.bBlockingHit;
}

bool UJTSWallClimbComponent::ProbeClimbSurface(FHitResult& OutHit) const
{
	const AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	if (!IsValid(Character) || GetWorld() == nullptr) return false;
	const FVector Up = GetGravityUp();
	// Once attached, reach back along the actual surface normal. A horizontal ray
	// can miss a 40-degree planet slope even though the capsule is still touching it.
	const FVector Forward = bClimbing ? -FVector(SurfaceNormal).GetSafeNormal()
		: FVector::VectorPlaneProject(Character->GetActorForwardVector(), Up).GetSafeNormal();
	if (Forward.IsNearlyZero()) return false;
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Height = IsValid(Capsule) ? Capsule->GetScaledCapsuleHalfHeight() : 90.0f;
	const float Radius = IsValid(Capsule) ? Capsule->GetScaledCapsuleRadius() : 42.0f;
	const FVector WallRight = FVector::CrossProduct(bClimbing ? FVector(SurfaceNormal) : -Forward, Up).GetSafeNormal();
	OutHit = FHitResult();
	const float ProbeHeights[] = { 0.0f, 0.15f, -0.20f, -0.45f };
	for (const float HeightFraction : ProbeHeights)
	{
		const float SideOffsets[] = { 0.0f, -0.45f, 0.45f };
		for (const float SideFraction : SideOffsets)
		{
			if (!bClimbing && !FMath::IsNearlyZero(SideFraction)) continue;
			const FVector Start = Character->GetActorLocation() + Up * (Height * HeightFraction)
				+ WallRight * (Radius * SideFraction);
			FHitResult Candidate;
			if (TraceClimbSurface(Start, Start + Forward * ProbeDistance, Candidate)
				&& (IsValidSurfaceNormal(Candidate.ImpactNormal)
					|| (bClimbing && FVector::DotProduct(Candidate.ImpactNormal.GetSafeNormal(),
						FVector(SurfaceNormal).GetSafeNormal()) > 0.75f))
				&& (!OutHit.bBlockingHit || Candidate.Distance < OutHit.Distance))
			{
				Candidate.TraceStart = Start;
				OutHit = Candidate;
			}
		}
	}
	return OutHit.bBlockingHit;
}

void UJTSWallClimbComponent::BeginClimb(const FHitResult& Hit)
{
	AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	UCharacterMovementComponent* Movement = IsValid(Character) ? Character->GetCharacterMovement() : nullptr;
	if (!IsValid(Movement)) return;
	const FVector Up = GetGravityUp();
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = IsValid(Capsule) ? Capsule->GetScaledCapsuleRadius() : 42.0f;
	const float Height = IsValid(Capsule) ? Capsule->GetScaledCapsuleHalfHeight() : 90.0f;
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	const float CapsuleReach = Radius + FMath::Max(0.0f, Height - Radius)
		* FMath::Abs(FVector::DotProduct(Normal, IsValid(Capsule) ? Capsule->GetUpVector() : Up));
	const float ProbeHeight = FMath::Clamp(FVector::DotProduct(Hit.TraceStart - Character->GetActorLocation(), Up),
		-Height * 0.5f, Height * 0.5f);
	const FVector Target = Hit.ImpactPoint + Normal * (CapsuleReach + 7.0f)
		+ FVector::VectorPlaneProject(-Up * ProbeHeight, Normal);
	FHitResult MoveHit;
	if (!Character->SetActorLocation(Target, true, &MoveHit)
		&& FVector::DistSquared(Character->GetActorLocation(), Target) > 100.0f) return;
	SurfaceNormal = Normal;
	if (!bSavedGravityScale)
	{
		SavedGravityScale = Movement->GravityScale;
		bSavedGravityScale = true;
	}
	Movement->GravityScale = 0.0f;
	if (UJTSPlanetGravityComponent* Gravity = Character->FindComponentByClass<UJTSPlanetGravityComponent>())
	{
		Gravity->SetSurfaceGravitySuspended(true);
	}
	Movement->StopMovementImmediately();
	Movement->SetMovementMode(MOVE_Flying);
	bClimbing = true;
	bJumpGrabArmed = false;
	GripLocation = Character->GetActorLocation();
	GripSurfaceComponent = Hit.GetComponent();
	LockBodyFacingToSurface();
	MissingSurfaceSeconds = 0.0f;
	bStepActive = false;
	bHasPendingLeap = false;
	bLeaping = false;
	bMantling = false;
	bLeadHandLeft = true;
	StepDirection = Up;
	StepStartWorldTime = -1.0f;
	FaceSurface();
	Character->RefreshClimbEquipmentPresentation();
	Character->ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("Climb attached: %s Location=%s Normal=%s"),
		*GetNameSafe(Character), *GripLocation.ToCompactString(), *Normal.ToCompactString());
}

void UJTSWallClimbComponent::EndClimb(bool bWalkOff, const TCHAR* Reason)
{
	AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	UCharacterMovementComponent* Movement = IsValid(Character) ? Character->GetCharacterMovement() : nullptr;
	UE_LOG(LogTemp, Log, TEXT("Climb ended: %s Reason=%s Location=%s Stamina=%.1f"),
		*GetNameSafe(Character), Reason, IsValid(Character) ? *Character->GetActorLocation().ToCompactString() : TEXT("None"),
		IsValid(Character) && IsValid(Character->GetStaminaComponent())
			? Character->GetStaminaComponent()->GetCurrentStamina() : -1.0f);
	bClimbing = false;
	GripSurfaceComponent.Reset();
	RestoreBodyFacingSettings();
	MissingSurfaceSeconds = 0.0f;
	bStepActive = false;
	bHasPendingLeap = false;
	bLeaping = false;
	bMantling = false;
	StepStartWorldTime = -1.0f;
	if (IsValid(Character))
	{
		if (UJTSPlanetGravityComponent* Gravity = Character->FindComponentByClass<UJTSPlanetGravityComponent>())
		{
			Gravity->SetSurfaceGravitySuspended(false);
		}
		Character->ForceNetUpdate();
	}
	if (IsValid(Movement))
	{
		if (bSavedGravityScale) Movement->GravityScale = SavedGravityScale;
		bSavedGravityScale = false;
		Movement->SetMovementMode(bWalkOff ? MOVE_Walking : MOVE_Falling);
	}
	if (IsValid(Character)) Character->RefreshClimbEquipmentPresentation();
}

void UJTSWallClimbComponent::FaceSurface()
{
	AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	if (!IsValid(Character)) return;
	const FVector Up = GetGravityUp();
	const FVector IntoWall = FVector::VectorPlaneProject(-FVector(SurfaceNormal), Up).GetSafeNormal();
	if (!IntoWall.IsNearlyZero()) Character->SetActorRotation(FRotationMatrix::MakeFromZX(Up, IntoWall).Rotator());
}

void UJTSWallClimbComponent::LockBodyFacingToSurface()
{
	AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	UCharacterMovementComponent* Movement = IsValid(Character) ? Character->GetCharacterMovement() : nullptr;
	if (!IsValid(Movement)) return;
	if (!bSavedFacingSettings)
	{
		bSavedControllerPitch = Character->bUseControllerRotationPitch;
		bSavedControllerYaw = Character->bUseControllerRotationYaw;
		bSavedControllerRoll = Character->bUseControllerRotationRoll;
		bSavedOrientToMovement = Movement->bOrientRotationToMovement;
		bSavedControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
		bSavedFacingSettings = true;
	}
	Character->bUseControllerRotationPitch = false;
	Character->bUseControllerRotationYaw = false;
	Character->bUseControllerRotationRoll = false;
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = false;
}

void UJTSWallClimbComponent::RestoreBodyFacingSettings()
{
	if (!bSavedFacingSettings) return;
	AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	UCharacterMovementComponent* Movement = IsValid(Character) ? Character->GetCharacterMovement() : nullptr;
	if (IsValid(Movement))
	{
		Character->bUseControllerRotationPitch = bSavedControllerPitch;
		Character->bUseControllerRotationYaw = bSavedControllerYaw;
		Character->bUseControllerRotationRoll = bSavedControllerRoll;
		Movement->bOrientRotationToMovement = bSavedOrientToMovement;
		Movement->bUseControllerDesiredRotation = bSavedControllerDesiredRotation;
	}
	bSavedFacingSettings = false;
}

bool UJTSWallClimbComponent::TryTopOut(const FVector& WishDirection)
{
	AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!IsValid(Character) || World == nullptr || FVector::DotProduct(WishDirection, GetGravityUp()) < 0.5f) return false;
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!IsValid(Capsule) || !IsValid(Movement) || bMantling) return false;
	const FVector Up = GetGravityUp();
	const FVector IntoWall = FVector::VectorPlaneProject(-FVector(SurfaceNormal), Up).GetSafeNormal();
	const FVector AwayFromWall = -IntoWall;
	if (IntoWall.IsNearlyZero()) return false;
	const float Height = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FVector Start = Character->GetActorLocation();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, Height);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JTSClimbMantle), false, Character);
	// Search from deeper onto the ledge toward the lip. A center supported only
	// by the thin edge is a poor place to switch back to walking.
	const float ForwardDepths[] = { Radius + 100.0f, Radius + 60.0f, Radius + 20.0f };
	for (const float Depth : ForwardDepths)
	{
		const FVector TraceStart = Start + IntoWall * Depth + Up * (Height + 140.0f);
		FHitResult FloorHit;
		if (!TraceClimbSurface(TraceStart, TraceStart - Up * (Height + 300.0f), FloorHit)
			|| !FloorHit.bBlockingHit) continue;
		const float FloorUp = FVector::DotProduct(FloorHit.ImpactNormal.GetSafeNormal(), Up);
		if (FloorUp < Movement->GetWalkableFloorZ()) continue;
		const float FloorRise = FVector::DotProduct(FloorHit.ImpactPoint - Start, Up);
		if (FloorRise > Height * 1.05f || FloorRise < -Height * 0.45f) continue;
		const float CapsuleSupport = Radius + (Height - Radius) * FloorUp;
		const FVector Landing = FloorHit.ImpactPoint + Up * ((CapsuleSupport + 4.0f) / FloorUp);
		const FVector Lift = Start + Up * (FVector::DotProduct(Landing - Start, Up) + 12.0f)
			+ AwayFromWall * 14.0f;
		if (World->OverlapBlockingTestByChannel(Landing, Character->GetActorQuat(), ECC_Pawn, Shape, Params)) continue;
		FHitResult PathHit;
		if (World->SweepSingleByChannel(PathHit, Start, Lift, Character->GetActorQuat(), ECC_Pawn, Shape, Params)
			|| World->SweepSingleByChannel(PathHit, Lift, Landing, Character->GetActorQuat(), ECC_Pawn, Shape, Params)) continue;
		MantleStart = Start;
		MantleLift = Lift;
		MantleLanding = Landing;
		bMantling = true;
		bStepActive = false;
		bHasPendingLeap = false;
		bLeaping = false;
		StepDirection = Up;
		StepElapsed = 0.0f;
		ActiveStepDuration = MantleSeconds;
		StepStartWorldTime = World->GetTimeSeconds();
		Character->ForceNetUpdate();
		return true;
	}
	return false;
}

void UJTSWallClimbComponent::AdvanceMantle(float DeltaTime)
{
	AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	if (!IsValid(Character) || !bMantling) return;
	StepElapsed += FMath::Max(0.0f, DeltaTime);
	const float Alpha = FMath::Clamp(StepElapsed / FMath::Max(0.1f, ActiveStepDuration), 0.0f, 1.0f);
	const float LiftAlpha = FMath::SmoothStep(0.0f, 0.52f, Alpha);
	const float CrossAlpha = FMath::SmoothStep(0.52f, 1.0f, Alpha);
	const FVector Lifted = FMath::Lerp(MantleStart, MantleLift, LiftAlpha);
	const FVector Target = FMath::Lerp(Lifted, MantleLanding, CrossAlpha);
	FHitResult MoveHit;
	Character->SetActorLocation(Target, true, &MoveHit);
	if (MoveHit.bBlockingHit && FVector::DistSquared(Character->GetActorLocation(), Target) > FMath::Square(8.0f))
	{
		Character->SetActorLocation(GripLocation, true);
		bMantling = false;
		StepStartWorldTime = -1.0f;
		Character->ForceNetUpdate();
		return;
	}
	if (Alpha >= 1.0f)
	{
		EndClimb(true, TEXT("MantleComplete"));
	}
}

bool UJTSWallClimbComponent::TryBottomOut()
{
	AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	const UCapsuleComponent* Capsule = IsValid(Character) ? Character->GetCapsuleComponent() : nullptr;
	UCharacterMovementComponent* Movement = IsValid(Character) ? Character->GetCharacterMovement() : nullptr;
	if (!IsValid(Capsule) || !IsValid(Movement)) return false;
	const FVector Up = GetGravityUp();
	const float Height = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Start = Character->GetActorLocation() + FVector(SurfaceNormal) * 12.0f + Up * 20.0f;
	FHitResult FloorHit;
	if (!TraceClimbSurface(Start, Start - Up * (Height + 90.0f), FloorHit)
		|| !FloorHit.bBlockingHit
		|| FVector::DotProduct(FloorHit.ImpactNormal, Up) < Movement->GetWalkableFloorZ()) return false;
	const float FeetClearance = FVector::DotProduct(Character->GetActorLocation() - FloorHit.ImpactPoint, Up) - Height;
	if (FeetClearance < -10.0f || FeetClearance > 24.0f) return false;
	FHitResult MoveHit;
	const FVector Landing = FloorHit.ImpactPoint + Up * (Height + 3.0f);
	if (!Character->SetActorLocation(Landing, true, &MoveHit)
		&& FVector::DistSquared(Character->GetActorLocation(), Landing) > FMath::Square(12.0f)) return false;
	EndClimb(true, TEXT("BottomOut"));
	return true;
}

void UJTSWallClimbComponent::StartStep(const FVector& WishDirection, bool bLeap)
{
	AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	UWorld* World = GetWorld();
	UJTSStaminaComponent* Stamina = IsValid(Character) ? Character->GetStaminaComponent() : nullptr;
	if (!bClimbing || bMantling || !IsValid(Character) || World == nullptr || !IsValid(Stamina)) return;
	if (bStepActive)
	{
		// A press during a held movement key is common. Execute it as soon as the
		// current grip is planted instead of silently discarding the jump.
		if (bLeap && !WishDirection.IsNearlyZero())
		{
			PendingLeapDirection = WishDirection.GetSafeNormal();
			bHasPendingLeap = true;
		}
		return;
	}
	const FVector Up = GetGravityUp();
	const FVector Normal = FVector(SurfaceNormal).GetSafeNormal();
	const FVector WallUp = FVector::VectorPlaneProject(Up, Normal).GetSafeNormal();
	const FVector WallRight = FVector::CrossProduct(Normal, Up).GetSafeNormal();
	const FVector Intent = WishDirection.GetSafeNormal();
	const float Vertical = FVector::DotProduct(Intent, WallUp);
	const float Horizontal = FVector::DotProduct(Intent, WallRight);
	if (Intent.IsNearlyZero()) return;
	const bool bVertical = FMath::Abs(Vertical) >= FMath::Abs(Horizontal);
	const FVector Along = bLeap
		? (WallUp * FMath::Max(0.0f, Vertical) + WallRight * Horizontal).GetSafeNormal()
		: (bVertical ? WallUp * FMath::Sign(Vertical) : WallRight * FMath::Sign(Horizontal));
	if (Along.IsNearlyZero()) return;
	const bool bDescending = !bLeap && bVertical && Vertical < -0.5f;
	if (bDescending && TryBottomOut()) return;
	if (!bDescending && TryTopOut(Along)) return;
	const float Distance = bLeap ? LeapDistance : bDescending ? DescendDistance : StepDistance;
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = IsValid(Capsule) ? Capsule->GetScaledCapsuleRadius() : 42.0f;
	const float Height = IsValid(Capsule) ? Capsule->GetScaledCapsuleHalfHeight() : 90.0f;
	FHitResult NextHit;
	FVector Target = FVector::ZeroVector;
	bool bFoundGrip = false;
	// Long moves can cross a curved mesh seam. Prefer the full stride, then a
	// nearer reachable hold; never let a missed trace eject the character.
	const float ReachFractions[] = { 1.0f, 0.82f, 0.65f };
	for (const float ReachFraction : ReachFractions)
	{
		const float ReachDistance = Distance * ReachFraction;
		const FVector TraceStart = Character->GetActorLocation() + Along * ReachDistance
			+ Up * (Height * 0.15f) + Normal * 20.0f;
		FHitResult Candidate;
		if (!TraceClimbSurface(TraceStart, TraceStart - Normal * (ProbeDistance + 40.0f), Candidate)
			|| !Candidate.bBlockingHit || !IsValidSurfaceNormal(Candidate.ImpactNormal)) continue;
		const FVector CandidateNormal = Candidate.ImpactNormal.GetSafeNormal();
		const float CapsuleReach = Radius + FMath::Max(0.0f, Height - Radius)
			* FMath::Abs(FVector::DotProduct(CandidateNormal, IsValid(Capsule) ? Capsule->GetUpVector() : Up));
		const FVector CandidateTarget = Candidate.ImpactPoint + CandidateNormal * (CapsuleReach + 7.0f)
			+ FVector::VectorPlaneProject(-Up * (Height * 0.15f), CandidateNormal);
		if (FVector::DotProduct(CandidateTarget - Character->GetActorLocation(), Along) < ReachDistance * 0.4f
			|| FVector::DistSquared(Character->GetActorLocation(), CandidateTarget) > FMath::Square(ReachDistance + 55.0f)) continue;
		NextHit = Candidate;
		Target = CandidateTarget;
		bFoundGrip = true;
		break;
	}
	if (!bFoundGrip)
	{
		if (bDescending && TryBottomOut()) return;
		TryTopOut(Along);
		return;
	}
	const FVector NextNormal = NextHit.ImpactNormal.GetSafeNormal();
	if (!Stamina->Spend(bLeap ? LeapCost : StepCost)) return;
	StepStart = Character->GetActorLocation();
	StepTarget = Target;
	StepTargetNormal = NextNormal;
	StepTargetSurfaceComponent = NextHit.GetComponent();
	StepDirection = Along;
	bLeadHandLeft = bVertical ? !bLeadHandLeft : Horizontal < 0.0f;
	bLeaping = bLeap;
	StepStartWorldTime = World->GetTimeSeconds();
	ActiveStepDuration = bLeap ? LeapSeconds : bDescending ? DescendSeconds : StepSeconds;
	StepElapsed = 0.0f;
	bStepActive = true;
	Character->ForceNetUpdate();
}

void UJTSWallClimbComponent::AdvanceStep(float DeltaTime)
{
	AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	if (!IsValid(Character) || !bStepActive) return;
	StepElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(StepElapsed / FMath::Max(0.1f, ActiveStepDuration), 0.0f, 1.0f);
	// Lift the capsule clear of convex and uneven meshes while moving between grips.
	const bool bDescending = !bLeaping
		&& FVector::DotProduct(FVector(StepDirection), GetGravityUp()) < -0.5f;
	const EJTSClimbMotion Motion = bLeaping ? EJTSClimbMotion::Leap
		: bDescending ? EJTSClimbMotion::Descend : EJTSClimbMotion::Step;
	const FJTSClimbMotionSample MotionSample = FJTSClimbMotionSample::Evaluate(Alpha, Motion);
	const float Clearance = bLeaping ? 32.0f : bDescending ? 18.0f : 24.0f;
	const FVector Target = FMath::Lerp(StepStart, StepTarget, MotionSample.Travel)
		+ FVector(SurfaceNormal) * (Clearance * FMath::Sin(PI * MotionSample.Travel));
	FHitResult SweepHit;
	Character->SetActorLocation(Target, true, &SweepHit);
	const bool bBlocked = SweepHit.bBlockingHit
		&& FVector::DistSquared(Character->GetActorLocation(), Target) > FMath::Square(8.0f);
	if (bBlocked || Alpha >= 1.0f)
	{
		if (bBlocked)
		{
			if (bDescending && TryBottomOut()) return;
			Character->SetActorLocation(GripLocation, true);
		}
		else
		{
			SurfaceNormal = StepTargetNormal.GetSafeNormal();
			GripLocation = Character->GetActorLocation();
			GripSurfaceComponent = StepTargetSurfaceComponent;
			MissingSurfaceSeconds = 0.0f;
		}
		// A blocked move is a missed grip, not an instruction to jump off the wall.
		bStepActive = false;
		bLeaping = false;
		StepStartWorldTime = -1.0f;
		if (bHasPendingLeap)
		{
			const FVector LeapDirection = PendingLeapDirection;
			bHasPendingLeap = false;
			StartStep(LeapDirection, true);
		}
	}
	if (bClimbing) FaceSurface();
}

bool UJTSWallClimbComponent::TryAutoAttach()
{
	AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!IsValid(Owner) || World == nullptr || bClimbing) return false;
	const float Now = World->GetTimeSeconds();
	if (Now < NextAttachRequestSeconds) return false;
	NextAttachRequestSeconds = Now + 0.18f;
	if (!CanClimbNow()) return false;
	FHitResult Hit;
	if (!ProbeClimbSurface(Hit)) return false;
	if (!Owner->HasAuthority())
	{
		ServerTryAutoAttach();
		return true;
	}
	BeginClimb(Hit);
	return bClimbing;
}

void UJTSWallClimbComponent::ServerTryAutoAttach_Implementation()
{
	TryAutoAttach();
}

void UJTSWallClimbComponent::ArmJumpGrab()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || bClimbing) return;
	if (!Owner->HasAuthority())
	{
		ServerArmJumpGrab();
		return;
	}
	bJumpGrabArmed = true;
}

void UJTSWallClimbComponent::ServerArmJumpGrab_Implementation()
{
	ArmJumpGrab();
}

void UJTSWallClimbComponent::TryJumpLandingGrip(const FHitResult& LandingHit)
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || bClimbing || !bJumpGrabArmed) return;
	bJumpGrabArmed = false;
	if (!LandingHit.bBlockingHit || !IsValidSurfaceNormal(LandingHit.ImpactNormal)
		|| !CanClimbNow()) return;
	BeginClimb(LandingHit);
}

void UJTSWallClimbComponent::ToggleAttach()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return;
	if (!Owner->HasAuthority())
	{
		ServerToggleAttach();
		return;
	}
	if (bClimbing)
	{
		EndClimb(false, TEXT("Manual"));
		return;
	}
	const UWorld* World = GetWorld();
	const float Now = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	if (Now < NextAttachRequestSeconds || !CanClimbNow()) return;
	NextAttachRequestSeconds = Now + 0.35f;
	FHitResult Hit;
	if (ProbeClimbSurface(Hit)) BeginClimb(Hit);
}

void UJTSWallClimbComponent::ServerToggleAttach_Implementation()
{
	ToggleAttach();
}

void UJTSWallClimbComponent::SubmitClimbIntent(const FVector& WishDirection)
{
	if (GetOwner() == nullptr) return;
	if (GetOwner()->HasAuthority()) StartStep(WishDirection, false);
	else ServerSubmitClimbIntent(WishDirection.GetSafeNormal());
}

void UJTSWallClimbComponent::ServerSubmitClimbIntent_Implementation(FVector_NetQuantizeNormal WishDirection)
{
	StartStep(WishDirection, false);
}

void UJTSWallClimbComponent::SubmitClimbLeap(const FVector& WishDirection)
{
	if (GetOwner() == nullptr) return;
	if (GetOwner()->HasAuthority()) StartStep(WishDirection, true);
	else ServerSubmitClimbLeap(WishDirection.GetSafeNormal());
}

void UJTSWallClimbComponent::ServerSubmitClimbLeap_Implementation(FVector_NetQuantizeNormal WishDirection)
{
	StartStep(WishDirection, true);
}

void UJTSWallClimbComponent::SubmitClimbDrop()
{
	if (GetOwner() == nullptr) return;
	if (GetOwner()->HasAuthority())
	{
		if (!bClimbing) return;
		const UWorld* World = GetWorld();
		NextAttachRequestSeconds = (World != nullptr ? World->GetTimeSeconds() : 0.0f) + 0.65f;
		EndClimb(false, TEXT("DirectedDrop"));
	}
	else ServerSubmitClimbDrop();
}

void UJTSWallClimbComponent::ServerSubmitClimbDrop_Implementation()
{
	SubmitClimbDrop();
}

void UJTSWallClimbComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	if (!IsValid(Character)) return;
	if (!bClimbing)
	{
		// Only an armed jump can auto-catch. Wait for descent and near contact so
		// walking past a wall or jumping away from it cannot start a climb.
		if (!Character->HasAuthority() || !bJumpGrabArmed) return;
		const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		const FVector Up = GetGravityUp();
		if (!IsValid(Movement) || !IsValid(Capsule) || !Movement->IsFalling()
			|| FVector::DotProduct(Movement->Velocity, Up) >= -10.0f || !CanClimbNow()) return;
		FHitResult Hit;
		const float Radius = Capsule->GetScaledCapsuleRadius();
		const float Height = Capsule->GetScaledCapsuleHalfHeight();
		if (ProbeClimbSurface(Hit))
		{
			const float CapsuleReach = Radius + FMath::Max(0.0f, Height - Radius)
				* FMath::Abs(FVector::DotProduct(Hit.ImpactNormal.GetSafeNormal(), Capsule->GetUpVector()));
			if (Hit.Distance <= CapsuleReach + JumpGrabSurfaceGapCm) BeginClimb(Hit);
		}
		return;
	}
	LockBodyFacingToSurface();
	FaceSurface();
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		if (Movement->MovementMode != MOVE_Flying) Movement->SetMovementMode(MOVE_Flying);
		Movement->StopMovementImmediately();
	}
	Character->ConsumeMovementInputVector();
	if (!Character->HasAuthority()) return;
	UJTSStaminaComponent* Stamina = Character->GetStaminaComponent();
	const UJTSHealthComponent* Health = Character->GetHealthComponent();
	if (Character->IsBoarded() || Character->GetAttachParentActor() != nullptr
		|| (IsValid(Health) && Health->IsDead()) || !IsValid(Stamina)
		|| !Stamina->Spend(FMath::Max(0.0f, DeltaTime) * ClingDrainPerSecond))
	{
		EndClimb(false, TEXT("InvalidStateOrExhausted"));
		return;
	}
	if (bStepActive)
	{
		AdvanceStep(DeltaTime);
		return;
	}
	if (bMantling)
	{
		AdvanceMantle(DeltaTime);
		return;
	}
	// A stationary grip is an anchored state. Camera orbit, stale movement input,
	// and small network corrections must not translate the capsule away from it.
	if (FVector::DistSquared(Character->GetActorLocation(), GripLocation) > FMath::Square(3.0f))
	{
		Character->SetActorLocation(GripLocation, true);
	}
	FHitResult Hit;
	if (!ProbeClimbSurface(Hit))
	{
		if (GripSurfaceComponent.IsValid()
			&& GripSurfaceComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision
			&& FVector::DistSquared(Character->GetActorLocation(), GripLocation) <= FMath::Square(12.0f))
		{
			// A still-valid static grip does not disappear because one triangle trace misses.
			MissingSurfaceSeconds = 0.0f;
			return;
		}
		MissingSurfaceSeconds += FMath::Max(0.0f, DeltaTime);
		if (MissingSurfaceSeconds >= ContactGraceSeconds) EndClimb(false, TEXT("LostSurface"));
		return;
	}
	MissingSurfaceSeconds = 0.0f;
	FaceSurface();
}
