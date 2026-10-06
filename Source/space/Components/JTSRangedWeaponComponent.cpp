// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSRangedWeaponComponent.h"

#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSStellarWeaponComponent.h"
#include "space/Components/JTSCriticalDamageType.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSWallClimbComponent.h"
#include "space/Components/JTSWeaponVisualComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSWeaponProgression.h"
#include "space/Interaction/JTSCriticalHitTarget.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Weapons/JTSGrenadeProjectile.h"
#include "space/Weapons/JTSProjectileActor.h"
#include "space/Weapons/JTSProjectileImpactActor.h"
#include "space/World/JTSMoonResourceActor.h"

namespace
{
	// Upgrade bonuses of the held weapon instance; replicated inventory means clients predict with the same numbers.
	FJTSResolvedWeaponStats ResolveHeldStats(const AActor* Owner)
	{
		const UJTSInventoryComponent* const Inventory = IsValid(Owner) ? Owner->FindComponentByClass<UJTSInventoryComponent>() : nullptr;
		return IsValid(Inventory) ? FJTSWeaponProgression::Resolve(Inventory->GetActiveItem()) : FJTSResolvedWeaponStats();
	}
}

UJTSRangedWeaponComponent::UJTSRangedWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UJTSRangedWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		if (UJTSInventoryComponent* const Inventory = GetOwner()->FindComponentByClass<UJTSInventoryComponent>())
		{
			Inventory->OnInventoryChanged.AddDynamic(this, &UJTSRangedWeaponComponent::HandleInventoryChanged);
		}
	}
}

void UJTSRangedWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearFireTimer();
	bFireHeld = false;
	bIsAiming = false;
	Super::EndPlay(EndPlayReason);
}

const UJTSItemDefinition* UJTSRangedWeaponComponent::GetActiveRangedDefinition() const
{
	const UJTSInventoryComponent* Inventory = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UJTSInventoryComponent>() : nullptr;
	if (!IsValid(Inventory)) return nullptr;
	const UJTSItemDefinition* Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, Inventory->GetActiveItemId());
	return IsValid(Definition) && Definition->IsRangedWeapon() ? Definition : nullptr;
}

bool UJTSRangedWeaponComponent::CanUseWeapon() const
{
	const auto* Stellar = GetOwner() ? GetOwner()->FindComponentByClass<UJTSStellarWeaponComponent>() : nullptr;
	if (Stellar && Stellar->HasActiveWeapon()) return false;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!IsValid(Pawn)) return false;
	if (const AJTSCharacter* Character = Cast<AJTSCharacter>(Pawn))
	{
		if (Character->IsBoarded()) return false;
		if (const UJTSWallClimbComponent* Climb = Character->FindComponentByClass<UJTSWallClimbComponent>();
			IsValid(Climb) && Climb->IsClimbing()) return false;
	}
	const UJTSHealthComponent* Health = Pawn->FindComponentByClass<UJTSHealthComponent>();
	return !IsValid(Health) || !Health->IsDead();
}

bool UJTSRangedWeaponComponent::HasActiveRangedWeapon() const
{
	return GetActiveRangedDefinition() != nullptr;
}

bool UJTSRangedWeaponComponent::IsEmptyHanded() const
{
	const UJTSInventoryComponent* const Inventory = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UJTSInventoryComponent>() : nullptr;
	if (!IsValid(Inventory)) return false;
	const FJTSItemInstance ActiveItem = Inventory->GetActiveItem();
	if (ActiveItem.IsEmpty()) return true;
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ActiveItem.ItemId);
	return IsValid(Definition) && !Definition->IsHoldable();
}

float UJTSRangedWeaponComponent::GetActiveAimFOV() const
{
	const UJTSItemDefinition* Definition = GetActiveRangedDefinition();
	return IsValid(Definition) ? FMath::Clamp(Definition->RangedAimFOV, 30.0f, 120.0f) : 60.0f;
}

void UJTSRangedWeaponComponent::StartAim()
{
	if (!CanUseWeapon()) return;
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		if (HasActiveRangedWeapon() || IsEmptyHanded())
		{
			bIsAiming = true;
			ServerStartAim();
		}
		return;
	}
	bIsAiming = HasActiveRangedWeapon() || IsEmptyHanded();
}

