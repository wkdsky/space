// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Animation/JTSAnimInstance.h"

#include "space/Player/JTSCharacter.h"

float UJTSAnimInstance::GetAimPitch() const
{
	return AimPitch;
}

void UJTSAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const AJTSCharacter* const Character = Cast<AJTSCharacter>(TryGetPawnOwner());
	AimPitch = IsValid(Character) ? Character->GetAimPitch() : 0.0f;
}
