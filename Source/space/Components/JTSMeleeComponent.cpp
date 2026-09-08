#include "space/Components/JTSMeleeComponent.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSPlayerEquipmentComponent.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "TimerManager.h"

namespace
{
	void AddOwnerAndAttachedActorsToIgnoreList(FCollisionQueryParams& QueryParams, const APawn* AttackingPawn)
	{
		if (!IsValid(AttackingPawn))
		{
			return;
		}

		QueryParams.AddIgnoredActor(AttackingPawn);

		TArray<AActor*> AttachedActors;
		AttackingPawn->GetAttachedActors(AttachedActors, true, true);
		for (AActor* const AttachedActor : AttachedActors)
		{
			QueryParams.AddIgnoredActor(AttachedActor);
		}
	}
}

UJTSMeleeComponent::UJTSMeleeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UJTSMeleeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Cast<APawn>(GetOwner()) == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("JTSMeleeComponent on '%s' requires a pawn owner."), *GetNameSafe(GetOwner()));
		return;
	}

	RefreshMeleeTarget();
	if (UWorld* const World = GetWorld(); World != nullptr && TargetRefreshInterval > 0.0f)
	{
		World->GetTimerManager().SetTimer(
			TargetRefreshTimerHandle,
			this,
			&UJTSMeleeComponent::RefreshMeleeTarget,
			TargetRefreshInterval,
			true);
	}
}

void UJTSMeleeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TargetRefreshTimerHandle);
	}
	ClearAttackFailSafeTimer();

	CurrentMeleeTarget = nullptr;
	HitActorsThisSwing.Reset();
	bAttackHeld = false;
	bAttackBuffered = false;
	bIsAttacking = false;
	CurrentAttackType = EJTSAttackType::Punch;
	NextAttackTime = 0.0;
	Super::EndPlay(EndPlayReason);
}

void UJTSMeleeComponent::RefreshMeleeTarget()
{
	SetCurrentMeleeTarget(FindBestMeleeTarget(Cast<APawn>(GetOwner())));
}

void UJTSMeleeComponent::AttackPressed()
{
	bAttackHeld = true;
	if (bIsAttacking)
	{
		bAttackBuffered = true;
		return;
	}

	StartAttack();
}

void UJTSMeleeComponent::AttackReleased()
{
	bAttackHeld = false;
}

void UJTSMeleeComponent::StartAttack()
{
	if (bIsAttacking || !IsValid(Cast<APawn>(GetOwner())))
	{
		return;
	}

	bAttackBuffered = false;
	BeginAttack(ResolveAttackType());
}

void UJTSMeleeComponent::StopAttack()
{
	EndAttackState();
}

void UJTSMeleeComponent::TryChainAttack()
{
	if (!bIsAttacking || (!bAttackHeld && !bAttackBuffered))
	{
		return;
	}

	// A hold or any number of rapid presses still produces only one following punch.
	bAttackBuffered = false;
	BeginAttack(CurrentAttackType);
}

void UJTSMeleeComponent::FinishCurrentAttack()
{
	if (!bIsAttacking)
	{
		return;
	}

	// This also covers a press that lands after the attack-chain notify but before montage end.
	if (bAttackHeld || bAttackBuffered)
	{
		TryChainAttack();
		return;
	}

	EndAttackState();
}

void UJTSMeleeComponent::BeginAttack(EJTSAttackType AttackType)
{
	if (!IsValid(Cast<APawn>(GetOwner())))
	{
		EndAttackState();
		return;
	}

	// Every montage segment is one new swing. Repeated AttackHit notifies can never re-hit this set.
	HitActorsThisSwing.Reset();
	CurrentAttackType = AttackType;
	bIsAttacking = true;
	ResetAttackFailSafeTimer();
	OnAttackStarted.Broadcast(CurrentAttackType);
}

void UJTSMeleeComponent::ResetAttackFailSafeTimer()
{
	UWorld* const World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(AttackFailSafeTimerHandle);
	World->GetTimerManager().SetTimer(
		AttackFailSafeTimerHandle,
		this,
		&UJTSMeleeComponent::HandleAttackFailSafeTimeout,
		FMath::Max(AttackFailSafeTime, 1.0f),
		false);
}

