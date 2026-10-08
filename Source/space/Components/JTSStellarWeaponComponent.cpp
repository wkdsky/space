#include "space/Components/JTSStellarWeaponComponent.h"
#include "space/Components/JTSStellarAbilityComponent.h"
#include "space/Components/JTSStellarLoadoutComponent.h"
#include "space/Components/JTSStellarTargetComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSRangedWeaponComponent.h"
#include "space/Components/JTSMeleeComponent.h"
#include "space/Components/JTSWallClimbComponent.h"
#include "space/Components/JTSWeaponVisualComponent.h"
#include "space/Components/JTSCriticalDamageType.h"
#include "space/Interaction/JTSCriticalHitTarget.h"
#include "space/Items/JTSStellarLootTable.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Weapons/JTSStellarEffectActor.h"
#include "space/Weapons/JTSBlackHoleField.h"
#include "space/World/JTSPlanetAnchor.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

UJTSStellarWeaponComponent::UJTSStellarWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UJTSStellarWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	Catalog = WeaponCatalog.LoadSynchronous();
	RefreshEquipmentBinding();
}

UJTSStellarLoadoutComponent* UJTSStellarWeaponComponent::GetLoadout() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AJTSPlayerState* PS = Pawn ? Pawn->GetPlayerState<AJTSPlayerState>() : nullptr;
	return PS ? PS->GetStellarLoadout() : nullptr;
}

const FJTSStellarWeaponDefinition* UJTSStellarWeaponComponent::ResolveDefinition(FJTSStellarWeaponBinding& Binding) const
{
	const auto* Loadout = GetLoadout();
	return Catalog && Loadout && Loadout->GetActiveWeapon(Binding) ? Catalog->Find(Binding.CoreId, Binding.AttachmentId) : nullptr;
}

bool UJTSStellarWeaponComponent::HasActiveWeapon() const
{
	if (GetOwner()->GetLocalRole() == ROLE_SimulatedProxy)
		return !EquippedVisual.CoreId.IsNone() && !EquippedVisual.AttachmentId.IsNone();
	FJTSStellarWeaponBinding Binding;
	return ResolveDefinition(Binding) != nullptr;
}

const FJTSStellarWeaponDefinition* UJTSStellarWeaponComponent::GetEquippedWeaponDefinition() const
{
	if (GetOwner()->GetLocalRole() == ROLE_SimulatedProxy)
		return Catalog ? Catalog->Find(EquippedVisual.CoreId, EquippedVisual.AttachmentId) : nullptr;
	FJTSStellarWeaponBinding Binding;
	return ResolveDefinition(Binding);
}

FLinearColor UJTSStellarWeaponComponent::GetEquippedCoreColor() const
{
	const auto* Definition = GetEquippedWeaponDefinition();
	if (!Definition) return FLinearColor::White;
	const auto* Table = Catalog ? Catalog->LootTable.Get() : nullptr;
	return Table ? Table->GetCoreActivationColor(Definition->CoreId) : Definition->Color;
}

bool UJTSStellarWeaponComponent::CanUseWeapon() const
{
	const auto* Character = Cast<AJTSCharacter>(GetOwner());
	const auto* Health = GetOwner()->FindComponentByClass<UJTSHealthComponent>();
	const auto* Climb = GetOwner()->FindComponentByClass<UJTSWallClimbComponent>();
	return Character && Character->CanUseCombatInput() && (!Health || !Health->IsDead()) && (!Climb || !Climb->IsClimbing());
}

bool UJTSStellarWeaponComponent::IsAiming() const
{
	const auto* Def = GetEquippedWeaponDefinition();
	return bLocalSecondary && Def && (Def->Mode == EJTSStellarWeaponMode::Jet || Def->Mode == EJTSStellarWeaponMode::Focus || Def->Mode == EJTSStellarWeaponMode::Diffusion);
}

bool UJTSStellarWeaponComponent::IsCasting() const
{
	const auto* Def = GetEquippedWeaponDefinition();
	if (!Def || !GetWorld()) return false;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const bool bPredicted = Pawn && Pawn->IsLocallyControlled()
		&& (bLocalSecondary || (bLocalPrimary && Def->Mode != EJTSStellarWeaponMode::BlackHole));
	return bPredicted || bReplicatedCasting || GetWorld()->GetTimeSeconds() < CastGestureUntil;
}

void UJTSStellarWeaponComponent::CycleWeapon()
{
	if (!CanUseWeapon()) return;
	if (GetOwner()->HasAuthority()) ServerCycleWeapon_Implementation();
	else
	{
		StopChannels();
		ServerCycleWeapon();
	}
}

