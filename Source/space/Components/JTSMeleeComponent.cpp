#include "space/Components/JTSMeleeComponent.h"
#include "space/Components/JTSStellarWeaponComponent.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSCriticalDamageType.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSWeaponVisualComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSWeaponProgression.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerState.h"
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
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(true);
}

void UJTSMeleeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bMeleeSwingClockActive)
	{
		MeleeSwingClockElapsed += DeltaTime;
		if (GetOwner() != nullptr && GetOwner()->HasAuthority() && CurrentAttackType != EJTSAttackType::Punch)
		{
			SweepHeldWeaponTip();
		}
		if (MeleeSwingClockElapsed >= FMath::Max(0.15f, HeldWeaponRecoveryDelay))
		{
			bMeleeSwingClockActive = false;
			MeleeSwingClockElapsed = 0.0f;
		}
	}
	if (bPunchVisualClockActive)
	{
		PunchVisualElapsed += DeltaTime;
		// The fist is travelling toward its endpoint for the whole approach. Sample that path
		// each frame so a target crossed before the hit timer still counts as this one swing.
		if (GetOwner() != nullptr && GetOwner()->HasAuthority()
			&& CurrentAttackType == EJTSAttackType::Punch
			&& PunchVisualElapsed <= GetUnarmedPunchHitDelay() + DeltaTime)
		{
			SweepPunchFist();
		}
		if (PunchVisualElapsed >= GetUnarmedPunchRecoveryDelay() + 0.05f)
		{
			bPunchVisualClockActive = false;
			PunchVisualElapsed = 0.0f;
		}
	}
	if (!bMeleeSwingClockActive && !bPunchVisualClockActive)
	{
		SetComponentTickEnabled(false);
	}
}

void UJTSMeleeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Cast<APawn>(GetOwner()) == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("JTSMeleeComponent on '%s' requires a pawn owner."), *GetNameSafe(GetOwner()));
		return;
	}
	if (UJTSWeaponVisualComponent* Visual = GetOwner()->FindComponentByClass<UJTSWeaponVisualComponent>())
	{
		AddTickPrerequisiteComponent(Visual);
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
	ClearHeldWeaponTimers();

	CurrentMeleeTarget = nullptr;
	HitActorsThisSwing.Reset();
	bHasPreviousPunchSample = false;
	bHasPreviousWeaponTip = false;
	bAttackHeld = false;
	bAttackBuffered = false;
	bIsAttacking = false;
	CurrentAttackType = EJTSAttackType::Punch;
	bCurrentPunchUsesLeft = false;
	bCurrentPunchIsComboContinuation = false;
	bMeleeSwingClockActive = false;
	MeleeSwingClockElapsed = 0.0f;
	bPunchVisualClockActive = false;
	PunchVisualElapsed = 0.0f;
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
	// A click already counted as this swing, or as the one swing buffered inside it.
	// Clearing the buffer here is what lets go of the button stop the next chop.
	if (CurrentAttackType != EJTSAttackType::Punch)
	{
		bAttackBuffered = false;
	}
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		ServerReleaseAttack();
	}
}

void UJTSMeleeComponent::StartAttack()
{
	const auto* Stellar = GetOwner() ? GetOwner()->FindComponentByClass<UJTSStellarWeaponComponent>() : nullptr;
	if (Stellar && Stellar->HasActiveWeapon()) return;
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

	// Empty-hand punches stay one follow-up. A held tool keeps chopping for as long
	// as the button is down, and a click that lands during a swing is the next chop.
	if (CurrentAttackType == EJTSAttackType::Punch)
	{
		bAttackBuffered = false;
	}
	else if (!bAttackHeld)
	{
		bAttackBuffered = false;
	}
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
	// A tool that is still held, or that received another click during this swing, starts
	// the next chop here. Releasing the button leaves this recovery as the last swing.
	if (bAttackHeld || bAttackBuffered)
	{
		TryChainAttack();
		return;
	}

	EndAttackState();
}

