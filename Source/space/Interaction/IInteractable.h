// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "IInteractable.generated.h"

class APawn;

/**
 * Contract for world actors that can be used by a pawn.
 */
UINTERFACE(BlueprintType)
class SPACE_API UInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Defines whether an actor is available for interaction, its prompt, and its response.
 */
class SPACE_API IInteractable
{
	GENERATED_BODY()

public:
	/** Returns whether this actor is currently available to the supplied pawn. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(APawn* InteractingPawn) const;

	/** Returns the text shown to the player for the current interaction target. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionPrompt(APawn* InteractingPawn) const;

	/** Performs this actor's interaction behavior. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(APawn* InteractingPawn);

	/** Availability of the prompt is independent from permission to execute (e.g. a required tool). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	bool CanExecuteInteraction(APawn* Pawn) const;
	virtual bool CanExecuteInteraction_Implementation(APawn* Pawn) const { return true; }
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	bool UsesSharedInteractionProgress() const;
	virtual bool UsesSharedInteractionProgress_Implementation() const { return false; }
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	float GetSharedInteractionProgress() const;
	virtual float GetSharedInteractionProgress_Implementation() const { return 0; }
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	void SetInteractionHeld(APawn* Pawn, bool bHeld);
	virtual void SetInteractionHeld_Implementation(APawn* Pawn, bool bHeld) {}

	/** Zero keeps ordinary press interactions; positive values require a server-timed hold. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	float GetInteractionHoldDuration(APawn* InteractingPawn) const;
	virtual float GetInteractionHoldDuration_Implementation(APawn* InteractingPawn) const { return 0.0f; }
};
