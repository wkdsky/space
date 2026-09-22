#include "space/Components/JTSMeleeComponent.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "space/World/JTSMoonSurfaceGameplaySettings.h"
#include "space/World/JTSMoonAntActor.h"
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
	SetIsReplicatedByDefault(true);
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
	ClearUnarmedPunchTimers();

	CurrentMeleeTarget = nullptr;
	CachedMeleeTarget.Reset();
	HitActorsThisSwing.Reset();
	bAttackHeld = false;
	bAttackBuffered = false;
	bIsAttacking = false;
	CurrentAttackType = EJTSAttackType::Punch;
	bCurrentPunchUsesLeft = false;
	bCurrentPunchIsComboContinuation = false;
	NextAttackTime = 0.0;
	Super::EndPlay(EndPlayReason);
}

void UJTSMeleeComponent::RefreshMeleeTarget()
{
	SetCurrentMeleeTarget(FindBestMeleeTarget(Cast<APawn>(GetOwner())));
}

void UJTSMeleeComponent::AttackPressed()
{
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		bAttackHeld = true;
		ServerStartAttack();
		return;
	}
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
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		ServerReleaseAttack();
	}
}

void UJTSMeleeComponent::StartAttack()
{
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		ServerStartAttack();
		return;
	}
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
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}
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
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}
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

	const bool bIsComboContinuation = bIsAttacking && AttackType == EJTSAttackType::Punch;

	// Every montage segment is one new swing. Repeated hit requests can never re-hit this set.
	HitActorsThisSwing.Reset();
	CachedMeleeTarget.Reset();
	CurrentAttackType = AttackType;
	bIsAttacking = true;
	bCurrentPunchIsComboContinuation = bIsComboContinuation;

	if (CurrentAttackType == EJTSAttackType::Punch)
	{
		bCurrentPunchUsesLeft = !bCurrentPunchUsesLeft;
		APawn* const AttackingPawn = Cast<APawn>(GetOwner());
		AActor* Candidate = nullptr;
		FVector CandidateLocation = FVector::ZeroVector;
		if (FindBestPunchCandidate(
			AttackingPawn,
			Candidate,
			CandidateLocation,
			false,
			true,
			PunchTargetAcquireRadius))
		{
			CachedMeleeTarget = Candidate;
		}

		if (bDebugMeleeAim && IsValid(AttackingPawn))
		{
			if (UWorld* const World = GetWorld())
			{
				DrawDebugSphere(
					World,
					AttackingPawn->GetActorLocation(),
					FMath::Max(1.0f, PunchTargetAcquireRadius),
					24,
					FColor::Cyan,
					false,
					0.8f,
					0,
					0.75f);
				if (IsValid(Candidate))
				{
					DrawDebugSphere(World, CandidateLocation, 18.0f, 12, FColor::Yellow, false, 0.8f, 0, 1.0f);
					DrawDebugString(
						World,
						CandidateLocation + FVector(0.0f, 0.0f, 28.0f),
						FString::Printf(TEXT("Cached: %s"), *GetNameSafe(Candidate)),
						nullptr,
						FColor::Yellow,
						0.8f,
						true);
				}
			}
		}
	}

	if (CurrentAttackType == EJTSAttackType::Punch)
	{
		ClearAttackFailSafeTimer();
		ScheduleUnarmedPunchEvents();
	}
	else
	{
		ResetAttackFailSafeTimer();
	}

	MulticastBeginAttackPresentation(CurrentAttackType, bCurrentPunchUsesLeft, bCurrentPunchIsComboContinuation);
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

void UJTSMeleeComponent::ScheduleUnarmedPunchEvents()
{
	UWorld* const World = GetWorld();
	if (!IsValid(World) || GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}

	ClearUnarmedPunchTimers();
	const float HitDelay = FMath::Max(0.01f, UnarmedPunchHitDelay);
	const float ChainDelay = FMath::Max(HitDelay, UnarmedPunchChainDelay);
	const float RecoveryDelay = FMath::Max(ChainDelay + 0.01f, UnarmedPunchRecoveryDelay);
	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.SetTimer(UnarmedPunchHitTimerHandle, this, &UJTSMeleeComponent::HandleUnarmedPunchHit, HitDelay, false);
	TimerManager.SetTimer(UnarmedPunchChainTimerHandle, this, &UJTSMeleeComponent::HandleUnarmedPunchChainWindow, ChainDelay, false);
	TimerManager.SetTimer(UnarmedPunchRecoveryTimerHandle, this, &UJTSMeleeComponent::HandleUnarmedPunchRecovery, RecoveryDelay, false);
}