void UJTSMeleeComponent::BeginAttack(EJTSAttackType AttackType)
{
	const auto* Stellar = GetOwner() ? GetOwner()->FindComponentByClass<UJTSStellarWeaponComponent>() : nullptr;
	if (Stellar && Stellar->HasActiveWeapon()) { EndAttackState(); return; }
	if (!IsValid(Cast<APawn>(GetOwner())))
	{
		EndAttackState();
		return;
	}

	const bool bIsComboContinuation = bIsAttacking && AttackType == EJTSAttackType::Punch;

	// Every montage segment is one new swing. Repeated hit requests can never re-hit this set.
	HitActorsThisSwing.Reset();
	bHasPreviousPunchSample = false;
	bHasPreviousWeaponTip = false;
	CurrentAttackType = AttackType;
	bIsAttacking = true;
	bCurrentPunchIsComboContinuation = bIsComboContinuation;

	if (CurrentAttackType == EJTSAttackType::Punch) bCurrentPunchUsesLeft = !bCurrentPunchUsesLeft;

	if (CurrentAttackType == EJTSAttackType::Punch)
	{
		ClearAttackFailSafeTimer();
		ScheduleUnarmedPunchEvents();
	}
	else
	{
		// Weapon/tool montages are optional project presentation. Gameplay must still swing,
		// hit, and chain while held when no Blueprint notify graph has been configured.
		ClearAttackFailSafeTimer();
		ScheduleHeldWeaponEvents();
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
	const float HitDelay = GetUnarmedPunchHitDelay();
	const float ChainDelay = GetUnarmedPunchChainDelay();
	const float RecoveryDelay = GetUnarmedPunchRecoveryDelay();
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

void UJTSMeleeComponent::ScheduleHeldWeaponEvents()
{
	UWorld* const World = GetWorld();
	if (!IsValid(World) || GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}

	ClearHeldWeaponTimers();
	const float HitDelay = FMath::Max(0.01f, HeldWeaponHitDelay);
	const float ChainDelay = FMath::Max(HitDelay, HeldWeaponChainDelay);
	const float RecoveryDelay = FMath::Max(ChainDelay + 0.01f, HeldWeaponRecoveryDelay);
	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.SetTimer(HeldWeaponHitTimerHandle, this, &UJTSMeleeComponent::HandleHeldWeaponHit, HitDelay, false);
	// A held button, or a click that landed during this chop, starts the next one as
	// this arc finishes. Waiting for the longer recovery left a pause between swings.
	if (bAttackHeld || bAttackBuffered)
	{
		TimerManager.SetTimer(HeldWeaponChainTimerHandle, this, &UJTSMeleeComponent::HandleHeldWeaponChainWindow, ChainDelay, false);
	}
	TimerManager.SetTimer(HeldWeaponRecoveryTimerHandle, this, &UJTSMeleeComponent::HandleHeldWeaponRecovery, RecoveryDelay, false);
}

void UJTSMeleeComponent::ClearHeldWeaponTimers()
{
	if (UWorld* const World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(HeldWeaponHitTimerHandle);
		TimerManager.ClearTimer(HeldWeaponChainTimerHandle);
		TimerManager.ClearTimer(HeldWeaponRecoveryTimerHandle);
	}
}

void UJTSMeleeComponent::HandleHeldWeaponHit()
{
	if (CurrentAttackType != EJTSAttackType::Punch)
	{
		PerformHitCheck();
	}
}

void UJTSMeleeComponent::HandleHeldWeaponChainWindow()
{
	if (CurrentAttackType != EJTSAttackType::Punch)
	{
		TryChainAttack();
	}
}

void UJTSMeleeComponent::HandleHeldWeaponRecovery()
{
	if (CurrentAttackType != EJTSAttackType::Punch)
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
	ClearHeldWeaponTimers();

	bIsAttacking = false;
	bAttackBuffered = false;
	bCurrentPunchUsesLeft = false;
	bCurrentPunchIsComboContinuation = false;
	HitActorsThisSwing.Reset();
	bHasPreviousPunchSample = false;
	bHasPreviousWeaponTip = false;
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
	if (!bIsAttacking || HitActorsThisSwing.Num() >= PunchMaxTargets) return;
	if (CurrentAttackType == EJTSAttackType::Punch) SweepPunchFist();
	else SweepHeldWeaponTip();
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
	if (!IsValid(World) || !IsValid(AttackingPawn) || MoonSettings == nullptr || !IsMoonMeleeAvailable())
	{
		return false;
	}

	const double CurrentTime = static_cast<double>(World->GetTimeSeconds());
	if (CurrentTime < NextAttackTime)
	{
		return false;
	}

	if (bIsAttacking) return false;
	StartAttack();
	if (!bIsAttacking) return false;

	float AttackInterval = MoonSettings->GetAttackCooldown();
	if (const UJTSInventoryComponent* const Inventory = AttackingPawn->FindComponentByClass<UJTSInventoryComponent>())
	{
		if (const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, Inventory->GetActiveItemId()))
		{
			if (Definition->IsHoldable())
			{
				const FJTSResolvedWeaponStats Stats = FJTSWeaponProgression::Resolve(Inventory->GetActiveItem());
				AttackInterval = Stats.ScaleInterval(Definition->MeleeAttackInterval);
			}
		}
	}
	NextAttackTime = CurrentTime + static_cast<double>(FMath::Max(0.08f, AttackInterval));
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

float UJTSMeleeComponent::GetMeleeSwingPhase() const
{
	if (!bMeleeSwingClockActive)
	{
		return 0.0f;
	}
	const float Duration = FMath::Max(0.15f, HeldWeaponRecoveryDelay);
	return FMath::Clamp(MeleeSwingClockElapsed / Duration, 0.0f, 1.0f);
}

bool UJTSMeleeComponent::IsCurrentPunchLeft() const
{
	return bCurrentPunchUsesLeft;
}

float UJTSMeleeComponent::GetPunchVisualElapsed() const
{
	return bPunchVisualClockActive ? PunchVisualElapsed : 0.0f;
}

float UJTSMeleeComponent::GetConfirmedPunchHitFeedbackAlpha() const
{
	return GetWorld() != nullptr
		? FMath::Clamp(1.0f - static_cast<float>((GetWorld()->GetTimeSeconds() - LastConfirmedPunchHitSeconds) / 0.16), 0.0f, 1.0f)
		: 0.0f;
}

bool UJTSMeleeComponent::IsPunchVisualActive() const
{
	return bPunchVisualClockActive;
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

	return Definition->MeleeAttackClass;
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
		|| !GetHeldItemAimView(AttackingPawn, CameraLocation, AimDirection)
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

bool UJTSMeleeComponent::GetPunchFistPath(APawn* AttackingPawn, FVector& OutStart, FVector& OutEnd) const
{
	if (!IsValid(AttackingPawn))
	{
		return false;
	}

	const FVector Up = AttackingPawn->GetActorUpVector();
	const FVector Forward = FVector::VectorPlaneProject(AttackingPawn->GetActorForwardVector(), Up).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();
	if (Forward.IsNearlyZero() || Right.IsNearlyZero())
	{
		return false;
	}

	FVector CameraLocation;
	FVector CameraDirection;
	const float CameraPitch = GetPlayerAimView(AttackingPawn, CameraLocation, CameraDirection)
		? FMath::Asin(FMath::Clamp(FVector::DotProduct(CameraDirection, Up), -1.0f, 1.0f))
		: 0.0f;
	const FQuat PitchRotation(Right, -FMath::Clamp(CameraPitch, FMath::DegreesToRadians(-55.0f), FMath::DegreesToRadians(55.0f)));
	const FVector PunchForward = PitchRotation.RotateVector(Forward);
	const FVector PunchUp = PitchRotation.RotateVector(Up);
	const float Chest = AttackingPawn->GetSimpleCollisionHalfHeight() * 0.50f;
	const FVector Shoulder = AttackingPawn->GetActorLocation()
		+ Up * Chest
		+ Right * (bCurrentPunchUsesLeft ? -19.0f : 19.0f);
	// The visual wrist begins about 9 cm in front of the upper-arm joint. At
	// contact the waist turn and lead shoulder add reach to the straightened
	// arm. This path remains available when a dedicated server skips mesh poses.
	OutStart = Shoulder + PunchForward * 31.0f - PunchUp * 8.0f;
	OutEnd = Shoulder + PunchForward * 60.0f - PunchUp * 8.0f;
	return true;
}

void UJTSMeleeComponent::SweepPunchFist()
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || !bIsAttacking
		|| CurrentAttackType != EJTSAttackType::Punch || HitActorsThisSwing.Num() >= PunchMaxTargets)
	{
		return;
	}

	APawn* const AttackingPawn = Cast<APawn>(GetOwner());
	UWorld* const World = GetWorld();
	FVector FistStart = FVector::ZeroVector;
	FVector FistEnd = FVector::ZeroVector;
	if (!IsValid(AttackingPawn) || !IsValid(World) || !GetPunchFistPath(AttackingPawn, FistStart, FistEnd))
	{
		return;
	}

	const float HitDelay = GetUnarmedPunchHitDelay();
	const float GuardTime = bCurrentPunchIsComboContinuation
		? 0.0f : FMath::Min(0.035f, HitDelay * 0.32f);
	const float Travel = FMath::Pow(FMath::Clamp(
		(PunchVisualElapsed - GuardTime) / FMath::Max(0.01f, HitDelay - GuardTime),
		0.0f, 1.0f), 1.45f);
	FVector Sample = FMath::Lerp(FistStart, FistEnd, Travel);
	if (const USkeletalMeshComponent* const Mesh = AttackingPawn->FindComponentByClass<USkeletalMeshComponent>())
	{
		const FName WristName = bCurrentPunchUsesLeft ? TEXT("Wrist_L") : TEXT("Wrist_R");
		if (Mesh->GetBoneIndex(WristName) != INDEX_NONE && Mesh->PoseTickedThisFrame())
		{
			Sample = Mesh->GetBoneLocation(WristName, EBoneSpaces::WorldSpace);
		}
	}
	const FVector SweepStart = bHasPreviousPunchSample ? PreviousPunchSample : Sample;
	PreviousPunchSample = Sample;
	bHasPreviousPunchSample = true;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JTSPunchFistSweep), false, AttackingPawn);
	AddOwnerAndAttachedActorsToIgnoreList(QueryParams, AttackingPawn);
	FHitResult Hit;
	const bool bBlockingHit = World->SweepSingleByChannel(Hit, SweepStart, Sample, FQuat::Identity,
		ECC_Visibility, FCollisionShape::MakeSphere(18.0f), QueryParams);
	AActor* const Candidate = bBlockingHit ? Hit.GetActor() : nullptr;
	const bool bValidContact = IsValidDamageTarget(Candidate, AttackingPawn)
		&& !HitActorsThisSwing.Contains(Candidate)
		&& IsWithinPunchRange(AttackingPawn, Hit.ImpactPoint)
		&& HasMeleeLineOfSight(AttackingPawn, Candidate, Hit.ImpactPoint);
	if (bValidContact && ApplyAttackToTarget(Candidate, AttackingPawn, EJTSMeleeAttackType::Punch))
	{
		HitActorsThisSwing.Add(Candidate);
		ClientConfirmPunchHit();
		MulticastPunchImpact(Hit.ImpactPoint);
	}
	if (bDebugMeleeAim)
	{
		const FColor Color = bValidContact ? FColor::Green : FColor::Orange;
		DrawDebugLine(World, SweepStart, Sample, Color, false, 0.6f, 0, 1.25f);
		DrawDebugSphere(World, Sample, 18.0f, 10, Color, false, 0.6f, 0, 1.0f);
	}
}