void UJTSRangedWeaponComponent::StopAim()
{
	bIsAiming = false;
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority()) ServerStopAim();
}

void UJTSRangedWeaponComponent::CancelForClimb()
{
	bIsAiming = false;
	bFireHeld = false;
	ClearFireTimer();
}

void UJTSRangedWeaponComponent::StartFire()
{
	if (bFireHeld) return;
	if (!CanUseWeapon()) return;
	const UJTSItemDefinition* Definition = GetActiveRangedDefinition();
	if (!IsValid(Definition)) return;
	bFireHeld = true;
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		TryLocalShot(Definition);
		ServerStartFire();
	}
	else
	{
		FireOnce();
	}
	// Holding attack repeats every ranged weapon at its own fire interval; a tap still fires once.
	ScheduleHeldFire(Definition);
}

void UJTSRangedWeaponComponent::StopFire()
{
	bFireHeld = false;
	ClearFireTimer();
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority()) ServerStopFire();
}

void UJTSRangedWeaponComponent::ServerStartFire_Implementation()
{
	StartFire();
}

void UJTSRangedWeaponComponent::ServerStopFire_Implementation()
{
	bFireHeld = false;
	ClearFireTimer();
}

void UJTSRangedWeaponComponent::ServerStartAim_Implementation()
{
	StartAim();
}

void UJTSRangedWeaponComponent::ServerStopAim_Implementation()
{
	bIsAiming = false;
}

float UJTSRangedWeaponComponent::ServerNow() const
{
	const UWorld* const World = GetWorld();
	if (World == nullptr) return 0.0f;
	if (const AGameStateBase* const State = World->GetGameState()) return State->GetServerWorldTimeSeconds();
	return World->GetTimeSeconds();
}

FJTSRangedWeaponStats UJTSRangedWeaponComponent::ResolveActiveStats() const
{
	const UJTSInventoryComponent* const Inventory = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UJTSInventoryComponent>() : nullptr;
	const UJTSItemDefinition* const Definition = GetActiveRangedDefinition();
	if (!IsValid(Inventory) || !IsValid(Definition)) return FJTSRangedWeaponStats();
	return FJTSWeaponProgression::ResolveRanged(Definition, Inventory->GetActiveItem());
}

float UJTSRangedWeaponComponent::CurrentEnergy(const FJTSRangedWeaponStats& Stats) const
{
	if (!Stats.UsesEnergy()) return 0.0f;
	return FMath::Clamp(Runtime.Energy + Stats.RechargePerSecond * FMath::Max(0.0f, ServerNow() - Runtime.EnergyStamp),
		0.0f, Stats.EnergyCapacity);
}

bool UJTSRangedWeaponComponent::CanShootNow(const FJTSRangedWeaponStats& Stats) const
{
	if (Stats.UsesEnergy())
	{
		return !IsEnergyLocked() && CurrentEnergy(Stats) >= FMath::Min(Stats.EnergyPerShot, Stats.EnergyCapacity);
	}
	if (Stats.HasMagazine()) return !IsReloading() && Runtime.Ammo > 0;
	return true;
}

bool UJTSRangedWeaponComponent::UsesMagazine() const { return ResolveActiveStats().HasMagazine(); }
bool UJTSRangedWeaponComponent::UsesEnergy() const { return ResolveActiveStats().UsesEnergy(); }

int32 UJTSRangedWeaponComponent::GetAmmo() const
{
	const FJTSRangedWeaponStats Stats = ResolveActiveStats();
	const bool bAuthority = GetOwner() != nullptr && GetOwner()->HasAuthority();
	return FMath::Clamp(bAuthority ? Runtime.Ammo : PredictedAmmo, 0, Stats.MagazineSize);
}

int32 UJTSRangedWeaponComponent::GetMagazineSize() const { return ResolveActiveStats().MagazineSize; }

bool UJTSRangedWeaponComponent::IsReloading() const
{
	return Runtime.ReloadStartTime >= 0.0f && ServerNow() < Runtime.ReloadStartTime + Runtime.ReloadDuration;
}

float UJTSRangedWeaponComponent::GetReloadProgress() const
{
	if (!IsReloading() || Runtime.ReloadDuration <= KINDA_SMALL_NUMBER) return 0.0f;
	return FMath::Clamp((ServerNow() - Runtime.ReloadStartTime) / Runtime.ReloadDuration, 0.0f, 1.0f);
}