void UJTSMeleeComponent::ClearUnarmedPunchTimers()
{
	if (UWorld* const World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(UnarmedPunchHitTimerHandle);
		TimerManager.ClearTimer(UnarmedPunchChainTimerHandle);
		TimerManager.ClearTimer(UnarmedPunchRecoveryTimerHandle);
	}
}

void UJTSMeleeComponent::HandleUnarmedPunchHit()
{
	if (CurrentAttackType == EJTSAttackType::Punch)
	{
		PerformHitCheck();
	}
}

void UJTSMeleeComponent::HandleUnarmedPunchChainWindow()
{
	if (CurrentAttackType == EJTSAttackType::Punch)
	{
		TryChainAttack();
	}
}

void UJTSMeleeComponent::HandleUnarmedPunchRecovery()
{
	if (CurrentAttackType == EJTSAttackType::Punch)
	{
		FinishCurrentAttack();
	}
}

void UJTSMeleeComponent::EndAttackState()
{
	const bool bWasAttacking = bIsAttacking;
	const EJTSAttackType FinishedAttackType = CurrentAttackType;
	ClearAttackFailSafeTimer();
	ClearUnarmedPunchTimers();

	bIsAttacking = false;
	bAttackBuffered = false;
	bCurrentPunchUsesLeft = false;
	bCurrentPunchIsComboContinuation = false;
	HitActorsThisSwing.Reset();
	CachedMeleeTarget.Reset();
	if (bWasAttacking && GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		MulticastEndAttackPresentation(FinishedAttackType);
	}
}

void UJTSMeleeComponent::PerformHitCheck()
{
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		ServerPerformHitCheck();
		return;
	}
	APawn* const AttackingPawn = Cast<APawn>(GetOwner());
	UWorld* const World = GetWorld();
	if (!bIsAttacking || !IsValid(AttackingPawn) || !IsValid(World) || PunchMaxTargets < 1)
	{
		return;
	}
	if (HitActorsThisSwing.Num() >= PunchMaxTargets)
	{
		return;
	}

	if (CurrentAttackType == EJTSAttackType::Punch)
	{
		AActor* Candidate = CachedMeleeTarget.Get();
		FVector CandidateLocation = FVector::ZeroVector;
		bool bUsingCachedTarget = false;
		if (IsValidDamageTarget(Candidate, AttackingPawn) && !HitActorsThisSwing.Contains(Candidate))
		{
			CandidateLocation = GetMeleeTargetAimPoint(Candidate);
			const float AllowedRange = Candidate->IsA<AJTSMoonAntActor>()
				? FMath::Min(205.0f, FMath::Max(PunchRange, CachedTargetGraceRange))
				: PunchRange;
			bUsingCachedTarget = IsWithinMeleeRange(AttackingPawn, Candidate->GetActorLocation(), AllowedRange)
				&& HasMeleeLineOfSight(AttackingPawn, Candidate, CandidateLocation);
		}

		if (!bUsingCachedTarget)
		{
			CachedMeleeTarget.Reset();
			Candidate = nullptr;
			CandidateLocation = FVector::ZeroVector;
			if (!FindBestPunchCandidate(
				AttackingPawn,
				Candidate,
				CandidateLocation,
				false,
				true,
				PunchTargetAcquireRadius)
				|| !IsWithinPunchRange(AttackingPawn, Candidate != nullptr ? Candidate->GetActorLocation() : FVector::ZeroVector)
				|| HitActorsThisSwing.Contains(Candidate))
			{
				return;
			}
		}

		const bool bApplied = ApplyAttackToTarget(Candidate, AttackingPawn, EJTSMeleeAttackType::Punch);
		if (bApplied)
		{
			// PunchMaxTargets is intentionally clamped to one. This set also protects duplicate AttackHit notifies.
			HitActorsThisSwing.Add(Candidate);
		}

		if (bDebugMeleeAim)
		{
			const FColor FinalColor = bApplied ? FColor::Green : FColor::Red;
			DrawDebugSphere(World, CandidateLocation, 24.0f, 12, FinalColor, false, 1.5f, 0, 2.0f);
			DrawDebugLine(World, AttackingPawn->GetActorLocation(), CandidateLocation, FinalColor, false, 1.5f, 0, 1.5f);
			DrawDebugString(
				World,
				CandidateLocation + FVector(0.0f, 0.0f, 28.0f),
				FString::Printf(TEXT("%s %s Damage %.1f"), bUsingCachedTarget ? TEXT("Cached") : TEXT("Reacquired"), *GetNameSafe(Candidate), PunchDamage),
				nullptr,
				FinalColor,
				1.5f,
				true);
		}
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
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		ServerTryAttack();
		return true;
	}
	UWorld* const World = GetWorld();
	APawn* const AttackingPawn = Cast<APawn>(GetOwner());
	const AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this);
	const IJTSMoonSurfaceGameplaySettings* const MoonSettings = IsValid(SurfaceController) ? SurfaceController->GetMoonSettings() : nullptr;
	if (!IsValid(AttackingPawn) || MoonSettings == nullptr || !IsMoonMeleeAvailable())
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

	float AttackInterval = MoonSettings->GetAttackCooldown();
	if (const UJTSInventoryComponent* const Inventory = AttackingPawn->FindComponentByClass<UJTSInventoryComponent>())
	{
		if (const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, Inventory->GetActiveItemId()))
		{
			if (Definition->IsHoldable())
			{
				AttackInterval = Definition->MeleeAttackInterval;
			}
		}
	}
	NextAttackTime = CurrentTime + static_cast<double>(FMath::Max(0.08f, AttackInterval));
	RefreshMeleeTarget();
	return true;
}

