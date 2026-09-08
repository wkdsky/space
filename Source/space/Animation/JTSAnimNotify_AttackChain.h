// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"

#include "JTSAnimNotify_AttackChain.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;

/** Evaluates held or buffered input at an attack montage's combo link window. */
UCLASS(meta = (DisplayName = "JTS Attack Chain"))
class SPACE_API UJTSAnimNotify_AttackChain : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