float UJTSRangedWeaponComponent::GetEnergyFraction() const
{
	const FJTSRangedWeaponStats Stats = ResolveActiveStats();
	return Stats.UsesEnergy() ? CurrentEnergy(Stats) / Stats.EnergyCapacity : 0.0f;
}

bool UJTSRangedWeaponComponent::IsEnergyLocked() const
{
	const FJTSRangedWeaponStats Stats = ResolveActiveStats();
	return Stats.UsesEnergy() && Runtime.bEnergyLocked
		&& CurrentEnergy(Stats) < Stats.ResumeFraction * Stats.EnergyCapacity;
}

float UJTSRangedWeaponComponent::GetEnergyResumeFraction() const
{
	const FJTSRangedWeaponStats Stats = ResolveActiveStats();
	return Stats.UsesEnergy() ? Stats.ResumeFraction : 0.0f;
}

void UJTSRangedWeaponComponent::OnRep_Runtime()
{
	PredictedAmmo = Runtime.Ammo;
}

void UJTSRangedWeaponComponent::HandleInventoryChanged(int32 UsedSlots, int32 Capacity)
{
	SyncActiveWeapon();
}

void UJTSRangedWeaponComponent::SyncActiveWeapon()
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority()) return;
	const UJTSInventoryComponent* const Inventory = GetOwner()->FindComponentByClass<UJTSInventoryComponent>();
	const UJTSItemDefinition* const Definition = GetActiveRangedDefinition();
	if (!IsValid(Inventory)) return;
	const FJTSItemInstance ActiveItem = Inventory->GetActiveItem();
	const FGuid TargetId = IsValid(Definition) && !ActiveItem.IsEmpty() ? ActiveItem.InstanceId : FGuid();
	const FJTSRangedWeaponStats Stats = IsValid(Definition)
		? FJTSWeaponProgression::ResolveRanged(Definition, ActiveItem) : FJTSRangedWeaponStats();
	if (TargetId == TrackedInstanceId)
	{
		// Same weapon: an upgrade may have shrunk the magazine or the energy bar.
		if (TargetId.IsValid() && (Runtime.Ammo > Stats.MagazineSize || Runtime.Energy > Stats.EnergyCapacity))
		{
			Runtime.Ammo = FMath::Min(Runtime.Ammo, Stats.MagazineSize);
			Runtime.Energy = FMath::Min(Runtime.Energy, Stats.EnergyCapacity);
		}
		return;
	}
	if (TrackedInstanceId.IsValid())
	{
		FJTSStoredWeaponState& Parked = ParkedStates.FindOrAdd(TrackedInstanceId);
		Parked.Ammo = Runtime.Ammo;
		Parked.Energy = Runtime.Energy;
		Parked.Stamp = Runtime.EnergyStamp;
		Parked.bLocked = Runtime.bEnergyLocked;
	}
	if (UWorld* const World = GetWorld()) World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	Runtime = FJTSRangedRuntimeState();
	TrackedInstanceId = TargetId;
	if (TargetId.IsValid())
	{
		if (const FJTSStoredWeaponState* const Parked = ParkedStates.Find(TargetId))
		{
			Runtime.Ammo = FMath::Clamp(Parked->Ammo, 0, Stats.MagazineSize);
			Runtime.Energy = FMath::Min(Parked->Energy, Stats.EnergyCapacity);
			Runtime.EnergyStamp = Parked->Stamp;
			Runtime.bEnergyLocked = Parked->bLocked;
			ParkedStates.Remove(TargetId);
		}
		else
		{
			Runtime.Ammo = Stats.MagazineSize;
			Runtime.Energy = Stats.EnergyCapacity;
			Runtime.EnergyStamp = ServerNow();
		}
	}
	PredictedAmmo = Runtime.Ammo;
}

void UJTSRangedWeaponComponent::BeginReload(const FJTSRangedWeaponStats& Stats)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || GetWorld() == nullptr) return;
	if (!Stats.HasMagazine() || IsReloading() || Runtime.Ammo >= Stats.MagazineSize) return;
	Runtime.ReloadStartTime = ServerNow();
	Runtime.ReloadDuration = Stats.ReloadSeconds;
	GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, this, &UJTSRangedWeaponComponent::FinishReload,
		Stats.ReloadSeconds, false);
}