void UJTSMeleeComponent::ClearAttackFailSafeTimer()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackFailSafeTimerHandle);
	}
}

void UJTSMeleeComponent::HandleAttackFailSafeTimeout()
{
	// Missing notifies must never leave the component attacking forever or create an automatic combo.
	EndAttackState();
}

void UJTSMeleeComponent::EndAttackState()
{
	ClearAttackFailSafeTimer();

	bIsAttacking = false;
	bAttackBuffered = false;
	HitActorsThisSwing.Reset();
}

void UJTSMeleeComponent::PerformHitCheck()
{
	APawn* const AttackingPawn = Cast<APawn>(GetOwner());
	UWorld* const World = GetWorld();
	if (!bIsAttacking || !IsValid(AttackingPawn) || !IsValid(World) || PunchMaxTargets < 1)
	{
		return;
	}

	FVector CameraLocation;
	FVector AimDirection;
	if (!GetPlayerAimView(AttackingPawn, CameraLocation, AimDirection))
	{
		return;
	}

	const FVector AimTraceEnd = CameraLocation + AimDirection * FMath::Max(0.0f, MeleeAimTraceDistance);
	if (bDebugMeleeAim)
	{
		DrawDebugLine(World, CameraLocation, AimTraceEnd, FColor::Cyan, false, 1.5f, 0, 1.5f);
		DrawDebugSphere(World, AimTraceEnd, FMath::Max(1.0f, MeleeAimAssistRadius), 12, FColor::Cyan, false, 1.5f, 0, 0.75f);
		DrawDebugSphere(World, AttackingPawn->GetActorLocation(), PunchRange, 24, FColor(255, 180, 0), false, 1.5f, 0, 0.5f);
	}

	AActor* Candidate = nullptr;
	FVector CandidateLocation = FVector::ZeroVector;
	if (!FindBestAimCandidate(AttackingPawn, Candidate, CandidateLocation, false))
	{
		return;
	}

	const FVector TargetLogicalLocation = Candidate->GetActorLocation();
	const bool bWithinRange = IsWithinPunchRange(AttackingPawn, TargetLogicalLocation);
	const bool bHasLineOfSight = bWithinRange && HasMeleeLineOfSight(AttackingPawn, Candidate, CandidateLocation);
	if (bDebugMeleeAim)
	{
		const FColor CandidateColor = bWithinRange && bHasLineOfSight ? FColor::Yellow : FColor::Red;
		DrawDebugSphere(World, CandidateLocation, 18.0f, 12, CandidateColor, false, 1.5f, 0, 1.5f);
		DrawDebugLine(World, AttackingPawn->GetActorLocation(), TargetLogicalLocation, CandidateColor, false, 1.5f, 0, 1.5f);
	}

	if (!bWithinRange || !bHasLineOfSight || HitActorsThisSwing.Contains(Candidate))
	{
		return;
	}

	const EJTSMeleeAttackType AttackType = GetCurrentAttackType();
	const float Damage = GetDamageForAttackType(AttackType);
	const bool bApplied = ApplyAttackToTarget(Candidate, AttackingPawn, AttackType);
	if (bApplied)
	{
		// PunchMaxTargets is intentionally clamped to one. Keeping a per-swing set also protects us from
		// duplicate AttackHit notifies or overlapping trace frames.
		HitActorsThisSwing.Add(Candidate);
	}

	if (bDebugMeleeAim)
	{
		const FColor FinalColor = bApplied ? FColor::Green : FColor::Red;
		DrawDebugSphere(World, CandidateLocation, 24.0f, 12, FinalColor, false, 1.5f, 0, 2.0f);
		DrawDebugString(
			World,
			CandidateLocation + FVector(0.0f, 0.0f, 28.0f),
			FString::Printf(TEXT("%s  Damage %.1f"), *GetNameSafe(Candidate), Damage),
			nullptr,
			FinalColor,
			1.5f,
			true);
	}
}

