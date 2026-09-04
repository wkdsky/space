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

	/** Adds one resource when capacity remains and reports whether it succeeded. */
	UFUNCTION(BlueprintCallable, Category = "Carry")
	bool TryAddResource(EJTSResourceType ResourceType);

	/** Copies every carried resource to the output, then clears the carry inventory. */
	UFUNCTION(BlueprintCallable, Category = "Carry")
	bool TryTakeAllResources(TArray<EJTSResourceType>& OutResources);

	/** Returns the number of resources currently carried. */
	UFUNCTION(BlueprintPure, Category = "Carry")
	int32 GetCarriedItemCount() const;

	/** Returns the fixed number of available carry slots. */
	UFUNCTION(BlueprintPure, Category = "Carry")
	int32 GetCarryCapacity() const;

	/** Returns whether every carry slot is occupied. */
	UFUNCTION(BlueprintPure, Category = "Carry")
	bool IsFull() const;

	/** Returns the resources currently occupying the carry slots. */
	UFUNCTION(BlueprintPure, Category = "Carry")
	const TArray<EJTSResourceType>& GetCarriedResources() const;

	/** Broadcast after a resource is successfully added. */
	UPROPERTY(BlueprintAssignable, Category = "Carry")
	FOnCarriedResourcesChanged OnCarriedResourcesChanged;

private:
	static constexpr int32 CarryCapacity = 2;

	/** Resources currently held by this actor. Each entry consumes one slot. */
	UPROPERTY(VisibleAnywhere, Category = "Carry")
	TArray<EJTSResourceType> CarriedResources;
};
