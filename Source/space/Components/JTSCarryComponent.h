// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Items/JTSResourceTypes.h"

#include "JTSCarryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCarriedResourcesChanged, int32, CarriedItemCount, int32, CarryCapacity);

/**
 * Compatibility projection for pre-v1 callers. Resources now live in UJTSInventoryComponent as
 * item instances; this component intentionally exposes the former resource-only API.
 */
UCLASS(ClassGroup = (Items), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSCarryComponent();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Returns whether one additional resource can fit in the carry inventory. */
	UFUNCTION(BlueprintPure, Category = "Carry")
	bool CanCarryResource(EJTSResourceType ResourceType) const;

	/** Returns whether the requested number of carry slots can fit in the inventory. */
	UFUNCTION(BlueprintPure, Category = "Carry")
	bool CanCarryResources(int32 ResourceAmount) const;

	/** Adds one resource when capacity remains and reports whether it succeeded. */
	UFUNCTION(BlueprintCallable, Category = "Carry")
	bool TryAddResource(EJTSResourceType ResourceType);

	/** Atomically adds ResourceAmount copies of a resource when all required slots are available. */
	UFUNCTION(BlueprintCallable, Category = "Carry")
	bool TryAddResources(EJTSResourceType ResourceType, int32 ResourceAmount);

	/** Copies every carried resource amount to the output, then clears the carry inventory. */
	UFUNCTION(BlueprintCallable, Category = "Carry")
	bool TryTakeAllResources(TMap<EJTSResourceType, int32>& OutResources);

	/** Returns the number of resources currently carried. */
	UFUNCTION(BlueprintPure, Category = "Carry")
	int32 GetCarriedItemCount() const;

	/** Returns the base number of carry slots before equipment bonuses. */
	UFUNCTION(BlueprintPure, Category = "Carry")
	int32 GetBaseCapacity() const;

	/** Returns the effective number of available carry slots, including equipment bonuses. */
	UFUNCTION(BlueprintPure, Category = "Carry")
	int32 GetCarryCapacity() const;

	/** Returns whether every carry slot is occupied. */
	UFUNCTION(BlueprintPure, Category = "Carry")
	bool IsFull() const;

	/** Returns the amount carried for a single resource type. */
	UFUNCTION(BlueprintPure, Category = "Carry")
	int32 GetCarriedResourceAmount(EJTSResourceType ResourceType) const;

	/** Returns the resource amounts currently occupying the carry slots. */
	const TMap<EJTSResourceType, int32>& GetCarriedResources() const;

	/** Returns the resources in their actual inventory-slot order. Every array entry occupies one slot. */
	const TArray<EJTSResourceType>& GetCarriedItems() const;

	/** Copies the resources in slots that would no longer fit at NewCapacity without changing inventory state. */
	bool GetOverflowItemsForCapacity(int32 NewCapacity, TArray<EJTSResourceType>& OutOverflowItems) const;

	/** Removes exactly the previously inspected overflow after all corresponding world drops have been created. */
	bool CommitOverflowRemovalForCapacity(int32 NewCapacity, const TArray<EJTSResourceType>& ExpectedOverflowItems);

	/** Broadcasts the current count and effective capacity after an equipment capacity change. */
	void NotifyCapacityChanged();
	/** Server-only snapshot restore hook used by expedition loading. */
	void RestoreCarriedItems(const TArray<EJTSResourceType>& NewItems);

	/** Broadcast after a resource is successfully added. */
	UPROPERTY(BlueprintAssignable, Category = "Carry")
	FOnCarriedResourcesChanged OnCarriedResourcesChanged;

private:
	class UJTSInventoryComponent* GetInventoryComponent() const;
	void RebuildCarriedResourceAmounts() const;

	UFUNCTION()
	void HandleInventoryChanged(int32 UsedSlots, int32 Capacity);

	/** Lazily rebuilt flattened legacy view. New gameplay code should use UJTSInventoryComponent. */
	mutable TArray<EJTSResourceType> CarriedItems;

	/** Aggregated resource view retained for deposits and native presentation. */
	mutable TMap<EJTSResourceType, int32> CarriedResources;
};
