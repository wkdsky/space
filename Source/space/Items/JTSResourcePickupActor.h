// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/IInteractable.h"
#include "space/Items/JTSResourceTypes.h"

#include "JTSResourcePickupActor.generated.h"

class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
struct FHitResult;

/**
 * A single world resource that can be picked up through the shared interaction system.
 */
UCLASS()
class SPACE_API AJTSResourcePickupActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AJTSResourcePickupActor();

	UFUNCTION(BlueprintPure, Category = "Resource")
	EJTSResourceType GetResourceType() const;

	/** Returns the world-space half extents of the visible resource mesh for placement calculations. */
	FVector GetVisualBoundsExtent() const;

	UFUNCTION(BlueprintCallable, Category = "Resource")
	void InitializeResource(EJTSResourceType NewResourceType);

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION()
	void HandlePickupTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

private:
	bool IsEarthCollectionActive() const;
	bool TryPickup(APawn* InteractingPawn);
	void ApplyResourceAppearance();

	/** Non-visual transform root for the pickup actor. */
	UPROPERTY(VisibleAnywhere, Category = "Resource")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Temporary visible primitive for the world resource. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ResourceMesh;

	/** Pawn-only overlap volume used for automatic pickup. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> PickupTrigger;

	/** Resource added to the interacting pawn when this pickup succeeds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (AllowPrivateAccess = "true"))
	EJTSResourceType ResourceType = EJTSResourceType::Fuel;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ResourceMaterial;

	bool bPickupConsumed = false;
};
