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

	UFUNCTION(BlueprintPure, Category = "Aim", meta = (BlueprintThreadSafe))
	float GetAimYaw() const;

	UFUNCTION(BlueprintPure, Category = "Aim", meta = (BlueprintThreadSafe))
	bool IsWeaponAiming() const;

	UFUNCTION(BlueprintPure, Category = "Aim", meta = (BlueprintThreadSafe))
	bool HasRangedWeapon() const;

	UFUNCTION(BlueprintPure, Category = "Equipment", meta = (BlueprintThreadSafe))
	bool HasHeldItem() const;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Updated from AJTSCharacter once per animation update. */
	UPROPERTY(BlueprintReadOnly, Category = "Aim", meta = (AllowPrivateAccess = "true"))
	float AimPitch = 0.0f;

	/** Camera-relative horizontal aim angle for a Blueprint upper-body pose. */
	UPROPERTY(BlueprintReadOnly, Category = "Aim", meta = (AllowPrivateAccess = "true"))
	float AimYaw = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Aim", meta = (AllowPrivateAccess = "true"))
	bool bWeaponAiming = false;

	/** The existing bool blend selects its stable gun-pointing upper-body pose when true. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	bool bHasRangedWeapon = false;

	/** Compatibility flag consumed by the existing AnimGraph; true for any active Holdable item. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	bool bHasHeldItem = false;

	bool bActiveRangedWeapon = false;
};