void UJTSRangedWeaponComponent::FinishReload()
{
	const FJTSRangedWeaponStats Stats = ResolveActiveStats();
	Runtime.Ammo = Stats.MagazineSize;
	Runtime.ReloadStartTime = -1.0f;
	PredictedAmmo = Runtime.Ammo;
}

void UJTSRangedWeaponComponent::RequestReload()
{
	if (GetOwner() == nullptr) return;
	if (GetOwner()->HasAuthority()) ServerReload_Implementation();
	else ServerReload();
}

void UJTSRangedWeaponComponent::ServerReload_Implementation()
{
	if (!CanUseWeapon()) return;
	SyncActiveWeapon();
	BeginReload(ResolveActiveStats());
}

void UJTSRangedWeaponComponent::ApplyChain(const FVector& Origin, AActor* FirstTarget, float Damage,
	const FJTSRangedWeaponStats& Stats, EJTSItemId ShotItem)
{
	APawn* const Pawn = Cast<APawn>(GetOwner());
	if (!IsValid(Pawn) || GetWorld() == nullptr || Stats.ChainTargets <= 0) return;
	TSet<AActor*> Visited;
	Visited.Add(FirstTarget);
	Visited.Add(Pawn);
	FVector From = Origin;
	float JumpDamage = Damage;
	for (int32 Jump = 0; Jump < Stats.ChainTargets; ++Jump)
	{
		JumpDamage *= 1.0f - Stats.ChainFalloff;
		TArray<FOverlapResult> Overlaps;
		const FCollisionShape Sphere = FCollisionShape::MakeSphere(Stats.ChainRangeCm);
		GetWorld()->OverlapMultiByChannel(Overlaps, From, FQuat::Identity, ECC_Pawn, Sphere);
		GetWorld()->OverlapMultiByChannel(Overlaps, From, FQuat::Identity, ECC_WorldDynamic, Sphere);
		AActor* Best = nullptr;
		FVector BestPoint = FVector::ZeroVector;
		float BestDistance = TNumericLimits<float>::Max();
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* const Candidate = Overlap.GetActor();
			if (!IsValid(Candidate) || Visited.Contains(Candidate) || Candidate->FindComponentByClass<UJTSHealthComponent>() == nullptr) continue;
			const FVector Point = Candidate->GetActorLocation();
			const float Distance = FVector::Dist(From, Point);
			FHitResult Block;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(JTSArcChain), false, Pawn);
			Params.AddIgnoredActor(Candidate);
			if (Distance < BestDistance && !GetWorld()->LineTraceSingleByChannel(Block, From, Point, ECC_Visibility, Params))
			{
				Best = Candidate;
				BestPoint = Point;
				BestDistance = Distance;
			}
		}
		if (Best == nullptr) break;
		Visited.Add(Best);
		UGameplayStatics::ApplyDamage(Best, JumpDamage, Pawn->GetController(), Pawn, UDamageType::StaticClass());
		MulticastShotTrace(From, BestPoint, (From - BestPoint).GetSafeNormal(), ShotItem, true, true, false);
		From = BestPoint;
	}
}

