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
#include "space/World/JTSMoonSurfaceController.h"
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
	// E can be pressed between timer passes. Refresh here so interaction always uses the direction
	// the player is looking at when the input is actually committed.
	RefreshInteractable();
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

	if (AActor* const PickupTarget = FindBestWorldPickup(InteractingPawn))
	{
		return PickupTarget;
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

	const float RetainHalfAngle = FMath::Max(
		InteractionViewHalfAngleDegrees,
		InteractionRetainViewHalfAngleDegrees);
	if (AActor* const StickyTarget = CurrentInteractable.Get();
		IsValidInteractable(StickyTarget, InteractingPawn))
	{
		float StickyAlignment = 0.0f;
		float StickyDistanceSquared = 0.0f;
		if (IsInteractionTargetVisible(
			InteractingPawn,
			StickyTarget,
			ViewLocation,
			ViewForward,
			RetainHalfAngle,
			StickyAlignment,
			StickyDistanceSquared))
		{
			return StickyTarget;
		}
	}

	AActor* BestTarget = nullptr;
	float BestViewAlignment = -1.0f;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	TSet<AActor*> EvaluatedCandidates;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* const Candidate = OverlapResult.GetActor();
		if (EvaluatedCandidates.Contains(Candidate) || !IsValidInteractable(Candidate, InteractingPawn))
		{
			continue;
		}
		EvaluatedCandidates.Add(Candidate);

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

		// Screen/aim alignment determines intent; distance only breaks near-identical angles.
		const bool bBetterAlignment = ViewAlignment > BestViewAlignment + KINDA_SMALL_NUMBER;
		const bool bEquivalentAlignment = FMath::IsNearlyEqual(ViewAlignment, BestViewAlignment, KINDA_SMALL_NUMBER);
		if (bBetterAlignment || (bEquivalentAlignment && DistanceSquared < BestDistanceSquared))
		{
			BestViewAlignment = ViewAlignment;
			BestDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

AActor* UInteractionComponent::FindBestWorldPickup(APawn* InteractingPawn)
{
	UWorld* const World = GetWorld();
	AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this);
	const AJTSMoonGameMode* const MoonSettings = IsValid(SurfaceController)
		? SurfaceController->GetMoonSettings()
		: nullptr;
	if (!IsValid(InteractingPawn)
		|| !IsValid(SurfaceController)
		|| !SurfaceController->IsSurfaceGameplayInitialized()
		|| !SurfaceController->OwnsSurfaceActor(InteractingPawn)
		|| !IsValid(MoonSettings))
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
	const float MaxDistance = MoonSettings->GetPickupMaxDistance();
	const float MaxDistanceSquared = FMath::Square(MaxDistance);
	const float ViewportScale = FMath::Max(0.1f, static_cast<float>(ViewportHeight) / 1080.0f);
	const float AcquireRadiusSquared = FMath::Square(MoonSettings->GetPickupAcquireRadius() * ViewportScale);
	const float RetainRadiusSquared = FMath::Square(MoonSettings->GetPickupRetainRadius() * ViewportScale);
	const float AimRayRadiusSquared = FMath::Square(MoonSettings->GetPickupAimRayRadius());

	auto IsPickupCandidate = [
		this,
		World,
		SurfaceController,
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
		if (!IsValid(SurfaceController)
			|| !SurfaceController->OwnsSurfaceActor(Pickup)
			|| !IsValidInteractable(Pickup, InteractingPawn))
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

	const FVector TargetLocation = GetInteractionTargetWorldLocation(Candidate);
	OutPawnDistanceSquared = FVector::DistSquared(InteractingPawn->GetActorLocation(), TargetLocation);
	if (OutPawnDistanceSquared > FMath::Square(InteractionRadius))
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

FVector UInteractionComponent::GetInteractionTargetWorldLocation(const AActor* Candidate) const
{
	if (!IsValid(Candidate))
	{
		return FVector::ZeroVector;
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
