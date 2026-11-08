#pragma once
#include "CoreMinimal.h"
class UJTSMiningStrikeComponent;
class USkeletalMeshComponent;
namespace JTSMiningPose
{
	bool Apply(const UJTSMiningStrikeComponent* Strike, USkeletalMeshComponent* Mesh, TArray<FTransform>& Pose,
		TFunctionRef<void(FName,FName,const FVector&)> AimSegment, TFunctionRef<void(bool)> CloseGrip);
}
