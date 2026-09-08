// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Animation/JTSAnimNotify_AttackHit.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "space/Components/JTSMeleeComponent.h"

void UJTSAnimNotify_AttackHit::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	APawn* const OwningPawn = Cast<APawn>(MeshComp->GetOwner());
	if (!IsValid(OwningPawn))
	{
		return;
	}

	if (UJTSMeleeComponent* const MeleeComponent = OwningPawn->FindComponentByClass<UJTSMeleeComponent>())
	{
		MeleeComponent->PerformHitCheck();
	}
}