void UJTSStellarWeaponComponent::ServerCycleWeapon_Implementation()
{
	if (!CanUseWeapon()) return;
	auto* Loadout = GetLoadout();
	if (!Loadout) return;
	if (Catalog) Loadout->ConfigureLootTable(Catalog->LootTable.LoadSynchronous());
	Loadout->RefreshParticipants();
	const auto Weapons = Loadout->GetWeapons();
	int32 Next = INDEX_NONE;
	for (const auto& Weapon : Weapons) if (Catalog && Catalog->Find(Weapon.CoreId, Weapon.AttachmentId)
		&& Weapon.CoreSlot > Loadout->GetActiveCoreSlot()) { Next = Weapon.CoreSlot; break; }
	if (Next == INDEX_NONE) for (const auto& Weapon : Weapons) if (Catalog && Catalog->Find(Weapon.CoreId, Weapon.AttachmentId))
		{ Next = Weapon.CoreSlot; break; }
	if (Next == INDEX_NONE) return;
	StopChannels();
	Loadout->SelectWeapon(Next);
}

void UJTSStellarWeaponComponent::ReturnToNormalWeapon()
{
	StopChannels();
	// Keep return and cycle requests on the same owner actor channel so 1-9 then Tab stay ordered.
	if (GetOwner()->HasAuthority()) ServerReturnToNormalWeapon_Implementation();
	else ServerReturnToNormalWeapon();
}

void UJTSStellarWeaponComponent::ServerReturnToNormalWeapon_Implementation()
{
	if (auto* Loadout = GetLoadout()) Loadout->SelectWeapon(INDEX_NONE);
}

void UJTSStellarWeaponComponent::SetPrimary(bool bHeld)
{
	if (bHeld && (!CanUseWeapon() || !HasActiveWeapon())) return;
	const auto* Def = GetEquippedWeaponDefinition();
	if (bHeld && !bLocalPrimary && Def && Def->Mode == EJTSStellarWeaponMode::BlackHole)
		CastGestureUntil = GetWorld()->GetTimeSeconds() + 0.55;
	bLocalPrimary = bHeld;
	SubmitLocalInput();
}

void UJTSStellarWeaponComponent::SetSecondary(bool bHeld)
{
	if (bHeld && (!CanUseWeapon() || !HasActiveWeapon())) return;
	bLocalSecondary = bHeld;
	SubmitLocalInput();
}

void UJTSStellarWeaponComponent::GetLocalViewRay(FVector& Origin, FVector& Direction) const
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) { Origin = FVector::ZeroVector; Direction = FVector::ForwardVector; return; }
	FJTSStellarWeaponBinding Binding;
	const auto* Definition = ResolveDefinition(Binding);
	Direction = Pawn->GetControlRotation().Vector().GetSafeNormal();
	Origin = Pawn->GetPawnViewLocation();
	if (Definition && Definition->Mode == EJTSStellarWeaponMode::BlackHole)
		if (const auto* Controller = Cast<APlayerController>(Pawn->GetController()))
		{
			FRotator ViewRotation;
			Controller->GetPlayerViewPoint(Origin, ViewRotation);
			Direction = ViewRotation.Vector().GetSafeNormal();
		}
}

void UJTSStellarWeaponComponent::SubmitLocalInput()
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled()) return;
	FJTSStellarWeaponBinding Binding;
	ResolveDefinition(Binding);
	FVector ViewOrigin, Direction;
	GetLocalViewRay(ViewOrigin, Direction);
	if (Pawn->HasAuthority()) ServerSetInput_Implementation(bLocalPrimary, bLocalSecondary,
		Binding.CoreInstanceId, Binding.AttachmentInstanceId, Direction, ViewOrigin);
	else ServerSetInput(bLocalPrimary, bLocalSecondary, Binding.CoreInstanceId, Binding.AttachmentInstanceId, Direction, ViewOrigin);
	if (bLocalPrimary || bLocalSecondary)
		GetWorld()->GetTimerManager().SetTimer(HeartbeatTimer, this, &ThisClass::LocalHeartbeat, 0.1f, true);
	else GetWorld()->GetTimerManager().ClearTimer(HeartbeatTimer);
}

void UJTSStellarWeaponComponent::LocalHeartbeat()
{
	if (!CanUseWeapon() || !HasActiveWeapon()) { StopChannels(); return; }
	APawn* Pawn = Cast<APawn>(GetOwner());
	FVector Origin, Direction;
	GetLocalViewRay(Origin, Direction);
	if (Pawn->HasAuthority()) ServerUpdateAim_Implementation(Direction);
	else ServerUpdateAim(Direction);
}

void UJTSStellarWeaponComponent::ServerUpdateAim_Implementation(FVector_NetQuantizeNormal Direction)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || Direction.ContainsNaN() || !FMath::IsNearlyEqual(static_cast<float>(Direction.SizeSquared()), 1.0f, 0.05f)
		|| FVector::DotProduct(Pawn->GetBaseAimRotation().Vector(), Direction) < 0.5f) return;
	AimDirection = Direction.GetSafeNormal();
	LastHeartbeat = GetWorld()->GetTimeSeconds();
}