AActor* UJTSMeleeComponent::GetCurrentMeleeTarget() const
{
	return CurrentMeleeTarget.Get();
}

bool UJTSMeleeComponent::IsUnarmedComboActive() const
{
	return bIsAttacking && CurrentAttackType == EJTSAttackType::Punch;
}

bool UJTSMeleeComponent::IsCurrentPunchLeft() const
{
	return bCurrentPunchUsesLeft;
}

bool UJTSMeleeComponent::IsContinuingUnarmedCombo() const
{
	return IsUnarmedComboActive() && bCurrentPunchIsComboContinuation;
}

EJTSMeleeAttackType UJTSMeleeComponent::GetCurrentAttackType() const
{
	const APawn* const AttackingPawn = Cast<APawn>(GetOwner());
	const UJTSInventoryComponent* const Inventory = IsValid(AttackingPawn)
		? AttackingPawn->FindComponentByClass<UJTSInventoryComponent>()
		: nullptr;
	if (!IsValid(Inventory))
	{
		return EJTSMeleeAttackType::Punch;
	}

	const FJTSItemInstance ActiveItem = Inventory->GetActiveItem();
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ActiveItem.ItemId);
	if (!IsValid(Definition) || !Definition->IsHoldable())
	{
		return EJTSMeleeAttackType::Punch;
	}

	switch (ActiveItem.ItemId)
	{
	case EJTSItemId::Knife:
		return EJTSMeleeAttackType::Knife;

	case EJTSItemId::Axe:
		return EJTSMeleeAttackType::Axe;

	case EJTSItemId::Pickaxe:
		return EJTSMeleeAttackType::Tool;

	default:
		return EJTSMeleeAttackType::Improvised;
	}
}

EJTSAttackType UJTSMeleeComponent::ResolveAttackType() const
{
	const APawn* const AttackingPawn = Cast<APawn>(GetOwner());
	const UJTSInventoryComponent* const Inventory = IsValid(AttackingPawn)
		? AttackingPawn->FindComponentByClass<UJTSInventoryComponent>()
		: nullptr;
	if (!IsValid(Inventory))
	{
		return EJTSAttackType::Punch;
	}

	const FJTSItemInstance ActiveItem = Inventory->GetActiveItem();
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ActiveItem.ItemId);
	if (!IsValid(Definition) || !Definition->IsHoldable())
	{
		return EJTSAttackType::Punch;
	}
	if (Definition->MiningWork > KINDA_SMALL_NUMBER)
	{
		return EJTSAttackType::Tool;
	}
	return EJTSAttackType::MeleeWeapon;
}

