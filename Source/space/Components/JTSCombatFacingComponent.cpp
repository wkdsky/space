#include "JTSCombatFacingComponent.h"
#include "JTSHealthComponent.h"
#include "JTSMeleeComponent.h"
#include "JTSRangedWeaponComponent.h"
#include "JTSStellarWeaponComponent.h"
#include "JTSWallClimbComponent.h"
#include "space/Player/JTSCharacter.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UJTSCombatFacingComponent::UJTSCombatFacingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UJTSCombatFacingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSCombatFacingComponent, SwingForward);
}

bool UJTSCombatFacingComponent::CanFaceForCombat() const
{
	const auto* Character = Cast<AJTSCharacter>(GetOwner());
	if (!Character || Character->IsBoarded()) return false;
	const auto* Health = Character->GetHealthComponent();
	const auto* Climb = Character->FindComponentByClass<UJTSWallClimbComponent>();
	return (!Health || !Health->IsDead()) && (!Climb || !Climb->IsClimbing());
}

bool UJTSCombatFacingComponent::GetViewForward(FVector& Forward) const
{
	FVector Origin, Direction;
	const auto* View = GetOwner()->FindComponentByClass<UJTSRangedWeaponComponent>();
	if (!View || !View->GetCameraRay(Origin, Direction)) return false;
	Forward = FVector::VectorPlaneProject(Direction, GetOwner()->GetActorUpVector()).GetSafeNormal();
	// Looking straight down/up must not invent a world-Z heading or reverse the feet.
	if (Forward.IsNearlyZero()) Forward = GetOwner()->GetActorForwardVector();
	return true;
}

bool UJTSCombatFacingComponent::UsesViewFacing() const
{
	if (!CanFaceForCombat()) return false;
	const auto* Character = Cast<AJTSCharacter>(GetOwner());
	const auto* Gun = GetOwner()->FindComponentByClass<UJTSRangedWeaponComponent>();
	const auto* Stellar = GetOwner()->FindComponentByClass<UJTSStellarWeaponComponent>();
	return Character->IsFirstPersonView()
		|| (Gun && (Gun->IsAiming() || (Gun->HasActiveRangedWeapon() && Gun->IsFireHeld())))
		|| (Stellar && Stellar->HasActiveWeapon() && (Stellar->IsAiming() || Stellar->IsCasting()));
}

bool UJTSCombatFacingComponent::GetDesiredFacing(FVector& Forward) const
{
	if (!CanFaceForCombat()) return false;
	const auto* Melee = GetOwner()->FindComponentByClass<UJTSMeleeComponent>();
	// A swing owns its heading until recovery. Backward/sideways movement cannot retarget it.
	if ((Melee && Melee->IsAttacking())
		|| (Cast<APawn>(GetOwner())->IsLocallyControlled() && GetWorld()->GetTimeSeconds() < PredictedSwingUntil))
	{
		Forward = FVector::VectorPlaneProject(SwingForward, GetOwner()->GetActorUpVector()).GetSafeNormal();
		return !Forward.IsNearlyZero();
	}
	return UsesViewFacing() && GetViewForward(Forward);
}

void UJTSCombatFacingComponent::PrepareAttack(bool bMelee)
{
	if (!CanFaceForCombat()) return;
	FVector Forward;
	if (!GetViewForward(Forward)) return;
	if (bMelee)
	{
		const auto* Melee = GetOwner()->FindComponentByClass<UJTSMeleeComponent>();
		if (Melee && Melee->IsAttacking()) return; // Buffering input cannot bend the active swing.
		if (!GetOwner()->HasAuthority()) PredictedSwingUntil = GetWorld()->GetTimeSeconds() + 0.25;
		BeginMeleeSwing(Forward);
	}
	else CastChecked<AJTSCharacter>(GetOwner())->AlignBodyForAction(Forward);
}

void UJTSCombatFacingComponent::BeginMeleeSwing(const FVector& Forward)
{
	if (!CanFaceForCombat()) return;
	const FVector Heading = FVector::VectorPlaneProject(Forward, GetOwner()->GetActorUpVector()).GetSafeNormal();
	if (Heading.IsNearlyZero()) return;
	SwingForward = Heading;
	CastChecked<AJTSCharacter>(GetOwner())->AlignBodyForAction(Heading);
}