void UJTSStellarWeaponComponent::ServerSetInput_Implementation(bool bPrimary, bool bSecondary,
	FGuid CoreId, FGuid AttachmentId, FVector_NetQuantizeNormal Direction, FVector_NetQuantize CastViewOrigin)
{
	if (!bPrimary && !bSecondary)
	{
		FJTSStellarWeaponBinding Released; const auto* Definition = ResolveDefinition(Released);
		if (CanUseWeapon() && Definition && Released.CoreInstanceId == ChannelBinding.CoreInstanceId && Released.AttachmentInstanceId == ChannelBinding.AttachmentInstanceId)
			if (auto* Abilities = GetOwner()->FindComponentByClass<UJTSStellarAbilityComponent>()) Abilities->InputChanged(*Definition, Released, false, false, AimDirection);
		StopServerChannels(); return;
	}
	auto* Loadout = GetLoadout();
	if (!Loadout) return;
	if (Catalog) Loadout->ConfigureLootTable(Catalog->LootTable.LoadSynchronous());
	Loadout->RefreshParticipants();
	FJTSStellarWeaponBinding Binding;
	const auto* Def = ResolveDefinition(Binding);
	if (!CanUseWeapon() || !Def || Binding.CoreInstanceId != CoreId || Binding.AttachmentInstanceId != AttachmentId) return;
	// Presentation-only combinations can raise their scepter, but never execute another mode's attacks.
	const APawn* AimPawn = Cast<APawn>(GetOwner());
	if (!AimPawn || Direction.ContainsNaN() || !FMath::IsNearlyEqual(static_cast<float>(Direction.SizeSquared()), 1.0f, 0.05f)
		|| FVector::DotProduct(AimPawn->GetBaseAimRotation().Vector(), Direction) < 0.5f) return;
	ServerUpdateAim_Implementation(Direction);
	if (GetWorld()->GetTimeSeconds() - LastHeartbeat > 0.35) return;
	if (auto* Ranged = GetOwner()->FindComponentByClass<UJTSRangedWeaponComponent>()) Ranged->CancelForClimb();
	const bool bNewField = bPrimary && !bServerPrimary && Def->Mode == EJTSStellarWeaponMode::BlackHole;
	ChannelBinding = Binding;
	if (auto* Abilities = GetOwner()->FindComponentByClass<UJTSStellarAbilityComponent>()) Abilities->InputChanged(*Def, Binding, bPrimary, bSecondary, AimDirection);
	bServerPrimary = bPrimary;
	bServerSecondary = bSecondary;
	bReplicatedCasting = bSecondary || (bPrimary && Def->Mode != EJTSStellarWeaponMode::BlackHole);
	GetOwner()->ForceNetUpdate();
	if (bNewField) TryCastBlackHole(*Def, Binding, CastViewOrigin);
	if (!bSecondary && Def->Mode == EJTSStellarWeaponMode::BlackHole)
	{
		// A press owns no continuous attack channel. Keep the input edge latch until release.
		RemoveForces();
		Loadout->SetChannelActive(false);
		GetWorld()->GetTimerManager().ClearTimer(PulseTimer);
		MulticastStopEffect();
		return;
	}
	if (BoundLoadout.Get() != Loadout)
	{
		if (BoundLoadout.IsValid()) BoundLoadout->OnLoadoutChanged.RemoveDynamic(this, &ThisClass::HandleLoadoutChanged);
		BoundLoadout = Loadout;
		Loadout->OnLoadoutChanged.AddDynamic(this, &ThisClass::HandleLoadoutChanged);
	}
	if (!GetWorld()->GetTimerManager().IsTimerActive(PulseTimer))
	{
		LastPulseTime = GetWorld()->GetTimeSeconds();
		AreaAccumulator = 0;
		GetWorld()->GetTimerManager().SetTimer(PulseTimer, this, &ThisClass::ServerPulse, 0.025f, true);
	}
}

