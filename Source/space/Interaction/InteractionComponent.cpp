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
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "space/Systems/JTSWorldPickupRegistrySubsystem.h"
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

bool UInteractionComponent::TryInteract()
{
	APawn* const InteractingPawn = Cast<APawn>(GetOwner());
	AActor* const Target = GetCurrentInteractable();
	if (!IsValidInteractable(Target, InteractingPawn))
	{
		return false;
	}

	IInteractable::Execute_Interact(Target, InteractingPawn);
	return true;
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

AActor* UInteractionComponent::FindBestInteractable(APawn* InteractingPawn)
{
	UWorld* const World = GetWorld();
	if (!IsValid(InteractingPawn) || World == nullptr || InteractionRadius <= 0.0f)
	{
		return nullptr;
	}

	if (AActor* const PickupTarget = FindBestWorldPickup(InteractingPawn))
	{
		return PickupTarget;
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

AActor* UInteractionComponent::FindBestWorldPickup(APawn* InteractingPawn)
{
	UWorld* const World = GetWorld();
	const AJTSMoonGameMode* const MoonGameMode = World != nullptr
		? World->GetAuthGameMode<AJTSMoonGameMode>()
		: nullptr;
	if (!IsValid(InteractingPawn) || !IsValid(MoonGameMode))
	{
		return nullptr;
	}

	APlayerController* const PlayerController = Cast<APlayerController>(InteractingPawn->GetController());
	APlayerCameraManager* const CameraManager = PlayerController != nullptr ? PlayerController->PlayerCameraManager : nullptr;
	UJTSWorldPickupRegistrySubsystem* const PickupRegistry = World->GetSubsystem<UJTSWorldPickupRegistrySubsystem>();
	if (!IsValid(PlayerController) || !IsValid(CameraManager) || !IsValid(PickupRegistry))
	{
		return nullptr;
	}

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth <= 0 || ViewportHeight <= 0)
	{
		return nullptr;
	}

	const FVector CameraLocation = CameraManager->GetCameraLocation();
	const FVector CameraForward = CameraManager->GetCameraRotation().Vector().GetSafeNormal();
	if (CameraForward.IsNearlyZero())
	{
		return nullptr;
	}
	const FVector2D ViewportCenter(static_cast<float>(ViewportWidth) * 0.5f, static_cast<float>(ViewportHeight) * 0.5f);
	const float MaxDistance = MoonGameMode->GetPickupMaxDistance();
	const float MaxDistanceSquared = FMath::Square(MaxDistance);
	const float ViewportScale = FMath::Max(0.1f, static_cast<float>(ViewportHeight) / 1080.0f);
	const float AcquireRadiusSquared = FMath::Square(MoonGameMode->GetPickupAcquireRadius() * ViewportScale);
	const float RetainRadiusSquared = FMath::Square(MoonGameMode->GetPickupRetainRadius() * ViewportScale);
	const float AimRayRadiusSquared = FMath::Square(MoonGameMode->GetPickupAimRayRadius());

	auto IsPickupCandidate = [
		this,
		World,
		InteractingPawn,
		PlayerController,
		CameraLocation,
		CameraForward,
		ViewportCenter,
		MaxDistanceSquared,
		AimRayRadiusSquared](
		AJTSWorldPickupActor* Pickup,
		float ScreenRadiusSquared,
		float& OutScreenDistanceSquared,
		float& OutWorldDistanceSquared)
	{
		OutScreenDistanceSquared = TNumericLimits<float>::Max();
		OutWorldDistanceSquared = TNumericLimits<float>::Max();
		if (!IsValidInteractable(Pickup, InteractingPawn))
		{
			return false;
		}

		const FVector PickupTargetLocation = Pickup->GetInteractionTargetWorldLocation();
		const FVector ToPickup = PickupTargetLocation - CameraLocation;
		const float ForwardDistance = FVector::DotProduct(ToPickup, CameraForward);
		OutWorldDistanceSquared = FVector::DistSquared(InteractingPawn->GetActorLocation(), PickupTargetLocation);
		if (OutWorldDistanceSquared > MaxDistanceSquared || ForwardDistance <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		FVector2D PickupScreenLocation;
		if (!PlayerController->ProjectWorldLocationToScreen(PickupTargetLocation, PickupScreenLocation, true))
		{
			return false;
		}

		OutScreenDistanceSquared = FVector2D::DistSquared(PickupScreenLocation, ViewportCenter);
		if (OutScreenDistanceSquared > ScreenRadiusSquared)
		{
			return false;
		}

		const FVector ClosestAimPoint = CameraLocation + CameraForward * ForwardDistance;
		if (FVector::DistSquared(PickupTargetLocation, ClosestAimPoint) > AimRayRadiusSquared)
		{
			return false;
		}

		FCollisionQueryParams VisibilityParams(SCENE_QUERY_STAT(JTSPickupLineOfSight), false, InteractingPawn);
		VisibilityParams.AddIgnoredActor(InteractingPawn);
		FHitResult VisibilityHit;
		if (World->LineTraceSingleByChannel(VisibilityHit, CameraLocation, PickupTargetLocation, ECC_Visibility, VisibilityParams)
			&& VisibilityHit.bBlockingHit && VisibilityHit.GetActor() != Pickup)
		{
			return false;
		}

		return true;
	};

	if (AJTSWorldPickupActor* const StickyPickup = Cast<AJTSWorldPickupActor>(CurrentInteractable.Get()))
	{
		float StickyScreenDistanceSquared = 0.0f;
		float StickyWorldDistanceSquared = 0.0f;
		if (IsPickupCandidate(StickyPickup, RetainRadiusSquared, StickyScreenDistanceSquared, StickyWorldDistanceSquared))
		{
			return StickyPickup;
		}
	}

	TArray<AJTSWorldPickupActor*> RegisteredPickups;
	PickupRegistry->GetRegisteredPickups(RegisteredPickups);
	AJTSWorldPickupActor* BestPickup = nullptr;
	float BestScreenDistanceSquared = TNumericLimits<float>::Max();
	float BestWorldDistanceSquared = TNumericLimits<float>::Max();
	for (AJTSWorldPickupActor* const Pickup : RegisteredPickups)
	{
		float ScreenDistanceSquared = 0.0f;
		float WorldDistanceSquared = 0.0f;
		if (!IsPickupCandidate(Pickup, AcquireRadiusSquared, ScreenDistanceSquared, WorldDistanceSquared))
		{
			continue;
		}

		const bool bCloserToCrosshair = ScreenDistanceSquared + 4.0f < BestScreenDistanceSquared;
		const bool bEqualCrosshairDistance = FMath::Abs(ScreenDistanceSquared - BestScreenDistanceSquared) <= 4.0f;
		if (bCloserToCrosshair || (bEqualCrosshairDistance && WorldDistanceSquared < BestWorldDistanceSquared))
		{
			BestPickup = Pickup;
			BestScreenDistanceSquared = ScreenDistanceSquared;
			BestWorldDistanceSquared = WorldDistanceSquared;
		}
	}

	return BestPickup;
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