void UJTSMeleeComponent::SweepHeldWeaponTip()
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || !bIsAttacking
		|| CurrentAttackType == EJTSAttackType::Punch || HitActorsThisSwing.Num() >= PunchMaxTargets)
	{
		return;
	}
	APawn* const AttackingPawn = Cast<APawn>(GetOwner());
	const UJTSWeaponVisualComponent* const Visual = IsValid(AttackingPawn)
		? AttackingPawn->FindComponentByClass<UJTSWeaponVisualComponent>() : nullptr;
	UWorld* const World = GetWorld();
	FVector Tip = FVector::ZeroVector;
	if (!IsValid(World) || !IsValid(Visual) || !Visual->GetHeldItemTipWorldLocation(Tip)
		|| FVector::DistSquared(Tip, AttackingPawn->GetActorLocation()) > FMath::Square(PunchRange + 100.0f))
	{
		bHasPreviousWeaponTip = false;
		return;
	}
	const FVector SweepStart = bHasPreviousWeaponTip ? PreviousWeaponTip : Tip;
	PreviousWeaponTip = Tip;
	bHasPreviousWeaponTip = true;
	if (MeleeSwingClockElapsed < FMath::Max(0.0f, HeldWeaponHitDelay - 0.14f)
		|| MeleeSwingClockElapsed > HeldWeaponHitDelay + 0.08f)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JTSHeldWeaponSweep), false, AttackingPawn);
	AddOwnerAndAttachedActorsToIgnoreList(QueryParams, AttackingPawn);
	FHitResult Hit;
	const bool bBlockingHit = World->SweepSingleByChannel(Hit, SweepStart, Tip, FQuat::Identity,
		ECC_Visibility, FCollisionShape::MakeSphere(FMath::Max(1.0f, MeleeAimAssistRadius)), QueryParams);
	AActor* const Candidate = bBlockingHit ? Hit.GetActor() : nullptr;
	const bool bValidContact = IsValidDamageTarget(Candidate, AttackingPawn)
		&& !HitActorsThisSwing.Contains(Candidate)
		&& IsWithinPunchRange(AttackingPawn, Hit.ImpactPoint)
		&& HasMeleeLineOfSight(AttackingPawn, Candidate, Hit.ImpactPoint);
	if (bValidContact && ApplyAttackToTarget(Candidate, AttackingPawn, GetCurrentAttackType()))
	{
		HitActorsThisSwing.Add(Candidate);
	}
	if (bDebugMeleeAim)
	{
		const FColor Color = bValidContact ? FColor::Green : FColor::Orange;
		DrawDebugLine(World, SweepStart, Tip, Color, false, 0.6f, 0, 1.25f);
		DrawDebugSphere(World, Tip, FMath::Max(1.0f, MeleeAimAssistRadius), 10, Color, false, 0.6f, 0, 1.0f);
	}
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

