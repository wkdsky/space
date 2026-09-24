// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Animation/JTSAnimInstance.h"

#include "GameFramework/Controller.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSRangedWeaponComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Player/JTSCharacter.h"

float UJTSAnimInstance::GetAimPitch() const
{
	return AimPitch;
}

float UJTSAnimInstance::GetAimYaw() const
{
	return AimYaw;
}

bool UJTSAnimInstance::IsWeaponAiming() const
{
	return bWeaponAiming;
}

bool UJTSAnimInstance::HasRangedWeapon() const
{
	return bActiveRangedWeapon;
}

bool UJTSAnimInstance::HasHeldItem() const
{
	return bHasHeldItem;
}

void UJTSAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const AJTSCharacter* const Character = Cast<AJTSCharacter>(TryGetPawnOwner());
	AimYaw = 0.0f;
	bWeaponAiming = false;
	bHasRangedWeapon = false;
	bHasHeldItem = false;
	bActiveRangedWeapon = false;
	if (IsValid(Character))
	{
		if (const UJTSInventoryComponent* const Inventory = Character->FindComponentByClass<UJTSInventoryComponent>())
		{
			const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, Inventory->GetActiveItemId());
			bHasHeldItem = IsValid(Definition) && Definition->IsHoldable() && !Inventory->GetActiveItem().IsEmpty();
		}
		if (const UJTSRangedWeaponComponent* const Ranged = Character->FindComponentByClass<UJTSRangedWeaponComponent>())
		{
			bWeaponAiming = Ranged->IsAiming();
			bActiveRangedWeapon = Ranged->HasActiveRangedWeapon();
			// Only the head follows camera pitch while aiming. Ease it back to forward on release.
			const float HeadPitchTarget = bWeaponAiming && bActiveRangedWeapon
				? FMath::Clamp(Character->GetAimPitch(), -55.0f, 55.0f) : 0.0f;
			AimPitch = FMath::FInterpTo(AimPitch, HeadPitchTarget, DeltaSeconds, 14.0f);
		}
		if (const AController* const CharacterController = Character->GetController())
		{
			const FRotator RelativeAim = (CharacterController->GetControlRotation() - Character->GetActorRotation()).GetNormalized();
			AimYaw = FRotator::NormalizeAxis(RelativeAim.Yaw);
		}
	}
	bHasRangedWeapon = bActiveRangedWeapon;
}
