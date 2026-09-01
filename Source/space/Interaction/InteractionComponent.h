// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"

#include "InteractionComponent.generated.h"

class AActor;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionTargetChanged, AActor*, PreviousTarget, AActor*, NewTarget);

/**
 * Finds nearby IInteractable actors for its owning pawn and executes the selected target.
 */
UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class SPACE_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent();

	/** Updates the nearest valid interaction target immediately. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RefreshInteractable();

	/** Attempts to interact with the nearest valid target. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void TryInteract();

	/** Returns the target selected by the most recent detection pass. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetCurrentInteractable() const;

	/** Returns the prompt supplied by the currently selected target, if it is still valid. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetCurrentInteractionPrompt() const;

	/** Broadcast when the nearest valid target changes. Useful for interaction prompt UI. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractionTargetChanged OnInteractionTargetChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	AActor* FindBestInteractable(APawn* InteractingPawn) const;
	bool IsValidInteractable(AActor* Candidate, APawn* InteractingPawn) const;
	void SetCurrentInteractable(AActor* NewTarget);

	/** Radius, in centimeters, used to look for IInteractable actors. */
	UPROPERTY(EditAnywhere, Category = "Interaction", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float InteractionRadius = 250.0f;

	/** Frequency used to refresh the target without adding per-frame Character logic. */
	UPROPERTY(EditAnywhere, Category = "Interaction", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float DetectionInterval = 0.1f;

	/** The closest valid IInteractable actor found during the latest detection pass. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> CurrentInteractable;

	FTimerHandle DetectionTimerHandle;
};