void UJTSRangedWeaponComponent::FireRay(const FVector& Start, const FVector& Direction, const FJTSRangedWeaponStats& Stats,
	const UJTSItemDefinition* Definition, const FJTSResolvedWeaponStats& Upgrades, bool& bAnyDamageable, bool& bAnyCritical)
{
	APawn* const Pawn = Cast<APawn>(GetOwner());
	const UJTSInventoryComponent* const Inventory = IsValid(Pawn) ? Pawn->FindComponentByClass<UJTSInventoryComponent>() : nullptr;
	if (!IsValid(Pawn) || !IsValid(Inventory) || GetWorld() == nullptr) return;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(JTSRangedShot), false, Pawn);
	Query.AddIgnoredActor(Pawn);
	TArray<AActor*> AttachedActors;
	Pawn->GetAttachedActors(AttachedActors, true, true);
	for (AActor* AttachedActor : AttachedActors) Query.AddIgnoredActor(AttachedActor);

	const float Range = FMath::Max(100.0f, Definition->RangedRange);
	const int32 Passes = 1 + FMath::Clamp(Stats.PierceCount, 0, 5);
	FVector RayStart = Start;
	float Travelled = 0.0f;
	float PierceScale = 1.0f;
	for (int32 Pass = 0; Pass < Passes; ++Pass)
	{
		const FVector TraceEnd = RayStart + Direction * (Range - Travelled);
		FHitResult Hit;
		GetWorld()->LineTraceSingleByChannel(Hit, RayStart, TraceEnd, ECC_Visibility, Query);
		const bool bHitSomething = Hit.bBlockingHit;
		const FVector End = bHitSomething ? Hit.ImpactPoint : TraceEnd;
		Travelled += FVector::Dist(RayStart, End);
		const FVector Normal = bHitSomething && !Hit.ImpactNormal.IsNearlyZero() ? Hit.ImpactNormal.GetSafeNormal() : -Direction;
		AActor* const HitActor = bHitSomething ? Hit.GetActor() : nullptr;
		bool bDamageable = false;
		bool bCritical = false;
		if (AJTSMoonResourceActor* const Resource = Cast<AJTSMoonResourceActor>(HitActor))
		{
			// Mining stays one weapon-wide number per trigger pull; pellets share it.
			Resource->ApplyMiningWork(Pawn, Inventory->GetActiveItemId(),
				Upgrades.ScaleDamage(Definition->MiningWork) / static_cast<float>(FMath::Max(1, Stats.Pellets)));
		}
		else if (IsValid(HitActor) && HitActor->FindComponentByClass<UJTSHealthComponent>() != nullptr)
		{
			float CriticalMultiplier = 1.0f;
			if (const IJTSCriticalHitTarget* const CriticalTarget = Cast<IJTSCriticalHitTarget>(HitActor))
			{
				CriticalMultiplier = FMath::Clamp(CriticalTarget->GetCriticalHitMultiplier(Hit), 1.0f, 5.0f);
				if (CriticalMultiplier > 1.0f + KINDA_SMALL_NUMBER) CriticalMultiplier += Upgrades.Get(EJTSWeaponStat::WeakPoint);
			}
			if (CriticalMultiplier <= 1.0f + KINDA_SMALL_NUMBER)
			{
				const AJTSPlayerState* const PlayerState = Pawn->GetPlayerState<AJTSPlayerState>();
				if (IsValid(PlayerState) && FMath::FRandRange(0.0f, 100.0f) < PlayerState->GetCriticalChancePercent())
				{
					CriticalMultiplier = FJTSPlayerProgressionRules::CriticalDamageMultiplier;
				}
			}
			bCritical = CriticalMultiplier > 1.0f + KINDA_SMALL_NUMBER;
			const float BaseDamage = Stats.DamagePerPellet * Stats.FalloffMultiplier(Travelled) * PierceScale;
			bDamageable = BaseDamage > 0.0f;
			bCritical = bCritical && bDamageable;
			UGameplayStatics::ApplyPointDamage(HitActor, BaseDamage * CriticalMultiplier, Direction, Hit,
				Pawn->GetController(), Pawn,
				bCritical ? UJTSCriticalDamageType::StaticClass() : UDamageType::StaticClass());
			if (bDamageable && Stats.ChainTargets > 0) ApplyChain(End, HitActor, BaseDamage, Stats, Definition->ItemId);
		}
		MulticastShotTrace(RayStart, End, Normal, Definition->ItemId, bHitSomething, bDamageable, bCritical);
		bAnyDamageable |= bDamageable;
		bAnyCritical |= bCritical;
		// A ray only continues through targets it actually damaged.
		if (!bDamageable || Pass + 1 >= Passes || Range - Travelled <= 50.0f) break;
		Query.AddIgnoredActor(HitActor);
		RayStart = End + Direction * 2.0f;
		PierceScale *= 0.75f;
	}
}

