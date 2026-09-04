// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSCarryComponent.h"

UJTSCarryComponent::UJTSCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UJTSCarryComponent::CanCarryResource(EJTSResourceType ResourceType) const
{
	return !IsFull();
}

bool UJTSCarryComponent::TryAddResource(EJTSResourceType ResourceType)
{
	if (!CanCarryResource(ResourceType))
	{
		return false;
	}

	CarriedResources.Add(ResourceType);
	OnCarriedResourcesChanged.Broadcast(GetCarriedItemCount(), GetCarryCapacity());
	return true;
}

bool UJTSCarryComponent::TryTakeAllResources(TArray<EJTSResourceType>& OutResources)
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
	return CarriedResources.Num();
}

int32 UJTSCarryComponent::GetCarryCapacity() const
{
	return CarryCapacity;
}

bool UJTSCarryComponent::IsFull() const
{
	return GetCarriedItemCount() >= GetCarryCapacity();
}

const TArray<EJTSResourceType>& UJTSCarryComponent::GetCarriedResources() const
{
	return CarriedResources;
}