bool UJTSMeleeComponent::TryAttack()
{
	UWorld* const World = GetWorld();
	APawn* const AttackingPawn = Cast<APawn>(GetOwner());
	const AJTSMoonGameMode* const MoonGameMode = World != nullptr ? World->GetAuthGameMode<AJTSMoonGameMode>() : nullptr;
	if (!IsValid(AttackingPawn) || !IsValid(MoonGameMode) || !IsMoonMeleeAvailable())
	{
		return false;
	}

	const double CurrentTime = static_cast<double>(World->GetTimeSeconds());
	if (CurrentTime < NextAttackTime)
	{
		return false;
	}

	RefreshMeleeTarget();
	AActor* const Target = CurrentMeleeTarget.Get();
	if (!IsValidMeleeTarget(Target, AttackingPawn)
		|| (bIsAttacking && HitActorsThisSwing.Contains(Target)))
	{
		return false;
	}

	const EJTSMeleeAttackType AttackType = GetCurrentAttackType();
	if (!ApplyAttackToTarget(Target, AttackingPawn, AttackType))
	{
		return false;
	}
	if (bIsAttacking)
	{
		HitActorsThisSwing.Add(Target);
	}

	NextAttackTime = CurrentTime + static_cast<double>(MoonGameMode->GetAttackCooldown());
	RefreshMeleeTarget();
	return true;
}

AActor* UJTSMeleeComponent::GetCurrentMeleeTarget() const
{
	return CurrentMeleeTarget.Get();
}

EJTSMeleeAttackType UJTSMeleeComponent::GetCurrentAttackType() const
{
	const APawn* const AttackingPawn = Cast<APawn>(GetOwner());
	const UJTSPlayerEquipmentComponent* const EquipmentComponent = IsValid(AttackingPawn)
		? AttackingPawn->FindComponentByClass<UJTSPlayerEquipmentComponent>()
		: nullptr;
	if (!IsValid(EquipmentComponent))
	{
		return EJTSMeleeAttackType::Punch;
	}

	switch (EquipmentComponent->GetEquipmentSlot(EquipmentComponent->GetSelectedEquipmentSlotIndex()))
	{
	case EJTSEquipmentType::Knife:
		return EJTSMeleeAttackType::Knife;

	case EJTSEquipmentType::Axe:
		return EJTSMeleeAttackType::Axe;

	default:
		return EJTSMeleeAttackType::Punch;
	}
}

EJTSAttackType UJTSMeleeComponent::ResolveAttackType() const
{
	const APawn* const AttackingPawn = Cast<APawn>(GetOwner());
	const UJTSPlayerEquipmentComponent* const EquipmentComponent = IsValid(AttackingPawn)
		? AttackingPawn->FindComponentByClass<UJTSPlayerEquipmentComponent>()
		: nullptr;
	if (!IsValid(EquipmentComponent))
	{
		return EJTSAttackType::Punch;
	}

	switch (EquipmentComponent->GetEquipmentSlot(EquipmentComponent->GetSelectedEquipmentSlotIndex()))
	{
	case EJTSEquipmentType::Knife:
	case EJTSEquipmentType::Axe:
		return EJTSAttackType::MeleeWeapon;

	case EJTSEquipmentType::Pickaxe:
		return EJTSAttackType::Tool;

	default:
		return EJTSAttackType::Punch;
	}
}

AActor* UJTSMeleeComponent::FindBestMeleeTarget(APawn* AttackingPawn) const
{
	AActor* Target = nullptr;
	FVector TargetLocation = FVector::ZeroVector;
	if (!FindBestAimCandidate(AttackingPawn, Target, TargetLocation, true)
		|| !IsWithinPunchRange(AttackingPawn, Target != nullptr ? Target->GetActorLocation() : FVector::ZeroVector)
		|| !HasMeleeLineOfSight(AttackingPawn, Target, TargetLocation))
	{
		return nullptr;
	}

	return Target;
}

