#include "space/Components/JTSStellarAbilityComponent.h"
#include "space/Components/JTSStellarWeaponComponent.h"
#include "space/Components/JTSStellarLoadoutComponent.h"
#include "space/Components/JTSStellarSupportComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Weapons/JTSStellarProjectile.h"
#include "space/Weapons/JTSStellarDrone.h"
#include "space/Weapons/JTSStellarAreaField.h"
#include "space/Weapons/JTSStellarEffectActor.h"
#include "space/Weapons/JTSStellarCombat.h"
#include "space/Components/JTSWeaponVisualComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

UJTSStellarAbilityComponent::UJTSStellarAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false; SetIsReplicatedByDefault(true);
}
void UJTSStellarAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority()) GetWorld()->GetTimerManager().SetTimer(MaintenanceTimer, this, &ThisClass::ValidatePersistentEffects, .25f, true);
}
bool UJTSStellarAbilityComponent::Pay(float Cost)
{
	const auto* Weapon = GetOwner()->FindComponentByClass<UJTSStellarWeaponComponent>();
	auto* Loadout = Weapon ? Weapon->GetLoadout() : nullptr;
	return GetOwner()->HasAuthority() && Loadout && FMath::IsFinite(Cost) && Cost > 0 && Loadout->ConsumeEnergy(Cost);
}
void UJTSStellarAbilityComponent::InputChanged(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, bool Primary, bool Secondary, FVector Direction)
{
	if (!GetOwner()->HasAuthority() || Binding.EffectiveLevels.Num() != 6) return;
	const auto& X = Binding.EffectiveLevels; const double Now = GetWorld()->GetTimeSeconds();
	const bool Press = Secondary && !bHeldSecondary, Release = !Secondary && bHeldSecondary;
	bHeldSecondary = Secondary;
	if (Def.Mode == EJTSStellarWeaponMode::Explosion || Def.Mode == EJTSStellarWeaponMode::Shaping)
	{
		if (Press && Now >= NextSecondary.FindRef(Def.Mode))
		{
			ChargeStart = Now; ChargingCore = Binding.CoreInstanceId; ChargingAttachment = Binding.AttachmentInstanceId;
			const auto* Weapon = GetOwner()->FindComponentByClass<UJTSStellarWeaponComponent>();
			const auto* Loadout = Weapon ? Weapon->GetLoadout() : nullptr;
			if (Def.Mode == EJTSStellarWeaponMode::Shaping && Now >= NextParry && Loadout && Loadout->GetEnergy() >= Def.SecondaryEnergy)
			{
				NextParry = Now + Def.SecondaryCooldown;
				if (auto* Support = GetOwner()->FindComponentByClass<UJTSStellarSupportComponent>()) Support->BeginParry(.12f + .012f * X[5], Direction);
			}
		}
		if (Release && ChargeStart >= 0 && ChargingCore == Binding.CoreInstanceId && ChargingAttachment == Binding.AttachmentInstanceId)
		{
			const float Required = Def.Mode == EJTSStellarWeaponMode::Shaping ? Def.ChargeSeconds - .04f * X[3] : Def.ChargeSeconds;
			if (Now - ChargeStart + .001 >= Required && Now >= NextSecondary.FindRef(Def.Mode) && Pay(Def.SecondaryEnergy))
			{
				NextSecondary.Add(Def.Mode, Now + (Def.Mode == EJTSStellarWeaponMode::Shaping ? Def.SecondaryCooldown - .3f * X[4] : Def.SecondaryCooldown * (1 - .03 * X[5])));
				if (Def.Mode == EJTSStellarWeaponMode::Explosion) LaunchExplosion(Def, Binding, Direction, true);
				else Slash(Def, Binding, Direction, true);
			}
			ChargeStart = -1;
		}
	}
	if (Press && (Def.Mode == EJTSStellarWeaponMode::Radiance || Def.Mode == EJTSStellarWeaponMode::Shadow)
		&& Now >= NextSecondary.FindRef(Def.Mode) && Pay(Def.SecondaryEnergy))
	{
		NextSecondary.Add(Def.Mode, Now + Def.SecondaryCooldown * (Def.Mode == EJTSStellarWeaponMode::Radiance ? 1 - .03 * X[4] : 1));
		GrantTeamShield(Def, Binding);
	}
	if (Press && Def.Mode == EJTSStellarWeaponMode::Instance)
	{
		for (auto& Robot : Robots) if (Robot.IsValid()) Robot->Detonate(); Robots.Reset();
	}
	if (Press && Def.Mode == EJTSStellarWeaponMode::Disassembly)
	{
		bEngineering = !bEngineering; LockTarget.Reset(); LockSeconds = 0; LockProgress = 0;
	}
}
bool UJTSStellarAbilityComponent::ExecutePulse(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, bool Primary, bool Secondary, FVector Direction, float Delta)
{
	if (!GetOwner()->HasAuthority() || Binding.EffectiveLevels.Num() != 6) return false;
	// UE may catch up a repeating timer twice within one server frame. No time elapsed
	// on the second callback: it must neither charge energy nor cancel the channel.
	if (Delta <= KINDA_SMALL_NUMBER) return true;
	const auto& X = Binding.EffectiveLevels; const double Now = GetWorld()->GetTimeSeconds();
	float Cost = 0;
	switch (Def.Mode)
	{
	case EJTSStellarWeaponMode::Healing: if (Primary || Secondary) Cost = Def.EnergyPerSecond * (1 - .03 * X[5]); break;
	case EJTSStellarWeaponMode::Freezing: Cost = ((Primary ? Def.EnergyPerSecond : 0) + (Secondary ? Def.SecondaryEnergy : 0)) * (1 - .03 * X[5]); break;
	case EJTSStellarWeaponMode::Radiance: case EJTSStellarWeaponMode::Shadow: if (Primary) Cost = Def.EnergyPerSecond * (1 - .03 * X[5]); break;
	default: break;
	}
	if (Cost > 0 && !Pay(Cost * Delta)) return false;
	AreaSeconds += Delta;
	const bool AreaDue = AreaSeconds >= .2f;
	if (Def.Mode == EJTSStellarWeaponMode::Healing && AreaSeconds >= .2f)
		Heal(Def, Binding, Direction, Secondary, AreaSeconds);
	if ((Def.Mode == EJTSStellarWeaponMode::Radiance || Def.Mode == EJTSStellarWeaponMode::Shadow || Def.Mode == EJTSStellarWeaponMode::Freezing) && AreaSeconds >= .2f)
		AreaChannel(Def, Binding, Primary, Secondary, AreaSeconds);
	if (AreaSeconds >= .2f) AreaSeconds = 0;
	const bool bAttack = Primary && (!Secondary || (Def.Mode != EJTSStellarWeaponMode::Explosion && Def.Mode != EJTSStellarWeaponMode::Shaping));
	if (bAttack && Now >= NextPrimary.FindRef(Def.Mode))
	{
		switch (Def.Mode)
		{
		case EJTSStellarWeaponMode::Explosion: case EJTSStellarWeaponMode::Shaping:
			if (!Pay(Def.PrimaryEnergyPerCast)) return false;
			NextPrimary.Add(Def.Mode, Now + Def.PrimaryInterval);
			if (Def.Mode == EJTSStellarWeaponMode::Explosion) LaunchExplosion(Def, Binding, Direction, false);
			else Slash(Def, Binding, Direction, false);
			break;
		case EJTSStellarWeaponMode::Diffusion:
			if (!Pay(Def.EnergyPerSecond * Def.PrimaryInterval)) return false;
			NextPrimary.Add(Def.Mode, Now + Def.PrimaryInterval); RayAttack(Def, Binding, Direction); break;
		case EJTSStellarWeaponMode::Freezing:
			NextPrimary.Add(Def.Mode, Now + Def.PrimaryInterval); RayAttack(Def, Binding, Direction); break;
		case EJTSStellarWeaponMode::Instance:
			if (GetRobotCount() < FMath::Min(9, 4 + FMath::FloorToInt(X[0] / 2)))
			{
				if (!Pay(Def.PrimaryEnergyPerCast)) return false;
				NextPrimary.Add(Def.Mode, Now + Def.ChargeSeconds - .1f * X[5]); MakeRobot(Def, Binding, Direction);
			}
			break;
		default: break;
		}
	}
	if (Def.Mode == EJTSStellarWeaponMode::Disassembly && Primary) return Disassemble(Def, Binding, Direction, Delta);
	if (AreaDue && Secondary && ChargeStart >= 0 && (Def.Mode == EJTSStellarWeaponMode::Explosion || Def.Mode == EJTSStellarWeaponMode::Shaping))
	{
		auto* Pawn = Cast<APawn>(GetOwner());
		const bool Shaping = Def.Mode == EJTSStellarWeaponMode::Shaping;
		const FVector Point = Shaping ? Pawn->GetActorLocation() : JTSStellarCombat::AimPoint(Pawn, Direction, Def.RangeCentimeters);
		MulticastArea(Binding.CoreId, Binding.AttachmentId, Point, JTSStellarCombat::SurfaceUp(Pawn, Point), Shaping ? Def.RangeCentimeters + 12 * X[2] : Def.AreaRadiusCentimeters + 15 * X[1], false, false);
	}
	return true;
}
void UJTSStellarAbilityComponent::CancelCast()
{
	ChargeStart = -1; bHeldSecondary = false; AreaSeconds = 0; LockTarget.Reset(); LockSeconds = 0; LockProgress = 0;
}
int32 UJTSStellarAbilityComponent::GetRobotCount() const
{
	int32 Count = 0; for (const auto& Robot : Robots) if (Robot.IsValid() && !Robot->IsActorBeingDestroyed()) ++Count; return Count;
}
void UJTSStellarAbilityComponent::TrackArea(AJTSStellarAreaField* Area)
{
	Areas.RemoveAll([](const auto& A) { return !A.IsValid() || A->IsActorBeingDestroyed(); });
	while (Areas.Num() >= 2) { Areas[0]->Destroy(); Areas.RemoveAt(0); } Areas.Add(Area);
}
void UJTSStellarAbilityComponent::ValidatePersistentEffects()
{
	if (!GetOwner()->HasAuthority()) return;
	const auto* Weapon = GetOwner()->FindComponentByClass<UJTSStellarWeaponComponent>();
	const auto* Loadout = Weapon ? Weapon->GetLoadout() : nullptr;
	const auto* Health = GetOwner()->FindComponentByClass<UJTSHealthComponent>();
	const auto Weapons = Loadout ? Loadout->GetWeapons() : TArray<FJTSStellarWeaponBinding>();
	FJTSStellarWeaponBinding Active; const bool bActive = Loadout && Loadout->GetActiveWeapon(Active);
	const auto Valid = [&](FGuid Core, FGuid Attachment) { return (!Health || !Health->IsDead()) && Weapons.ContainsByPredicate([&](const auto& B) { return B.CoreInstanceId == Core && B.AttachmentInstanceId == Attachment; }); };
	for (auto& Robot : Robots) if (Robot.IsValid())
	{
		if (!Valid(Robot->GetCoreId(), Robot->GetAttachmentId())) Robot->Destroy();
		else if (!bActive || Active.CoreInstanceId != Robot->GetCoreId() || Active.AttachmentInstanceId != Robot->GetAttachmentId()) Robot->LimitRemainingLife(8);
	}
	for (auto& Area : Areas) if (Area.IsValid() && !Valid(Area->GetCoreId(), Area->GetAttachmentId())) Area->Destroy();
	for (auto& Orb : Projectiles) if (Orb.IsValid() && !Valid(Orb->GetCoreId(), Orb->GetAttachmentId())) Orb->LimitRemainingLife(.5f);
	Robots.RemoveAll([](const auto& R) { return !R.IsValid() || R->IsActorBeingDestroyed(); });
	Areas.RemoveAll([](const auto& A) { return !A.IsValid() || A->IsActorBeingDestroyed(); });
	Projectiles.RemoveAll([](const auto& A) { return !A.IsValid() || A->IsActorBeingDestroyed(); });
}
void UJTSStellarAbilityComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (IsValid(LocalChannelArea)) LocalChannelArea->Destroy();
	if (IsValid(LocalChannelLink)) LocalChannelLink->Destroy();
	GetWorld()->GetTimerManager().ClearTimer(MaintenanceTimer);
	for (auto& R : Robots) if (R.IsValid()) R->Destroy();
	for (auto& A : Areas) if (A.IsValid()) A->Destroy();
	for (auto& A : Projectiles) if (A.IsValid()) A->Destroy();
	Super::EndPlay(Reason);
}
void UJTSStellarAbilityComponent::StopPresentation()
{
	if (IsValid(LocalChannelArea)) LocalChannelArea->Destroy(); LocalChannelArea = nullptr;
	if (IsValid(LocalChannelLink)) LocalChannelLink->Destroy(); LocalChannelLink = nullptr;
}
void UJTSStellarAbilityComponent::RefundShieldAbsorption(FGuid CastId, float Absorbed)
{
	if (!GetOwner()->HasAuthority() || !ShieldRefunds.Contains(CastId)) return;
	float& Refunded = ShieldRefunds.FindChecked(CastId);
	const float Amount = FMath::Min(5.f - Refunded, Absorbed * .05f);
	if (auto* Weapon = GetOwner()->FindComponentByClass<UJTSStellarWeaponComponent>())
		if (auto* Loadout = Weapon->GetLoadout()) { Loadout->RefundEnergy(Amount); Refunded += Amount; }
}
void UJTSStellarAbilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME_CONDITION(UJTSStellarAbilityComponent, bEngineering, COND_OwnerOnly); DOREPLIFETIME_CONDITION(UJTSStellarAbilityComponent, LockProgress, COND_OwnerOnly);
}
