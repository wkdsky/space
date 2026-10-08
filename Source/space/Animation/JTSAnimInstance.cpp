// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Animation/JTSAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSMeleeComponent.h"
#include "space/Components/JTSRangedWeaponComponent.h"
#include "space/Components/JTSStellarWeaponComponent.h"
#include "space/Components/JTSWallClimbComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerState.h"

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
	TurnShuffleAlpha = 0.0f;
	bWeaponAiming = false;
	bHasRangedWeapon = false;
	bHasHeldItem = false;
	bActiveRangedWeapon = false;
	bStellarScepterHeld = false;
	bool bStellarCasting = false;
	bMeleeHeld = false;
	bTwoHandHeld = false;
	if (IsValid(Character))
	{
		if (const UJTSInventoryComponent* const Inventory = Character->FindComponentByClass<UJTSInventoryComponent>())
		{
			const EJTSItemId ActiveId = Inventory->GetActiveItemId();
			const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ActiveId);
			bHasHeldItem = IsValid(Definition) && Definition->IsHoldable() && !Inventory->GetActiveItem().IsEmpty();
			const bool bGun = bHasHeldItem && Definition->IsRangedWeapon();
			bTwoHandHeld = bHasHeldItem && Definition->HeldPresentation.bDualWield;
			bMeleeHeld = bHasHeldItem && !bGun && !bTwoHandHeld;
		}
		if (const UJTSRangedWeaponComponent* const Ranged = Character->FindComponentByClass<UJTSRangedWeaponComponent>())
		{
			bActiveRangedWeapon = Ranged->HasActiveRangedWeapon();
			bWeaponAiming = Ranged->IsAiming() && bActiveRangedWeapon;
		}
		if (const UJTSStellarWeaponComponent* const Stellar = Character->FindComponentByClass<UJTSStellarWeaponComponent>();
			IsValid(Stellar) && Stellar->HasActiveWeapon())
		{
			const auto* Definition = Stellar->GetEquippedWeaponDefinition();
			const bool bStellarMelee = Definition && Definition->bMeleePresentation;
			bStellarScepterHeld = Definition && Definition->bUprightScepter;
			bStellarCasting = Stellar->IsCasting();
			bHasHeldItem = true;
			bActiveRangedWeapon = !bStellarMelee;
			bWeaponAiming = !bStellarMelee && Stellar->IsAiming();
			bTwoHandHeld = false;
			bMeleeHeld = bStellarMelee;
		}
		bMeleeHeld = bMeleeHeld && !bActiveRangedWeapon;

		const UCharacterMovementComponent* const Movement = Character->GetCharacterMovement();
		// Planet landing leaves MOVE_Falling on until the floor sweep catches up, and a
		// standing character on that mode was holding the jump tuck. A real jump still
		// starts on the button press, before the movement mode leaves the ground.
		const bool bPressedJump = Character->bPressedJump;
		const bool bAirborne = bPressedJump || (IsValid(Movement) && Movement->IsFalling() && !Character->IsSupportedByFloor());
		if (bAirborne)
		{
			AirTime += DeltaSeconds;
			// Knees come up immediately and stay tucked for the whole jump.
			// Opening them again on the way down was cutting the pose off in midair.
			const float Rise = FMath::Clamp(AirTime / 0.08f, 0.0f, 1.0f);
			JumpTuckAlpha = FMath::FInterpTo(JumpTuckAlpha, Rise, DeltaSeconds, 22.0f);
		}
		else
		{
			AirTime = 0.0f;
			JumpTuckAlpha = FMath::FInterpTo(JumpTuckAlpha, 0.0f, DeltaSeconds, 9.0f);
		}

		const UJTSWallClimbComponent* const GroundClimb = Character->FindComponentByClass<UJTSWallClimbComponent>();
		const bool bGroundClimbing = IsValid(GroundClimb) && GroundClimb->IsClimbing();
		if (bGroundClimbing || ClimbBlendAlpha > 0.02f)
		{
			// The selected slot stays intact, but the animation graph must see empty hands.
			bHasHeldItem = false;
			bActiveRangedWeapon = false;
			bWeaponAiming = false;
			bMeleeHeld = false;
			bTwoHandHeld = false;
			bStellarScepterHeld = false;
			bStellarCasting = false;
		}
		const AJTSPlayerState* const GroundState = Character->GetPlayerState<AJTSPlayerState>();
		const bool bGroundDead = IsValid(GroundState)
			&& GroundState->GetExpeditionStatus() == EJTSPlayerExpeditionStatus::Dead;
		const float SpeedScale = IsValid(GroundState)
			? FMath::Max(0.1f, GroundState->GetRunSpeedMultiplier()) : 1.0f;
		const bool bGroundEligible = !bAirborne && !bGroundClimbing && ClimbBlendAlpha <= 0.02f
			&& !Character->IsBoarded() && !bGroundDead;
		const FVector GroundVelocity = IsValid(Movement) ? Movement->Velocity : FVector::ZeroVector;
		const FVector GroundUp = Character->GetActorUpVector();
		const FVector GroundPlanar = FVector::VectorPlaneProject(GroundVelocity, GroundUp);
		const float GroundSpeed = GroundPlanar.Size();
		const FVector CurrentFacing = FVector::VectorPlaneProject(
			Character->GetActorForwardVector(), GroundUp).GetSafeNormal();
		float TurnRate = 0.0f;
		if (bHasPreviousFacing && DeltaSeconds > KINDA_SMALL_NUMBER && !CurrentFacing.IsNearlyZero())
		{
			const FVector PreviousFacing = FVector::VectorPlaneProject(
				PreviousFacingForward, GroundUp).GetSafeNormal();
			const float TurnDegrees = FMath::RadiansToDegrees(FMath::Atan2(
				FVector::DotProduct(FVector::CrossProduct(PreviousFacing, CurrentFacing), GroundUp),
				FVector::DotProduct(PreviousFacing, CurrentFacing)));
			if (FMath::Abs(TurnDegrees) < 45.0f) TurnRate = TurnDegrees / DeltaSeconds;
		}
		PreviousFacingForward = CurrentFacing;
		bHasPreviousFacing = !CurrentFacing.IsNearlyZero();
		const float TargetLean = bGroundEligible && GroundSpeed > 60.0f
			? -FMath::Clamp(TurnRate / 520.0f, -1.0f, 1.0f) * 7.0f : 0.0f;
		TurnLeanDegrees = FMath::FInterpTo(TurnLeanDegrees, TargetLean, DeltaSeconds, 11.0f);
		// A new step starts at the selected walk/run stride, rather than spending
		// its first few frames interpolating through tiny idle strides. The pose's
		// running phase still follows the blend space player after evaluation.
		const bool bGroundMoving = bGroundEligible && GroundSpeed > 20.0f && JumpTuckAlpha < 0.02f;
		// Normal movement tops out at 500 cm/s; sprint starts above it and reaches
		// 800 cm/s. Keep the authored walk untouched until actual speed passes the
		// normal walk band, then blend in the stylized run presentation.
		const float TargetRun = FMath::GetMappedRangeValueClamped(
			FVector2D(550.0f * SpeedScale, 700.0f * SpeedScale), FVector2D(0.0f, 1.0f), GroundSpeed);
		if (bGroundMoving && !bGroundGaitMoving)
		{
			GaitBlend = 1.0f + TargetRun;
		}
		else
		{
			GaitBlend = FMath::FInterpTo(GaitBlend, bGroundMoving ? 1.0f + TargetRun : 0.0f, DeltaSeconds, 8.0f);
		}
		bGroundGaitMoving = bGroundMoving;
		if (bMeleeHeld)
		{
			const UJTSMeleeComponent* const Melee = Character->FindComponentByClass<UJTSMeleeComponent>();
			const bool bHeld = IsValid(Melee) && Melee->IsAttackInputHeld();
			const bool bSwingActive = IsValid(Melee) && Melee->GetMeleeSwingPhase() > 0.0f;
			// The attack clock restarts on every server swing. The chop itself does not:
			// a held button keeps the hand on the arc, and a click owes exactly one cycle.
			if (!bHeld && !bSwingActive)
			{
				bMeleeChopLatched = false;
			}
			const bool bOweChop = bHeld || (bSwingActive && !bMeleeChopLatched);

			// Cut, a beat on the hit, then a raise that ends on the same pose the next cut leaves.
			const float RaisedPose = 0.34f;
			const float CutSeconds = 0.15f;
			const float HitSeconds = 0.07f;
			const float RaiseSeconds = 0.30f;
			const float Cycle = CutSeconds + HitSeconds + RaiseSeconds;
			auto SampleChop = [&](const float Stroke)
			{
				if (Stroke < CutSeconds)
				{
					const float T = Stroke / CutSeconds;
					const float Accelerated = T * T;
					// Slow off the top, then the whole arc dumps into the bottom.
					MeleeChopAlpha = FMath::Lerp(RaisedPose, 1.0f, Accelerated * Accelerated);
				}
				else if (Stroke < CutSeconds + HitSeconds)
				{
					MeleeChopAlpha = 1.0f;
				}
				else
				{
					const float T = (Stroke - CutSeconds - HitSeconds) / RaiseSeconds;
					// Fast off the hit, easing into the top so the next cut has a beat to leave from.
					const float EaseOut = 1.0f - (1.0f - T) * (1.0f - T) * (1.0f - T);
					MeleeChopAlpha = FMath::Lerp(1.0f, RaisedPose, EaseOut);
				}
			};

			if (!bMeleeChopRaised)
			{
				if (!bOweChop)
				{
					MeleeChopStroke = 0.0f;
					MeleeChopAlpha = FMath::FInterpTo(MeleeChopAlpha, 0.0f, DeltaSeconds, 7.0f);
				}
				else
				{
					// First press only lifts. The cut starts from that raised pose, never from the carry.
					MeleeChopAlpha = FMath::FInterpTo(MeleeChopAlpha, RaisedPose, DeltaSeconds, 7.0f);
					if (MeleeChopAlpha > RaisedPose - 0.04f)
					{
						bMeleeChopRaised = true;
						MeleeChopStroke = 0.0f;
						MeleeChopAlpha = RaisedPose;
					}
				}
			}
			else
			{
				const float NextStroke = MeleeChopStroke + DeltaSeconds;
				if (NextStroke >= Cycle && !bHeld)
				{
					// The owed chop has come back to the top. Release settles from there to the carry.
					bMeleeChopRaised = false;
					bMeleeChopLatched = true;
					MeleeChopStroke = 0.0f;
					MeleeChopAlpha = FMath::FInterpTo(MeleeChopAlpha, 0.0f, DeltaSeconds, 6.0f);
				}
				else
				{
					MeleeChopStroke = FMath::Fmod(NextStroke, Cycle);
					SampleChop(MeleeChopStroke);
				}
			}
		}
		else
		{
			bMeleeChopRaised = false;
			bMeleeChopLatched = false;
			MeleeChopStroke = 0.0f;
			MeleeChopAlpha = FMath::FInterpTo(MeleeChopAlpha, 0.0f, DeltaSeconds, 8.0f);
		}

		const UJTSWallClimbComponent* const Climb = Character->FindComponentByClass<UJTSWallClimbComponent>();
		const bool bClimbing = IsValid(Climb) && Climb->IsClimbing();
		bClimbMantling = bClimbing && Climb->IsMantling();
		const float PoseDelta = FMath::Clamp(DeltaSeconds, 0.0f, 0.05f);
		const float MantleStandAlpha = bClimbMantling
			? FMath::SmoothStep(0.43f, 0.95f, Climb->GetStepAlpha()) : 0.0f;
		ClimbBlendAlpha = bClimbMantling
			? FMath::Min(ClimbBlendAlpha, 1.0f - MantleStandAlpha)
			: FMath::FInterpConstantTo(ClimbBlendAlpha, bClimbing ? 1.0f : 0.0f,
				PoseDelta, bClimbing ? 7.0f : 6.0f);
		if (bClimbing)
		{
			ClimbStepAlpha = Climb->GetStepAlpha();
			bClimbLeadLeft = Climb->IsLeadHandLeft();
			bClimbLeaping = Climb->IsLeaping();
			LastClimbSurfaceNormal = Climb->GetSurfaceNormal();
			LastClimbStepDirection = Climb->GetStepDirection();
			if (!bWasClimbing)
			{
				// The first grab arrests the fall; the body then sags under the arms.
				ClimbSagCm = 0.0f;
				ClimbSagVelocity = 85.0f;
				ClimbSwayCm = 0.0f;
				ClimbSwayVelocity = 0.0f;
			}
			const float StepStartTime = Climb->GetStepStartWorldTime();
			const float SideIntent = FVector::DotProduct(Climb->GetStepDirection(), Character->GetActorRightVector());
			const float SupportSideSign = FMath::Abs(SideIntent) > 0.2f
				? -FMath::Sign(SideIntent) : (bClimbLeadLeft ? 1.0f : -1.0f);
			if (StepStartTime >= 0.0f && StepStartTime != LastClimbStepStartTime)
			{
				// Load the hand and foot that stay planted before the searching side leaves.
				ClimbSwayVelocity += SupportSideSign * (bClimbLeaping ? 62.0f : 42.0f);
				LastClimbStepStartTime = StepStartTime;
				PreviousClimbStepAlpha = 0.0f;
			}
			const bool bDescending = !bClimbLeaping
				&& FVector::DotProduct(Climb->GetStepDirection(), Character->GetActorUpVector()) < -0.5f;
			const EJTSClimbMotion Motion = bClimbLeaping ? EJTSClimbMotion::Leap
				: bDescending ? EJTSClimbMotion::Descend : EJTSClimbMotion::Step;
			const FJTSClimbMotionSample Phase = FJTSClimbMotionSample::Evaluate(ClimbStepAlpha, Motion);
			if (!bClimbMantling && PreviousClimbStepAlpha < 0.78f && ClimbStepAlpha >= 0.78f)
			{
				// Fingers catch first; the shoulders then absorb the hanging weight.
				ClimbSagVelocity += bClimbLeaping ? 100.0f : bDescending ? 72.0f : 48.0f;
				ClimbSwayVelocity -= SupportSideSign * (bClimbLeaping ? 80.0f : 52.0f);
			}
			PreviousClimbStepAlpha = ClimbStepAlpha;
			const float Steepness = FMath::Clamp(1.0f - FVector::DotProduct(
				Climb->GetSurfaceNormal(), Character->GetActorUpVector()) / 0.77f, 0.0f, 1.0f);
			const float DesiredSag = bClimbMantling
				? FMath::Lerp(3.0f + Steepness * 11.0f, 0.0f,
					FMath::SmoothStep(0.20f, 0.80f, ClimbStepAlpha))
				: 3.0f + Steepness * 11.0f
					+ Phase.Load * (bClimbLeaping ? 11.0f : bDescending ? -3.0f : 5.0f);
			const float SagAcceleration = (DesiredSag - ClimbSagCm) * FMath::Square(19.0f)
				- ClimbSagVelocity * 2.0f * 0.72f * 19.0f;
			ClimbSagVelocity += SagAcceleration * PoseDelta;
			ClimbSagCm = FMath::Clamp(ClimbSagCm + ClimbSagVelocity * PoseDelta, -3.0f, 25.0f);
			const float SwayAcceleration = -ClimbSwayCm * FMath::Square(13.0f)
				- ClimbSwayVelocity * 2.0f * 0.42f * 13.0f;
			ClimbSwayVelocity += SwayAcceleration * PoseDelta;
			ClimbSwayCm = FMath::Clamp(ClimbSwayCm + ClimbSwayVelocity * PoseDelta, -8.0f, 8.0f);
			JumpTuckAlpha = 0.0f;
			GaitBlend = 0.0f;
		}
		else
		{
			ClimbStepAlpha = 0.0f;
			bClimbLeaping = false;
			bClimbMantling = false;
			LastClimbStepStartTime = -1.0f;
			PreviousClimbStepAlpha = 0.0f;
			bClimbContactsValid = false;
			ClimbContactStepStartTime = -1.0f;
		}
		bWasClimbing = bClimbing;
		const bool bClimbPoseActive = bClimbing || ClimbBlendAlpha > 0.02f;
		// Free look never twists the torso or head away from wall handholds.
		{
			const float HeadPitchTarget = bClimbPoseActive ? 0.0f
				: FMath::Clamp(Character->GetAimPitch(), -55.0f, 55.0f);
			AimPitch = bClimbPoseActive ? 0.0f : FMath::FInterpTo(AimPitch, HeadPitchTarget, DeltaSeconds, 14.0f);
		}
		// Ordinary grounded third person keeps the player's facing. The arms only cover
		// the view while it is already close to the body. Past that cone the arms
		// return forward instead of dragging the barrel onto screen center.
		// First person and weapon aiming already turn the whole actor with the view.
		// Ordinary third-person jumps keep the movement heading instead.
		const bool bWholeBodyOnView = bClimbPoseActive || Character->IsFirstPersonView()
			|| bWeaponAiming;
		AimYaw = bWholeBodyOnView ? 0.0f : Character->GetPresentationAimYaw();
		TurnShuffleAlpha = bWholeBodyOnView ? 0.0f : Character->GetTurnShuffleAlpha();
		const float YawLimit = 48.0f;
		const float DesiredUpperYaw = FMath::Clamp(AimYaw, -YawLimit, YawLimit);
		UpperBodyYaw = bClimbPoseActive ? 0.0f : FMath::FInterpTo(
			UpperBodyYaw,
			DesiredUpperYaw,
			DeltaSeconds,
			bWholeBodyOnView ? 24.0f : 9.0f);

		// Small alternating weight transfers accompany the capsule turn.
		ShufflePhase += DeltaSeconds * (TurnShuffleAlpha > 0.05f ? 17.0f : 0.0f);
		const float Step = FMath::Max(0.0f, FMath::Sin(ShufflePhase));
		const float Other = FMath::Max(0.0f, FMath::Sin(ShufflePhase + PI));
		ShuffleLeftLift = 13.0f * TurnShuffleAlpha * Step;
		ShuffleRightLift = 13.0f * TurnShuffleAlpha * Other;
	}
	StellarCastAlpha = FMath::FInterpTo(StellarCastAlpha, bStellarCasting ? 1.0f : 0.0f,
		DeltaSeconds, FMath::Max(1.0f, StellarPoseBlendSpeed));
	bHasRangedWeapon = bActiveRangedWeapon;
}

