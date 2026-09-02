// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/IInteractable.h"
#include "space/Items/JTSResourceTypes.h"

#include "JTSResourcePickupActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/**
 * A single world resource that can be picked up through the shared interaction system.
 */
UCLASS()
class SPACE_API AJTSResourcePickupActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AJTSResourcePickupActor();

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

private:
	/** Non-visual transform root for the pickup actor. */
	UPROPERTY(VisibleAnywhere, Category = "Resource")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Temporary visible primitive for the world resource. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ResourceMesh;

	/** Resource added to the interacting pawn when this pickup succeeds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (AllowPrivateAccess = "true"))
	EJTSResourceType ResourceType = EJTSResourceType::Fuel;
};