bool UJTSMeleeComponent::FindBestAimCandidate(
	APawn* AttackingPawn,
	AActor*& OutTarget,
	FVector& OutTargetLocation,
	bool bRequireMeleeTargetInterface) const
{
	OutTarget = nullptr;
	OutTargetLocation = FVector::ZeroVector;

	UWorld* const World = GetWorld();
	FVector CameraLocation;
	FVector AimDirection;
	if (!IsValid(AttackingPawn)
		|| !IsValid(World)
		|| !GetPlayerAimView(AttackingPawn, CameraLocation, AimDirection)
		|| MeleeAimTraceDistance <= KINDA_SMALL_NUMBER
		|| MeleeAimAssistRadius < 0.0f)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JTSMeleeAimTrace), false, AttackingPawn);
	AddOwnerAndAttachedActorsToIgnoreList(QueryParams, AttackingPawn);

	TArray<FHitResult> HitResults;
	const FCollisionShape AimShape = FCollisionShape::MakeSphere(MeleeAimAssistRadius);
	const FVector TraceEnd = CameraLocation + AimDirection * MeleeAimTraceDistance;
	if (!World->SweepMultiByChannel(HitResults, CameraLocation, TraceEnd, FQuat::Identity, ECC_Visibility, AimShape, QueryParams))
	{
		return false;
	}

	float BestAimAlignment = -1.0f;
	float BestPawnDistanceSquared = TNumericLimits<float>::Max();
	for (const FHitResult& HitResult : HitResults)
	{
		AActor* const Candidate = HitResult.GetActor();
		const bool bIsValidCandidate = bRequireMeleeTargetInterface
			? IsValidMeleeTarget(Candidate, AttackingPawn)
			: IsValidDamageTarget(Candidate, AttackingPawn);
		if (!bIsValidCandidate)
		{
			continue;
		}

		FVector CandidateLocation = HitResult.ImpactPoint;
		if (CandidateLocation.IsNearlyZero())
		{
			CandidateLocation = Candidate->GetActorLocation();
		}
		FVector CameraToCandidate = CandidateLocation - CameraLocation;
		if (CameraToCandidate.IsNearlyZero())
		{
			CameraToCandidate = Candidate->GetActorLocation() - CameraLocation;
		}

		const float AimAlignment = FVector::DotProduct(AimDirection, CameraToCandidate.GetSafeNormal());
		const float PawnDistanceSquared = FVector::DistSquared(AttackingPawn->GetActorLocation(), Candidate->GetActorLocation());
		if (AimAlignment > BestAimAlignment + KINDA_SMALL_NUMBER
			|| (FMath::IsNearlyEqual(AimAlignment, BestAimAlignment) && PawnDistanceSquared < BestPawnDistanceSquared))
		{
			OutTarget = Candidate;
			OutTargetLocation = CandidateLocation;
			BestAimAlignment = AimAlignment;
			BestPawnDistanceSquared = PawnDistanceSquared;
		}
	}

	return IsValid(OutTarget);
}

bool UJTSMeleeComponent::IsValidMeleeTarget(AActor* Candidate, APawn* AttackingPawn) const
{
	return IsValid(Candidate)
		&& Candidate != GetOwner()
		&& IsValid(AttackingPawn)
		&& Candidate->GetClass()->ImplementsInterface(UJTSMeleeTarget::StaticClass())
		&& IJTSMeleeTarget::Execute_CanReceiveMeleeHit(Candidate, AttackingPawn);
}

bool UJTSMeleeComponent::IsValidDamageTarget(AActor* Candidate, APawn* AttackingPawn) const
{
	if (!IsValid(Candidate) || Candidate == GetOwner() || !IsValid(AttackingPawn))
	{
		return false;
	}

	const bool bImplementsMeleeTarget = Candidate->GetClass()->ImplementsInterface(UJTSMeleeTarget::StaticClass());
	if (bImplementsMeleeTarget && !IJTSMeleeTarget::Execute_CanReceiveMeleeHit(Candidate, AttackingPawn))
	{
		return false;
	}

	if (const UJTSHealthComponent* const HealthComponent = Candidate->FindComponentByClass<UJTSHealthComponent>())
	{
		return !HealthComponent->IsDead();
	}

	return bImplementsMeleeTarget;
}