void UJTSAnimInstance::NativePostEvaluateAnimation()
{
	Super::NativePostEvaluateAnimation();
	if (!GroundPoseMachine.IsNone() && !GroundPoseState.IsNone())
	{
		const int32 PlayerIndex = GetInstanceAssetPlayerIndex(GroundPoseMachine, GroundPoseState);
		if (PlayerIndex != INDEX_NONE)
		{
			// Keep the run's subtle shoulder variation and pelvis pulse on the
			// authored locomotion clock. Walking arms stay still.
			GaitPhase = FMath::Frac(GetInstanceAssetPlayerTimeFraction(PlayerIndex));
		}
	}
	ApplyFacingPose();
}

void UJTSAnimInstance::ApplyFacingPose()
{
	USkeletalMeshComponent* const Mesh = GetSkelMeshComponent();
	// Empty, melee, and gun each replace the clip arms every frame. Skipping the gun
	// while the view is still left the idle clip's hanging arms in place.
	if (!IsValid(Mesh))
	{
		return;
	}

	const USkeletalMesh* const SkeletalMesh = Mesh->GetSkeletalMeshAsset();
	const FReferenceSkeleton* const Skeleton = SkeletalMesh != nullptr ? &SkeletalMesh->GetRefSkeleton() : nullptr;
	TArray<FTransform>& Pose = Mesh->GetEditableComponentSpaceTransforms();
	if (Skeleton == nullptr || Mesh->GetOwner() == nullptr || Pose.Num() == 0)
	{
		return;
	}

	const FQuat ComponentRotation = Mesh->GetComponentQuat();

	// Rotates only the named bone. The previous subtree rewrite moved every descendant's location
	// and left the renderer on the unedited buffer, so the arms never matched the intended pose.
	auto TurnBone = [&](const FName BoneName, const FVector& Axis, float Degrees)
	{
		if (FMath::IsNearlyZero(Degrees))
		{
			return;
		}
		const int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
		if (!Pose.IsValidIndex(BoneIndex))
		{
			return;
		}
		const FVector LocalAxis = ComponentRotation.UnrotateVector(
			Mesh->GetOwner()->GetActorQuat().RotateVector(Axis)).GetSafeNormal();
		if (LocalAxis.IsNearlyZero())
		{
			return;
		}
		Pose[BoneIndex].SetRotation(FQuat(LocalAxis, FMath::DegreesToRadians(Degrees)) * Pose[BoneIndex].GetRotation());
		Pose[BoneIndex].NormalizeRotation();
	};

	// Swings a bone so the segment to its child lies along Aim, and carries the subtree.
	// A fixed angle on the clip rotation misses because these bones do not share one axis.
	auto AimSegment = [&](const FName BoneName, const FName ChildName, const FVector& Aim)
	{
		const int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
		const int32 ChildIndex = Mesh->GetBoneIndex(ChildName);
		if (Skeleton == nullptr || !Pose.IsValidIndex(BoneIndex) || !Pose.IsValidIndex(ChildIndex))
		{
			return;
		}
		const FVector AimDir = Aim.GetSafeNormal();
		const FVector Origin = Pose[BoneIndex].GetLocation();
		const FVector Dir = (Pose[ChildIndex].GetLocation() - Origin).GetSafeNormal();
		if (Dir.IsNearlyZero() || AimDir.IsNearlyZero() || FVector::DotProduct(Dir, AimDir) > 0.999f)
		{
			return;
		}
		const FQuat Delta = FQuat::FindBetweenNormals(Dir, AimDir);
		for (int32 Index = 0; Index < Pose.Num(); ++Index)
		{
			int32 Walk = Index;
			bool bUnder = false;
			while (Walk >= 0)
			{
				if (Walk == BoneIndex)
				{
					bUnder = true;
					break;
				}
				Walk = Skeleton->GetParentIndex(Walk);
			}
			if (!bUnder)
			{
				continue;
			}
			Pose[Index].SetLocation(Origin + Delta.RotateVector(Pose[Index].GetLocation() - Origin));
			Pose[Index].SetRotation(Delta * Pose[Index].GetRotation());
			Pose[Index].NormalizeRotation();
		}
	};

	const FVector ActorForward = ComponentRotation.UnrotateVector(Mesh->GetOwner()->GetActorForwardVector());
	const FVector ActorUp = ComponentRotation.UnrotateVector(Mesh->GetOwner()->GetActorUpVector());
	const FVector ActorRight = ComponentRotation.UnrotateVector(Mesh->GetOwner()->GetActorRightVector());
	// Component-space bone locations are not actor-aligned on this mesh: the clip
	// stores the body along component +Y. Actor axes have to be expressed in that
	// same frame before a hinge or an aim direction will move the drawn limb.
	// Casual_2's spine is Hips -> Abdomen -> Torso -> Chest. Pelvis and Spine_03
	// are not on this skeleton, so the body axis comes from Hips to Chest.
	const int32 SpineBone = Mesh->GetBoneIndex(TEXT("Chest"));
	const int32 PelvisBone = Mesh->GetBoneIndex(TEXT("Hips"));
	FVector PoseUp = ActorUp;
	FVector PoseForward = ActorForward;
	FVector PoseRight = ActorRight;
	if (Pose.IsValidIndex(SpineBone) && Pose.IsValidIndex(PelvisBone))
	{
		const FVector Spine = (Pose[SpineBone].GetLocation() - Pose[PelvisBone].GetLocation()).GetSafeNormal();
		if (!Spine.IsNearlyZero() && FVector::DotProduct(Spine, ActorUp) > 0.2f)
		{
			PoseUp = Spine;
			PoseRight = FVector::CrossProduct(PoseUp, ActorForward).GetSafeNormal();
			if (PoseRight.IsNearlyZero())
			{
				PoseRight = ActorRight;
			}
			PoseForward = FVector::CrossProduct(PoseRight, PoseUp).GetSafeNormal();
		}
	}

	// Folds the fingers of one hand around a grip. The idle clip leaves them open, so a
	// held item was floating in an open palm. Curl is applied around the palm normal, which
	// is the same side for both hands because the finger bones mirror across the body.
	float GripAmount = 1.0f;
	auto CloseGrip = [&](const bool bRightHand)
	{
		const TCHAR* const Suffix = bRightHand ? TEXT("_R") : TEXT("_L");
		const int32 WristIndex = Mesh->GetBoneIndex(*FString::Printf(TEXT("Wrist%s"), Suffix));
		const int32 IndexRoot = Mesh->GetBoneIndex(*FString::Printf(TEXT("Index1%s"), Suffix));
		const int32 MiddleRoot = Mesh->GetBoneIndex(*FString::Printf(TEXT("Middle1%s"), Suffix));
		if (!Pose.IsValidIndex(WristIndex) || !Pose.IsValidIndex(IndexRoot) || !Pose.IsValidIndex(MiddleRoot))
		{
			return;
		}

		const FVector KnuckleSpan = Pose[MiddleRoot].GetLocation() - Pose[IndexRoot].GetLocation();
		const FVector FingerOut = Pose[IndexRoot].GetLocation() - Pose[WristIndex].GetLocation();
		FVector PalmNormal = FVector::CrossProduct(KnuckleSpan, FingerOut).GetSafeNormal();
		if (PalmNormal.IsNearlyZero())
		{
			return;
		}
		// The knuckle span points from index toward pinky. Crossing it with the finger
		// direction points out of the back of this hand, so the opposite closes the fist.
		if (FVector::DotProduct(PalmNormal, PoseUp) < 0.0f)
		{
			PalmNormal *= -1.0f;
		}
		PalmNormal *= -1.0f;

		auto CurlChain = [&](const TCHAR* BonePrefix, const float RootDegrees, const float JointDegrees)
		{
			const FQuat RootCurl(PalmNormal, FMath::DegreesToRadians(RootDegrees * GripAmount));
			const FQuat JointCurl(PalmNormal, FMath::DegreesToRadians(JointDegrees * GripAmount));
			for (int32 Joint = 1; Joint <= 3; ++Joint)
			{
				const int32 BoneIndex = Mesh->GetBoneIndex(*FString::Printf(TEXT("%s%d%s"), BonePrefix, Joint, Suffix));
				if (!Pose.IsValidIndex(BoneIndex))
				{
					continue;
				}
				const FQuat Curl = Joint == 1 ? RootCurl : JointCurl;
				const FVector Origin = Pose[BoneIndex].GetLocation();
				for (int32 Index = 0; Index < Pose.Num(); ++Index)
				{
					int32 Walk = Index;
					bool bUnder = false;
					while (Walk >= 0)
					{
						if (Walk == BoneIndex)
						{
							bUnder = true;
							break;
						}
						Walk = Skeleton->GetParentIndex(Walk);
					}
					if (!bUnder)
					{
						continue;
					}
					Pose[Index].SetLocation(Origin + Curl.RotateVector(Pose[Index].GetLocation() - Origin));
					Pose[Index].SetRotation(Curl * Pose[Index].GetRotation());
					Pose[Index].NormalizeRotation();
				}
			}
		};

		CurlChain(TEXT("Index"), 68.0f, 42.0f);
		CurlChain(TEXT("Middle"), 72.0f, 44.0f);
		CurlChain(TEXT("Ring"), 70.0f, 42.0f);
		CurlChain(TEXT("Pinky"), 64.0f, 38.0f);
		CurlChain(TEXT("Thumb"), 38.0f, 28.0f);
	};

	// Places a root-parented foot under the knee. Foot_L/R are not children of the legs,
	// so folding the shin leaves the skinned foot planted in the idle clip.
	auto PlaceFoot = [&](const FName FootName, const FVector& WorldOffset)
	{
		const int32 FootIndex = Mesh->GetBoneIndex(FootName);
		if (!Pose.IsValidIndex(FootIndex))
		{
			return;
		}
		const FVector Offset = ComponentRotation.UnrotateVector(WorldOffset);
		Pose[FootIndex].SetLocation(Pose[FootIndex].GetLocation() + Offset);
	};

	// One shared yaw, split along the spine so the head ends on the camera heading.
	// TurnBone does not carry children, so these weights have to add up to 1.
	// Past the rear cone BodyYaw is already returning to zero.
	const float BodyYaw = FMath::Clamp(UpperBodyYaw, -48.0f, 48.0f);
	TurnBone(TEXT("Abdomen"), FVector::UpVector, BodyYaw * 0.28f);
	TurnBone(TEXT("Chest"), FVector::UpVector, BodyYaw * 0.32f);
	TurnBone(TEXT("Abdomen"), FVector::ForwardVector, TurnLeanDegrees * 0.65f);
	TurnBone(TEXT("Chest"), FVector::ForwardVector, TurnLeanDegrees * 0.35f);
	TurnBone(TEXT("Neck"), FVector::UpVector, BodyYaw * 0.22f);
	TurnBone(TEXT("Head"), FVector::UpVector, BodyYaw * 0.18f);
	TurnBone(TEXT("Head"), FVector::RightVector, -AimPitch * 0.85f);

	const AJTSCharacter* const PoseCharacter = Cast<AJTSCharacter>(Mesh->GetOwner());
	const UJTSWallClimbComponent* const Climb = PoseCharacter != nullptr
		? PoseCharacter->FindComponentByClass<UJTSWallClimbComponent>()
		: nullptr;
	if (IsValid(Climb) && (Climb->IsClimbing() || ClimbBlendAlpha > 0.02f))
	{
		// Keep a copy for the short grab/release blend. The capsule remains authoritative;
		// only the bones show weight, reach and the follow-through after leaving the wall.
		const TArray<FTransform> UnclimbedPose = Pose;
		const FVector WallNormal = ComponentRotation.UnrotateVector(
			Climb->IsClimbing() ? Climb->GetSurfaceNormal() : LastClimbSurfaceNormal).GetSafeNormal();
		const FVector WallForward = FVector::VectorPlaneProject(-WallNormal, PoseUp).GetSafeNormal();
		const FVector WallRight = FVector::CrossProduct(PoseUp, WallForward).GetSafeNormal();
		const FVector Step = ComponentRotation.UnrotateVector(
			Climb->IsClimbing() ? Climb->GetStepDirection() : LastClimbStepDirection).GetSafeNormal();
		const float Alpha = FMath::Clamp(ClimbStepAlpha, 0.0f, 1.0f);
		const bool bLeap = bClimbLeaping;
		const bool bDescending = !bLeap && FVector::DotProduct(Step, PoseUp) < -0.5f;
		const EJTSClimbMotion Motion = bLeap ? EJTSClimbMotion::Leap
			: bDescending ? EJTSClimbMotion::Descend : EJTSClimbMotion::Step;
		const FJTSClimbMotionSample Phase = FJTSClimbMotionSample::Evaluate(Alpha, Motion);
		const FVector Front = WallForward.IsNearlyZero() ? PoseForward : WallForward;
		const FVector Side = WallRight.IsNearlyZero() ? PoseRight : WallRight;
		const float Steepness = FMath::Clamp(1.0f - FVector::DotProduct(WallNormal, PoseUp) / 0.77f,
			0.0f, 1.0f);

		// Casual_2's foot bones are root siblings, so save their offsets from
		// the ankles before shifting the hips and solving the legs.
		struct FClimbShoeRest
		{
			int32 Ankle;
			int32 Foot;
			FVector AnklePosition;
			FVector FootOffset;
		};
		FClimbShoeRest Shoes[2];
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const TCHAR* Suffix = Index == 0 ? TEXT("_L") : TEXT("_R");
			FClimbShoeRest& Shoe = Shoes[Index];
			Shoe.Ankle = Mesh->GetBoneIndex(*FString::Printf(TEXT("LowerLeg%s_end"), Suffix));
			Shoe.Foot = Mesh->GetBoneIndex(*FString::Printf(TEXT("Foot%s"), Suffix));
			Shoe.AnklePosition = Pose.IsValidIndex(Shoe.Ankle) ? Pose[Shoe.Ankle].GetLocation() : FVector::ZeroVector;
			Shoe.FootOffset = Pose.IsValidIndex(Shoe.Foot) ? Pose[Shoe.Foot].GetLocation() - Shoe.AnklePosition : FVector::ZeroVector;
		}
		FVector BaseShoulders[2];
		FVector BaseHips[2];
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const TCHAR* Suffix = Index == 0 ? TEXT("_L") : TEXT("_R");
			const int32 Shoulder = Mesh->GetBoneIndex(*FString::Printf(TEXT("UpperArm%s"), Suffix));
			const int32 Hip = Mesh->GetBoneIndex(*FString::Printf(TEXT("UpperLeg%s"), Suffix));
			BaseShoulders[Index] = Pose.IsValidIndex(Shoulder) ? Pose[Shoulder].GetLocation() : FVector::ZeroVector;
			BaseHips[Index] = Pose.IsValidIndex(Hip) ? Pose[Hip].GetLocation() : FVector::ZeroVector;
		}
		// Casual_2 parents Hips and both UpperLeg bones to Body. Shift that
		// common branch so pelvis skin and thighs cannot tear apart at the crotch.
		const int32 Body = Mesh->GetBoneIndex(TEXT("Body"));
		if (Pose.IsValidIndex(Body))
		{
			const FVector BodyOffset = Front * (9.0f - Steepness * 2.0f - (bLeap ? 4.0f * Phase.Load : 0.0f))
				- PoseUp * (3.0f + ClimbSagCm + (bLeap ? 6.0f * Phase.Load + 3.0f * Phase.Catch : 0.0f)
					+ (bDescending ? 5.0f * Phase.Travel : 0.0f))
				+ PoseUp * (bClimbMantling ? 14.0f * FMath::SmoothStep(0.25f, 0.70f, Alpha) : 0.0f)
				+ Side * ClimbSwayCm;
			for (int32 Bone = 0; Bone < Pose.Num(); ++Bone)
			{
				int32 Parent = Bone;
				while (Parent >= 0 && Parent != Body) Parent = Skeleton->GetParentIndex(Parent);
				if (Parent == Body) Pose[Bone].AddToTranslation(BodyOffset);
			}
			// The chest stays nearer the hold while the pelvis hangs outward beneath it.
			AimSegment(TEXT("Abdomen"), TEXT("Torso"), PoseUp + Front * (0.10f + 0.12f * Steepness));
		}

		auto PlaceLimb = [&](const FName RootName, const FName MidName, const FName EndName,
			const FVector& Target, const FVector& BendPole)
		{
			const int32 Root = Mesh->GetBoneIndex(RootName);
			const int32 Mid = Mesh->GetBoneIndex(MidName);
			const int32 End = Mesh->GetBoneIndex(EndName);
			if (!Pose.IsValidIndex(Root) || !Pose.IsValidIndex(Mid) || !Pose.IsValidIndex(End)) return;
			const FVector Origin = Pose[Root].GetLocation();
			const float Upper = FVector::Distance(Origin, Pose[Mid].GetLocation());
			const float Lower = FVector::Distance(Pose[Mid].GetLocation(), Pose[End].GetLocation());
			if (Upper < 1.0f || Lower < 1.0f) return;
			const FVector Goal = Target - Origin;
			const FVector Along = Goal.GetSafeNormal();
			if (Along.IsNearlyZero()) return;
			const float Distance = FMath::Clamp(Goal.Size(), FMath::Abs(Upper - Lower) + 0.5f, Upper + Lower - 0.5f);
			const float JointAlong = (Upper * Upper - Lower * Lower + Distance * Distance) / (2.0f * Distance);
			const float JointOut = FMath::Sqrt(FMath::Max(0.0f, Upper * Upper - JointAlong * JointAlong));
			FVector Pole = FVector::VectorPlaneProject(BendPole, Along).GetSafeNormal();
			if (Pole.IsNearlyZero()) Pole = FVector::CrossProduct(Along, Side).GetSafeNormal();
			const FVector Elbow = Origin + Along * JointAlong + Pole * JointOut;
			AimSegment(RootName, MidName, Elbow - Origin);
			AimSegment(MidName, EndName, Origin + Along * Distance - Pose[Mid].GetLocation());
		};
		auto SeatShoeBranch = [&](const int32 Root, const FVector& NewRoot)
		{
			if (!Pose.IsValidIndex(Root)) return;
			const FVector Translation = NewRoot - Pose[Root].GetLocation();
			for (int32 Bone = 0; Bone < Pose.Num(); ++Bone)
			{
				int32 Parent = Bone;
				while (Parent >= 0 && Parent != Root) Parent = Skeleton->GetParentIndex(Parent);
				if (Parent != Root) continue;
				Pose[Bone].AddToTranslation(Translation);
			}
		};
		const auto HasPoseBone = [&](const FName BoneName)
		{
			return Pose.IsValidIndex(Mesh->GetBoneIndex(BoneName));
		};
		const bool bAnchoredClimb = Climb->IsClimbing() && !bClimbMantling
			&& HasPoseBone(TEXT("UpperArm_L")) && HasPoseBone(TEXT("UpperArm_R"))
			&& HasPoseBone(TEXT("Wrist_L")) && HasPoseBone(TEXT("Wrist_R"))
			&& HasPoseBone(TEXT("UpperLeg_L")) && HasPoseBone(TEXT("UpperLeg_R"))
			&& HasPoseBone(TEXT("LowerLeg_L_end")) && HasPoseBone(TEXT("LowerLeg_R_end"));
		const FTransform MeshToWorld = Mesh->GetComponentTransform();
		const FVector ContactNormal = Climb->GetSurfaceNormal().GetSafeNormal();
		auto SampleContact = [&](const FVector& DesiredWorld, const float Clearance)
		{
			FVector SurfacePoint;
			FVector SurfaceNormal;
			return Climb->FindPoseContact(DesiredWorld, SurfacePoint, SurfaceNormal)
				? SurfacePoint + SurfaceNormal * Clearance : DesiredWorld;
		};
		if (bAnchoredClimb && !bClimbContactsValid)
		{
			for (int32 Index = 0; Index < 2; ++Index)
			{
				const float Sign = Index == 0 ? -1.0f : 1.0f;
				const TCHAR* Suffix = Index == 0 ? TEXT("_L") : TEXT("_R");
				const int32 Shoulder = Mesh->GetBoneIndex(*FString::Printf(TEXT("UpperArm%s"), Suffix));
				const int32 Hip = Mesh->GetBoneIndex(*FString::Printf(TEXT("UpperLeg%s"), Suffix));
				const FVector HandRest = Pose[Shoulder].GetLocation() + Front * 25.0f
					+ PoseUp * (17.0f - Sign * 3.0f) + Side * (Sign * 5.0f);
				const FVector FootRest = Pose[Hip].GetLocation() + Front * 24.0f
					- PoseUp * (ClimbFootDropCm - Sign * 3.0f) + Side * (Sign * 6.0f);
				ClimbHandContactsWorld[Index] = SampleContact(MeshToWorld.TransformPosition(HandRest), 6.0f);
				ClimbFootContactsWorld[Index] = SampleContact(MeshToWorld.TransformPosition(FootRest), 9.0f);
			}
			bClimbContactsValid = true;
		}
		if (bAnchoredClimb)
		{
			auto FinishPreviousStep = [&]()
			{
				const bool bReached = FVector::DistSquared(PoseCharacter->GetActorLocation(),
					ClimbPreviousStepTargetWorld) < FMath::Square(28.0f);
				for (int32 Index = 0; Index < 2; ++Index)
				{
					ClimbHandContactsWorld[Index] = bReached ? ClimbHandStepEndWorld[Index] : ClimbHandStepStartWorld[Index];
					ClimbFootContactsWorld[Index] = bReached ? ClimbFootStepEndWorld[Index] : ClimbFootStepStartWorld[Index];
				}
			};
			const float StepTime = Climb->GetStepStartWorldTime();
			if (StepTime < 0.0f && ClimbContactStepStartTime >= 0.0f)
			{
				FinishPreviousStep();
				ClimbContactStepStartTime = -1.0f;
			}
			else if (StepTime >= 0.0f && StepTime != ClimbContactStepStartTime)
			{
				if (ClimbContactStepStartTime >= 0.0f) FinishPreviousStep();
				const FVector Displacement = Climb->GetStepTarget() - Climb->GetStepStart();
				for (int32 Index = 0; Index < 2; ++Index)
				{
					ClimbHandStepStartWorld[Index] = ClimbHandContactsWorld[Index];
					ClimbFootStepStartWorld[Index] = ClimbFootContactsWorld[Index];
					ClimbHandStepEndWorld[Index] = SampleContact(ClimbHandContactsWorld[Index] + Displacement, 6.0f);
					ClimbFootStepEndWorld[Index] = SampleContact(ClimbFootContactsWorld[Index] + Displacement, 9.0f);
				}
				ClimbPreviousStepTargetWorld = Climb->GetStepTarget();
				ClimbContactStepStartTime = StepTime;
			}
		}
		float HandGrip[2] = { 1.0f, 1.0f };

		for (int32 Index = 0; Index < 2; ++Index)
		{
			const bool bLeft = Index == 0;
			const float Sign = bLeft ? -1.0f : 1.0f;
			const bool bLead = bLeft == bClimbLeadLeft;
			const TCHAR* Suffix = bLeft ? TEXT("_L") : TEXT("_R");
			const FName ShoulderName(*FString::Printf(TEXT("UpperArm%s"), Suffix));
			const FName ElbowName(*FString::Printf(TEXT("LowerArm%s"), Suffix));
			const FName WristName(*FString::Printf(TEXT("Wrist%s"), Suffix));
			const FName HipName(*FString::Printf(TEXT("UpperLeg%s"), Suffix));
			const FName KneeName(*FString::Printf(TEXT("LowerLeg%s"), Suffix));
			const FName AnkleName(*FString::Printf(TEXT("LowerLeg%s_end"), Suffix));
			const int32 Shoulder = Mesh->GetBoneIndex(ShoulderName);
			const int32 Hip = Mesh->GetBoneIndex(HipName);
			if (!Pose.IsValidIndex(Shoulder) || !Pose.IsValidIndex(Hip)) continue;
			FVector HandTarget;
			FVector FootTarget;
			if (bAnchoredClimb)
			{
				const bool bActiveStep = ClimbContactStepStartTime >= 0.0f;
				const float HandBegin = bLeap ? (bLead ? 0.16f : 0.29f)
					: bLead ? (bDescending ? 0.22f : 0.10f) : 0.42f;
				const float HandEnd = bLeap ? (bLead ? 0.68f : 0.84f)
					: bLead ? (bDescending ? 0.65f : 0.47f) : 0.84f;
				const bool bFootLeads = bDescending ? bLead : !bLead;
				const float FootBegin = bLeap ? (bFootLeads ? 0.08f : 0.18f)
					: bFootLeads ? 0.04f : 0.38f;
				const float FootEnd = bLeap ? (bFootLeads ? 0.85f : 0.93f)
					: bFootLeads ? (bDescending ? 0.37f : 0.32f) : 0.82f;
				const float HandSwing = bActiveStep ? FMath::SmoothStep(HandBegin, HandEnd, Alpha) : 0.0f;
				const float FootSwing = bActiveStep ? FMath::SmoothStep(FootBegin, FootEnd, Alpha) : 0.0f;
				const FVector HandWorld = bActiveStep
					? FMath::Lerp(ClimbHandStepStartWorld[Index], ClimbHandStepEndWorld[Index], HandSwing)
						+ ContactNormal * (bLeap ? 11.0f : 7.0f) * FMath::Sin(PI * HandSwing)
					: ClimbHandContactsWorld[Index];
				const FVector FootWorld = bActiveStep
					? FMath::Lerp(ClimbFootStepStartWorld[Index], ClimbFootStepEndWorld[Index], FootSwing)
						+ ContactNormal * (bLeap ? 12.0f : 6.0f) * FMath::Sin(PI * FootSwing)
						+ PoseCharacter->GetActorUpVector() * (bDescending ? 2.0f : 5.0f) * FMath::Sin(PI * FootSwing)
					: ClimbFootContactsWorld[Index];
				HandTarget = MeshToWorld.InverseTransformPosition(HandWorld);
				FootTarget = MeshToWorld.InverseTransformPosition(FootWorld);
				if (bActiveStep && FootSwing > KINDA_SMALL_NUMBER)
				{
					// Once a foot leaves its hold, keep it below the pelvis as the body pulls up.
					// Otherwise the knee folds into the torso during a fast leap.
					const float FootDrop = FVector::DotProduct(Pose[Hip].GetLocation() - FootTarget, PoseUp);
					FootTarget -= PoseUp * FMath::Max(0.0f, ClimbMovingFootMinDropCm - FootDrop);
				}
				HandGrip[Index] = 1.0f - 0.72f * FMath::Sin(PI * HandSwing);
			}
			else
			{
				// Mantle and release use a short procedural transition toward the floor pose.
				HandTarget = BaseShoulders[Index] + Front * 27.0f
					+ PoseUp * (18.0f + (bLeft ? 3.0f : -3.0f))
					+ Side * (Sign * 7.0f) + Step * (bLead ? 18.0f : -5.0f) * Phase.Reach;
				FootTarget = BaseHips[Index] + Front * 24.0f
					- PoseUp * (ClimbFootDropCm - (bClimbMantling ? 20.0f * FMath::SmoothStep(0.24f, 0.68f, Alpha) : 0.0f))
					+ Side * (Sign * 8.0f);
				HandGrip[Index] = 1.0f - 0.5f * Phase.Reach;
			}
			PlaceLimb(ShoulderName, ElbowName, WristName, HandTarget,
				-PoseUp * 0.50f + Side * (Sign * 0.82f) + WallNormal * 0.16f);
			PlaceLimb(HipName, KneeName, AnkleName, FootTarget,
				-WallNormal * ClimbKneeWallwardWeight + Side * (Sign * ClimbKneeSideWeight) - PoseUp * 0.12f);
			const FClimbShoeRest& Shoe = Shoes[Index];
			if (Pose.IsValidIndex(Shoe.Ankle))
			{
				const FVector AnkleNow = Pose[Shoe.Ankle].GetLocation();
				// Casual_2 shoes are root siblings. Carry their position with the ankle,
				// but not the shin's IK rotation: that turns a planted shoe on its edge.
				SeatShoeBranch(Shoe.Foot, AnkleNow + Shoe.FootOffset);
			}
		}
		// Planted fingers hold. Each searching hand opens only while it moves.
		GripAmount = HandGrip[1];
		CloseGrip(true);
		GripAmount = HandGrip[0];
		CloseGrip(false);
		GripAmount = 1.0f;
		for (int32 Bone = 0; Bone < Pose.Num(); ++Bone)
		{
			const FTransform ClimbTransform = Pose[Bone];
			Pose[Bone].Blend(UnclimbedPose[Bone], ClimbTransform, ClimbBlendAlpha);
		}
	}
	else if (bTwoHandHeld)
	{
		// Each pistol follows its own forearm and remains separated at the shoulders.
		const AJTSCharacter* OwnerCharacter = Cast<AJTSCharacter>(Mesh->GetOwner());
		const float Pitch = IsValid(OwnerCharacter) ? OwnerCharacter->GetAimPitch() : 0.0f;
		const FVector Aim = FQuat(PoseUp, FMath::DegreesToRadians(BodyYaw)).RotateVector(
			FQuat(PoseRight, FMath::DegreesToRadians(-Pitch)).RotateVector(PoseForward));
		const FVector RightAim = (Aim + PoseRight * 0.10f).GetSafeNormal();
		const FVector LeftAim = (Aim - PoseRight * 0.10f).GetSafeNormal();
		AimSegment(TEXT("UpperArm_L"), TEXT("LowerArm_L"), LeftAim);
		AimSegment(TEXT("LowerArm_L"), TEXT("Wrist_L"), LeftAim);
		AimSegment(TEXT("UpperArm_R"), TEXT("LowerArm_R"), RightAim);
		AimSegment(TEXT("LowerArm_R"), TEXT("Wrist_R"), RightAim);
		CloseGrip(true);
		CloseGrip(false);
	}
	else if (bStellarScepterHeld)
	{
		// Raise the arm and bend the elbow to present the orb above the shoulder.
		// Weapon presentation separately keeps the shaft on local gravity up, including on planets.
		const FVector Forward = FQuat(PoseUp, FMath::DegreesToRadians(BodyYaw)).RotateVector(PoseForward);
		const FVector Right = FVector::CrossProduct(PoseUp, Forward).GetSafeNormal();
		const float UpperPitch = FMath::Lerp(StellarCarryUpperArmDegrees, StellarCastUpperArmDegrees, StellarCastAlpha);
		const float ForePitch = FMath::Lerp(StellarCarryForearmDegrees, StellarCastForearmDegrees, StellarCastAlpha);
		AimSegment(TEXT("UpperArm_R"), TEXT("LowerArm_R"), FQuat(Right, FMath::DegreesToRadians(-UpperPitch)).RotateVector(Forward));
		AimSegment(TEXT("LowerArm_R"), TEXT("Wrist_R"), FQuat(Right, FMath::DegreesToRadians(-ForePitch)).RotateVector(Forward));
		AimSegment(TEXT("UpperArm_L"), TEXT("LowerArm_L"), -PoseUp);
		AimSegment(TEXT("LowerArm_L"), TEXT("Wrist_L"), -PoseUp);
		CloseGrip(true);
	}
	else if (bActiveRangedWeapon)
	{
		// The gun stays glued in the right hand. Pitch and the same limited yaw swing
		// that whole arm together, so the barrel never leaves the forearm.
		const AJTSCharacter* const OwnerCharacter = Cast<AJTSCharacter>(Mesh->GetOwner());
		const float PitchDegrees = OwnerCharacter != nullptr ? OwnerCharacter->GetAimPitch() : 0.0f;
		const FVector Pitched = FQuat(PoseRight, FMath::DegreesToRadians(-PitchDegrees)).RotateVector(PoseForward);
		const FVector Reach = FQuat(PoseUp, FMath::DegreesToRadians(BodyYaw)).RotateVector(Pitched);
		AimSegment(TEXT("UpperArm_R"), TEXT("LowerArm_R"), Reach);
		AimSegment(TEXT("LowerArm_R"), TEXT("Wrist_R"), Reach);
		AimSegment(TEXT("UpperArm_L"), TEXT("LowerArm_L"), -PoseUp);
		AimSegment(TEXT("LowerArm_L"), TEXT("Wrist_L"), -PoseUp);
		CloseGrip(true);
	}
	else if (bMeleeHeld)
	{
		// Present-arms L at rest. A chop lifts the shoulder with the forearm, then the
		// forearm runs the long arc. The blade is rigid in the hand, so the lift stays
		// in front of the shoulder. 0 is the upright carry, 0.34 is the raised tool,
		// 1 is the strike. The last part of the cut tips the wrist so the head bites.
		const float Chop = FMath::Clamp(MeleeChopAlpha, 0.0f, 1.0f);
		const float Raised = 0.34f;
		const float ChopDegrees = Chop <= Raised
			? FMath::Lerp(0.0f, 72.0f, Chop / Raised)
			: FMath::Lerp(72.0f, -58.0f, (Chop - Raised) / (1.0f - Raised));
		const float Bite = FMath::Clamp((Chop - 0.72f) / 0.28f, 0.0f, 1.0f);
		const float SwingDegrees = ChopDegrees - Bite * Bite * 24.0f;
		const FVector FoldR = (PoseForward * 0.90f - PoseRight * 0.28f).GetSafeNormal();
		// Positive degrees lift the forearm. The opposite sign drove it straight down.
		const FVector ForeAim = FQuat(PoseRight, FMath::DegreesToRadians(-SwingDegrees)).RotateVector(FoldR);
		// The upper arm stays forward of the ribs at rest and through the cut, so the
		// elbow and the tool clear the torso. A raise adds a little more reach.
		const float ShoulderLift = FMath::Clamp(ChopDegrees / 72.0f, 0.0f, 1.0f);
		const FVector UpperAim = (-PoseUp + FoldR * (0.52f + 0.20f * ShoulderLift)).GetSafeNormal();

		// Only the holding hand folds into the L. The other arm hangs the way empty hands do.
		AimSegment(TEXT("UpperArm_L"), TEXT("LowerArm_L"), -PoseUp);
		AimSegment(TEXT("LowerArm_L"), TEXT("Wrist_L"), -PoseUp);
		AimSegment(TEXT("UpperArm_R"), TEXT("LowerArm_R"), UpperAim);
		AimSegment(TEXT("LowerArm_R"), TEXT("Wrist_R"), ForeAim);
		CloseGrip(true);
	}
	else
	{
		ApplyStiffUnarmedArms(Pose, Mesh, Skeleton, PoseUp, PoseForward, PoseRight);
	}
	if (!(IsValid(Climb) && Climb->IsClimbing()) && ClimbBlendAlpha <= 0.02f)
	{
		ApplyStylizedRunStride(Pose, Mesh, Skeleton, PoseUp, PoseForward, PoseRight);
	}

	if (JumpTuckAlpha > 0.02f && ClimbBlendAlpha < 0.5f)
	{
		// Kneeling tuck for the whole jump. A turn in the air must not swap this
		// for the shuffle, or the legs snap open before the landing.
		// Foot_L/R and PT_L/R hang off Root, so a folded ankle leaves the shoe mesh
		// stretched to a foot that never left the ground. Each shoe bone moves with
		// the same delta the ankle just received.
		const float Bend = JumpTuckAlpha;
		const int32 FootR = Mesh->GetBoneIndex(TEXT("Foot_R"));
		const int32 FootL = Mesh->GetBoneIndex(TEXT("Foot_L"));
		const int32 ToeR = Mesh->GetBoneIndex(TEXT("PT_R"));
		const int32 ToeL = Mesh->GetBoneIndex(TEXT("PT_L"));
		const int32 AnkleR = Mesh->GetBoneIndex(TEXT("LowerLeg_R_end"));
		const int32 AnkleL = Mesh->GetBoneIndex(TEXT("LowerLeg_L_end"));
		const FVector RestFootR = Pose.IsValidIndex(FootR) ? Pose[FootR].GetLocation() : FVector::ZeroVector;
		const FVector RestFootL = Pose.IsValidIndex(FootL) ? Pose[FootL].GetLocation() : FVector::ZeroVector;
		const FVector RestToeR = Pose.IsValidIndex(ToeR) ? Pose[ToeR].GetLocation() : FVector::ZeroVector;
		const FVector RestToeL = Pose.IsValidIndex(ToeL) ? Pose[ToeL].GetLocation() : FVector::ZeroVector;
		const FVector RestAnkleR = Pose.IsValidIndex(AnkleR) ? Pose[AnkleR].GetLocation() : FVector::ZeroVector;
		const FVector RestAnkleL = Pose.IsValidIndex(AnkleL) ? Pose[AnkleL].GetLocation() : FVector::ZeroVector;

		const FVector ThighAim = (ActorForward * Bend
			- ActorUp * (1.0f - Bend * (1.0f - JumpThighDownBias))).GetSafeNormal();
		AimSegment(TEXT("UpperLeg_R"), TEXT("LowerLeg_R"), ThighAim);
		AimSegment(TEXT("UpperLeg_L"), TEXT("LowerLeg_L"), ThighAim);

		auto FoldShin = [&](const FName ShinName, const FName AnkleName, FQuat& OutFold)
		{
			OutFold = FQuat::Identity;
			const int32 ShinIndex = Mesh->GetBoneIndex(ShinName);
			const int32 AnkleIndex = Mesh->GetBoneIndex(AnkleName);
			if (!Pose.IsValidIndex(ShinIndex) || !Pose.IsValidIndex(AnkleIndex))
			{
				return;
			}
			const FVector Knee = Pose[ShinIndex].GetLocation();
			const FVector ShinDir = (Pose[AnkleIndex].GetLocation() - Knee).GetSafeNormal();
			if (ShinDir.IsNearlyZero())
			{
				return;
			}
			const FQuat Fold = FQuat(ActorRight, FMath::DegreesToRadians(JumpShinFoldDegrees * Bend));
			const FVector Folded = Fold.RotateVector(ShinDir);
			OutFold = FQuat::FindBetweenNormals(ShinDir, Folded);
			Pose[AnkleIndex].SetLocation(Knee + OutFold.RotateVector(Pose[AnkleIndex].GetLocation() - Knee));
			Pose[AnkleIndex].SetRotation(OutFold * Pose[AnkleIndex].GetRotation());
			Pose[AnkleIndex].NormalizeRotation();
			Pose[ShinIndex].SetRotation(OutFold * Pose[ShinIndex].GetRotation());
			Pose[ShinIndex].NormalizeRotation();
		};
		FQuat FoldR = FQuat::Identity;
		FQuat FoldL = FQuat::Identity;
		FoldShin(TEXT("LowerLeg_R"), TEXT("LowerLeg_R_end"), FoldR);
		FoldShin(TEXT("LowerLeg_L"), TEXT("LowerLeg_L_end"), FoldL);

		// The shoe bones were recorded before the shin folded. Reattach each one
		// with the same delta the ankle just received, so the shoe keeps its shape.
		auto SeatShoe = [&](const int32 ShoeIndex, const int32 AnkleIndex, const FVector& RestShoe, const FVector& RestAnkle, const FQuat& Fold)
		{
			if (!Pose.IsValidIndex(ShoeIndex) || !Pose.IsValidIndex(AnkleIndex))
			{
				return;
			}
			Pose[ShoeIndex].SetLocation(Pose[AnkleIndex].GetLocation() + Fold.RotateVector(RestShoe - RestAnkle));
			Pose[ShoeIndex].SetRotation(Fold * Pose[ShoeIndex].GetRotation());
			Pose[ShoeIndex].NormalizeRotation();
		};
		SeatShoe(FootR, AnkleR, RestFootR, RestAnkleR, FoldR);
		SeatShoe(FootL, AnkleL, RestFootL, RestAnkleL, FoldL);
		SeatShoe(ToeR, AnkleR, RestToeR, RestAnkleR, FoldR);
		SeatShoe(ToeL, AnkleL, RestToeL, RestAnkleL, FoldL);
	}
	else if (TurnShuffleAlpha >= 0.02f && GaitBlend <= 0.02f)
	{
		const AJTSCharacter* const TurningCharacter = Cast<AJTSCharacter>(Mesh->GetOwner());
		const float TurnSign = IsValid(TurningCharacter) ? TurningCharacter->GetTurnDirectionSign() : 1.0f;
		const auto StepLeg = [&TurnBone, TurnSign, this](const FName Thigh, const FName Shin, float LiftDegrees, float SideSign)
		{
			TurnBone(Thigh, FVector::RightVector, -LiftDegrees);
			TurnBone(Thigh, FVector::UpVector, SideSign * LiftDegrees * 0.35f * TurnSign);
			TurnBone(Thigh, FVector::ForwardVector, SideSign * 4.0f * TurnShuffleAlpha);
			TurnBone(Shin, FVector::RightVector, LiftDegrees * 0.85f);
		};
		StepLeg(TEXT("UpperLeg_L"), TEXT("LowerLeg_L"), ShuffleLeftLift, -1.0f);
		StepLeg(TEXT("UpperLeg_R"), TEXT("LowerLeg_R"), ShuffleRightLift, 1.0f);
	}

	// Rebuild component space so a rotated shoulder carries the forearm and hand with it.
	// Do not call ApplyEditedComponentSpaceTransforms here. In the editor it flips the double
	// buffer and, because it clears bHasValidBoneTransform first, copies the unedited pose
	// back over this write. The following FinalizeBoneTransform publishes the edited buffer.
	if (Skeleton != nullptr)
	{
		TArray<FTransform> LocalPose;
		LocalPose.SetNum(Pose.Num());
		for (int32 BoneIndex = 0; BoneIndex < Pose.Num(); ++BoneIndex)
		{
			const int32 ParentIndex = Skeleton->GetParentIndex(BoneIndex);
			if (ParentIndex < 0 || !Pose.IsValidIndex(ParentIndex))
			{
				LocalPose[BoneIndex] = Pose[BoneIndex];
				continue;
			}
			LocalPose[BoneIndex] = Pose[BoneIndex].GetRelativeTransform(Pose[ParentIndex]);
		}
		for (int32 BoneIndex = 0; BoneIndex < Pose.Num(); ++BoneIndex)
		{
			const int32 ParentIndex = Skeleton->GetParentIndex(BoneIndex);
			if (ParentIndex < 0 || !Pose.IsValidIndex(ParentIndex))
			{
				Pose[BoneIndex] = LocalPose[BoneIndex];
				continue;
			}
			Pose[BoneIndex] = LocalPose[BoneIndex] * Pose[ParentIndex];
		}
	}

}

