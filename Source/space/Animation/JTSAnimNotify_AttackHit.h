// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"

#include "JTSAnimNotify_AttackHit.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;

/** Dispatches an attack's hit frame to the melee component on the mesh owner's pawn. */
UCLASS(meta = (DisplayName = "JTS Attack Hit"))
class SPACE_API UJTSAnimNotify_AttackHit : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
