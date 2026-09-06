// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSCarryComponent.h"

UJTSCarryComponent::UJTSCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UJTSCarryComponent::CanCarryResource(EJTSResourceType ResourceType) const
{
	return CanCarryResources(1);
}

bool UJTSCarryComponent::CanCarryResources(int32 ResourceAmount) const
{
	return ResourceAmount > 0
		&& ResourceAmount <= GetCarryCapacity() - GetCarriedItemCount();
}

bool UJTSCarryComponent::TryAddResource(EJTSResourceType ResourceType)
{
	return TryAddResources(ResourceType, 1);
}

bool UJTSCarryComponent::TryAddResources(EJTSResourceType ResourceType, int32 ResourceAmount)
{
	if (!CanCarryResources(ResourceAmount))
	{
		return false;
	}

	CarriedResources.FindOrAdd(ResourceType) += ResourceAmount;

	OnCarriedResourcesChanged.Broadcast(GetCarriedItemCount(), GetCarryCapacity());
	return true;
}

bool UJTSCarryComponent::TryTakeAllResources(TMap<EJTSResourceType, int32>& OutResources)
{
	OutResources.Reset();

	if (CarriedResources.IsEmpty())
	{
		return false;
	}

	OutResources = CarriedResources;
	CarriedResources.Reset();
	OnCarriedResourcesChanged.Broadcast(GetCarriedItemCount(), GetCarryCapacity());
	return true;
}

int32 UJTSCarryComponent::GetCarriedItemCount() const
{
	int32 CarriedItemCount = 0;
	for (const TPair<EJTSResourceType, int32>& CarriedResource : CarriedResources)
	{
		CarriedItemCount += FMath::Max(0, CarriedResource.Value);
	}

	return CarriedItemCount;
}

int32 UJTSCarryComponent::GetCarryCapacity() const
{
	return CarryCapacity;
}

bool UJTSCarryComponent::IsFull() const
{
	return GetCarriedItemCount() >= GetCarryCapacity();
}

int32 UJTSCarryComponent::GetCarriedResourceAmount(EJTSResourceType ResourceType) const
{
	const int32* const ResourceAmount = CarriedResources.Find(ResourceType);
	return ResourceAmount != nullptr ? FMath::Max(0, *ResourceAmount) : 0;
}

const TMap<EJTSResourceType, int32>& UJTSCarryComponent::GetCarriedResources() const
{
	return CarriedResources;
}
