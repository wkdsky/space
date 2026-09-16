// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSCarryComponent.h"

#include "space/Components/JTSInventoryComponent.h"
#include "space/Items/JTSItemDefinitionLibrary.h"

UJTSCarryComponent::UJTSCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UJTSCarryComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UJTSInventoryComponent* const Inventory = GetInventoryComponent())
	{
		Inventory->OnInventoryChanged.AddDynamic(this, &UJTSCarryComponent::HandleInventoryChanged);
	}
	RebuildCarriedResourceAmounts();
}

bool UJTSCarryComponent::CanCarryResource(EJTSResourceType ResourceType) const
{
	const UJTSInventoryComponent* const Inventory = GetInventoryComponent();
	return IsValid(Inventory) && Inventory->CanAddItem(UJTSItemDefinitionLibrary::GetItemIdForResource(ResourceType), 1);
}

bool UJTSCarryComponent::CanCarryResources(int32 ResourceAmount) const
{
	if (ResourceAmount <= 0)
	{
		return false;
	}
	// The historic API did not carry a type. All regular materials use identical stack rules in v1.
	const UJTSInventoryComponent* const Inventory = GetInventoryComponent();
	return IsValid(Inventory) && Inventory->CanAddItem(EJTSItemId::Rock, ResourceAmount);
}

bool UJTSCarryComponent::TryAddResource(EJTSResourceType ResourceType)
{
	return TryAddResources(ResourceType, 1);
}

bool UJTSCarryComponent::TryAddResources(EJTSResourceType ResourceType, int32 ResourceAmount)
{
	UJTSInventoryComponent* const Inventory = GetInventoryComponent();
	const EJTSItemId ItemId = UJTSItemDefinitionLibrary::GetItemIdForResource(ResourceType);
	return IsValid(Inventory) && ItemId != EJTSItemId::None && Inventory->TryAddItemById(ItemId, ResourceAmount);
}

bool UJTSCarryComponent::TryTakeAllResources(TMap<EJTSResourceType, int32>& OutResources)
{
	if (UJTSInventoryComponent* const Inventory = GetInventoryComponent())
	{
		return Inventory->TryTakeAllResources(OutResources);
	}
	OutResources.Reset();
	return false;
}

int32 UJTSCarryComponent::GetCarriedItemCount() const
{
	int32 Result = 0;
	for (const TPair<EJTSResourceType, int32>& Pair : GetCarriedResources())
	{
		Result += Pair.Value;
	}
	return Result;
}

int32 UJTSCarryComponent::GetBaseCapacity() const
{
	const UJTSInventoryComponent* const Inventory = GetInventoryComponent();
	return IsValid(Inventory) ? Inventory->GetBaseInventoryCapacity() : 0;
}

int32 UJTSCarryComponent::GetCarryCapacity() const
{
	const UJTSInventoryComponent* const Inventory = GetInventoryComponent();
	return IsValid(Inventory) ? Inventory->GetInventoryCapacity() : 0;
}

bool UJTSCarryComponent::IsFull() const
{
	const UJTSInventoryComponent* const Inventory = GetInventoryComponent();
	return IsValid(Inventory) && !Inventory->HasAvailableSlot();
}

int32 UJTSCarryComponent::GetCarriedResourceAmount(EJTSResourceType ResourceType) const
{
	const UJTSInventoryComponent* const Inventory = GetInventoryComponent();
	return IsValid(Inventory) ? Inventory->GetResourceAmount(ResourceType) : 0;
}

const TMap<EJTSResourceType, int32>& UJTSCarryComponent::GetCarriedResources() const
{
	RebuildCarriedResourceAmounts();
	return CarriedResources;
}

const TArray<EJTSResourceType>& UJTSCarryComponent::GetCarriedItems() const
{
	RebuildCarriedResourceAmounts();
	return CarriedItems;
}

bool UJTSCarryComponent::GetOverflowItemsForCapacity(int32 NewCapacity, TArray<EJTSResourceType>& OutOverflowItems) const
{
	OutOverflowItems.Reset();
	const TArray<EJTSResourceType>& Items = GetCarriedItems();
	if (NewCapacity < 0 || NewCapacity > Items.Num())
	{
		return NewCapacity >= 0;
	}
	for (int32 Index = NewCapacity; Index < Items.Num(); ++Index)
	{
		OutOverflowItems.Add(Items[Index]);
	}
	return true;
}

bool UJTSCarryComponent::CommitOverflowRemovalForCapacity(int32 NewCapacity, const TArray<EJTSResourceType>& ExpectedOverflowItems)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return false;
	}
	TArray<EJTSResourceType> CurrentOverflow;
	if (!GetOverflowItemsForCapacity(NewCapacity, CurrentOverflow) || CurrentOverflow != ExpectedOverflowItems)
	{
		return false;
	}
	UJTSInventoryComponent* const Inventory = GetInventoryComponent();
	if (!IsValid(Inventory))
	{
		return false;
	}
	for (const EJTSResourceType Resource : CurrentOverflow)
	{
		if (!Inventory->TryRemoveItem(UJTSItemDefinitionLibrary::GetItemIdForResource(Resource), 1))
		{
			return false;
		}
	}
	return true;
}

void UJTSCarryComponent::NotifyCapacityChanged()
{
	OnCarriedResourcesChanged.Broadcast(GetCarriedItemCount(), GetCarryCapacity());
}

void UJTSCarryComponent::RestoreCarriedItems(const TArray<EJTSResourceType>& NewItems)
{
	if (UJTSInventoryComponent* const Inventory = GetInventoryComponent())
	{
		Inventory->RestoreLegacyResources(NewItems);
	}
}

UJTSInventoryComponent* UJTSCarryComponent::GetInventoryComponent() const
{
	return GetOwner() != nullptr ? GetOwner()->FindComponentByClass<UJTSInventoryComponent>() : nullptr;
}

void UJTSCarryComponent::RebuildCarriedResourceAmounts() const
{
	CarriedItems.Reset();
	CarriedResources.Reset();
	if (const UJTSInventoryComponent* const Inventory = GetInventoryComponent())
	{
		CarriedResources = Inventory->GetResourceAmounts();
		for (const TPair<EJTSResourceType, int32>& Pair : CarriedResources)
		{
			for (int32 Index = 0; Index < Pair.Value; ++Index)
			{
				CarriedItems.Add(Pair.Key);
			}
		}
	}
}

void UJTSCarryComponent::HandleInventoryChanged(int32 /*UsedSlots*/, int32 /*Capacity*/)
{
	RebuildCarriedResourceAmounts();
	OnCarriedResourcesChanged.Broadcast(GetCarriedItemCount(), GetCarryCapacity());
}

void UJTSCarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}