namespace
{
	float StiffSmooth(const float Value)
	{
		const float Clamped = FMath::Clamp(Value, 0.0f, 1.0f);
		return Clamped * Clamped * (3.0f - 2.0f * Clamped);
	}

	void StiffAimSegment(
		TArray<FTransform>& Pose,
		const USkeletalMeshComponent* Mesh,
		const FReferenceSkeleton* Skeleton,
		const FName BoneName,
		const FName ChildName,
		const FVector& Aim)
	{
		const int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
		const int32 ChildIndex = Mesh->GetBoneIndex(ChildName);
		if (Skeleton == nullptr || !Pose.IsValidIndex(BoneIndex) || !Pose.IsValidIndex(ChildIndex))
		{
			return;
		}
		const FVector AimDir = Aim.GetSafeNormal();
		const FVector Origin = Pose[BoneIndex].GetLocation();
		const FVector Dir = (Pose[ChildIndex].GetLocation() - Origin).GetSafeNormal();
		if (Dir.IsNearlyZero() || AimDir.IsNearlyZero() || FVector::DotProduct(Dir, AimDir) > 0.999f)
		{
			return;
		}
		const FQuat Delta = FQuat::FindBetweenNormals(Dir, AimDir);
		for (int32 Index = 0; Index < Pose.Num(); ++Index)
		{
			int32 Walk = Index;
			bool bUnder = false;
			while (Walk >= 0)
			{
				if (Walk == BoneIndex)
				{
					bUnder = true;
					break;
				}
				Walk = Skeleton->GetParentIndex(Walk);
			}
			if (!bUnder)
			{
				continue;
			}
			Pose[Index].SetLocation(Origin + Delta.RotateVector(Pose[Index].GetLocation() - Origin));
			Pose[Index].SetRotation(Delta * Pose[Index].GetRotation());
			Pose[Index].NormalizeRotation();
		}
	}