bool UJTSMeleeComponent::GetHeldItemAimView(APawn* AttackingPawn, FVector& OutOrigin, FVector& OutAimDirection) const
{
	OutOrigin = FVector::ZeroVector;
	OutAimDirection = FVector::ZeroVector;
	if (!IsValid(AttackingPawn))
	{
		return false;
	}

	// A held tool swings along the body. Third person keeps the camera free of that facing,
	// so the trace starts at the pawn and runs out of the actor forward, not the camera.
	const FVector Up = AttackingPawn->GetActorUpVector();
	OutAimDirection = FVector::VectorPlaneProject(AttackingPawn->GetActorForwardVector(), Up).GetSafeNormal();
	if (OutAimDirection.IsNearlyZero())
	{
		OutAimDirection = AttackingPawn->GetActorForwardVector().GetSafeNormal();
	}
	OutOrigin = AttackingPawn->GetActorLocation();
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
		float Damage = GetDamageForAttackType(AttackType);
		const AJTSPlayerState* const PlayerState = AttackingPawn->GetPlayerState<AJTSPlayerState>();
		const bool bCritical = Damage > KINDA_SMALL_NUMBER && IsValid(PlayerState)
			&& FMath::FRandRange(0.0f, 100.0f) < PlayerState->GetCriticalChancePercent();
		if (bCritical) Damage *= FJTSPlayerProgressionRules::CriticalDamageMultiplier;
		return Damage > KINDA_SMALL_NUMBER
			&& UGameplayStatics::ApplyDamage(Target, Damage, AttackingPawn->GetController(), AttackingPawn,
				bCritical ? UJTSCriticalDamageType::StaticClass() : UDamageType::StaticClass()) > 0.0f;
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
	if (CurrentAttackType != EJTSAttackType::Punch)
	{
		bAttackBuffered = false;
	}
}