bool UJTSRangedWeaponComponent::FireOnce()
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || GetWorld() == nullptr || !CanUseWeapon()) return false;
	const UJTSInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UJTSInventoryComponent>();
	const UJTSItemDefinition* Definition = GetActiveRangedDefinition();
	APawn* Pawn = Cast<APawn>(GetOwner());
	const UJTSWeaponVisualComponent* Visual = IsValid(Pawn)
		? Pawn->FindComponentByClass<UJTSWeaponVisualComponent>() : nullptr;
	if (!IsValid(Inventory) || !IsValid(Definition) || !IsValid(Pawn) || !IsValid(Visual)) return false;

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now + KINDA_SMALL_NUMBER < NextFireTimeSeconds) return false;
	SyncActiveWeapon();
	const FJTSItemInstance ActiveItem = Inventory->GetActiveItem();
	const FJTSResolvedWeaponStats Upgrades = FJTSWeaponProgression::Resolve(ActiveItem);
	const FJTSRangedWeaponStats Stats = FJTSWeaponProgression::ResolveRanged(Definition, ActiveItem);
	// An empty magazine reloads by itself; a held trigger then resumes once the reload finishes.
	if (Stats.HasMagazine() && !IsReloading() && Runtime.Ammo <= 0)
	{
		BeginReload(Stats);
		return false;
	}
	if (!CanShootNow(Stats)) return false;

	const bool bLeftShot = Definition->HeldPresentation.bDualWield && bNextLeftServerShot;
	FTransform MuzzleTransform;
	if (!Visual->GetMuzzleWorldTransform(MuzzleTransform, bLeftShot)
		|| FVector::DistSquared(MuzzleTransform.GetLocation(), Pawn->GetActorLocation()) > FMath::Square(350.0f))
	{
		return false;
	}
	NextFireTimeSeconds = Now + FMath::Max(0.05f, Stats.FireInterval);
	if (Definition->HeldPresentation.bDualWield) bNextLeftServerShot = !bNextLeftServerShot;

	if (Stats.UsesEnergy())
	{
		Runtime.Energy = FMath::Max(0.0f, CurrentEnergy(Stats) - Stats.EnergyPerShot);
		Runtime.EnergyStamp = ServerNow();
		if (Runtime.Energy <= KINDA_SMALL_NUMBER) Runtime.bEnergyLocked = true;
	}
	else if (Stats.HasMagazine())
	{
		Runtime.Ammo = FMath::Max(0, Runtime.Ammo - 1);
	}

	const float SpreadDegrees = Stats.Pellets > 1
		? Definition->RangedHipSpreadDegrees : Upgrades.ScaleSpread(bIsAiming ? Definition->RangedAimSpreadDegrees : Definition->RangedHipSpreadDegrees);
	const FVector Forward = MuzzleTransform.GetUnitAxis(EAxis::X).GetSafeNormal();
	const FVector MuzzleStart = MuzzleTransform.GetLocation();
	// Shotguns keep a wide pattern even when aiming; the aim bonus applies to single rays only.
	const float PatternSpread = Stats.Pellets > 1
		? Upgrades.ScaleSpread(bIsAiming ? Definition->RangedAimSpreadDegrees + 1.0f : Definition->RangedHipSpreadDegrees)
		: SpreadDegrees;
	bool bAnyDamageable = false;
	bool bAnyCritical = false;
	if (Stats.IsProjectile())
	{
		const FVector Direction = FMath::VRandCone(Forward, FMath::DegreesToRadians(FMath::Clamp(PatternSpread, 0.0f, 12.0f)));
		FActorSpawnParameters Spawn;
		Spawn.Owner = Pawn;
		Spawn.Instigator = Pawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AJTSGrenadeProjectile* const Grenade = GetWorld()->SpawnActor<AJTSGrenadeProjectile>(
			AJTSGrenadeProjectile::StaticClass(), FTransform(Direction.Rotation(), MuzzleStart), Spawn))
		{
			Grenade->Launch(Direction, Stats.ProjectileSpeed, Stats.ProjectileGravityScale, Stats.DamagePerPellet,
				Stats.BlastRadiusCm, Stats.Fragments, Stats.FragmentDamage, Upgrades.ScaleDamage(Definition->MiningWork),
				Definition->ItemId, Definition->AccentColor);
		}
		// A short flash streak carries the muzzle sound and kick for everyone.
		MulticastShotTrace(MuzzleStart, MuzzleStart + Direction * 60.0f, -Direction, Definition->ItemId, false, false, false);
	}
	else
	{
		for (int32 Pellet = 0; Pellet < FMath::Clamp(Stats.Pellets, 1, 14); ++Pellet)
		{
			const FVector Direction = FMath::VRandCone(Forward, FMath::DegreesToRadians(FMath::Clamp(PatternSpread, 0.0f, 12.0f)));
			FireRay(MuzzleStart, Direction, Stats, Definition, Upgrades, bAnyDamageable, bAnyCritical);
		}
		if (bAnyDamageable) ClientConfirmRangedHit(bAnyCritical);
	}
	if (Stats.HasMagazine() && Runtime.Ammo <= 0) BeginReload(Stats);
	return true;
}