	void StiffCurlFingers(
		TArray<FTransform>& Pose,
		const USkeletalMeshComponent* Mesh,
		const FReferenceSkeleton* Skeleton,
		const bool bRightHand,
		const float Amount)
	{
		if (Skeleton == nullptr || Amount <= 0.01f)
		{
			return;
		}
		const TCHAR* const Suffix = bRightHand ? TEXT("_R") : TEXT("_L");
		const int32 IndexKnuckle = Mesh->GetBoneIndex(*FString::Printf(TEXT("Index2%s"), Suffix));
		const int32 MiddleRoot = Mesh->GetBoneIndex(*FString::Printf(TEXT("Middle1%s"), Suffix));
		const int32 MiddleKnuckle = Mesh->GetBoneIndex(*FString::Printf(TEXT("Middle2%s"), Suffix));
		const int32 PinkyKnuckle = Mesh->GetBoneIndex(*FString::Printf(TEXT("Pinky2%s"), Suffix));
		const int32 ThumbKnuckle = Mesh->GetBoneIndex(*FString::Printf(TEXT("Thumb2%s"), Suffix));
		if (!Pose.IsValidIndex(IndexKnuckle) || !Pose.IsValidIndex(MiddleRoot)
			|| !Pose.IsValidIndex(MiddleKnuckle) || !Pose.IsValidIndex(PinkyKnuckle)
			|| !Pose.IsValidIndex(ThumbKnuckle))
		{
			return;
		}
		// All four first finger bones share one position on this mesh. The actual
		// knuckle spread starts at bone 2, so use that spread to find the palm.
		const FVector KnuckleSpan = Pose[IndexKnuckle].GetLocation() - Pose[PinkyKnuckle].GetLocation();
		const FVector FingerOut = Pose[MiddleKnuckle].GetLocation() - Pose[MiddleRoot].GetLocation();
		FVector PalmInside = FVector::CrossProduct(KnuckleSpan, FingerOut).GetSafeNormal();
		if (PalmInside.IsNearlyZero())
		{
			return;
		}
		if (FVector::DotProduct(PalmInside, Pose[ThumbKnuckle].GetLocation() - Pose[MiddleKnuckle].GetLocation()) < 0.0f)
		{
			PalmInside *= -1.0f;
		}

		auto RotateBranch = [&](const int32 BoneIndex, const FVector& Axis, const float Degrees)
		{
			if (!Pose.IsValidIndex(BoneIndex) || Axis.IsNearlyZero())
			{
				return;
			}
			const FQuat Curl(Axis, FMath::DegreesToRadians(Degrees * Amount));
			const FVector Origin = Pose[BoneIndex].GetLocation();
			for (int32 Index = 0; Index < Pose.Num(); ++Index)
			{
				int32 Walk = Index;
				while (Walk >= 0 && Walk != BoneIndex)
				{
					Walk = Skeleton->GetParentIndex(Walk);
				}
				if (Walk != BoneIndex)
				{
					continue;
				}
				Pose[Index].SetLocation(Origin + Curl.RotateVector(Pose[Index].GetLocation() - Origin));
				Pose[Index].SetRotation(Curl * Pose[Index].GetRotation());
				Pose[Index].NormalizeRotation();
			}
		};
		auto CurlFinger = [&](const TCHAR* Prefix, const float KnuckleDegrees, const float TipDegrees)
		{
			const int32 Root = Mesh->GetBoneIndex(*FString::Printf(TEXT("%s1%s"), Prefix, Suffix));
			const int32 Knuckle = Mesh->GetBoneIndex(*FString::Printf(TEXT("%s2%s"), Prefix, Suffix));
			const int32 Middle = Mesh->GetBoneIndex(*FString::Printf(TEXT("%s3%s"), Prefix, Suffix));
			if (!Pose.IsValidIndex(Root) || !Pose.IsValidIndex(Knuckle) || !Pose.IsValidIndex(Middle))
			{
				return;
			}
			const FVector FingerOutward = (Pose[Knuckle].GetLocation() - Pose[Root].GetLocation()).GetSafeNormal();
			const FVector CurlAxis = FVector::CrossProduct(FingerOutward, PalmInside).GetSafeNormal();
			RotateBranch(Root, CurlAxis, 8.0f);
			RotateBranch(Knuckle, CurlAxis, KnuckleDegrees);
			RotateBranch(Middle, CurlAxis, TipDegrees);
		};
		CurlFinger(TEXT("Index"), 80.0f, 65.0f);
		CurlFinger(TEXT("Middle"), 82.0f, 67.0f);
		CurlFinger(TEXT("Ring"), 80.0f, 65.0f);
		CurlFinger(TEXT("Pinky"), 76.0f, 62.0f);

		const int32 ThumbRoot = Mesh->GetBoneIndex(*FString::Printf(TEXT("Thumb1%s"), Suffix));
		const int32 ThumbTip = Mesh->GetBoneIndex(*FString::Printf(TEXT("Thumb3%s"), Suffix));
		if (Pose.IsValidIndex(ThumbRoot) && Pose.IsValidIndex(ThumbTip))
		{
			const FVector ThumbOut = (Pose[ThumbKnuckle].GetLocation() - Pose[ThumbRoot].GetLocation()).GetSafeNormal();
			const FVector TowardPalm = (Pose[MiddleKnuckle].GetLocation() - Pose[ThumbRoot].GetLocation()).GetSafeNormal();
			RotateBranch(ThumbRoot, FVector::CrossProduct(ThumbOut, TowardPalm).GetSafeNormal(), 25.0f);
			const FVector ThumbEnd = (Pose[ThumbTip].GetLocation() - Pose[ThumbKnuckle].GetLocation()).GetSafeNormal();
			const FVector TowardFingers = (Pose[MiddleKnuckle].GetLocation() - Pose[ThumbKnuckle].GetLocation()).GetSafeNormal();
			RotateBranch(ThumbKnuckle, FVector::CrossProduct(ThumbEnd, TowardFingers).GetSafeNormal(), 45.0f);
		}
	}
}

