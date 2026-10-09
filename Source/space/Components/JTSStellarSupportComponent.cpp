#include "space/Components/JTSStellarSupportComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSStaminaComponent.h"
#include "space/Components/JTSStellarWeaponComponent.h"
#include "space/Components/JTSStellarLoadoutComponent.h"
#include "space/Components/JTSStellarAbilityComponent.h"
#include "space/Weapons/JTSStellarEffectActor.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UJTSStellarSupportComponent::UJTSStellarSupportComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}
float UJTSStellarSupportComponent::GetShield() const
{
	const auto* State = GetWorld()->GetGameState();
	const double Now = State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const auto* Health = GetOwner()->FindComponentByClass<UJTSHealthComponent>();
	if (!Health || Health->IsDead()) return 0;
	return FMath::Min(Health->GetMaxHealth() * .6f,
		(Now < LightEnd ? LightShield : 0) + (Now < DarkEnd ? DarkShield : 0) + (Now < WaterEnd ? WaterShield : 0));
}
void UJTSStellarSupportComponent::HealPulse(float HF, float SF, float Overflow, float Emergency, float Delta, TSubclassOf<AJTSStellarEffectActor> EffectClass)
{
	auto* Health = GetOwner()->FindComponentByClass<UJTSHealthComponent>();
	if (!GetOwner()->HasAuthority() || !Health || Health->IsDead()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= HealWindowStart + 1) { HealWindowStart = Now; HealedInWindow = 0; }
	float Requested = Health->GetMaxHealth() * HF * Delta;
	if (Health->GetHealthNormalized() < .3f && Now >= EmergencyAfter)
	{
		Requested += Health->GetMaxHealth() * Emergency;
		EmergencyAfter = Now + 20;
	}
	Requested = FMath::Min(Requested, FMath::Max(0.f, Health->GetMaxHealth() * .2f - HealedInWindow));
	const float Restored = Health->Heal(Requested);
	HealedInWindow += Requested;
	if (Overflow > 0 && Requested > Restored)
	{
		if (Now >= WaterEnd) WaterShield = 0;
		const float Room = FMath::Max(0.f, Health->GetMaxHealth() * .6f - (Now < LightEnd ? LightShield : 0) - (Now < DarkEnd ? DarkShield : 0));
		WaterShield = FMath::Min3(Room, Health->GetMaxHealth() * FMath::Min(.2f, Overflow), WaterShield + Requested - Restored);
		WaterEnd = Now + 3;
		if (EffectClass) ShieldEffectClass = EffectClass;
		UpdatePresentation();
	}
	if (auto* Stamina = GetOwner()->FindComponentByClass<UJTSStaminaComponent>()) Stamina->Restore(Stamina->GetMaxStamina() * SF * Delta);
}
void UJTSStellarSupportComponent::GrantShield(bool bDark, float Fraction, float Duration, APawn* Source, FGuid CastId, TSubclassOf<AJTSStellarEffectActor> EffectClass)
{
	const auto* Health = GetOwner()->FindComponentByClass<UJTSHealthComponent>();
	if (!GetOwner()->HasAuthority() || !Health || Health->IsDead()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	float& Shield = bDark ? DarkShield : LightShield;
	double& End = bDark ? DarkEnd : LightEnd;
	Shield = FMath::Max(Now < End ? Shield : 0, Health->GetMaxHealth() * FMath::Clamp(Fraction, 0.f, .4f));
	const float Other = (bDark ? Now < LightEnd ? LightShield : 0 : Now < DarkEnd ? DarkShield : 0) + (Now < WaterEnd ? WaterShield : 0);
	Shield = FMath::Min(Shield, FMath::Max(0.f, Health->GetMaxHealth() * .6f - Other));
	End = Now + Duration;
	if (bDark) { DarkCaster = Source; DarkRefund = 0; DarkCastId = CastId; }
	if (EffectClass) ShieldEffectClass = EffectClass;
	UpdatePresentation();
}
void UJTSStellarSupportComponent::BeginParry(float Duration, FVector Facing)
{
	if (!GetOwner()->HasAuthority()) return;
	ParryEnd = GetWorld()->GetTimeSeconds() + Duration;
	ParryFacing = Facing.GetSafeNormal();
}
float UJTSStellarSupportComponent::AbsorbDamage(float Damage, AActor* Causer)
{
	if (!GetOwner()->HasAuthority()) return Damage;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < ParryEnd && IsValid(Causer) && FVector::DotProduct(ParryFacing,
		(Causer->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal()) > .25f)
	{
		ParryEnd = 0; // One actual incoming attack consumes the window.
		return 0;
	}
	float Budget = GetShield();
	for (int32 I = 0; I < 3; ++I)
	{
		float& Shield = I == 0 ? LightShield : I == 1 ? DarkShield : WaterShield;
		const double End = I == 0 ? LightEnd : I == 1 ? DarkEnd : WaterEnd;
		if (Now >= End) { Shield = 0; continue; }
		const float Used = FMath::Min3(Shield, Damage, Budget);
		Shield -= Used; Damage -= Used; Budget -= Used;
		if (I == 1 && Used > 0 && DarkCaster.IsValid() && DarkCastId.IsValid())
		{
			if (auto* Ability = DarkCaster->FindComponentByClass<UJTSStellarAbilityComponent>()) Ability->RefundShieldAbsorption(DarkCastId, Used);
		}
		else if (I == 1 && Used > 0 && DarkCaster.IsValid())
			if (auto* Weapon = DarkCaster->FindComponentByClass<UJTSStellarWeaponComponent>())
				if (auto* Loadout = Weapon->GetLoadout())
				{
					const float Refund = FMath::Min(5.f - DarkRefund, Used * .05f);
					Loadout->RefundEnergy(Refund); DarkRefund += Refund;
				}
	}
	return Damage;
}
void UJTSStellarSupportComponent::UpdatePresentation()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (!ShieldEffectClass || GetShield() <= 0)
	{
		if (IsValid(LocalShield)) LocalShield->Destroy(); LocalShield = nullptr;
		GetWorld()->GetTimerManager().ClearTimer(PresentationTimer); return;
	}
	if (!IsValid(LocalShield))
	{
		FActorSpawnParameters P; P.Owner = GetOwner();
		LocalShield = GetWorld()->SpawnActor<AJTSStellarEffectActor>(ShieldEffectClass, GetOwner()->GetActorTransform(), P);
	}
	if (LocalShield) LocalShield->UpdateOwnerShield(110, WaterShield > LightShield && WaterShield > DarkShield ? FLinearColor(.1f,.85f,.65f) : DarkShield > LightShield ? FLinearColor(.3f,.08f,.6f) : FLinearColor(1,.8f,.3f));
	if (!GetWorld()->GetTimerManager().IsTimerActive(PresentationTimer)) GetWorld()->GetTimerManager().SetTimer(PresentationTimer, this, &ThisClass::UpdatePresentation, .2f, true);
}
void UJTSStellarSupportComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(PresentationTimer);
	if (IsValid(LocalShield)) LocalShield->Destroy();
	Super::EndPlay(Reason);
}
void UJTSStellarSupportComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSStellarSupportComponent, LightShield);
	DOREPLIFETIME(UJTSStellarSupportComponent, DarkShield);
	DOREPLIFETIME(UJTSStellarSupportComponent, WaterShield);
	DOREPLIFETIME(UJTSStellarSupportComponent, LightEnd);
	DOREPLIFETIME(UJTSStellarSupportComponent, DarkEnd);
	DOREPLIFETIME(UJTSStellarSupportComponent, WaterEnd);
	DOREPLIFETIME(UJTSStellarSupportComponent, ShieldEffectClass);
}