void UJTSRangedWeaponComponent::ScheduleHeldFire(const UJTSItemDefinition* Definition)
{
	if (!IsValid(Definition) || GetWorld() == nullptr) return;
	const EJTSItemId StartedItem = Definition->ItemId;
	// Polls faster than any fire interval so a reload or recharge that just finished resumes firing at once.
	// The server still enforces the real interval inside FireOnce.
	GetWorld()->GetTimerManager().SetTimer(AutomaticFireTimerHandle, [this, StartedItem]()
	{
		const UJTSItemDefinition* Current = GetActiveRangedDefinition();
		if (!bFireHeld || !CanUseWeapon() || !IsValid(Current) || Current->ItemId != StartedItem)
		{
			bFireHeld = false;
			ClearFireTimer();
			return;
		}
		if (GetOwner() != nullptr && GetOwner()->HasAuthority()) FireOnce();
		else TryLocalShot(Current);
	}, 0.04f, true);
}

bool UJTSRangedWeaponComponent::TryLocalShot(const UJTSItemDefinition* Definition)
{
	const FJTSRangedWeaponStats Stats = ResolveActiveStats();
	if (Stats.UsesEnergy())
	{
		if (IsEnergyLocked() || CurrentEnergy(Stats) < FMath::Min(Stats.EnergyPerShot, Stats.EnergyCapacity)) return false;
	}
	else if (Stats.HasMagazine() && (IsReloading() || PredictedAmmo <= 0))
	{
		return false;
	}
	if (!PlayLocalShotFeedback(Definition)) return false;
	if (Stats.HasMagazine()) PredictedAmmo = FMath::Max(0, PredictedAmmo - 1);
	return true;
}

void UJTSRangedWeaponComponent::ClearFireTimer()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(AutomaticFireTimerHandle);
}

bool UJTSRangedWeaponComponent::PlayLocalShotFeedback(const UJTSItemDefinition* Definition)
{
	if (!IsValid(Definition) || GetWorld() == nullptr) return false;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now + KINDA_SMALL_NUMBER < NextLocalFeedbackTimeSeconds) return false;
	const FJTSResolvedWeaponStats HeldStats = ResolveHeldStats(GetOwner());
	NextLocalFeedbackTimeSeconds = Now + FMath::Max(0.05f, ResolveActiveStats().FireInterval > 0.0f ? ResolveActiveStats().FireInterval : HeldStats.ScaleInterval(Definition->RangedFireInterval));
	LastLocalShotSeconds = Now;
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!IsValid(Pawn)) return false;
	const FLinearColor ShotColor = Definition->AccentColor;
	const bool bLeftShot = Definition->HeldPresentation.bDualWield && bNextLeftFeedbackShot;
	if (Definition->HeldPresentation.bDualWield) bNextLeftFeedbackShot = !bNextLeftFeedbackShot;
	UMaterialInterface* Glow = Definition->RangedGlowMaterial.LoadSynchronous();
	FVector SoundOrigin = Pawn->GetActorLocation();
	if (UJTSWeaponVisualComponent* Visual = Pawn->FindComponentByClass<UJTSWeaponVisualComponent>())
	{
		FVector MuzzleLocation;
		if (Visual->GetMuzzleWorldLocation(MuzzleLocation, bLeftShot)) SoundOrigin = MuzzleLocation;
		Visual->PlayShotPresentation(Glow, ShotColor, bLeftShot);
	}
	if (Pawn->IsLocallyControlled())
	{
		if (AJTSCharacter* Character = Cast<AJTSCharacter>(Pawn))
		{
			Character->ApplyWeaponViewKick(FMath::Clamp(HeldStats.ScaleViewKick(Definition->RangedViewKickDegrees), 0.0f, 8.0f));
		}
	}
	if (USoundBase* FireSound = Definition->RangedFireSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, SoundOrigin, 0.9f,
			FMath::FRandRange(0.97f, 1.03f));
	}
	return true;
}