void UJTSAnimInstance::ApplyStylizedRunStride(
	TArray<FTransform>& Pose,
	USkeletalMeshComponent* Mesh,
	const FReferenceSkeleton* Skeleton,
	const FVector& PoseUp,
	const FVector& PoseForward,
	const FVector& PoseRight)
{
	const float RunAlpha = FMath::Clamp(GaitBlend - 1.0f, 0.0f, 1.0f);
	if (RunAlpha <= 0.01f || JumpTuckAlpha > 0.02f || Skeleton == nullptr)
	{
		return;
	}

	const int32 LeftAnkle = Mesh->GetBoneIndex(TEXT("LowerLeg_L_end"));
	const int32 RightAnkle = Mesh->GetBoneIndex(TEXT("LowerLeg_R_end"));
	if (!Pose.IsValidIndex(LeftAnkle) || !Pose.IsValidIndex(RightAnkle))
	{
		return;
	}
	const FVector LeftAnkleStart = Pose[LeftAnkle].GetLocation();
	const FVector RightAnkleStart = Pose[RightAnkle].GetLocation();

	auto IsUnder = [&](const int32 BoneIndex, const int32 AncestorIndex)
	{
		int32 Parent = BoneIndex;
		while (Parent >= 0 && Parent != AncestorIndex)
		{
			Parent = Skeleton->GetParentIndex(Parent);
		}
		return Parent == AncestorIndex;
	};
	const int32 Body = Mesh->GetBoneIndex(TEXT("Body"));
	const int32 Hips = Mesh->GetBoneIndex(TEXT("Hips"));
	const int32 LeftThigh = Mesh->GetBoneIndex(TEXT("UpperLeg_L"));
	const int32 RightThigh = Mesh->GetBoneIndex(TEXT("UpperLeg_R"));
	if (!Pose.IsValidIndex(Body) || !Pose.IsValidIndex(Hips)
		|| !Pose.IsValidIndex(LeftThigh) || !Pose.IsValidIndex(RightThigh)
		|| !IsUnder(Hips, Body) || !IsUnder(LeftThigh, Body) || !IsUnder(RightThigh, Body))
	{
		return;
	}
	// The butt, torso and both leg roots are children of Body. Move them as one
	// rigid block so the upper-body length stays the same, then solve each leg
	// back to its authored ankle target. The shoes are separate Root children.
	const float LagPulse = 0.90f + 0.10f * FMath::Cos(GaitPhase * 2.0f * TWO_PI);
	const FVector DesiredBodyOffset = -PoseForward * (RunPelvisBackOffsetCm * RunAlpha * LagPulse)
		- PoseUp * (RunPelvisDropCm * RunAlpha);
	auto CanReachAnkles = [&](const float OffsetScale)
	{
		const FVector Offset = DesiredBodyOffset * OffsetScale;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const bool bRight = Side == 1;
			const int32 Thigh = bRight ? RightThigh : LeftThigh;
			const int32 Knee = Mesh->GetBoneIndex(bRight ? TEXT("LowerLeg_R") : TEXT("LowerLeg_L"));
			const int32 Ankle = bRight ? RightAnkle : LeftAnkle;
			if (!Pose.IsValidIndex(Knee))
			{
				return false;
			}
			const float UpperLength = FVector::Distance(Pose[Thigh].GetLocation(), Pose[Knee].GetLocation());
			const float LowerLength = FVector::Distance(Pose[Knee].GetLocation(), Pose[Ankle].GetLocation());
			const FVector AnkleGoal = bRight ? RightAnkleStart : LeftAnkleStart;
			const float GoalDistance = FVector::Distance(Pose[Thigh].GetLocation() + Offset, AnkleGoal);
			if (GoalDistance <= FMath::Abs(UpperLength - LowerLength) + 0.5f
				|| GoalDistance >= UpperLength + LowerLength - 0.5f)
			{
				return false;
			}
		}
		return true;
	};
	float BodyOffsetScale = 1.0f;
	if (!CanReachAnkles(BodyOffsetScale))
	{
		float Low = 0.0f;
		float High = 1.0f;
		for (int32 Step = 0; Step < 8; ++Step)
		{
			const float Mid = 0.5f * (Low + High);
			if (CanReachAnkles(Mid))
			{
				Low = Mid;
			}
			else
			{
				High = Mid;
			}
		}
		BodyOffsetScale = Low;
	}
	const FVector BodyOffset = DesiredBodyOffset * BodyOffsetScale;
	for (int32 Index = 0; Index < Pose.Num(); ++Index)
	{
		if (IsUnder(Index, Body))
		{
			Pose[Index].AddToTranslation(BodyOffset);
		}
	}
	auto RotateBranch = [&](const int32 RootIndex, const FVector& From, const FVector& To, const FQuat& Rotation)
	{
		for (int32 Index = 0; Index < Pose.Num(); ++Index)
		{
			if (!IsUnder(Index, RootIndex))
			{
				continue;
			}
			Pose[Index].SetLocation(To + Rotation.RotateVector(Pose[Index].GetLocation() - From));
			Pose[Index].SetRotation(Rotation * Pose[Index].GetRotation());
			Pose[Index].NormalizeRotation();
		}
	};
	auto CarryShoe = [&](const bool bRight, const int32 Thigh, const FVector& OldAnkle, const FVector& NewAnkle, const FQuat& Rotation)
	{
		// On Casual_2, Foot and PT are root children rather than shin children.
		const int32 Foot = Mesh->GetBoneIndex(bRight ? TEXT("Foot_R") : TEXT("Foot_L"));
		const int32 Toe = Mesh->GetBoneIndex(bRight ? TEXT("PT_R") : TEXT("PT_L"));
		if (Pose.IsValidIndex(Foot) && !IsUnder(Foot, Thigh))
		{
			RotateBranch(Foot, OldAnkle, NewAnkle, Rotation);
		}
		if (Pose.IsValidIndex(Toe) && !IsUnder(Toe, Thigh) && (!Pose.IsValidIndex(Foot) || !IsUnder(Toe, Foot)))
		{
			RotateBranch(Toe, OldAnkle, NewAnkle, Rotation);
		}
	};
	auto SolveLegToAnkle = [&](const bool bRight, const FVector& AnkleGoal, const bool bMoveShoe)
	{
		const int32 Thigh = bRight ? RightThigh : LeftThigh;
		const int32 Knee = Mesh->GetBoneIndex(bRight ? TEXT("LowerLeg_R") : TEXT("LowerLeg_L"));
		const int32 Ankle = bRight ? RightAnkle : LeftAnkle;
		if (!Pose.IsValidIndex(Thigh) || !Pose.IsValidIndex(Knee))
		{
			return false;
		}
		const FVector HipPosition = Pose[Thigh].GetLocation();
		const FVector KneePosition = Pose[Knee].GetLocation();
		const FVector OldAnkle = Pose[Ankle].GetLocation();
		const float UpperLength = FVector::Distance(HipPosition, KneePosition);
		const float LowerLength = FVector::Distance(KneePosition, OldAnkle);
		const FVector HipToGoal = AnkleGoal - HipPosition;
		const float GoalDistance = HipToGoal.Size();
		if (UpperLength < 1.0f || LowerLength < 1.0f
			|| GoalDistance <= FMath::Abs(UpperLength - LowerLength) + 0.5f
			|| GoalDistance >= UpperLength + LowerLength - 0.5f)
		{
			return false;
		}
		const FVector GoalDirection = HipToGoal / GoalDistance;
		FVector KneePole = (KneePosition - HipPosition
			- GoalDirection * FVector::DotProduct(KneePosition - HipPosition, GoalDirection)).GetSafeNormal();
		if (KneePole.IsNearlyZero())
		{
			KneePole = (PoseForward - GoalDirection * FVector::DotProduct(PoseForward, GoalDirection)).GetSafeNormal();
			if (KneePole.IsNearlyZero())
			{
				KneePole = (PoseRight - GoalDirection * FVector::DotProduct(PoseRight, GoalDirection)).GetSafeNormal();
				if (KneePole.IsNearlyZero())
				{
					return false;
				}
			}
		}
		const float Along = (UpperLength * UpperLength + GoalDistance * GoalDistance - LowerLength * LowerLength)
			/ (2.0f * GoalDistance);
		const float Outward = FMath::Sqrt(FMath::Max(0.0f, UpperLength * UpperLength - Along * Along));
		const FVector KneeGoal = HipPosition + GoalDirection * Along + KneePole * Outward;
		const FQuat UpperTurn = FQuat::FindBetweenNormals(
			(KneePosition - HipPosition).GetSafeNormal(), (KneeGoal - HipPosition).GetSafeNormal());
		RotateBranch(Thigh, HipPosition, HipPosition, UpperTurn);
		const FVector NewKnee = Pose[Knee].GetLocation();
		const FQuat LowerTurn = FQuat::FindBetweenNormals(
			(Pose[Ankle].GetLocation() - NewKnee).GetSafeNormal(), (AnkleGoal - NewKnee).GetSafeNormal());
		RotateBranch(Knee, NewKnee, NewKnee, LowerTurn);
		if (bMoveShoe)
		{
			// Preserve the authored shoe orientation while the contact advances.
			CarryShoe(bRight, Thigh, OldAnkle, Pose[Ankle].GetLocation(), FQuat::Identity);
		}
		return true;
	};
	// The relocated pelvis bends both legs around the original ankle positions;
	// grounded shoes stay on the floor and the airborne shoe keeps its height.
	SolveLegToAnkle(false, LeftAnkleStart, false);
	SolveLegToAnkle(true, RightAnkleStart, false);
	auto AdvanceRearContact = [&](const bool bRight)
	{
		const int32 Thigh = bRight ? RightThigh : LeftThigh;
		const int32 Ankle = bRight ? RightAnkle : LeftAnkle;
		const FVector HipPosition = Pose[Thigh].GetLocation();
		const FVector OldAnkle = Pose[Ankle].GetLocation();
		// Judge the rear contact against the authored hip position. The stylized
		// pelvis setback must not turn off this small foot placement correction.
		const float BehindHip = FVector::DotProduct(HipPosition - BodyOffset - OldAnkle, PoseForward);
		const float OtherFootAbove = FVector::DotProduct(
			(bRight ? LeftAnkleStart - RightAnkleStart : RightAnkleStart - LeftAnkleStart), PoseUp);
		// Only adjust the low foot near the end of support. The airborne rear leg
		// keeps the authored toe-off path, and the leading foot keeps its landing.
		const float RearWeight = StiffSmooth((BehindHip - 1.0f) / 16.0f)
			* StiffSmooth((OtherFootAbove - 18.0f) / 20.0f);
		const float Advance = RunRearFootAdvanceCm * RunAlpha * RearWeight;
		if (Advance > 0.05f)
		{
			SolveLegToAnkle(bRight, OldAnkle + PoseForward * Advance, true);
		}
	};
	auto AccentLeg = [&](const bool bRight)
	{
		const int32 Thigh = Mesh->GetBoneIndex(bRight ? TEXT("UpperLeg_R") : TEXT("UpperLeg_L"));
		const int32 Knee = Mesh->GetBoneIndex(bRight ? TEXT("LowerLeg_R") : TEXT("LowerLeg_L"));
		const int32 Ankle = bRight ? RightAnkle : LeftAnkle;
		if (!Pose.IsValidIndex(Thigh) || !Pose.IsValidIndex(Knee))
		{
			return;
		}
		const FVector HipPosition = Pose[Thigh].GetLocation();
		const FVector ThighDirection = (Pose[Knee].GetLocation() - HipPosition).GetSafeNormal();
		const float ForwardSwing = FVector::DotProduct(ThighDirection, PoseForward);
		const float HeightDifference = FVector::DotProduct(
			(bRight ? RightAnkleStart - LeftAnkleStart : LeftAnkleStart - RightAnkleStart), PoseUp);
		// Accent the forward swing while its foot is raised. Leave the low support
		// foot on the authored contact path so the runner still pushes off the floor.
		const float SwingWeight = FMath::Clamp((ForwardSwing - 0.15f) / 0.45f, 0.0f, 1.0f);
		const float AirWeight = FMath::Clamp((HeightDifference - 2.0f) / 12.0f, 0.0f, 1.0f);
		const float Accent = RunStrideAccentDegrees * RunAlpha * SwingWeight * AirWeight;
		if (Accent <= 0.05f)
		{
			return;
		}

		const FVector OldAnklePosition = Pose[Ankle].GetLocation();
		const FQuat Pitch(PoseRight, FMath::DegreesToRadians(-Accent));
		RotateBranch(Thigh, HipPosition, HipPosition, Pitch);
		const FVector NewAnklePosition = Pose[Ankle].GetLocation();
		CarryShoe(bRight, Thigh, OldAnklePosition, NewAnklePosition, Pitch);
	};
	AdvanceRearContact(false);
	AdvanceRearContact(true);
	AccentLeg(false);
	AccentLeg(true);
}

