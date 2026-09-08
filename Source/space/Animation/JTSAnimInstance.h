// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"

#include "JTSAnimInstance.generated.h"

class AJTSCharacter;

/**
 * Supplies character-facing animation data to Animation Blueprints without coupling gameplay to a Blueprint graph.
 */
UCLASS()
class SPACE_API UJTSAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Pitch in degrees for the AnimGraph's upper-body aim adjustment. */
	UFUNCTION(BlueprintPure, Category = "Aim", meta = (BlueprintThreadSafe))
	float GetAimPitch() const;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Updated from AJTSCharacter once per animation update. */
	UPROPERTY(BlueprintReadOnly, Category = "Aim", meta = (AllowPrivateAccess = "true"))
	float AimPitch = 0.0f;
};
