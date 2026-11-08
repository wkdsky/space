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
		if (!bRight)
		{
			AimSegment(Upper,Lower,-Up+Forward*.12f-Right*.10f);
			AimSegment(Lower,Wrist,-Up+Forward*.10f);
			return;
		}
		const FVector Target=Frame.InverseTransformPosition(TargetWorld);
		const FVector FingerDirection=Frame.InverseTransformVectorNoScale(-ToolRotation.GetAxisZ()).GetSafeNormal();
		const float PalmLength=FVector::Distance(Pose[C].GetLocation(),Pose[D].GetLocation())*1.35f;
		const FVector WristTarget=Target-FingerDirection*PalmLength;
		const FVector Shoulder=Pose[A].GetLocation();
		const float UpperLength=FVector::Distance(Shoulder,Pose[B].GetLocation());
		const float LowerLength=FVector::Distance(Pose[B].GetLocation(),Pose[C].GetLocation());
		const FVector Along=(WristTarget-Shoulder).GetSafeNormal();
		const float Distance=FMath::Clamp(float((WristTarget-Shoulder).Size()),FMath::Abs(UpperLength-LowerLength)+.1f,UpperLength+LowerLength-.1f);
		const float ALong=(UpperLength*UpperLength-LowerLength*LowerLength+Distance*Distance)/(2*Distance);
		const float Out=FMath::Sqrt(FMath::Max(0.f,UpperLength*UpperLength-ALong*ALong));
		const FVector Pole=FVector::VectorPlaneProject(Right-Up*.25f,Along).GetSafeNormal(SMALL_NUMBER,Forward);
		const FVector Elbow=Shoulder+Along*ALong+Pole*Out;
		AimSegment(Upper,Lower,Elbow-Shoulder);
		AimSegment(Lower,Wrist,Shoulder+Along*Distance-Pose[B].GetLocation());
		AimSegment(Wrist,Finger,FingerDirection);
	};
	CloseGrip(true); CloseGrip(false);
	Hand(true,Grip);
	Hand(false,Grip);
	return true;
}