void UJTSRangedWeaponComponent::MulticastShotTrace_Implementation(FVector_NetQuantize MuzzleStart,
	FVector_NetQuantize TraceEnd, FVector_NetQuantizeNormal ImpactNormal, EJTSItemId ShotItem,
	bool bHitSomething, bool bDamageableHit, bool bCritical)
{
	if (GetWorld() == nullptr) return;
	APawn* Pawn = Cast<APawn>(GetOwner());
	const UJTSItemDefinition* Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ShotItem);
	const FLinearColor ShotColor = IsValid(Definition) ? Definition->AccentColor : FLinearColor(1.0f, 0.7f, 0.3f);
	UMaterialInterface* Glow = IsValid(Definition) ? Definition->RangedGlowMaterial.LoadSynchronous() : nullptr;
	// The owning client already played the trigger, kick, flash, and sound when input was pressed.
	if (IsValid(Pawn) && (!Pawn->IsLocallyControlled() || Pawn->HasAuthority()))
	{
		PlayLocalShotFeedback(Definition);
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Pawn;
	SpawnParameters.Instigator = Pawn;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AJTSProjectileActor* Tracer = GetWorld()->SpawnActor<AJTSProjectileActor>(
		AJTSProjectileActor::StaticClass(), FTransform::Identity, SpawnParameters))
	{
		Tracer->InitializeTracer(MuzzleStart, TraceEnd, Glow, ShotColor);
	}
	if (bHitSomething)
	{
		if (AJTSProjectileImpactActor* Impact = GetWorld()->SpawnActor<AJTSProjectileImpactActor>(
			AJTSProjectileImpactActor::StaticClass(), FTransform(FRotator::ZeroRotator, TraceEnd + FVector(ImpactNormal) * 0.5f), SpawnParameters))
		{
			Impact->InitializeImpact(ImpactNormal, Glow,
				bCritical ? FLinearColor(1.0f, 0.58f, 0.08f) : ShotColor, !bDamageableHit);
			if (bCritical) Impact->SetActorScale3D(FVector(1.55f));
		}
		if (IsValid(Definition))
		{
			if (USoundBase* ImpactSound = Definition->RangedImpactSound.LoadSynchronous())
			{
				UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, TraceEnd, bCritical ? 0.88f : 0.6f,
					bCritical ? FMath::FRandRange(1.12f, 1.19f) : FMath::FRandRange(0.94f, 1.06f));
			}
		}
	}
	if (bDebugShotTraces)
	{
		DrawDebugLine(GetWorld(), MuzzleStart, TraceEnd, FColor(80, 210, 255), false, 0.18f, 0, 1.2f);
	}
}

void UJTSRangedWeaponComponent::ClientConfirmRangedHit_Implementation(bool bCritical)
{
	if (GetWorld() != nullptr)
	{
		LastConfirmedHitSeconds = GetWorld()->GetTimeSeconds();
		LastConfirmedCriticalHitSeconds = bCritical ? LastConfirmedHitSeconds : -100.0;
	}
}

float UJTSRangedWeaponComponent::GetReticleKickAlpha() const
{
	return GetWorld() != nullptr
		? FMath::Clamp(1.0f - static_cast<float>((GetWorld()->GetTimeSeconds() - LastLocalShotSeconds) / 0.16), 0.0f, 1.0f)
		: 0.0f;
}

bool UJTSRangedWeaponComponent::HasRecentConfirmedHit() const
{
	return GetWorld() != nullptr && GetWorld()->GetTimeSeconds() - LastConfirmedHitSeconds < 0.16;
}

bool UJTSRangedWeaponComponent::HasRecentConfirmedCriticalHit() const
{
	return GetWorld() != nullptr && GetWorld()->GetTimeSeconds() - LastConfirmedCriticalHitSeconds < 0.24;
}

void UJTSRangedWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSRangedWeaponComponent, bIsAiming);
	DOREPLIFETIME_CONDITION(UJTSRangedWeaponComponent, Runtime, COND_OwnerOnly);
}
