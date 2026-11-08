#include "JTSMiningPose.h"
#include "space/Components/JTSMiningStrikeComponent.h"
#include "Components/SkeletalMeshComponent.h"

bool JTSMiningPose::Apply(const UJTSMiningStrikeComponent* Strike, USkeletalMeshComponent* Mesh, TArray<FTransform>& Pose,
	TFunctionRef<void(FName,FName,const FVector&)> AimSegment, TFunctionRef<void(bool)> CloseGrip)
{
	FVector Grip,Tip; FQuat ToolRotation; float Load;
	if (!Strike || !Strike->GetGripPose(Grip,ToolRotation,Tip,Load)) return false;
	const FTransform Frame=Mesh->GetComponentTransform();
	const FVector Up=Frame.InverseTransformVectorNoScale(Mesh->GetOwner()->GetActorUpVector());
	const FVector Forward=Frame.InverseTransformVectorNoScale(Mesh->GetOwner()->GetActorForwardVector());
	const FVector Right=Frame.InverseTransformVectorNoScale(Mesh->GetOwner()->GetActorRightVector());
	AimSegment(TEXT("Abdomen"),TEXT("Chest"),Up+Forward*Load);
	auto Hand=[&](const bool bRight, const FVector& TargetWorld)
	{
		const FName Upper=bRight?TEXT("UpperArm_R"):TEXT("UpperArm_L");
		const FName Lower=bRight?TEXT("LowerArm_R"):TEXT("LowerArm_L");
		const FName Wrist=bRight?TEXT("Wrist_R"):TEXT("Wrist_L");
		const FName Finger=bRight?TEXT("Index1_R"):TEXT("Index1_L");
		const int32 A=Mesh->GetBoneIndex(Upper), B=Mesh->GetBoneIndex(Lower), C=Mesh->GetBoneIndex(Wrist), D=Mesh->GetBoneIndex(Finger);
		if (!Pose.IsValidIndex(A)||!Pose.IsValidIndex(B)||!Pose.IsValidIndex(C)||!Pose.IsValidIndex(D)) return;
		const FVector Target=Frame.InverseTransformPosition(TargetWorld);
		// Upper arms hang beside the torso. Only the forearm turns toward the working grip.
		AimSegment(Upper,Lower,-Up+Forward*.08f+Right*(bRight?.025f:-.025f));
		const FVector Elbow=Pose[B].GetLocation();
		if (!bRight) { AimSegment(Lower,Wrist,-Up+Forward*.06f); return; }
		const FVector Palm=Pose[C].GetLocation()+(Pose[D].GetLocation()-Pose[C].GetLocation())*1.35f;
		const FQuat Turn=FQuat::FindBetweenNormals((Palm-Elbow).GetSafeNormal(),(Target-Elbow).GetSafeNormal());
		AimSegment(Lower,Wrist,Turn.RotateVector(Pose[C].GetLocation()-Elbow));
	};
	CloseGrip(true); CloseGrip(false);
	Hand(true,Grip);
	Hand(false,Grip);
	return true;
}