bool UJTSMeleeComponent::GetPlayerAimView(APawn* AttackingPawn, FVector& OutCameraLocation, FVector& OutAimDirection) const
{
	OutCameraLocation = FVector::ZeroVector;
	OutAimDirection = FVector::ZeroVector;
	if (!IsValid(AttackingPawn))
	{
		return false;
	}

	FRotator CameraRotation;
	if (APlayerController* const PlayerController = Cast<APlayerController>(AttackingPawn->GetController()))
	{
		// This is the active PlayerCameraManager viewpoint in both first- and third-person modes.
		PlayerController->GetPlayerViewPoint(OutCameraLocation, CameraRotation);
	}
	else
	{
		OutCameraLocation = AttackingPawn->GetPawnViewLocation();
		CameraRotation = AttackingPawn->GetViewRotation();
	}

	OutAimDirection = CameraRotation.Vector().GetSafeNormal();
	return !OutAimDirection.IsNearlyZero();
}

bool UJTSMeleeComponent::IsWithinPunchRange(APawn* AttackingPawn, const FVector& TargetLocation) const
{
	return IsValid(AttackingPawn)
		&& PunchRange > KINDA_SMALL_NUMBER
		&& FVector::DistSquared(AttackingPawn->GetActorLocation(), TargetLocation) <= FMath::Square(PunchRange);
}

bool UJTSMeleeComponent::HasMeleeLineOfSight(APawn* AttackingPawn, AActor* Candidate, const FVector& TargetLocation) const
{
	UWorld* const World = GetWorld();
	if (!IsValid(World) || !IsValid(AttackingPawn) || !IsValid(Candidate))
	{
		return false;
	}

	const FVector AttackOrigin = AttackingPawn->GetActorLocation();
	if (FVector::DistSquared(AttackOrigin, TargetLocation) <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JTSMeleeLineOfSight), false, AttackingPawn);
	AddOwnerAndAttachedActorsToIgnoreList(QueryParams, AttackingPawn);
	QueryParams.AddIgnoredActor(Candidate);

	FHitResult BlockingHit;
	return !World->LineTraceSingleByChannel(BlockingHit, AttackOrigin, TargetLocation, ECC_Visibility, QueryParams);
}

bool UJTSMeleeComponent::ApplyAttackToTarget(AActor* Target, APawn* AttackingPawn, EJTSMeleeAttackType AttackType)
{
	if (!IsValidDamageTarget(Target, AttackingPawn))
	{
		return false;
	}

	if (Target->FindComponentByClass<UJTSHealthComponent>() != nullptr)
	{
		const float Damage = GetDamageForAttackType(AttackType);
		return Damage > KINDA_SMALL_NUMBER
			&& UGameplayStatics::ApplyDamage(Target, Damage, AttackingPawn->GetController(), AttackingPawn, UDamageType::StaticClass()) > 0.0f;
	}

	// Legacy Moon targets (for example resource/tool interactions) keep their existing interface path.
	if (Target->GetClass()->ImplementsInterface(UJTSMeleeTarget::StaticClass()))
	{
		IJTSMeleeTarget::Execute_ReceiveMeleeHit(Target, AttackingPawn, AttackType);
		return true;
	}

	return false;
}

float UJTSMeleeComponent::GetDamageForAttackType(EJTSMeleeAttackType AttackType) const
{
	switch (AttackType)
	{
	case EJTSMeleeAttackType::Knife:
		return FMath::Max(0.0f, KnifeDamage);

	case EJTSMeleeAttackType::Axe:
		return FMath::Max(0.0f, AxeDamage);

	case EJTSMeleeAttackType::Punch:
	default:
		return FMath::Max(0.0f, PunchDamage);
	}
}

bool UJTSMeleeComponent::IsMoonMeleeAvailable() const
{
	const UWorld* const World = GetWorld();
	return World != nullptr && World->GetAuthGameMode<AJTSMoonGameMode>() != nullptr;
}

void UJTSMeleeComponent::SetCurrentMeleeTarget(AActor* NewTarget)
{
	if (CurrentMeleeTarget != NewTarget)
	{
		CurrentMeleeTarget = NewTarget;
	}
}
