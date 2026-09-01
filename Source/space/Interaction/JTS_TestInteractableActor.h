// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/IInteractable.h"

#include "JTS_TestInteractableActor.generated.h"

class UStaticMeshComponent;

/**
 * Minimal visible world actor used to verify the player interaction flow.
 */
UCLASS()
class SPACE_API AJTS_TestInteractableActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AJTS_TestInteractableActor();

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

private:
	/** Visible cube used as the in-world test target. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> TestMesh;
};