void UJTSStellarWeaponComponent::ServerPulse()
{
	FJTSStellarWeaponBinding Current;
	const auto* Def = ResolveDefinition(Current);
	auto* Loadout = GetLoadout();
	const double Now = GetWorld()->GetTimeSeconds();
	if (!CanUseWeapon() || !Def || !Loadout || Current.CoreInstanceId != ChannelBinding.CoreInstanceId
		|| Current.AttachmentInstanceId != ChannelBinding.AttachmentInstanceId
		// Timers due during a slow server frame can run before this frame's local heartbeat.
		// Discount that frame once; a missing client heartbeat still expires on normal frames.
		|| Now - LastHeartbeat > 0.35 + GetWorld()->GetDeltaSeconds())
		{ StopServerChannels(); return; }
	const float Delta = static_cast<float>(FMath::Clamp(Now - LastPulseTime, 0.0, 0.2));
	LastPulseTime = Now;
	// Point changes are adopted at the next attack/pulse, never rewrite the recorded allocation.
	ChannelBinding = Current;
	if (Def->Mode > EJTSStellarWeaponMode::PresentationOnly)
	{
		Loadout->SetChannelActive(bServerPrimary || (bServerSecondary && (Def->Mode == EJTSStellarWeaponMode::Healing || Def->Mode == EJTSStellarWeaponMode::Freezing || Def->Mode == EJTSStellarWeaponMode::Explosion || Def->Mode == EJTSStellarWeaponMode::Shaping)));
		if (auto* Abilities = GetOwner()->FindComponentByClass<UJTSStellarAbilityComponent>())
			if (!Abilities->ExecutePulse(*Def, Current, bServerPrimary, bServerSecondary, AimDirection, Delta)) StopServerChannels();
		return;
	}
	const auto& X = Current.EffectiveLevels;
	float Cost = 0;
	const bool bJetChannel = Def->Mode == EJTSStellarWeaponMode::Jet && (bServerPrimary || bServerSecondary);
	if (bJetChannel) Cost = Def->EnergyPerSecond * (1 - 0.03 * X[5]) * Delta;
	const bool bFocusShotDue = Def->Mode == EJTSStellarWeaponMode::Focus && bServerPrimary && Now + 0.0001 >= NextFocusShot;
	if (bFocusShotDue) Cost = Def->EnergyPerSecond / 8.0f;
	if (Def->Mode == EJTSStellarWeaponMode::BlackHole)
		Cost = (bServerSecondary ? 16.0f : 0) * (1 - 0.03 * X[5]) * Delta;
	Loadout->SetChannelActive(bJetChannel || (Def->Mode == EJTSStellarWeaponMode::BlackHole ? bServerSecondary :
		Def->Mode == EJTSStellarWeaponMode::Focus && bServerPrimary));
	if (Cost > 0 && !Loadout->ConsumeEnergy(Cost)) { StopServerChannels(); return; }
	AreaAccumulator += Delta;
	if (bFocusShotDue)
	{
		NextFocusShot = Now + 1.0 / (8 * (1 + 0.03 * X[4]));
		FocusShot(*Def, Current);
	}
	const float AreaInterval = Def->Mode == EJTSStellarWeaponMode::BlackHole ? 0.05f : 0.2f;
	if (AreaAccumulator + KINDA_SMALL_NUMBER >= AreaInterval)
	{
		if (bJetChannel) JetPulse(*Def, Current, AreaAccumulator);
		if (Def->Mode == EJTSStellarWeaponMode::BlackHole && bServerSecondary) RepulsionPulse(*Def, Current);
		AreaAccumulator = 0;
	}
}

void UJTSStellarWeaponComponent::JetPulse(const FJTSStellarWeaponDefinition& Def,
	const FJTSStellarWeaponBinding& Binding, float Delta)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	const auto& X = Binding.EffectiveLevels;
	const FVector Origin = Pawn->GetPawnViewLocation();
	const float Range = (bServerSecondary ? Def.FocusedRangeCentimeters : Def.RangeCentimeters) + Def.RangePerLevel * X[2];
	const float Angle = FMath::Clamp((bServerSecondary ? Def.FocusedConeAngleDegrees : Def.ConeAngleDegrees)
		+ (bServerSecondary ? 1.0f : 2.0f) * X[1], 5.0f, 160.0f);
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(Angle * 0.5f));
	TArray<AActor*> Targets;
	// Apply the hit budget after cone filtering; nearby off-axis enemies cannot consume it.
	// Range is the cone's axial length, matching the visible plume. A sphere of radius
	// Range would silently cut off its broad sides, especially with a 120-degree jet.
	UJTSStellarTargetComponent::QueryTargets(GetWorld(), Origin, Range / CosHalfAngle, MAX_int32, Targets);
	int32 HitCount = 0;
	for (AActor* Target : Targets)
	{
		const FVector To = Target->GetActorLocation() - Origin;
		if (FVector::DotProduct(To, AimDirection) > Range
			|| FVector::DotProduct(To.GetSafeNormal(), AimDirection) < CosHalfAngle
			|| !UJTSStellarTargetComponent::HasLineOfSight(GetWorld(), Origin, Target, Pawn)) continue;
		UGameplayStatics::ApplyDamage(Target, Def.BaseDamage * (1 + 0.04 * X[0]) * Delta,
			Pawn->GetController(), Pawn, UDamageType::StaticClass());
		if (IsValid(Target))
		{
			auto* Status = Target->FindComponentByClass<UJTSStellarTargetComponent>();
			Status->SetStatusPresentation(Def.TargetStatusEffectClass);
			Status->ApplyFire(Pawn, 50 * (1 + 0.1 * X[3]) * Delta, Def.StatusDamagePerSecond, FMath::FloorToInt(X[4] / 2));
		}
		if (++HitCount >= FMath::Clamp(Def.MaximumTargets, 1, 256)) break;
	}
	// The central plume stops at terrain, while each target uses its own occlusion ray.
	FVector VisualEnd = Origin + AimDirection * Range;
	FHitResult SurfaceHit;
	FCollisionQueryParams SurfaceParams(SCENE_QUERY_STAT(StellarJetSurface), false, Pawn);
	if (GetWorld()->LineTraceSingleByObjectType(SurfaceHit, Origin, VisualEnd,
		FCollisionObjectQueryParams(ECC_WorldStatic), SurfaceParams)) VisualEnd = SurfaceHit.ImpactPoint;
	MulticastEffect(Binding.CoreId, Binding.AttachmentId, Origin, VisualEnd,
		FVector::Dist(Origin, VisualEnd) * FMath::Tan(FMath::DegreesToRadians(Angle * 0.5f)), true, bServerSecondary,
		true, SurfaceHit.bBlockingHit);
}

