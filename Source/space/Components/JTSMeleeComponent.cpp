#include "space/Components/JTSMeleeComponent.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "space/Components/JTSPlayerEquipmentComponent.h"
#include "space/Modes/JTSMoonGameMode.h"

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

	CurrentMeleeTarget = nullptr;
	NextAttackTime = 0.0;
	Super::EndPlay(EndPlayReason);
}

void UJTSMeleeComponent::RefreshMeleeTarget()
{
	SetCurrentMeleeTarget(FindBestMeleeTarget(Cast<APawn>(GetOwner())));
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
	const float AttackRange = MoonGameMode->GetAttackRange();
	if (CameraForward.IsNearlyZero() || AttackRange <= KINDA_SMALL_NUMBER)
	{
		return nullptr;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JTSMeleeTargetTrace), false, AttackingPawn);
	QueryParams.AddIgnoredActor(AttackingPawn);
	const FCollisionShape AimShape = FCollisionShape::MakeSphere(MoonGameMode->GetAttackAimRadius());
	TArray<FHitResult> HitResults;
	const FVector TraceEnd = CameraLocation + CameraForward * AttackRange;
	if (!World->SweepMultiByChannel(HitResults, CameraLocation, TraceEnd, FQuat::Identity, ECC_Visibility, AimShape, QueryParams))
	{
		return nullptr;
	}

	AActor* BestTarget = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	const float AttackRangeSquared = FMath::Square(AttackRange);
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