void UJTSMeleeComponent::ClientConfirmPunchHit_Implementation()
{
	if (GetWorld() != nullptr)
	{
		LastConfirmedPunchHitSeconds = GetWorld()->GetTimeSeconds();
	}
	if (AJTSCharacter* const Character = Cast<AJTSCharacter>(GetOwner()))
	{
		Character->ApplyWeaponViewKick(PunchHitViewKickDegrees);
	}
}

void UJTSMeleeComponent::MulticastPunchImpact_Implementation(FVector_NetQuantize Location)
{
	if (IsValid(PunchImpactSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, PunchImpactSound, FVector(Location), 0.9f,
			FMath::FRandRange(0.97f, 1.03f));
	}
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
	if (AttackType == EJTSAttackType::Punch)
	{
		bPunchVisualClockActive = true;
		PunchVisualElapsed = 0.0f;
		SetComponentTickEnabled(true);
		if (IsValid(PunchSwingSound) && IsValid(GetOwner()))
		{
			UGameplayStatics::PlaySoundAtLocation(this, PunchSwingSound, GetOwner()->GetActorLocation(), 0.35f,
				FMath::FRandRange(0.97f, 1.03f));
		}
	}
	else
	{
		bMeleeSwingClockActive = true;
		MeleeSwingClockElapsed = 0.0f;
		SetComponentTickEnabled(true);
	}
	OnAttackStarted.Broadcast(AttackType);
}

void UJTSMeleeComponent::MulticastEndAttackPresentation_Implementation(EJTSAttackType AttackType)
{
	bIsAttacking = false;
	bAttackBuffered = false;
	bCurrentPunchIsComboContinuation = false;
	bMeleeSwingClockActive = false;
	MeleeSwingClockElapsed = 0.0f;
	bPunchVisualClockActive = false;
	PunchVisualElapsed = 0.0f;
	SetComponentTickEnabled(false);
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
				const FJTSResolvedWeaponStats Stats = FJTSWeaponProgression::Resolve(Inventory->GetActiveItem());
				return FMath::Max(0.0f, Stats.ScaleDamage(Definition->CombatDamage));
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