void UJTSStellarWeaponComponent::FocusShot(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	const auto& X = Binding.EffectiveLevels;
	FVector Start = Pawn->GetPawnViewLocation();
	const FVector Origin = Start;
	const FVector End = Start + AimDirection * Def.RangeCentimeters;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StellarFocus), false, Pawn);
	TSet<AActor*> Visited;
	FVector LastHit = End;
	FVector LastTargetHit = End;
	const int32 PierceHits = FMath::Clamp(Def.BasePierceTargets + FMath::FloorToInt(X[1] / 2), 1, 16);
	int32 Hits = 0;
	auto ApplyHit = [&](AActor* Target, const FHitResult* Hit, float Scale)
	{
		Visited.Add(Target); ++Hits;
		float WeakPoint = 1;
		if (Hit) if (const auto* Critical = Cast<IJTSCriticalHitTarget>(Target))
			if (Critical->GetCriticalHitMultiplier(*Hit) > 1.0f) WeakPoint = 2.0 + 0.05 * X[5];
		const float Damage = Def.BaseDamage * (1 + 0.04 * X[0]) * Scale * WeakPoint;
		if (Hit) UGameplayStatics::ApplyPointDamage(Target, Damage, AimDirection, *Hit, Pawn->GetController(), Pawn,
			WeakPoint > 1 ? UJTSCriticalDamageType::StaticClass() : UDamageType::StaticClass());
		else UGameplayStatics::ApplyDamage(Target, Damage, Pawn->GetController(), Pawn, UDamageType::StaticClass());
		if (IsValid(Target))
		{
			auto* Status = Target->FindComponentByClass<UJTSStellarTargetComponent>();
			Status->SetStatusPresentation(Def.TargetStatusEffectClass);
			Status->ApplyLightBurn(Pawn, Def.StatusDamagePerSecond * Scale, 2.0f, X[3] > 0 ? 2 : 0, 0.05 * X[3]);
		}
	};
	for (int32 Pass = 0; Pass < PierceHits; ++Pass)
	{
		FHitResult Hit;
		const bool bHit = Def.BeamRadiusCentimeters > 0
			? GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility,
				FCollisionShape::MakeSphere(Def.BeamRadiusCentimeters), Params)
			: GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
		if (!bHit) break;
		LastHit = Hit.ImpactPoint;
		AActor* Target = Hit.GetActor();
		const auto* Component = IsValid(Target) ? Target->FindComponentByClass<UJTSStellarTargetComponent>() : nullptr;
		if (!Component || !Component->IsAliveTarget()) break;
		ApplyHit(Target, &Hit, FMath::Pow(0.8f, Pass));
		LastTargetHit = Hit.ImpactPoint;
		Params.AddIgnoredActor(Target);
		Start = Hit.ImpactPoint + AimDirection * 2;
	}
	MulticastEffect(Binding.CoreId, Binding.AttachmentId, Origin, LastHit,
		FMath::Max(4.0f, Def.BeamRadiusCentimeters), true, bServerSecondary, true, Hits > 0 || LastHit != End);
	LastHit = LastTargetHit; // Chains start at the last struck enemy, even if the main beam meets a wall after it.
	for (int32 Bounce = 0; Bounce < FMath::Min(8, Def.BaseChainTargets + FMath::FloorToInt(X[2] / 2)) && Hits > 0; ++Bounce)
	{
		TArray<AActor*> Candidates;
		UJTSStellarTargetComponent::QueryTargets(GetWorld(), LastHit, Def.ChainRadiusCentimeters, 32, Candidates);
		AActor* Next = nullptr;
		for (auto* Candidate : Candidates) if (!Visited.Contains(Candidate)
			&& UJTSStellarTargetComponent::HasLineOfSight(GetWorld(), LastHit, Candidate, Pawn)) { Next = Candidate; break; }
		if (!Next) break;
		const FVector BounceStart = LastHit;
		ApplyHit(Next, nullptr, FMath::Pow(0.8f, Bounce + 1));
		LastHit = Next->GetActorLocation();
		MulticastEffect(Binding.CoreId, Binding.AttachmentId, BounceStart, LastHit,
			FMath::Max(4.0f, Def.BeamRadiusCentimeters), true, false, false, true);
	}
}

