#include "space/Components/JTSStellarAbilityComponent.h"
#include "space/Components/JTSStellarWeaponComponent.h"
#include "space/Components/JTSStellarLoadoutComponent.h"
#include "space/Components/JTSStellarTargetComponent.h"
#include "space/Components/JTSStellarSupportComponent.h"
#include "space/Components/JTSWeaponVisualComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Interaction/JTSDisassemblyTarget.h"
#include "space/Weapons/JTSStellarCombat.h"
#include "space/Weapons/JTSStellarProjectile.h"
#include "space/Weapons/JTSStellarDrone.h"
#include "space/Weapons/JTSStellarEffectActor.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

void UJTSStellarAbilityComponent::ForEachAlly(FVector Center, float Radius, TFunctionRef<void(APawn*)> Apply)
{
	APawn* Caster = Cast<APawn>(GetOwner());
	TSet<APawn*> Visited;
	const auto Visit = [&](APawn* Pawn)
	{
		const auto* Health = IsValid(Pawn) ? Pawn->FindComponentByClass<UJTSHealthComponent>() : nullptr;
		if (!Pawn || Visited.Contains(Pawn) || !Health || Health->IsDead() || FVector::DistSquared(Pawn->GetActorLocation(), Center) > FMath::Square(Radius)) return;
		Visited.Add(Pawn);
		if (Pawn == Caster || UJTSStellarTargetComponent::HasLineOfSight(GetWorld(), Caster->GetPawnViewLocation(), Pawn, Caster)) Apply(Pawn);
	};
	Visit(Caster);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) if (It->Get()) Visit(It->Get()->GetPawn());
}
void UJTSStellarAbilityComponent::GrantTeamShield(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding)
{
	const auto& X = Binding.EffectiveLevels; const bool Dark = Def.Mode == EJTSStellarWeaponMode::Shadow;
	const FGuid CastId = FGuid::NewGuid();
	if (ShieldRefunds.Num() >= 8) ShieldRefunds.Reset(); ShieldRefunds.Add(CastId, 0);
	ForEachAlly(GetOwner()->GetActorLocation(), 1000, [&](APawn* Pawn)
	{
		if (auto* Support = Pawn->FindComponentByClass<UJTSStellarSupportComponent>())
		{
			Support->GrantShield(Dark, (Dark ? .15f + .02f * X[4] : .2f + .02f * X[3]), 5, Cast<APawn>(GetOwner()), CastId, Def.EffectClass);
		}
	});
}
void UJTSStellarAbilityComponent::Heal(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector Direction, bool Self, float Delta)
{
	APawn* Caster = Cast<APawn>(GetOwner()); const auto& X = Binding.EffectiveLevels;
	const FVector Center = Self ? Caster->GetActorLocation() : JTSStellarCombat::AimPoint(Caster, Direction, Def.RangeCentimeters);
	const auto Apply = [&](APawn* Pawn)
	{
		if (auto* Support = Pawn->FindComponentByClass<UJTSStellarSupportComponent>())
		{
			Support->HealPulse(.06f + .0025f * X[0], .1f + .01f * X[1], .02f * X[3], .15f + .01f * X[4], Delta, Def.EffectClass);
			MulticastLink(Binding.CoreId, Binding.AttachmentId, Caster->GetPawnViewLocation(), Pawn->GetActorLocation(), 8, true, true);
		}
	};
	if (Self) Apply(Caster); else ForEachAlly(Center, Def.AreaRadiusCentimeters + 20 * X[2], Apply);
	const FVector Up = JTSStellarCombat::SurfaceUp(Caster, Center);
	FVector FieldCenter = Center;
	if (Self) if (const auto* Character = Cast<ACharacter>(Caster))
		FieldCenter -= Up * (Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 5.f);
	MulticastArea(Binding.CoreId, Binding.AttachmentId, FieldCenter, Up, Def.AreaRadiusCentimeters + 20 * X[2], false, false);
}
void UJTSStellarAbilityComponent::AreaChannel(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, bool Primary, bool Secondary, float Delta)
{
	const auto& X = Binding.EffectiveLevels; auto* Pawn = Cast<ACharacter>(GetOwner());
	if (!Pawn || (Def.Mode == EJTSStellarWeaponMode::Freezing ? !Secondary : !Primary)) return;
	const FVector Up = JTSStellarCombat::SurfaceUp(Pawn, Pawn->GetActorLocation());
	const FVector Ground = Pawn->GetActorLocation() - Up * Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const bool Shadow = Def.Mode == EJTSStellarWeaponMode::Shadow, Ice = Def.Mode == EJTSStellarWeaponMode::Freezing;
	const float Radius = Def.AreaRadiusCentimeters + (Ice ? 25 * X[3] : Shadow ? 30 * X[1] : 20 * X[1]);
	TArray<AActor*> Targets; UJTSStellarTargetComponent::QueryTargets(GetWorld(), Ground + Up * 80, Radius, Def.MaximumTargets, Targets);
	for (auto* Target : Targets)
	{
		if (Shadow && FMath::Abs(FVector::DotProduct(Target->GetActorLocation() - Ground, Up)) > Def.NearGroundHeight) continue;
		if (!UJTSStellarTargetComponent::HasLineOfSight(GetWorld(), Pawn->GetPawnViewLocation(), Target, Pawn)) continue;
		auto* Status = Target->FindComponentByClass<UJTSStellarTargetComponent>();
		Status->SetStatusPresentation(Def.TargetStatusEffectClass);
		if (Shadow)
		{
			Status->ApplyCorrosion(Pawn, Def.StatusDamagePerSecond * (1 + .05 * X[0]), .4f, .1f + .02f * X[3]);
			Status->ApplySlow(.2f + .03f * X[2], .4f);
		}
		else
		{
			UGameplayStatics::ApplyDamage(Target, Def.BaseDamage * (1 + (Ice ? .04 : .05) * X[0]) * Delta, Pawn->GetController(), Pawn, UDamageType::StaticClass());
			if (Ice) Status->ApplySlow(.3f, .4f); else Status->ApplyRoot(.6f + .06f * X[2]);
		}
	}
	MulticastArea(Binding.CoreId, Binding.AttachmentId, Ground + Up * 5, Up, Radius, false, false);
}
void UJTSStellarAbilityComponent::Slash(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector Direction, bool Circular)
{
	APawn* Pawn = Cast<APawn>(GetOwner()); const auto& X = Binding.EffectiveLevels;
	const FVector Up = JTSStellarCombat::SurfaceUp(Pawn, Pawn->GetActorLocation());
	const FVector Facing = FVector::VectorPlaneProject(Direction, Up).GetSafeNormal();
	const float Radius = Def.RangeCentimeters + 12 * X[2];
	const float Cos = FMath::Cos(FMath::DegreesToRadians((110 + 6 * X[1]) * .5));
	TArray<AActor*> Targets; UJTSStellarTargetComponent::QueryTargets(GetWorld(), Pawn->GetActorLocation(), Radius, Def.MaximumTargets, Targets);
	for (auto* Target : Targets) if ((Circular || FVector::DotProduct(Facing, FVector::VectorPlaneProject(Target->GetActorLocation() - Pawn->GetActorLocation(), Up).GetSafeNormal()) >= Cos)
		&& UJTSStellarTargetComponent::HasLineOfSight(GetWorld(), Pawn->GetPawnViewLocation(), Target, Pawn))
		UGameplayStatics::ApplyDamage(Target, Def.BaseDamage * (1 + .05 * X[0]) * (Circular ? 2.5f : 1.f), Pawn->GetController(), Pawn, UDamageType::StaticClass());
	MulticastArea(Binding.CoreId, Binding.AttachmentId, Pawn->GetActorLocation(), Up, Radius, Circular, true, Circular ? 360 : 110 + 6 * X[1]);
}
void UJTSStellarAbilityComponent::LaunchExplosion(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector Direction, bool Charged)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	const FVector Destination = JTSStellarCombat::AimPoint(Pawn, Direction, Def.RangeCentimeters);
	const FTransform Transform(FQuat::Identity, JTSStellarCombat::CastOrigin(Pawn));
	if (auto* Orb = GetWorld()->SpawnActorDeferred<AJTSStellarProjectile>(AJTSStellarProjectile::StaticClass(), Transform, Pawn, Pawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Orb->Initialize(Def, Binding, Destination, Charged); Orb->FinishSpawning(Transform); Projectiles.Add(Orb);
	}
}
void UJTSStellarAbilityComponent::RayAttack(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector Direction)
{
	APawn* Pawn = Cast<APawn>(GetOwner()); const auto& X = Binding.EffectiveLevels;
	const bool Ice = Def.Mode == EJTSStellarWeaponMode::Freezing;
	FVector Start = Pawn->GetPawnViewLocation(); const FVector Origin = Start, End = Start + Direction * Def.RangeCentimeters;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StellarAbilityRay), false, Pawn); TSet<AActor*> Visited;
	const int32 Limit = Ice ? 8 : 10;
	const int32 Pierce = FMath::Min(Limit, Def.BasePierceTargets + FMath::FloorToInt(X[2] / 2));
	const float BaseDamage = Def.BaseDamage * (1 + .04 * X[0]);
	FVector LastPoint = End; bool Impact = false;
	const auto Apply = [&](AActor* Target, float Scale)
	{
		Visited.Add(Target); auto* Status = Target->FindComponentByClass<UJTSStellarTargetComponent>();
		Status->SetStatusPresentation(Def.TargetStatusEffectClass);
		if (Ice) Status->ApplyCold(Pawn, 50 * Def.PrimaryInterval * (1 + .1 * X[1]) * Scale, 300 + 15 * X[4], BaseDamage * .5f);
		else Status->ApplyCorrosion(Pawn, Def.StatusDamagePerSecond * (1 + .06 * X[1]) * Scale, 3, .1f + .02f * X[4], X[5] > 0 ? 150 + 15 * X[5] : 0);
		UGameplayStatics::ApplyDamage(Target, BaseDamage * Scale, Pawn->GetController(), Pawn, UDamageType::StaticClass());
	};
	for (int32 I = 0; I < Pierce && Visited.Num() < Limit; ++I)
	{
		FHitResult Hit;
		if (!GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(FMath::Max(1.f, Def.BeamRadiusCentimeters)), Params)) break;
		LastPoint = Hit.ImpactPoint; Impact = true; auto* Target = Hit.GetActor();
		const auto* Status = IsValid(Target) ? Target->FindComponentByClass<UJTSStellarTargetComponent>() : nullptr;
		if (!Status || !Status->IsAliveTarget()) break;
		Apply(Target, FMath::Pow(.8f, I)); Params.AddIgnoredActor(Target); Start = Hit.ImpactPoint + Direction * 2;
		if (!Ice && X[3] >= 3)
		{
			TArray<AActor*> Neighbors; UJTSStellarTargetComponent::QueryTargets(GetWorld(), Hit.ImpactPoint, Def.ChainRadiusCentimeters, 24, Neighbors);
			int32 Branches = FMath::FloorToInt(X[3] / 3);
			for (auto* Next : Neighbors) if (Branches > 0 && Visited.Num() < Limit && !Visited.Contains(Next) && UJTSStellarTargetComponent::HasLineOfSight(GetWorld(), Hit.ImpactPoint, Next, Pawn))
			{
				Apply(Next, .3f); Params.AddIgnoredActor(Next); --Branches;
				MulticastLink(Binding.CoreId, Binding.AttachmentId, Hit.ImpactPoint, Next->GetActorLocation(), 5, false, true);
			}
		}
	}
	MulticastLink(Binding.CoreId, Binding.AttachmentId, Origin, LastPoint, FMath::Max(4.f, Def.BeamRadiusCentimeters), true, Impact);
}
void UJTSStellarAbilityComponent::MakeRobot(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector Direction)
{
	APawn* Pawn = Cast<APawn>(GetOwner()); const FVector Up = JTSStellarCombat::SurfaceUp(Pawn, Pawn->GetActorLocation());
	const FVector Source = JTSStellarCombat::CastOrigin(Pawn);
	FVector Location = Source + FVector::VectorPlaneProject(Direction, Up).GetSafeNormal() * 180;
	FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(StellarRobotBirth), false, Pawn);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Source, Location, FCollisionObjectQueryParams(ECC_WorldStatic), Params)) Location = Source;
	const FTransform Transform(FQuat::Identity, Location);
	if (auto* Drone = GetWorld()->SpawnActorDeferred<AJTSStellarDrone>(AJTSStellarDrone::StaticClass(), Transform, Pawn, Pawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Drone->Initialize(Def, Binding); Drone->FinishSpawning(Transform); Robots.Add(Drone);
		MulticastLink(Binding.CoreId, Binding.AttachmentId, Source, Location, 12, true, true);
	}
}
bool UJTSStellarAbilityComponent::Disassemble(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector Direction, float Delta)
{
	APawn* Pawn = Cast<APawn>(GetOwner()); const auto& X = Binding.EffectiveLevels; FHitResult Hit;
	const FVector Point = JTSStellarCombat::AimPoint(Pawn, Direction, Def.RangeCentimeters, &Hit);
	AActor* Target = Hit.GetActor(); auto* Status = IsValid(Target) ? Target->FindComponentByClass<UJTSStellarTargetComponent>() : nullptr;
	const bool Eligible = bEngineering ? IsValid(Target) && Target->Implements<UJTSDisassemblyTarget>() && IJTSDisassemblyTarget::Execute_CanDisassemble(Target, Pawn) : Status && Status->IsAliveTarget();
	if (!Eligible) { LockTarget.Reset(); LockSeconds = 0; LockProgress = 0; return true; }
	if (LockTarget.Get() != Target) { LockTarget = Target; LockSeconds = 0; }
	LockSeconds += Delta;
	const float Required = bEngineering ? 1.5f - .07f * X[4] : Def.ChargeSeconds - .035f * X[1];
	LockProgress = FMath::Clamp(LockSeconds / Required, 0.f, 1.f);
	MulticastLink(Binding.CoreId, Binding.AttachmentId, Pawn->GetPawnViewLocation(), Point, 4 + LockProgress * 12, true, true);
	if (LockProgress < 1) return true;
	if (!Pay(Def.PrimaryEnergyPerCast)) return false;
	LockSeconds = 0; LockProgress = 0;
	if (bEngineering)
	{
		if (!IJTSDisassemblyTarget::Execute_Disassemble(Target, Pawn))
			GetOwner()->FindComponentByClass<UJTSStellarWeaponComponent>()->GetLoadout()->RefundEnergy(Def.PrimaryEnergyPerCast);
		return true;
	}
	const float Budget = Def.BaseDamage * (1 + .05 * X[0]);
	TSet<AActor*> Visited; Visited.Add(Target);
	auto* TargetHealth = Target->FindComponentByClass<UJTSHealthComponent>();
	const bool Execute = TargetHealth && TargetHealth->GetHealth() <= Budget && (Status->GetTier() == EJTSStellarTargetTier::Normal
		|| (Status->GetTier() == EJTSStellarTargetTier::Elite && TargetHealth->GetHealthNormalized() <= .15f));
	if (Execute) TargetHealth->ApplyDamage(TargetHealth->GetHealth(), Pawn->GetController(), Pawn, true);
	else UGameplayStatics::ApplyDamage(Target, Budget, Pawn->GetController(), Pawn, UDamageType::StaticClass());
	const auto* Health = IsValid(Target) ? Target->FindComponentByClass<UJTSHealthComponent>() : nullptr;
	const bool Killed = !Health || Health->IsDead();
	if (Killed)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now >= RefundWindow + 1) { RefundWindow = Now; RefundInWindow = 0; }
		const float Refund = FMath::Min(5.f - RefundInWindow, static_cast<float>(1 + .4 * X[5]));
		GetOwner()->FindComponentByClass<UJTSStellarWeaponComponent>()->GetLoadout()->RefundEnergy(Refund); RefundInWindow += Refund;
		if (X[3] > 0) JTSStellarCombat::DamageArea(Pawn, Point, 150 + 15 * X[3], Budget * .2f, 32, &Visited);
	}
	TArray<AActor*> Candidates; UJTSStellarTargetComponent::QueryTargets(GetWorld(), Point, Def.ChainRadiusCentimeters, 24, Candidates);
	int32 Branches = FMath::FloorToInt(X[2] / 3);
	for (auto* Next : Candidates) if (Branches > 0 && !Visited.Contains(Next) && UJTSStellarTargetComponent::HasLineOfSight(GetWorld(), Point, Next, Pawn))
	{
		UGameplayStatics::ApplyDamage(Next, Budget * .4f, Pawn->GetController(), Pawn, UDamageType::StaticClass()); Visited.Add(Next); --Branches;
		MulticastLink(Binding.CoreId, Binding.AttachmentId, Point, Next->GetActorLocation(), 10, false, true);
	}
	MulticastArea(Binding.CoreId, Binding.AttachmentId, Point, JTSStellarCombat::SurfaceUp(Pawn, Point), 90, false, true);
	return true;
}
void UJTSStellarAbilityComponent::MulticastArea_Implementation(FName Core, FName Attachment, FVector_NetQuantize Center, FVector_NetQuantizeNormal Up, float Radius, bool Shield, bool Burst, float SweepAngle)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	const auto* Weapon = GetOwner()->FindComponentByClass<UJTSStellarWeaponComponent>(); const auto* Catalog = Weapon ? Weapon->WeaponCatalog.Get() : nullptr;
	const auto* Def = Catalog ? Catalog->Find(Core, Attachment) : nullptr;
	if (!Def || !Def->EffectClass) return;
	FActorSpawnParameters P; P.Owner = GetOwner();
	AJTSStellarEffectActor* FX = nullptr;
	if (!Burst)
	{
		if (IsValid(LocalChannelArea) && LocalChannelArea->GetClass() != Def->EffectClass) { LocalChannelArea->Destroy(); LocalChannelArea = nullptr; }
		if (!IsValid(LocalChannelArea)) LocalChannelArea = GetWorld()->SpawnActor<AJTSStellarEffectActor>(Def->EffectClass, FVector(Center), FRotator::ZeroRotator, P);
		FX = LocalChannelArea;
	}
	else FX = GetWorld()->SpawnActor<AJTSStellarEffectActor>(Def->EffectClass, FVector(Center), FRotator::ZeroRotator, P);
	if (FX)
	{
		FX->UpdateArea(Def->Mode, Center, Up, Radius, Def->Color, false, Shield, Burst, SweepAngle);
	}
}
void UJTSStellarAbilityComponent::MulticastLink_Implementation(FName Core, FName Attachment, FVector_NetQuantize Start, FVector_NetQuantize End, float Width, bool FromCore, bool Impact)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	const auto* Weapon = GetOwner()->FindComponentByClass<UJTSStellarWeaponComponent>(); const auto* Catalog = Weapon ? Weapon->WeaponCatalog.Get() : nullptr;
	const auto* Def = Catalog ? Catalog->Find(Core, Attachment) : nullptr;
	if (!Def || !Def->EffectClass) return;
	FVector Source = Start;
	if (FromCore) if (const auto* Visual = GetOwner()->FindComponentByClass<UJTSWeaponVisualComponent>()) Visual->GetMuzzleWorldLocation(Source);
	FActorSpawnParameters P; P.Owner = FromCore ? GetOwner() : nullptr;
	AJTSStellarEffectActor* FX = nullptr;
	if (FromCore && Def->Mode != EJTSStellarWeaponMode::Healing)
	{
		if (IsValid(LocalChannelLink) && LocalChannelLink->GetClass() != Def->EffectClass) { LocalChannelLink->Destroy(); LocalChannelLink = nullptr; }
		if (!IsValid(LocalChannelLink)) LocalChannelLink = GetWorld()->SpawnActor<AJTSStellarEffectActor>(Def->EffectClass, Source, FRotator::ZeroRotator, P);
		FX = LocalChannelLink;
	}
	else FX = GetWorld()->SpawnActor<AJTSStellarEffectActor>(Def->EffectClass, Source, FRotator::ZeroRotator, P);
	if (FX)
		FX->UpdateEffect(Def->Mode, Source, End, Width, Def->Color, true, false, 0, FromCore, Impact);
}
