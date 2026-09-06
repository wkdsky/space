// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Items/JTSResourceTypes.h"

#include "JTSCarryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCarriedResourcesChanged, int32, CarriedItemCount, int32, CarryCapacity);

/**
 * Stores the small, fixed-capacity set of resources carried by one actor.
 */
UCLASS(ClassGroup = (Items), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSCarryComponent();

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

	/** Returns the fixed number of available carry slots. */
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

	/** Broadcast after a resource is successfully added. */
	UPROPERTY(BlueprintAssignable, Category = "Carry")
	FOnCarriedResourcesChanged OnCarriedResourcesChanged;

private:
	static constexpr int32 CarryCapacity = 2;

	/** Resource amounts currently held by this actor. Every unit consumes one carry slot. */
	UPROPERTY(VisibleAnywhere, Category = "Carry")
	TMap<EJTSResourceType, int32> CarriedResources;
};
