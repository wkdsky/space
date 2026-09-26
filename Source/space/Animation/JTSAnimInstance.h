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
	virtual void NativePostEvaluateAnimation() override;
	void ApplyFacingPose();

	/** Updated from AJTSCharacter once per animation update. */
	UPROPERTY(BlueprintReadOnly, Category = "Aim", meta = (AllowPrivateAccess = "true"))
	float AimPitch = 0.0f;

	/** Camera-relative horizontal aim angle for a Blueprint upper-body pose. */
	UPROPERTY(BlueprintReadOnly, Category = "Aim", meta = (AllowPrivateAccess = "true"))
	float AimYaw = 0.0f;

	/**
	 * 0 while standing inside the front cone, 1 while the feet are shuffling to catch a large turn.
	 * The AnimGraph uses this to lift and cycle the legs without a dedicated shuffle clip.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Aim", meta = (AllowPrivateAccess = "true"))
	float TurnShuffleAlpha = 0.0f;

	/** Smoothed torso yaw. Stays inside the front cone while the feet catch a larger turn. */
	UPROPERTY(BlueprintReadOnly, Category = "Aim", meta = (AllowPrivateAccess = "true"))
	float UpperBodyYaw = 0.0f;

	/** Alternating lift used by the shuffle pose, in degrees. */
	UPROPERTY(BlueprintReadOnly, Category = "Aim", meta = (AllowPrivateAccess = "true"))
	float ShuffleLeftLift = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Aim", meta = (AllowPrivateAccess = "true"))
	float ShuffleRightLift = 0.0f;

	float ShufflePhase = 0.0f;
	float RawAimYaw = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Aim", meta = (AllowPrivateAccess = "true"))
	bool bWeaponAiming = false;

	/** The existing bool blend selects its stable gun-pointing upper-body pose when true. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	bool bHasRangedWeapon = false;

	/** Compatibility flag consumed by the existing AnimGraph; true for any active Holdable item. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	bool bHasHeldItem = false;

	bool bActiveRangedWeapon = false;

	/** Held item that uses the present-arms pose (knife, axe, pickaxe). Ranged weapons stay on the gun clip. */
	bool bMeleeHeld = false;

	/** 0 on the ground, rises while airborne so the jump tuck can play out and then release. */
	float JumpTuckAlpha = 0.0f;

	/** Seconds spent in the current fall, used to fold the legs and then open them again. */
	float AirTime = 0.0f;

	/** 0 at the upright carry, about 0.34 with the tool raised, 1 at the strike. */
	float MeleeChopAlpha = 0.0f;

	/** Seconds into the current cut-hold-raise cycle. The raised pose is both the end and the start. */
	float MeleeChopStroke = 0.0f;

	/** True after the tool has been raised, so the next motion is the cut rather than another lift from the carry. */
	bool bMeleeChopRaised = false;

	/** True once this press has played its chop, so a lingering attack clock cannot restart the lift. */
	bool bMeleeChopLatched = false;
};
