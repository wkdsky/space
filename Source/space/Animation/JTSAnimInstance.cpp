// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Animation/JTSAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSMeleeComponent.h"
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
	TurnShuffleAlpha = 0.0f;
	bWeaponAiming = false;
	bHasRangedWeapon = false;
	bHasHeldItem = false;
	bActiveRangedWeapon = false;
	bMeleeHeld = false;
	if (IsValid(Character))
	{
		if (const UJTSInventoryComponent* const Inventory = Character->FindComponentByClass<UJTSInventoryComponent>())
		{
			const EJTSItemId ActiveId = Inventory->GetActiveItemId();
			const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ActiveId);
			bHasHeldItem = IsValid(Definition) && Definition->IsHoldable() && !Inventory->GetActiveItem().IsEmpty();
			// Guns are the pistol, machine gun and sniper. A knife or pickaxe stays a chop
			// even when its data asset also carries a ranged capability bit.
			const bool bGun = ActiveId == EJTSItemId::Pistol
				|| ActiveId == EJTSItemId::MachineGun
				|| ActiveId == EJTSItemId::Sniper;
			bMeleeHeld = bHasHeldItem && !bGun;
		}
		if (const UJTSRangedWeaponComponent* const Ranged = Character->FindComponentByClass<UJTSRangedWeaponComponent>())
		{
			bActiveRangedWeapon = Ranged->HasActiveRangedWeapon();
			bWeaponAiming = Ranged->IsAiming() && bActiveRangedWeapon;
		}
		bMeleeHeld = bMeleeHeld && !bActiveRangedWeapon;

		const UCharacterMovementComponent* const Movement = Character->GetCharacterMovement();
		// The jump button is enough. Waiting for MOVE_Falling left the first frames
		// on the ground clip, which reads as the tuck starting late.
		const bool bAirborne = (IsValid(Movement) && Movement->IsFalling()) || Character->bPressedJump;
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
		// Head pitch always tracks the view, including the ordinary third-person stance.
		// Right-click aiming still uses the same pitch; it only changes the camera and crosshair.
		{
			const float HeadPitchTarget = FMath::Clamp(Character->GetAimPitch(), -55.0f, 55.0f);
			AimPitch = FMath::FInterpTo(AimPitch, HeadPitchTarget, DeltaSeconds, 14.0f);
		}
		if (const AController* const CharacterController = Character->GetController())
		{
			const FRotator RelativeAim = (CharacterController->GetControlRotation() - Character->GetActorRotation()).GetNormalized();
			RawAimYaw = FRotator::NormalizeAxis(RelativeAim.Yaw);
		}
		// Ordinary grounded third person still yaws only the chest toward the camera.
		// Aim, first person, and a jump already turn the whole actor with the view,
		// so an extra spine yaw there twists the upper body off the legs.
		// A held gun is the exception: the arm has to keep meeting the camera even
		// while the feet stay put, until the view leaves the arm's reach.
		const bool bWholeBodyOnView = Character->IsFirstPersonView()
			|| bAirborne
			|| bWeaponAiming;
		const bool bArmTracksView = bActiveRangedWeapon && !bWholeBodyOnView;
		// RawAimYaw is the full camera yaw. The presentation yaw fades out past the
		// cone, which would pull a held gun back to the chest before the body turns.
		AimYaw = bWholeBodyOnView
			? 0.0f
			: (bArmTracksView ? RawAimYaw : Character->GetPresentationAimYaw());
		TurnShuffleAlpha = bWholeBodyOnView ? 0.0f : Character->GetTurnShuffleAlpha();
		const float YawLimit = 48.0f;
		const float DesiredUpperYaw = FMath::Clamp(AimYaw, -YawLimit, YawLimit);
		UpperBodyYaw = FMath::FInterpTo(
			UpperBodyYaw,
			DesiredUpperYaw,
			DeltaSeconds,
			bWholeBodyOnView ? 24.0f : 9.0f);

		// Fast alternating steps. One foot plants while the other pops up, which reads as a
		// cartoon shuffle instead of a smooth combat turn.
		ShufflePhase += DeltaSeconds * (TurnShuffleAlpha > 0.05f ? 11.0f : 0.0f);
		const float Step = FMath::Max(0.0f, FMath::Sin(ShufflePhase));
		const float Other = FMath::Max(0.0f, FMath::Sin(ShufflePhase + PI));
		ShuffleLeftLift = 34.0f * TurnShuffleAlpha * Step;
		ShuffleRightLift = 34.0f * TurnShuffleAlpha * Other;
	}
	bHasRangedWeapon = bActiveRangedWeapon;
}

void UJTSAnimInstance::NativePostEvaluateAnimation()
{
	Super::NativePostEvaluateAnimation();
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
	const int32 SpineBone = Mesh->GetBoneIndex(TEXT("Spine_03"));
	const int32 PelvisBone = Mesh->GetBoneIndex(TEXT("Pelvis"));
	FVector PoseUp = ActorUp;
	FVector PoseForward = ActorForward;
	FVector PoseRight = ActorRight;
	if (Pose.IsValidIndex(SpineBone) && Pose.IsValidIndex(PelvisBone))
	{
		const FVector Spine = (Pose[SpineBone].GetLocation() - Pose[PelvisBone].GetLocation()).GetSafeNormal();
		if (!Spine.IsNearlyZero())
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
			const FQuat RootCurl(PalmNormal, FMath::DegreesToRadians(RootDegrees));
			const FQuat JointCurl(PalmNormal, FMath::DegreesToRadians(JointDegrees));
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
	TurnBone(TEXT("Neck"), FVector::UpVector, BodyYaw * 0.22f);
	TurnBone(TEXT("Head"), FVector::UpVector, BodyYaw * 0.18f);
	TurnBone(TEXT("Head"), FVector::RightVector, -AimPitch * 0.85f);

	if (bActiveRangedWeapon)
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
		// The idle clip itself holds the left arm out. Empty hands force both arms down.
		AimSegment(TEXT("UpperArm_R"), TEXT("LowerArm_R"), -PoseUp);
		AimSegment(TEXT("UpperArm_L"), TEXT("LowerArm_L"), -PoseUp);
		AimSegment(TEXT("LowerArm_R"), TEXT("Wrist_R"), -PoseUp);
		AimSegment(TEXT("LowerArm_L"), TEXT("Wrist_L"), -PoseUp);
	}

	if (JumpTuckAlpha > 0.02f)
	{
		// Kneeling tuck for the whole jump. A turn in the air must not swap this
		// for the shuffle, or the legs snap open before the landing.
		// Foot_L/R and the toe bones PT_L/R hang off Root, so the shoe mesh is
		// skinned between a folded ankle and a foot that never left the ground.
		// Each shoe bone is moved onto its ankle and rotated with the same fold.
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

		const FVector ThighAim = (ActorForward * Bend - ActorUp * (1.0f - Bend)).GetSafeNormal();
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
			const FQuat Fold = FQuat(ActorRight, FMath::DegreesToRadians(165.0f * Bend));
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
	else if (TurnShuffleAlpha >= 0.02f)
	{
		const float TurnSign = RawAimYaw >= 0.0f ? 1.0f : -1.0f;
		const auto StepLeg = [&TurnBone, TurnSign, this](const FName Thigh, const FName Shin, float LiftDegrees, float SideSign)
		{
			TurnBone(Thigh, FVector::RightVector, -LiftDegrees);
			TurnBone(Thigh, FVector::UpVector, SideSign * LiftDegrees * 0.35f * TurnSign);
			TurnBone(Thigh, FVector::ForwardVector, SideSign * 10.0f * TurnShuffleAlpha);
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