int32 UJTSStellarWeaponComponent::GetActiveBlackHoleCount() const
{
	int32 Count = 0;
	for (const auto& Field : BlackHoleFields) if (Field.IsValid() && !Field->IsActorBeingDestroyed()) ++Count;
	return Count;
}

bool UJTSStellarWeaponComponent::TryCastBlackHole(const FJTSStellarWeaponDefinition& Def,
	const FJTSStellarWeaponBinding& Binding, FVector CastViewOrigin)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	auto* Loadout = GetLoadout();
	if (!Pawn || !Pawn->HasAuthority() || !Loadout || Binding.EffectiveLevels.Num() != 6) return false;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextBlackHoleCast.FindRef(Binding.AttachmentInstanceId)) return false;
	const FVector Origin = Pawn->GetPawnViewLocation();
	const auto* Character = Cast<AJTSCharacter>(Pawn);
	const float CameraBound = Character ? Character->GetMaximumGameplayCameraOffset() : 0.0f;
	if (CastViewOrigin.ContainsNaN() || FVector::DistSquared(Origin, CastViewOrigin) > FMath::Square(CameraBound)) return false;
	const float Range = FMath::Max(1.0f, Def.RangeCentimeters);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StellarFieldPlacement), false, Pawn);
	TArray<AActor*> Bodies;
	UJTSStellarTargetComponent::QueryTargets(GetWorld(), Origin, Range, MAX_int32, Bodies);
	Params.AddIgnoredActors(Bodies);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (const auto* Controller = It->Get()) Params.AddIgnoredActor(Controller->GetPawn());
	// Find the reticle's actual surface, then measure from the player. An eye-parallel ray could hit nearer ground.
	if (!GetWorld()->LineTraceSingleByChannel(Hit, CastViewOrigin, CastViewOrigin + AimDirection * (Range + CameraBound), ECC_Visibility, Params) || !Hit.bBlockingHit
		|| Hit.ImpactPoint.ContainsNaN() || FVector::DistSquared(Origin, Hit.ImpactPoint) > FMath::Square(Range)) return false;
	if (Hit.GetActor() && Hit.GetActor()->FindComponentByClass<UJTSStellarTargetComponent>()) return false;
	FHitResult Obstruction;
	if (GetWorld()->LineTraceSingleByChannel(Obstruction, Origin, Hit.ImpactPoint + Hit.ImpactNormal,
		ECC_Visibility, Params) && FVector::DistSquared(Obstruction.ImpactPoint, Hit.ImpactPoint) > FMath::Square(2.0f)) return false;
	const FVector Up = Character && Character->GetGameplayPlanet()
		? Character->GetGameplayPlanet()->GetRadialUpVector(Hit.ImpactPoint) : Pawn->GetActorUpVector();
	// Walls and ceilings are not ground. This uses the current planet's radial up, never fixed world Z.
	if (FVector::DotProduct(Hit.ImpactNormal, Up) < 0.35f) return false;
	const float Cost = Def.BlackHoleEnergyPerCast * (1 - 0.03 * Binding.EffectiveLevels[5]);
	if (!FMath::IsFinite(Cost) || Cost <= 0 || Loadout->GetEnergy() + KINDA_SMALL_NUMBER < Cost) return false;
	const FVector Center = Hit.ImpactPoint + Hit.ImpactNormal * (FMath::Max(10.0f, Def.BlackHoleCoreRadiusCentimeters)
		+ FMath::Max(0.0f, Def.BlackHoleGroundClearanceCentimeters));
	const FTransform Transform(FQuat::Identity, Center);
	auto* Field = GetWorld()->SpawnActorDeferred<AJTSBlackHoleField>(AJTSBlackHoleField::StaticClass(), Transform,
		Pawn, Pawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Field) return false;
	if (!Loadout->ConsumeEnergy(Cost)) { Field->Destroy(); return false; }
	Field->Initialize(Def, Binding, Up, Hit.ImpactPoint);
	BlackHoleFields.RemoveAll([](const auto& Existing) { return !Existing.IsValid() || Existing->IsActorBeingDestroyed(); });
	const int32 Limit = FMath::Clamp(Def.BlackHoleBaseMaximumFields + FMath::FloorToInt(Binding.EffectiveLevels[3] / 2), 1, 6);
	int32 Matching = 0;
	for (const auto& Existing : BlackHoleFields) if (Existing->BelongsTo(Binding)) ++Matching;
	// Entries are appended in cast order. Replace oldest casts only after the new cast is validated and paid.
	for (int32 Index = 0; Matching >= Limit && Index < BlackHoleFields.Num();)
	{
		if (BlackHoleFields[Index]->BelongsTo(Binding))
		{
			BlackHoleFields[Index]->Destroy();
			BlackHoleFields.RemoveAt(Index);
			--Matching;
		}
		else ++Index;
	}
	BlackHoleFields.Add(Field);
	Field->FinishSpawning(Transform);
	NextBlackHoleCast.Add(Binding.AttachmentInstanceId, Now + FMath::Max(0.1f, Def.BlackHoleCastCooldown));
	MulticastCastGesture();
	return true;
}