AActor* UJTSMeleeComponent::FindBestMeleeTarget(APawn* AttackingPawn) const
{
	if (ResolveAttackType() == EJTSAttackType::Punch)
	{
		AActor* PunchTarget = nullptr;
		FVector PunchTargetLocation = FVector::ZeroVector;
		if (FindBestPunchCandidate(
			AttackingPawn,
			PunchTarget,
			PunchTargetLocation,
			false,
			true,
			PunchTargetAcquireRadius)
			&& IsWithinPunchRange(AttackingPawn, PunchTarget != nullptr ? PunchTarget->GetActorLocation() : FVector::ZeroVector))
		{
			return PunchTarget;
		}

		return nullptr;
	}

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

bool UJTSMeleeComponent::FindBestPunchCandidate(
	APawn* AttackingPawn,
	AActor*& OutTarget,
	FVector& OutTargetLocation,
	bool bRequireMeleeTargetInterface,
	bool bRequireLineOfSight,
	float MaximumTargetRange) const
{
	OutTarget = nullptr;
	OutTargetLocation = FVector::ZeroVector;

	UWorld* const World = GetWorld();
	FVector CameraLocation;
	FVector AimDirection;
	const float SearchRadius = FMath::Max(0.0f, MaximumTargetRange);
	if (!IsValid(AttackingPawn)
		|| !IsValid(World)
		|| SearchRadius <= KINDA_SMALL_NUMBER
		|| !GetPlayerAimView(AttackingPawn, CameraLocation, AimDirection))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JTSPunchCandidateOverlap), false, AttackingPawn);
	AddOwnerAndAttachedActorsToIgnoreList(QueryParams, AttackingPawn);
	TArray<FOverlapResult> OverlapResults;
	const FCollisionObjectQueryParams ObjectQueryParams(FCollisionObjectQueryParams::AllObjects);
	if (!World->OverlapMultiByObjectType(
		OverlapResults,
		AttackingPawn->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(SearchRadius),
		QueryParams))
	{
		return false;
	}

	float BestScore = TNumericLimits<float>::Max();
	TSet<AActor*> SeenActors;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* const Candidate = OverlapResult.GetActor();
		if (!IsValid(Candidate) || SeenActors.Contains(Candidate))
		{
			continue;
		}
		SeenActors.Add(Candidate);

		const bool bIsValidCandidate = bRequireMeleeTargetInterface
			? IsValidMeleeTarget(Candidate, AttackingPawn)
			: IsValidDamageTarget(Candidate, AttackingPawn);
		if (!bIsValidCandidate)
		{
			continue;
		}

		const float PawnDistance = FVector::Dist(AttackingPawn->GetActorLocation(), Candidate->GetActorLocation());
		if (PawnDistance > SearchRadius)
		{
			continue;
		}

		const FVector CandidateLocation = GetMeleeTargetAimPoint(Candidate);
		FVector CameraToCandidate = CandidateLocation - CameraLocation;
		if (CameraToCandidate.IsNearlyZero())
		{
			CameraToCandidate = Candidate->GetActorLocation() - CameraLocation;
		}
		if (CameraToCandidate.IsNearlyZero())
		{
			continue;
		}

		const bool bIsMoonAnt = Candidate->IsA<AJTSMoonAntActor>();
		const float AimAssistAngle = FMath::Max(
			0.0f,
			bIsMoonAnt ? MoonAntPunchAimAssistAngle : GenericPunchAimAssistAngle);
		if (AimAssistAngle <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float AimAlignment = FMath::Clamp(FVector::DotProduct(AimDirection, CameraToCandidate.GetSafeNormal()), -1.0f, 1.0f);
		const float AimAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(AimAlignment));
		if (AimAngleDegrees > AimAssistAngle)
		{
			continue;
		}

		if (bRequireLineOfSight && !HasMeleeLineOfSight(AttackingPawn, Candidate, CandidateLocation))
		{
			continue;
		}

		// Camera alignment is deliberately dominant: a closer side target must not replace the target under
		// the crosshair. MoonAnts receive only a small tie-breaking bias after both angle and distance are scored.
		const float AngleScore = AimAngleDegrees / AimAssistAngle;
		const float DistanceScore = PawnDistance / SearchRadius;
		const float SmallTargetBias = bIsMoonAnt ? 0.25f : 0.0f;
		const float Score = AngleScore * 100.0f + DistanceScore * 10.0f - SmallTargetBias;
		if (Score < BestScore)
		{
			BestScore = Score;
			OutTarget = Candidate;
			OutTargetLocation = CandidateLocation;
		}
	}

	return IsValid(OutTarget);
}