void UJTSAnimInstance::ApplyStiffUnarmedArms(
	TArray<FTransform>& Pose,
	USkeletalMeshComponent* Mesh,
	const FReferenceSkeleton* Skeleton,
	const FVector& PoseUp,
	const FVector& PoseForward,
	const FVector& PoseRight)
{
	const AJTSCharacter* const Character = Cast<AJTSCharacter>(Mesh->GetOwner());
	const UJTSMeleeComponent* const Melee = IsValid(Character)
		? Character->FindComponentByClass<UJTSMeleeComponent>()
		: nullptr;
	const bool bPunching = IsValid(Melee) && Melee->IsPunchVisualActive();
	const float PunchElapsed = bPunching ? Melee->GetPunchVisualElapsed() : 0.0f;
	const bool bPunchLeft = bPunching && Melee->IsCurrentPunchLeft();
	const bool bPunchCombo = bPunching && Melee->IsContinuingUnarmedCombo();
	const float PunchHitDelay = bPunching ? Melee->GetUnarmedPunchHitDelay() : 0.11f;
	const float PunchChainDelay = bPunching ? Melee->GetUnarmedPunchChainDelay() : 0.18f;
	const float PunchRecoveryDelay = bPunching ? Melee->GetUnarmedPunchRecoveryDelay() : 0.35f;
	// A follow-up starts while the first fist is still returning. Reconstruct
	// the outgoing hand at the chain boundary so its wrist and shoulder stay
	// continuous when the component starts the next legal attack segment.
	const float GuardTime = FMath::Min(0.035f, PunchHitDelay * 0.32f);
	const float ContactHoldEnd = FMath::Min(PunchChainDelay, PunchHitDelay + 0.018f);
	const float RetractEnd = FMath::Max(ContactHoldEnd + 0.01f,
		FMath::Min(PunchRecoveryDelay - 0.10f, PunchChainDelay + 0.075f));
	const float PreviousAtChain = 1.0f - StiffSmooth(FMath::Clamp(
		(PunchChainDelay - ContactHoldEnd) / (RetractEnd - ContactHoldEnd), 0.0f, 1.0f));
	const float PreviousExtend = bPunchCombo
		? PreviousAtChain * (1.0f - StiffSmooth(FMath::Clamp(PunchElapsed / 0.10f, 0.0f, 1.0f)))
		: 0.0f;
	const float GuardRise = bPunchCombo ? 1.0f
		: bPunching ? FMath::Clamp(PunchElapsed / GuardTime, 0.0f, 1.0f) : 0.0f;
	const float StrikeStart = bPunchCombo ? 0.0f : GuardTime;
	const float Strike = bPunching
		? FMath::Clamp((PunchElapsed - StrikeStart) / FMath::Max(0.01f, PunchHitDelay - StrikeStart), 0.0f, 1.0f)
		: 0.0f;
	const float StrikeStartExtend = bPunchCombo ? -0.12f * PreviousAtChain : -0.16f;
	const float PunchExtend = !bPunching ? 0.0f
		: !bPunchCombo && PunchElapsed < GuardTime ? -0.16f * GuardRise
		: PunchElapsed < PunchHitDelay ? FMath::Lerp(StrikeStartExtend, 1.0f, FMath::Pow(Strike, 1.45f))
		: PunchElapsed < ContactHoldEnd ? 1.0f
		: 1.0f - StiffSmooth(FMath::Clamp(
			(PunchElapsed - ContactHoldEnd) / (RetractEnd - ContactHoldEnd), 0.0f, 1.0f));
	const float FadeStart = FMath::Max(PunchChainDelay, PunchRecoveryDelay - 0.10f);
	const float PunchFade = bPunching && PunchElapsed > FadeStart
		? 1.0f - StiffSmooth(FMath::Clamp((PunchElapsed - FadeStart) / (PunchRecoveryDelay - FadeStart), 0.0f, 1.0f))
		: 1.0f;
	const float PunchEnter = bPunchCombo ? 1.0f : StiffSmooth(GuardRise);
	const float PunchCover = bPunching ? PunchFade * PunchEnter : 0.0f;
	const float PunchDrive = FMath::Clamp(PunchExtend, 0.0f, 1.0f) * PunchCover;
	const float PreviousDrive = PreviousExtend * PunchCover;
	const float TorsoDrive = PunchDrive - PreviousDrive;
	const float PunchCoil = bPunchCombo ? 0.0f : GuardRise * (1.0f - Strike) * PunchCover;
	const FQuat PunchAim(PoseRight, FMath::DegreesToRadians(-FMath::Clamp(IsValid(Character) ? Character->GetAimPitch() : 0.0f, -55.0f, 55.0f)));
	const FVector PunchForward = PunchAim.RotateVector(PoseForward);
	const FVector PunchUp = PunchAim.RotateVector(PoseUp);
	const float Run = FMath::Clamp(GaitBlend - 1.0f, 0.0f, 1.0f);
	const float Walk = FMath::Clamp(GaitBlend, 0.0f, 1.0f) * (1.0f - Run);
	// The source Run clip starts on the opposite support leg from Walk.
	const float Phase = GaitPhase * TWO_PI + PI * Run;

	auto PlaceArm = [&](const bool bRight)
	{
		// Walking holds both arms at the sides without a gait swing. Running uses a
		// broad horizontal upper-arm silhouette with both forearms hanging down.
		FVector WalkUpper = -PoseUp;
		const float WalkAbduction = 12.0f + 2.0f * Walk;
		WalkUpper = FQuat(PoseForward, FMath::DegreesToRadians(bRight ? WalkAbduction : -WalkAbduction)).RotateVector(WalkUpper).GetSafeNormal();
		FVector BendAxis = FVector::CrossProduct(WalkUpper, PoseForward).GetSafeNormal();
		if (BendAxis.IsNearlyZero())
		{
			BendAxis = PoseRight;
		}
		FVector WalkFore = FQuat(BendAxis, FMath::DegreesToRadians(-8.0f)).RotateVector(WalkUpper);
		if (FVector::DotProduct(WalkFore, PoseForward) < FVector::DotProduct(WalkUpper, PoseForward))
		{
			WalkFore = FQuat(BendAxis, FMath::DegreesToRadians(8.0f)).RotateVector(WalkUpper);
		}
		const FVector Side = bRight ? PoseRight : -PoseRight;
		const float Spread = FMath::DegreesToRadians(RunArmSpreadDegrees);
		const FVector RunUpper = (Side * FMath::Sin(Spread)
			- PoseUp * FMath::Cos(Spread)
			+ PoseForward * (0.08f + (bRight ? 1.0f : -1.0f) * FMath::Cos(Phase) * 0.06f)).GetSafeNormal();
		const FVector RunFore = -PoseUp;
		const FVector Upper = FMath::Lerp(WalkUpper, RunUpper, Run).GetSafeNormal();
		const FVector Fore = FMath::Lerp(WalkFore.GetSafeNormal(), RunFore, Run).GetSafeNormal();
		const FString Suffix = bRight ? TEXT("_R") : TEXT("_L");
		StiffAimSegment(Pose, Mesh, Skeleton, *FString::Printf(TEXT("UpperArm%s"), *Suffix), *FString::Printf(TEXT("LowerArm%s"), *Suffix), Upper);
		StiffAimSegment(Pose, Mesh, Skeleton, *FString::Printf(TEXT("LowerArm%s"), *Suffix), *FString::Printf(TEXT("Wrist%s"), *Suffix), Fore.GetSafeNormal());
	};

	PlaceArm(false);
	PlaceArm(true);

	if (PunchCover > 0.01f)
	{
		// The strike starts in the waist and shoulder, while the opposite arm
		// stays near the face. Rotate only the upper body so planted feet and the
		// walking cadence keep their existing motion.
		auto RotateTorsoBranch = [&](const FName BoneName, const FVector& Axis, const float Degrees)
		{
			const int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
			if (!Pose.IsValidIndex(BoneIndex) || Axis.IsNearlyZero() || FMath::IsNearlyZero(Degrees))
			{
				return;
			}
			const FVector Origin = Pose[BoneIndex].GetLocation();
			const FQuat Rotation(Axis, FMath::DegreesToRadians(Degrees));
			for (int32 Index = 0; Index < Pose.Num(); ++Index)
			{
				int32 Parent = Index;
				while (Parent >= 0 && Parent != BoneIndex)
				{
					Parent = Skeleton->GetParentIndex(Parent);
				}
				if (Parent != BoneIndex)
				{
					continue;
				}
				Pose[Index].SetLocation(Origin + Rotation.RotateVector(Pose[Index].GetLocation() - Origin));
				Pose[Index].SetRotation(Rotation * Pose[Index].GetRotation());
				Pose[Index].NormalizeRotation();
			}
		};
		const float LeadSign = bPunchLeft ? 1.0f : -1.0f;
		RotateTorsoBranch(TEXT("Abdomen"), PoseRight,
			FMath::Max(PunchDrive, PreviousDrive) * 6.0f - PunchCoil * 1.5f);
		RotateTorsoBranch(TEXT("Abdomen"), PoseUp,
			LeadSign * (TorsoDrive * 6.0f - PunchCoil * 2.0f));
		RotateTorsoBranch(TEXT("Chest"), PoseUp,
			LeadSign * (TorsoDrive * 11.0f - PunchCoil * 3.0f));

		// Shoulder_L/R are real clavicle parents on Casual_2. Bring the lead
		// shoulder forward with the trunk instead of asking a fully extended
		// elbow to create reach it cannot anatomically provide. The branch
		// translation keeps the upper arm and wrist lengths unchanged.
		auto ProtractShoulder = [&](const bool bRight)
		{
			const FName ShoulderName = bRight ? TEXT("Shoulder_R") : TEXT("Shoulder_L");
			const int32 ShoulderIndex = Mesh->GetBoneIndex(ShoulderName);
			const int32 UpperIndex = Mesh->GetBoneIndex(bRight ? TEXT("UpperArm_R") : TEXT("UpperArm_L"));
			const int32 LowerIndex = Mesh->GetBoneIndex(bRight ? TEXT("LowerArm_R") : TEXT("LowerArm_L"));
			const int32 WristIndex = Mesh->GetBoneIndex(bRight ? TEXT("Wrist_R") : TEXT("Wrist_L"));
			if (!Pose.IsValidIndex(ShoulderIndex) || !Pose.IsValidIndex(UpperIndex)
				|| !Pose.IsValidIndex(LowerIndex) || !Pose.IsValidIndex(WristIndex))
			{
				return;
			}
			const float ArmScale = (FVector::Distance(Pose[UpperIndex].GetLocation(), Pose[LowerIndex].GetLocation())
				+ FVector::Distance(Pose[LowerIndex].GetLocation(), Pose[WristIndex].GetLocation())) / 41.0f;
			const bool bLeadShoulder = bRight != bPunchLeft;
			const float LeadAmount = bLeadShoulder
				? 5.0f * PunchDrive - 1.5f * PunchCoil : 5.0f * PreviousDrive;
			const FVector Delta = PoseForward * (LeadAmount * ArmScale);
			for (int32 Index = 0; Index < Pose.Num(); ++Index)
			{
				int32 Parent = Index;
				while (Parent >= 0 && Parent != ShoulderIndex)
				{
					Parent = Skeleton->GetParentIndex(Parent);
				}
				if (Parent == ShoulderIndex)
				{
					Pose[Index].AddToTranslation(Delta);
				}
			}
		};
		ProtractShoulder(false);
		ProtractShoulder(true);

		auto PunchArm = [&](const bool bRight, const float Extend)
		{
			const FString Suffix = bRight ? TEXT("_R") : TEXT("_L");
			const int32 UpperIndex = Mesh->GetBoneIndex(*FString::Printf(TEXT("UpperArm%s"), *Suffix));
			const int32 LowerIndex = Mesh->GetBoneIndex(*FString::Printf(TEXT("LowerArm%s"), *Suffix));
			const int32 WristIndex = Mesh->GetBoneIndex(*FString::Printf(TEXT("Wrist%s"), *Suffix));
			if (!Pose.IsValidIndex(UpperIndex) || !Pose.IsValidIndex(LowerIndex) || !Pose.IsValidIndex(WristIndex))
			{
				return;
			}
			const FVector Shoulder = Pose[UpperIndex].GetLocation();
			const FVector RestElbow = Pose[LowerIndex].GetLocation();
			const FVector RestWrist = Pose[WristIndex].GetLocation();
			const float UpperLength = FVector::Distance(Shoulder, RestElbow);
			const float ForeLength = FVector::Distance(RestElbow, RestWrist);
			if (UpperLength < 1.0f || ForeLength < 1.0f)
			{
				return;
			}
			const FVector Side = bRight ? PoseRight : -PoseRight;
			const float ArmScale = (UpperLength + ForeLength) / 41.0f;
			// The guarded fist remains in front of the shoulder. A combo moves the
			// previous fist back along this line while the other one extends.
			const FVector GuardOffset = PunchForward * (FMath::Lerp(22.0f, 39.0f, Extend) * ArmScale)
				+ Side * (4.0f * ArmScale) - PunchUp * (8.0f * ArmScale);
			// Raise both fists together during a chain. Ease in across the first
			// part of the next strike so the previous punch does not pop upward.
			const float ComboLift = bPunchCombo
				? PunchComboArmLiftDegrees * StiffSmooth(FMath::Clamp(PunchElapsed / 0.08f, 0.0f, 1.0f))
				: 0.0f;
			const FQuat ComboLiftRotation(PoseRight, FMath::DegreesToRadians(-ComboLift));
			const FVector GuardToPunch = Shoulder + ComboLiftRotation.RotateVector(GuardOffset);
			const FVector BlendedWrist = FMath::Lerp(RestWrist, GuardToPunch, PunchCover);
			const FVector WristOffset = BlendedWrist - Shoulder;
			const float WristDistance = FMath::Clamp(WristOffset.Size(),
				FMath::Abs(UpperLength - ForeLength) + 0.5f, UpperLength + ForeLength - 0.5f);
			const FVector WristDirection = WristOffset.GetSafeNormal();
			if (WristDirection.IsNearlyZero())
			{
				return;
			}
			const FVector WristTarget = Shoulder + WristDirection * WristDistance;
			FVector ElbowPole = (Side - WristDirection * FVector::DotProduct(Side, WristDirection)).GetSafeNormal();
			if (ElbowPole.IsNearlyZero())
			{
				ElbowPole = (-PoseUp - WristDirection * FVector::DotProduct(-PoseUp, WristDirection)).GetSafeNormal();
			}
			const float Along = (UpperLength * UpperLength + WristDistance * WristDistance - ForeLength * ForeLength)
				/ (2.0f * WristDistance);
			const float Outward = FMath::Sqrt(FMath::Max(0.0f, UpperLength * UpperLength - Along * Along));
			const FVector ElbowTarget = Shoulder + WristDirection * Along + ElbowPole * Outward;
			StiffAimSegment(Pose, Mesh, Skeleton, *FString::Printf(TEXT("UpperArm%s"), *Suffix),
				*FString::Printf(TEXT("LowerArm%s"), *Suffix), ElbowTarget - Shoulder);
			StiffAimSegment(Pose, Mesh, Skeleton, *FString::Printf(TEXT("LowerArm%s"), *Suffix),
				*FString::Printf(TEXT("Wrist%s"), *Suffix), WristTarget - ElbowTarget);
		};
		// The last fist is still coming home while this one is already leaving.
		// On the first strike the opposite fist only counters the shoulder turn.
		const float CounterExtend = bPunchCombo ? PreviousExtend : -0.12f * PunchDrive;
		const float LeftExtend = bPunchLeft ? PunchExtend : CounterExtend;
		const float RightExtend = bPunchLeft ? CounterExtend : PunchExtend;
		PunchArm(false, LeftExtend);
		PunchArm(true, RightExtend);
	}

	const float Grip = FMath::Lerp(0.45f, 1.0f, PunchCover);
	StiffCurlFingers(Pose, Mesh, Skeleton, false, Grip);
	StiffCurlFingers(Pose, Mesh, Skeleton, true, Grip);
}
