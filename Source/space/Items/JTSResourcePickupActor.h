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

	static bool CanCollectResource(APawn* InteractingPawn, int32 ResourceAmount = 1, bool bRequireEarthCollection = true);

	static bool TryCollectResource(
		APawn* InteractingPawn,
		EJTSResourceType ResourceType,
		int32 ResourceAmount = 1,
		bool bRequireEarthCollection = true);

	UFUNCTION(BlueprintPure, Category = "Resource")
	EJTSResourceType GetResourceType() const;

	UFUNCTION(BlueprintPure, Category = "Resource")
	int32 GetResourceAmount() const;

	/** Returns the world-space half extents of the visible resource mesh for placement calculations. */
	FVector GetVisualBoundsExtent() const;

	UFUNCTION(BlueprintCallable, Category = "Resource")
	void InitializeResource(EJTSResourceType NewResourceType, int32 NewResourceAmount = 1);

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

	/** Number of resource units awarded by this pickup. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 ResourceAmount = 1;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ResourceMaterial;

	bool bPickupConsumed = false;
};