void UJTSStellarWeaponComponent::MulticastCastGesture_Implementation()
{
	CastGestureUntil = GetWorld()->GetTimeSeconds() + 0.55;
}

void UJTSStellarWeaponComponent::RepulsionPulse(const FJTSStellarWeaponDefinition& Def,
	const FJTSStellarWeaponBinding& Binding)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	const auto& X = Binding.EffectiveLevels;
	if (bServerSecondary)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		TArray<AActor*> Targets;
		// Include the narrow damping band outside the dome so a finishing push does not lose its source.
		UJTSStellarTargetComponent::QueryTargets(GetWorld(), Pawn->GetActorLocation(),
			Def.RepulsionRadiusCentimeters + Def.RepulsionExitOffsetCentimeters + 10.0f, Def.MaximumTargets, Targets);
		for (auto* Target : Targets)
		{
			const FGuid TargetKey = FGuid(0, 0, 0, Target->GetUniqueID());
			if (!UJTSStellarTargetComponent::HasLineOfSight(GetWorld(), Pawn->GetPawnViewLocation(), Target, Pawn)) continue;
			// The barrier is continuous while held. Damage cadence must never create gaps in its force.
			Target->FindComponentByClass<UJTSStellarTargetComponent>()->ApplyRepulsion(Pawn, Def.RepulsionStiffness,
				Def.RepulsionRadiusCentimeters, 0.2f, Def.RepulsionEntryDepthCentimeters, Def.RepulsionExitOffsetCentimeters);
			ControlledTargets.AddUnique(Target);
			if (Now < NextRepulsion.FindRef(TargetKey)) continue;
			NextRepulsion.Add(TargetKey, Now + 1.2 - 0.05 * X[4]);
			UGameplayStatics::ApplyDamage(Target, Def.BaseDamage * (1 + 0.05 * X[0]) * 0.8f,
				Pawn->GetController(), Pawn, UDamageType::StaticClass());
		}
	}
	MulticastEffect(Binding.CoreId, Binding.AttachmentId, Pawn->GetPawnViewLocation(), Pawn->GetActorLocation(),
		Def.RepulsionRadiusCentimeters, false, bServerSecondary);
}

void UJTSStellarWeaponComponent::RemoveForces()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	for (auto Target : ControlledTargets) if (Target.IsValid())
		if (auto* Component = Target->FindComponentByClass<UJTSStellarTargetComponent>()) Component->RemoveForce(Pawn);
	ControlledTargets.Reset();
}

void UJTSStellarWeaponComponent::StopServerChannels()
{
	if (auto* Abilities = GetOwner()->FindComponentByClass<UJTSStellarAbilityComponent>()) Abilities->CancelCast();
	bServerPrimary = false; bServerSecondary = false; AreaAccumulator = 0;
	bReplicatedCasting = false;
	GetOwner()->ForceNetUpdate();
	if (auto* Loadout = GetLoadout()) Loadout->SetChannelActive(false);
	RemoveForces();
	GetWorld()->GetTimerManager().ClearTimer(PulseTimer);
	MulticastStopEffect();
}

void UJTSStellarWeaponComponent::StopChannels()
{
	bLocalPrimary = false; bLocalSecondary = false;
	GetWorld()->GetTimerManager().ClearTimer(HeartbeatTimer);
	if (GetOwner()->HasAuthority()) StopServerChannels();
	else ServerSetInput(false, false, FGuid(), FGuid(), FVector::ForwardVector, FVector::ZeroVector);
	if (IsValid(LocalEffect)) LocalEffect->Destroy();
	LocalEffect = nullptr;
}

