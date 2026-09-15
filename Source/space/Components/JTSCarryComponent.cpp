// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSCarryComponent.h"

#include "space/Components/JTSPlayerEquipmentComponent.h"

#include "Net/UnrealNetwork.h"

UJTSCarryComponent::UJTSCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
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
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || !CanCarryResources(ResourceAmount))
	{
		return false;
	}

	for (int32 ResourceIndex = 0; ResourceIndex < ResourceAmount; ++ResourceIndex)
	{
		CarriedItems.Add(ResourceType);
	}
	CarriedResources.FindOrAdd(ResourceType) += ResourceAmount;

	OnCarriedResourcesChanged.Broadcast(GetCarriedItemCount(), GetCarryCapacity());
	return true;
}

bool UJTSCarryComponent::TryTakeAllResources(TMap<EJTSResourceType, int32>& OutResources)
{
	OutResources.Reset();

	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || CarriedItems.IsEmpty())
	{
		return false;
	}

	OutResources = CarriedResources;
	CarriedItems.Reset();
	CarriedResources.Reset();
	OnCarriedResourcesChanged.Broadcast(GetCarriedItemCount(), GetCarryCapacity());
	return true;
}

int32 UJTSCarryComponent::GetCarriedItemCount() const
{
	return CarriedItems.Num();
}

int32 UJTSCarryComponent::GetBaseCapacity() const
{
	return FMath::Max(1, BaseCapacity);
}

int32 UJTSCarryComponent::GetCarryCapacity() const
{
	return GetBaseCapacity() + GetEquipmentCapacityBonus();
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

const TArray<EJTSResourceType>& UJTSCarryComponent::GetCarriedItems() const
{
	return CarriedItems;
}

bool UJTSCarryComponent::GetOverflowItemsForCapacity(int32 NewCapacity, TArray<EJTSResourceType>& OutOverflowItems) const
{
	OutOverflowItems.Reset();
	if (NewCapacity < 0 || NewCapacity > CarriedItems.Num())
	{
		return NewCapacity >= 0;
	}

	for (int32 SlotIndex = NewCapacity; SlotIndex < CarriedItems.Num(); ++SlotIndex)
	{
		OutOverflowItems.Add(CarriedItems[SlotIndex]);
	}
	return true;
}

bool UJTSCarryComponent::CommitOverflowRemovalForCapacity(
	int32 NewCapacity,
	const TArray<EJTSResourceType>& ExpectedOverflowItems)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return false;
	}
	TArray<EJTSResourceType> CurrentOverflowItems;
	if (!GetOverflowItemsForCapacity(NewCapacity, CurrentOverflowItems)
		|| CurrentOverflowItems != ExpectedOverflowItems)
	{
		return false;
	}

	if (CurrentOverflowItems.IsEmpty())
	{
		return true;
	}

	CarriedItems.SetNum(NewCapacity, EAllowShrinking::No);
	RebuildCarriedResourceAmounts();
	OnCarriedResourcesChanged.Broadcast(GetCarriedItemCount(), GetCarryCapacity());
	return true;
}

void UJTSCarryComponent::NotifyCapacityChanged()
{
	OnCarriedResourcesChanged.Broadcast(GetCarriedItemCount(), GetCarryCapacity());
}

void UJTSCarryComponent::RestoreCarriedItems(const TArray<EJTSResourceType>& NewItems)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}
	CarriedItems = NewItems;
	CarriedItems.SetNum(FMath::Min(CarriedItems.Num(), GetCarryCapacity()), EAllowShrinking::No);
	RebuildCarriedResourceAmounts();
	OnCarriedResourcesChanged.Broadcast(GetCarriedItemCount(), GetCarryCapacity());
}

void UJTSCarryComponent::OnRep_CarriedItems()
{
	RebuildCarriedResourceAmounts();
	OnCarriedResourcesChanged.Broadcast(GetCarriedItemCount(), GetCarryCapacity());
}

void UJTSCarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSCarryComponent, CarriedItems);
}

int32 UJTSCarryComponent::GetEquipmentCapacityBonus() const
{
	const UJTSPlayerEquipmentComponent* const EquipmentComponent = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UJTSPlayerEquipmentComponent>()
		: nullptr;
	return IsValid(EquipmentComponent) ? EquipmentComponent->GetInventoryCapacityBonus() : 0;
}

void UJTSCarryComponent::RebuildCarriedResourceAmounts()
{
	CarriedResources.Reset();
	for (const EJTSResourceType ResourceType : CarriedItems)
	{
		++CarriedResources.FindOrAdd(ResourceType);
	}
}
