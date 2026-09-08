// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"

#include "JTSAnimNotify_AttackEnd.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;

/** Marks the current attack montage as complete after its chain window has passed. */
UCLASS(meta = (DisplayName = "JTS Attack End"))
class SPACE_API UJTSAnimNotify_AttackEnd : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