FVector UJTSMeleeComponent::GetMeleeTargetAimPoint(AActor* Candidate) const
{
	if (!IsValid(Candidate))
	{
		return FVector::ZeroVector;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Candidate->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	UPrimitiveComponent* BoundsComponent = nullptr;
	for (UPrimitiveComponent* const PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent) || !PrimitiveComponent->IsRegistered())
		{
			continue;
		}

		if (PrimitiveComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision
			&& PrimitiveComponent->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block)
		{
			return PrimitiveComponent->Bounds.Origin;
		}

		if (BoundsComponent == nullptr)
		{
			BoundsComponent = PrimitiveComponent;
		}
	}

	return BoundsComponent != nullptr
		? BoundsComponent->Bounds.Origin
		: Candidate->GetActorLocation();
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
	return IsWithinMeleeRange(AttackingPawn, TargetLocation, PunchRange);
}

bool UJTSMeleeComponent::IsWithinMeleeRange(APawn* AttackingPawn, const FVector& TargetLocation, float MaximumRange) const
{
	return IsValid(AttackingPawn)
		&& MaximumRange > KINDA_SMALL_NUMBER
		&& FVector::DistSquared(AttackingPawn->GetActorLocation(), TargetLocation) <= FMath::Square(MaximumRange);
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
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || !IsValidDamageTarget(Target, AttackingPawn))
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

void UJTSMeleeComponent::ServerStartAttack_Implementation()
{
	bAttackHeld = true;
	if (bIsAttacking)
	{
		bAttackBuffered = true;
		return;
	}

	StartAttack();
}

void UJTSMeleeComponent::ServerPerformHitCheck_Implementation()
{
	PerformHitCheck();
}

void UJTSMeleeComponent::ServerTryAttack_Implementation()
{
	TryAttack();
}

void UJTSMeleeComponent::ServerReleaseAttack_Implementation()
{
	bAttackHeld = false;
}

void UJTSMeleeComponent::MulticastBeginAttackPresentation_Implementation(
	EJTSAttackType AttackType,
	bool bUseLeftPunch,
	bool bIsComboContinuation)
{
	CurrentAttackType = AttackType;
	bCurrentPunchUsesLeft = bUseLeftPunch;
	bCurrentPunchIsComboContinuation = bIsComboContinuation;
	bIsAttacking = true;
	OnAttackStarted.Broadcast(AttackType);
}

void UJTSMeleeComponent::MulticastEndAttackPresentation_Implementation(EJTSAttackType AttackType)
{
	bIsAttacking = false;
	bAttackBuffered = false;
	bCurrentPunchIsComboContinuation = false;
	OnAttackFinished.Broadcast(AttackType);
}

float UJTSMeleeComponent::GetDamageForAttackType(EJTSMeleeAttackType AttackType) const
{
	if (AttackType != EJTSMeleeAttackType::Punch)
	{
		const APawn* const AttackingPawn = Cast<APawn>(GetOwner());
		const UJTSInventoryComponent* const Inventory = IsValid(AttackingPawn)
			? AttackingPawn->FindComponentByClass<UJTSInventoryComponent>()
			: nullptr;
		if (IsValid(Inventory))
		{
			if (const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, Inventory->GetActiveItemId()))
			{
				return FMath::Max(0.0f, Definition->CombatDamage);
			}
		}
	}

	switch (AttackType)
	{
	case EJTSMeleeAttackType::Knife:
		return FMath::Max(0.0f, KnifeDamage);

	case EJTSMeleeAttackType::Axe:
		return FMath::Max(0.0f, AxeDamage);

	case EJTSMeleeAttackType::Tool:
	case EJTSMeleeAttackType::Improvised:
		return FMath::Max(0.0f, PunchDamage);

	case EJTSMeleeAttackType::Punch:
	default:
		return FMath::Max(0.0f, PunchDamage);
	}
}

bool UJTSMeleeComponent::IsMoonMeleeAvailable() const
{
	const AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this);
	return IsValid(SurfaceController)
		&& SurfaceController->IsSurfaceGameplayInitialized()
		&& SurfaceController->OwnsSurfaceActor(GetOwner());
}

void UJTSMeleeComponent::SetCurrentMeleeTarget(AActor* NewTarget)
{
	if (CurrentMeleeTarget != NewTarget)
	{
		CurrentMeleeTarget = NewTarget;
	}
}
