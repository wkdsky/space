#include "space/Components/JTSMeleeComponent.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "space/Components/JTSPlayerEquipmentComponent.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "TimerManager.h"

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
}

void UJTSMeleeComponent::PerformHitCheck()
{
	APawn* const AttackingPawn = Cast<APawn>(GetOwner());
	UWorld* const World = GetWorld();
	if (!bIsAttacking
		|| !IsValid(AttackingPawn)
		|| !IsValid(World)
		|| AttackRange <= KINDA_SMALL_NUMBER
		|| AttackRadius < 0.0f
		|| AttackDamage <= 0.0f)
	{
		return;
	}

	const FVector Forward = AttackingPawn->GetActorForwardVector().GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		return;
	}

	const FVector TraceStart = AttackingPawn->GetActorLocation() + Forward * 40.0f;
	const FVector TraceEnd = TraceStart + Forward * AttackRange;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JTSMeleeAttackTrace), false, AttackingPawn);
	QueryParams.AddIgnoredActor(AttackingPawn);

	FHitResult HitResult;
	const FCollisionShape TraceShape = FCollisionShape::MakeSphere(AttackRadius);
	if (!World->SweepSingleByChannel(HitResult, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, TraceShape, QueryParams))
	{
		return;
	}

	AActor* const HitActor = HitResult.GetActor();
	if (!IsValid(HitActor))
	{
		return;
	}

	UGameplayStatics::ApplyDamage(
		HitActor,
		AttackDamage,
		AttackingPawn->GetController(),
		AttackingPawn,
		UDamageType::StaticClass());
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
	if (!IsValidMeleeTarget(Target, AttackingPawn))
	{
		return false;
	}

	const EJTSMeleeAttackType AttackType = GetCurrentAttackType();
	IJTSMeleeTarget::Execute_ReceiveMeleeHit(Target, AttackingPawn, AttackType);
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
	UWorld* const World = GetWorld();
	const AJTSMoonGameMode* const MoonGameMode = World != nullptr ? World->GetAuthGameMode<AJTSMoonGameMode>() : nullptr;
	APlayerController* const PlayerController = IsValid(AttackingPawn)
		? Cast<APlayerController>(AttackingPawn->GetController())
		: nullptr;
	if (!IsValid(AttackingPawn) || !IsValid(World) || !IsValid(MoonGameMode) || !IsValid(PlayerController))
	{
		return nullptr;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);
	const FVector CameraForward = CameraRotation.Vector().GetSafeNormal();
	const float MoonAttackRange = MoonGameMode->GetAttackRange();
	if (CameraForward.IsNearlyZero() || MoonAttackRange <= KINDA_SMALL_NUMBER)
	{
		return nullptr;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JTSMeleeTargetTrace), false, AttackingPawn);
	QueryParams.AddIgnoredActor(AttackingPawn);
	const FCollisionShape AimShape = FCollisionShape::MakeSphere(MoonGameMode->GetAttackAimRadius());
	TArray<FHitResult> HitResults;
	const FVector TraceEnd = CameraLocation + CameraForward * MoonAttackRange;
	if (!World->SweepMultiByChannel(HitResults, CameraLocation, TraceEnd, FQuat::Identity, ECC_Visibility, AimShape, QueryParams))
	{
		return nullptr;
	}

	AActor* BestTarget = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	const float AttackRangeSquared = FMath::Square(MoonAttackRange);
	for (const FHitResult& HitResult : HitResults)
	{
		AActor* const Candidate = HitResult.GetActor();
		if (!IsValidMeleeTarget(Candidate, AttackingPawn)
			|| FVector::DistSquared(AttackingPawn->GetActorLocation(), Candidate->GetActorLocation()) > AttackRangeSquared)
		{
			continue;
		}

		if (HitResult.Distance < BestDistance)
		{
			BestTarget = Candidate;
			BestDistance = HitResult.Distance;
		}
	}

	return BestTarget;
}

bool UJTSMeleeComponent::IsValidMeleeTarget(AActor* Candidate, APawn* AttackingPawn) const
{
	return IsValid(Candidate)
		&& Candidate != GetOwner()
		&& IsValid(AttackingPawn)
		&& Candidate->GetClass()->ImplementsInterface(UJTSMeleeTarget::StaticClass())
		&& IJTSMeleeTarget::Execute_CanReceiveMeleeHit(Candidate, AttackingPawn);
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
