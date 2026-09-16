// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Interaction/InteractionComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "space/Interaction/IInteractable.h"
#include "space/Items/JTSResourcePickupActor.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Systems/JTSWorldPickupRegistrySubsystem.h"
#include "TimerManager.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
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

bool UInteractionComponent::TryInteract()
{
	APawn* const InteractingPawn = Cast<APawn>(GetOwner());
	// E can be pressed between timer passes. Refresh here so interaction always uses the direction
	// the player is looking at when the input is actually committed.
	RefreshInteractable();
	AActor* const Target = GetCurrentInteractable();
	if (!IsValidInteractable(Target, InteractingPawn))
	{
		return false;
	}

	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		if (!CanServerInteractWith(InteractingPawn, Target))
		{
			return false;
		}
		IInteractable::Execute_Interact(Target, InteractingPawn);
	}
	else
	{
		ServerTryInteract(Target);
	}
	return true;
}

void UInteractionComponent::ServerTryInteract_Implementation(AActor* Target)
{
	APawn* const InteractingPawn = Cast<APawn>(GetOwner());
	if (CanServerInteractWith(InteractingPawn, Target))
	{
		IInteractable::Execute_Interact(Target, InteractingPawn);
	}
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

bool UInteractionComponent::IsInteractableInView(AActor* Candidate) const
{
	APawn* const InteractingPawn = Cast<APawn>(GetOwner());
	if (!IsValidInteractable(Candidate, InteractingPawn))
	{
		return false;
	}

	FVector ViewLocation;
	FVector ViewForward;
	if (!TryGetInteractionView(InteractingPawn, ViewLocation, ViewForward))
	{
		return false;
	}

	float ViewAlignment = 0.0f;
	float PawnDistanceSquared = 0.0f;
	return IsInteractionTargetVisible(
		InteractingPawn,
		Candidate,
		ViewLocation,
		ViewForward,
		InteractionViewHalfAngleDegrees,
		ViewAlignment,
		PawnDistanceSquared);
}

AActor* UInteractionComponent::FindBestInteractable(APawn* InteractingPawn)
{
	UWorld* const World = GetWorld();
	if (!IsValid(InteractingPawn) || World == nullptr || InteractionRadius <= 0.0f)
	{
		return nullptr;
	}

	FVector ViewLocation;
	FVector ViewForward;
	if (!TryGetInteractionView(InteractingPawn, ViewLocation, ViewForward))
	{
		return nullptr;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InteractionDetection), false, InteractingPawn);
	const FCollisionShape DetectionShape = FCollisionShape::MakeSphere(InteractionRadius);
	World->OverlapMultiByObjectType(
		OverlapResults,
		InteractingPawn->GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
		DetectionShape,
		QueryParams);

	// One candidate set is important: no interactable type is allowed to win just
	// because it was discovered by a more specialized detector.
	TSet<AActor*> CandidateActors;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		if (AActor* const Candidate = OverlapResult.GetActor())
		{
			CandidateActors.Add(Candidate);
		}
	}
	if (AActor* const PreviousTarget = CurrentInteractable.Get())
	{
		CandidateActors.Add(PreviousTarget);
	}
	if (UJTSWorldPickupRegistrySubsystem* const PickupRegistry = World->GetSubsystem<UJTSWorldPickupRegistrySubsystem>())
	{
		TArray<AJTSWorldPickupActor*> RegisteredPickups;
		PickupRegistry->GetRegisteredPickups(RegisteredPickups);
		for (AJTSWorldPickupActor* const Pickup : RegisteredPickups)
		{
			CandidateActors.Add(Pickup);
		}
	}

	FCollisionQueryParams AimQueryParams(SCENE_QUERY_STAT(InteractionAim), false, InteractingPawn);
	AimQueryParams.AddIgnoredActor(InteractingPawn);
	FHitResult AimHit;
	const bool bHasAimHit = World->LineTraceSingleByChannel(
		AimHit,
		ViewLocation,
		ViewLocation + ViewForward * FMath::Max(InteractionRadius, InteractionAimTraceDistance),
		ECC_Visibility,
		AimQueryParams);
	if (bHasAimHit && IsValid(AimHit.GetActor()))
	{
		CandidateActors.Add(AimHit.GetActor());
	}

	AActor* BestTarget = nullptr;
	bool bBestTargetIsAimHit = false;
	float BestViewAlignment = -1.0f;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (AActor* const Candidate : CandidateActors)
	{
		if (!IsValidInteractable(Candidate, InteractingPawn))
		{
			continue;
		}

		float ViewAlignment = 0.0f;
		float DistanceSquared = 0.0f;
		if (!IsInteractionTargetVisible(
			InteractingPawn,
			Candidate,
			ViewLocation,
			ViewForward,
			InteractionViewHalfAngleDegrees,
			ViewAlignment,
			DistanceSquared))
		{
			continue;
		}

		const bool bCandidateIsAimHit = bHasAimHit && AimHit.GetActor() == Candidate;
		// Priority is explicit and shared by every interaction: an actual camera ray hit,
		// then camera alignment, then physical proximity for near-equal view directions.
		const bool bBetterAimHit = bCandidateIsAimHit && !bBestTargetIsAimHit;
		const bool bSameAimHitClass = bCandidateIsAimHit == bBestTargetIsAimHit;
		const float AlignmentTolerance = FMath::Max(0.0f, InteractionAlignmentTieTolerance);
		const bool bBetterAlignment = bSameAimHitClass && ViewAlignment > BestViewAlignment + AlignmentTolerance;
		const bool bEquivalentAlignment = bSameAimHitClass
			&& FMath::Abs(ViewAlignment - BestViewAlignment) <= AlignmentTolerance;
		if (bBetterAimHit || bBetterAlignment || (bEquivalentAlignment && DistanceSquared < BestDistanceSquared))
		{
			bBestTargetIsAimHit = bCandidateIsAimHit;
			BestViewAlignment = ViewAlignment;
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

bool UInteractionComponent::TryGetInteractionView(
	APawn* InteractingPawn,
	FVector& OutViewLocation,
	FVector& OutViewForward) const
{
	if (!IsValid(InteractingPawn))
	{
		return false;
	}

	OutViewLocation = InteractingPawn->GetActorLocation();
	OutViewForward = InteractingPawn->GetActorForwardVector().GetSafeNormal();
	if (const APlayerController* const PlayerController = Cast<APlayerController>(InteractingPawn->GetController()))
	{
		if (const APlayerCameraManager* const CameraManager = PlayerController->PlayerCameraManager)
		{
			OutViewLocation = CameraManager->GetCameraLocation();
			OutViewForward = CameraManager->GetCameraRotation().Vector().GetSafeNormal();
		}
	}

	return !OutViewForward.IsNearlyZero();
}

bool UInteractionComponent::IsInteractionTargetVisible(
	APawn* InteractingPawn,
	AActor* Candidate,
	const FVector& ViewLocation,
	const FVector& ViewForward,
	float ViewHalfAngleDegrees,
	float& OutViewAlignment,
	float& OutPawnDistanceSquared) const
{
	OutViewAlignment = -1.0f;
	OutPawnDistanceSquared = TNumericLimits<float>::Max();
	if (!IsValid(InteractingPawn) || !IsValid(Candidate))
	{
		return false;
	}

	const FVector TargetLocation = GetInteractionTargetWorldLocation(Candidate, ViewLocation);
	OutPawnDistanceSquared = FVector::DistSquared(InteractingPawn->GetActorLocation(), TargetLocation);
	const AJTSSpacecraftActor* const Spacecraft = Cast<AJTSSpacecraftActor>(Candidate);
	const bool bUsesBoardingProximity = IsValid(Spacecraft)
		&& Spacecraft->IsPawnInBoardingRange(InteractingPawn);
	// The spacecraft first proves its own mesh-sized boarding/workshop proximity. Do not then
	// reject that same candidate against the generic 360 cm pickup/resource radius; camera cone and
	// Visibility LOS still run below for every candidate.
	if (!bUsesBoardingProximity && OutPawnDistanceSquared > FMath::Square(InteractionRadius))
	{
		return false;
	}

	const FVector ToTarget = TargetLocation - ViewLocation;
	const float ViewDistance = ToTarget.Size();
	if (ViewDistance <= KINDA_SMALL_NUMBER)
	{
		OutViewAlignment = 1.0f;
		return true;
	}

	OutViewAlignment = FVector::DotProduct(ViewForward, ToTarget / ViewDistance);
	const float ClampedHalfAngle = FMath::Clamp(ViewHalfAngleDegrees, 1.0f, 89.0f);
	const float MinimumAlignment = FMath::Cos(FMath::DegreesToRadians(ClampedHalfAngle));
	if (OutViewAlignment < MinimumAlignment)
	{
		return false;
	}

	return HasInteractionLineOfSight(InteractingPawn, Candidate, ViewLocation, TargetLocation);
}

bool UInteractionComponent::HasInteractionLineOfSight(
	APawn* InteractingPawn,
	AActor* Candidate,
	const FVector& ViewLocation,
	const FVector& TargetLocation) const
{
	if (!bRequireInteractionLineOfSight)
	{
		return true;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InteractionLineOfSight), false, InteractingPawn);
	QueryParams.AddIgnoredActor(InteractingPawn);
	FHitResult VisibilityHit;
	if (!World->LineTraceSingleByChannel(
		VisibilityHit,
		ViewLocation,
		TargetLocation,
		ECC_Visibility,
		QueryParams)
		|| !VisibilityHit.bBlockingHit)
	{
		return true;
	}

	if (VisibilityHit.GetActor() == Candidate)
	{
		return true;
	}

	// Some small pickup meshes intentionally have no Visibility collision. Treat a hit at the
	// requested target point as visible, while still rejecting geometry that blocks the ray earlier.
	constexpr float TargetPointTolerance = 18.0f;
	return FVector::DistSquared(VisibilityHit.ImpactPoint, TargetLocation) <= FMath::Square(TargetPointTolerance);
}

bool UInteractionComponent::CanServerInteractWith(APawn* InteractingPawn, AActor* Candidate) const
{
	if (!IsValid(InteractingPawn)
		|| !IsValid(Candidate)
		|| !Candidate->GetClass()->ImplementsInterface(UInteractable::StaticClass())
		|| !IInteractable::Execute_CanInteract(Candidate, InteractingPawn))
	{
		return false;
	}
	// The server deliberately validates against its own authoritative pawn eye
	// position, never a client-provided camera transform.
	const FVector Start = InteractingPawn->GetActorLocation() + FVector(0.0f, 0.0f, InteractingPawn->BaseEyeHeight);
	const FVector TargetLocation = GetInteractionTargetWorldLocation(Candidate, Start);
	const AJTSSpacecraftActor* const Spacecraft = Cast<AJTSSpacecraftActor>(Candidate);
	if (Spacecraft != nullptr)
	{
		return Spacecraft->IsPawnInBoardingRange(InteractingPawn);
	}
	const float ServerRadius = FMath::Max(0.0f, InteractionRadius) + 50.0f;
	if (FVector::DistSquared(InteractingPawn->GetActorLocation(), TargetLocation) > FMath::Square(ServerRadius))
	{
		return false;
	}
	if (!bRequireInteractionLineOfSight)
	{
		return true;
	}
	return HasInteractionLineOfSight(InteractingPawn, Candidate, Start, TargetLocation);
}

FVector UInteractionComponent::GetInteractionTargetWorldLocation(const AActor* Candidate, const FVector& ReferenceLocation) const
{
	if (!IsValid(Candidate))
	{
		return FVector::ZeroVector;
	}

	if (const AJTSSpacecraftActor* const Spacecraft = Cast<AJTSSpacecraftActor>(Candidate))
	{
		const APawn* const InteractingPawn = Cast<APawn>(GetOwner());
		return Spacecraft->GetBoardingInteractionTargetWorldLocation(
			!ReferenceLocation.IsNearlyZero()
				? ReferenceLocation
				: IsValid(InteractingPawn) ? InteractingPawn->GetActorLocation() : Candidate->GetActorLocation());
	}

	if (const AJTSWorldPickupActor* const WorldPickup = Cast<AJTSWorldPickupActor>(Candidate))
	{
		return WorldPickup->GetInteractionTargetWorldLocation();
	}

	if (const AJTSResourcePickupActor* const ResourcePickup = Cast<AJTSResourcePickupActor>(Candidate))
	{
		return ResourcePickup->GetInteractionAnchorWorldLocation();
	}

	const FBox CandidateBounds = Candidate->GetComponentsBoundingBox(true);
	return CandidateBounds.IsValid ? CandidateBounds.GetCenter() : Candidate->GetActorLocation();
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