void UJTSStellarWeaponComponent::HandleLoadoutChanged()
{
	if (auto* Abilities = GetOwner()->FindComponentByClass<UJTSStellarAbilityComponent>()) Abilities->ValidatePersistentEffects();
	FJTSStellarWeaponBinding Selected;
	if (GetOwner()->HasAuthority())
	{
		const bool bEquipped = ResolveDefinition(Selected) != nullptr;
		EquippedVisual.CoreId = bEquipped ? Selected.CoreId : NAME_None;
		EquippedVisual.AttachmentId = bEquipped ? Selected.AttachmentId : NAME_None;
		if (bEquipped)
		{
			if (auto* Melee = GetOwner()->FindComponentByClass<UJTSMeleeComponent>()) Melee->StopAttack();
			if (auto* Ranged = GetOwner()->FindComponentByClass<UJTSRangedWeaponComponent>()) Ranged->CancelForClimb();
		}
		GetOwner()->ForceNetUpdate();
	}
	// The owner's PlayerState loadout and the character's cosmetic state may arrive on different frames.
	OnRep_Equipped();
	FJTSStellarWeaponBinding Current;
	if (!ResolveDefinition(Current) || Current.CoreInstanceId != ChannelBinding.CoreInstanceId
		|| Current.AttachmentInstanceId != ChannelBinding.AttachmentInstanceId)
	{
		CastGestureUntil = 0;
		StopChannels();
	}
}

void UJTSStellarWeaponComponent::RefreshEquipmentBinding()
{
	auto* Loadout = GetLoadout();
	if (!Loadout) return;
	if (Catalog) Loadout->ConfigureLootTable(Catalog->LootTable.LoadSynchronous());
	if (BoundLoadout.Get() == Loadout) { HandleLoadoutChanged(); return; }
	if (BoundLoadout.IsValid()) BoundLoadout->OnLoadoutChanged.RemoveDynamic(this, &ThisClass::HandleLoadoutChanged);
	BoundLoadout = Loadout;
	Loadout->OnLoadoutChanged.AddDynamic(this, &ThisClass::HandleLoadoutChanged);
	HandleLoadoutChanged();
}

void UJTSStellarWeaponComponent::OnRep_Equipped()
{
	if (auto* Visual = GetOwner()->FindComponentByClass<UJTSWeaponVisualComponent>()) Visual->RefreshWeaponVisual();
}

void UJTSStellarWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSStellarWeaponComponent, EquippedVisual);
	DOREPLIFETIME(UJTSStellarWeaponComponent, bReplicatedCasting);
}

void UJTSStellarWeaponComponent::MulticastEffect_Implementation(FName Core, FName Attachment,
	FVector_NetQuantize Start, FVector_NetQuantize End, float Radius, bool bPrimary, bool bSecondary,
	bool bFromMuzzle, bool bImpact)
{
	if (GetNetMode() == NM_DedicatedServer || !Catalog) return;
	const auto* Def = Catalog->Find(Core, Attachment);
	if (!Def || !Def->EffectClass) return;
	if (!IsValid(LocalEffect))
	{
		FActorSpawnParameters Params; Params.Owner = GetOwner();
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		LocalEffect = GetWorld()->SpawnActor<AJTSStellarEffectActor>(Def->EffectClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}
	FVector VisualStart = Start;
	if (bFromMuzzle && Def->Mode != EJTSStellarWeaponMode::BlackHole)
		if (const auto* Visual = GetOwner()->FindComponentByClass<UJTSWeaponVisualComponent>())
		{
			FVector Muzzle;
			if (Visual->GetMuzzleWorldLocation(Muzzle)) VisualStart = Muzzle;
		}
	if (IsValid(LocalEffect)) LocalEffect->UpdateEffect(Def->Mode, VisualStart, End, Radius, Def->Color,
		bPrimary, bSecondary, Def->RepulsionRadiusCentimeters, bFromMuzzle, bImpact);
}

void UJTSStellarWeaponComponent::MulticastStopEffect_Implementation()
{
	if (auto* Abilities = GetOwner()->FindComponentByClass<UJTSStellarAbilityComponent>()) Abilities->StopPresentation();
	if (IsValid(LocalEffect)) LocalEffect->Destroy();
	LocalEffect = nullptr;
}

void UJTSStellarWeaponComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PulseTimer);
		World->GetTimerManager().ClearTimer(HeartbeatTimer);
	}
	if (BoundLoadout.IsValid()) { BoundLoadout->SetChannelActive(false); BoundLoadout->OnLoadoutChanged.RemoveDynamic(this, &ThisClass::HandleLoadoutChanged); }
	RemoveForces();
	for (const auto& Field : BlackHoleFields) if (Field.IsValid()) Field->Destroy();
	BlackHoleFields.Reset();
	if (IsValid(LocalEffect)) LocalEffect->Destroy();
	Super::EndPlay(Reason);
}
