// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Interaction/InteractionComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "space/Interaction/IInteractable.h"
#include "TimerManager.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Cast<APawn>(GetOwner()) == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("InteractionComponent on '%s' requires a pawn owner."), *GetNameSafe(GetOwner()));
		return;
	}

	RefreshInteractable();

	if (UWorld* World = GetWorld(); World != nullptr && DetectionInterval > 0.0f)
	{
		World->GetTimerManager().SetTimer(
			DetectionTimerHandle,
			this,
			&UInteractionComponent::RefreshInteractable,
			DetectionInterval,
			true);
	}
}

void UInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DetectionTimerHandle);
	}

	CurrentInteractable = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UInteractionComponent::RefreshInteractable()
{
	SetCurrentInteractable(FindBestInteractable(Cast<APawn>(GetOwner())));
}

void UInteractionComponent::TryInteract()
{
	RefreshInteractable();

	APawn* const InteractingPawn = Cast<APawn>(GetOwner());
	AActor* const Target = GetCurrentInteractable();
	if (!IsValidInteractable(Target, InteractingPawn))
	{
		return;
	}

	IInteractable::Execute_Interact(Target, InteractingPawn);
}

AActor* UInteractionComponent::GetCurrentInteractable() const
{
	return CurrentInteractable.Get();
}

FText UInteractionComponent::GetCurrentInteractionPrompt() const
{
	APawn* const InteractingPawn = Cast<APawn>(GetOwner());
	AActor* const Target = GetCurrentInteractable();
	if (!IsValidInteractable(Target, InteractingPawn))
	{
		return FText::GetEmpty();
	}

	return IInteractable::Execute_GetInteractionPrompt(Target, InteractingPawn);
}

AActor* UInteractionComponent::FindBestInteractable(APawn* InteractingPawn) const
{
	UWorld* const World = GetWorld();
	if (!IsValid(InteractingPawn) || World == nullptr || InteractionRadius <= 0.0f)
	{
		return nullptr;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InteractionDetection), false, InteractingPawn);
	const FCollisionShape DetectionShape = FCollisionShape::MakeSphere(InteractionRadius);
	const bool bFoundOverlap = World->OverlapMultiByObjectType(
		OverlapResults,
		InteractingPawn->GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
		DetectionShape,
		QueryParams);

	if (!bFoundOverlap)
	{
		return nullptr;
	}

	AActor* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* const Candidate = OverlapResult.GetActor();
		if (!IsValidInteractable(Candidate, InteractingPawn))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(InteractingPawn->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

bool UInteractionComponent::IsValidInteractable(AActor* Candidate, APawn* InteractingPawn) const
{
	return IsValid(Candidate)
		&& Candidate != GetOwner()
		&& IsValid(InteractingPawn)
		&& Candidate->GetClass()->ImplementsInterface(UInteractable::StaticClass())
		&& IInteractable::Execute_CanInteract(Candidate, InteractingPawn);
}

void UInteractionComponent::SetCurrentInteractable(AActor* NewTarget)
{
	if (CurrentInteractable == NewTarget)
	{
		return;
	}

	AActor* const PreviousTarget = CurrentInteractable.Get();
	CurrentInteractable = NewTarget;
	OnInteractionTargetChanged.Broadcast(PreviousTarget, NewTarget);
}
