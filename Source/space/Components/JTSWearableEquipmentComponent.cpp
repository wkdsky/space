// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSWearableEquipmentComponent.h"

#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSWorldPickupActor.h"

UJTSWearableEquipmentComponent::UJTSWearableEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	WearableSlots.SetNum(4);
}

void UJTSWearableEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		EnsureSlotCount();
	}
}

int32 UJTSWearableEquipmentComponent::ToIndex(EJTSWearableSlot Slot)
{
	switch (Slot)
	{
	case EJTSWearableSlot::Backpack: return 0;
	case EJTSWearableSlot::Body: return 1;
	case EJTSWearableSlot::Head: return 2;
	case EJTSWearableSlot::Accessory: return 3;
	default: return INDEX_NONE;
	}
}

FJTSItemInstance UJTSWearableEquipmentComponent::GetWearable(EJTSWearableSlot Slot) const
{
	const int32 Index = ToIndex(Slot);
	return WearableSlots.IsValidIndex(Index) ? WearableSlots[Index] : FJTSItemInstance();
}

bool UJTSWearableEquipmentComponent::HasWearable(EJTSWearableSlot Slot) const
{
	return !GetWearable(Slot).IsEmpty();
}

int32 UJTSWearableEquipmentComponent::GetEquippedWearableCount() const
{
	int32 EquippedCount = 0;
	for (const FJTSItemInstance& Item : WearableSlots)
	{
		EquippedCount += Item.IsEmpty() ? 0 : 1;
	}
	return EquippedCount;
}

int32 UJTSWearableEquipmentComponent::GetInventoryCapacityBonus() const
{
	int32 Result = 0;
	for (const FJTSItemInstance& Item : WearableSlots)
	{
		if (!Item.IsEmpty())
		{
			if (const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, Item.ItemId))
			{
				Result += FMath::Max(0, Definition->InventoryCapacityBonus);
			}
		}
	}
	return Result;
}

int32 UJTSWearableEquipmentComponent::GetInventoryCapacityBonusForSlot(int32 SlotIndex) const
{
	if (!WearableSlots.IsValidIndex(SlotIndex) || WearableSlots[SlotIndex].IsEmpty())
	{
		return 0;
	}
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, WearableSlots[SlotIndex].ItemId);
	return IsValid(Definition) ? FMath::Max(0, Definition->InventoryCapacityBonus) : 0;
}

bool UJTSWearableEquipmentComponent::CanEquipItem(const FJTSItemInstance& Item) const
{
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, Item.ItemId);
	const int32 SlotIndex = IsValid(Definition) ? ToIndex(Definition->WearableSlot) : INDEX_NONE;
	return !Item.IsEmpty()
		&& Item.StackCount == 1
		&& IsValid(Definition)
		&& Definition->IsWearable()
		&& WearableSlots.IsValidIndex(SlotIndex)
		&& WearableSlots[SlotIndex].IsEmpty();
}

bool UJTSWearableEquipmentComponent::TryEquipItem(const FJTSItemInstance& Item)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || !CanEquipItem(Item))
	{
		return false;
	}
	EnsureSlotCount();
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, Item.ItemId);
	const int32 SlotIndex = ToIndex(Definition->WearableSlot);
	WearableSlots[SlotIndex] = Item;
	WearableSlots[SlotIndex].StackCount = 1;
	if (!WearableSlots[SlotIndex].InstanceId.IsValid())
	{
		WearableSlots[SlotIndex].InstanceId = FGuid::NewGuid();
	}
	NotifyWearablesChanged();
	return true;
}

bool UJTSWearableEquipmentComponent::TryUnequipItem(EJTSWearableSlot Slot)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return false;
	}
	const int32 SlotIndex = ToIndex(Slot);
	if (!WearableSlots.IsValidIndex(SlotIndex) || WearableSlots[SlotIndex].IsEmpty())
	{
		return false;
	}

	UJTSInventoryComponent* const Inventory = GetOwner()->FindComponentByClass<UJTSInventoryComponent>();
	if (!IsValid(Inventory))
	{
		return false;
	}

	// Unequipping never destroys an item.  A backpack can only come off when its
	// reduced capacity still contains every carried item and one ordinary slot is
	// available for the backpack instance itself.  Players may use the explicit
	// drop path when they want overflow converted into world pickups.
	const int32 NewCapacity = Inventory->GetInventoryCapacity() - GetInventoryCapacityBonusForSlot(SlotIndex);
	TArray<FJTSItemInstance> OverflowItems;
	if (!Inventory->GetOverflowItemsForCapacity(NewCapacity, OverflowItems)
		|| !OverflowItems.IsEmpty()
		|| Inventory->GetUsedSlotCount() >= NewCapacity)
	{
		return false;
	}

	const FJTSItemInstance UnequippedItem = WearableSlots[SlotIndex];
	WearableSlots[SlotIndex].Clear();
	int32 Remaining = UnequippedItem.StackCount;
	if (!Inventory->TryAddItem(UnequippedItem, Remaining) || Remaining != 0)
	{
		WearableSlots[SlotIndex] = UnequippedItem;
		return false;
	}
	NotifyWearablesChanged();
	return true;
}

bool UJTSWearableEquipmentComponent::DropWearableItem(EJTSWearableSlot Slot)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return false;
	}
	const int32 SlotIndex = ToIndex(Slot);
	if (!WearableSlots.IsValidIndex(SlotIndex) || WearableSlots[SlotIndex].IsEmpty())
	{
		return false;
	}

	APawn* const OwnerPawn = Cast<APawn>(GetOwner());
	UJTSInventoryComponent* const Inventory = GetOwner()->FindComponentByClass<UJTSInventoryComponent>();
	if (!IsValid(OwnerPawn) || !IsValid(Inventory))
	{
		return false;
	}

	const int32 NewCapacity = Inventory->GetInventoryCapacity() - GetInventoryCapacityBonusForSlot(SlotIndex);
	TArray<FJTSItemInstance> OverflowItems;
	if (!Inventory->GetOverflowItemsForCapacity(NewCapacity, OverflowItems))
	{
		return false;
	}

	TArray<AJTSWorldPickupActor*> SpawnedPickups;
	auto SpawnDrop = [this, OwnerPawn, &SpawnedPickups](const FJTSItemInstance& Item)
	{
		AJTSWorldPickupActor* const Pickup = AJTSWorldPickupActor::SpawnGameplayDrop(
			GetWorld(), Item, OwnerPawn->GetActorLocation(), OwnerPawn, OwnerPawn, OwnerPawn->GetActorForwardVector());
		if (!IsValid(Pickup))
		{
			return false;
		}
		SpawnedPickups.Add(Pickup);
		return true;
	};

	for (const FJTSItemInstance& Overflow : OverflowItems)
	{
		if (!SpawnDrop(Overflow))
		{
			for (AJTSWorldPickupActor* Pickup : SpawnedPickups) { if (IsValid(Pickup)) { Pickup->Destroy(); } }
			return false;
		}
	}
	if (!SpawnDrop(WearableSlots[SlotIndex]))
	{
		for (AJTSWorldPickupActor* Pickup : SpawnedPickups) { if (IsValid(Pickup)) { Pickup->Destroy(); } }
		return false;
	}
	if (!Inventory->CommitOverflowRemovalForCapacity(NewCapacity, OverflowItems))
	{
		for (AJTSWorldPickupActor* Pickup : SpawnedPickups) { if (IsValid(Pickup)) { Pickup->Destroy(); } }
		return false;
	}

	WearableSlots[SlotIndex].Clear();
	NotifyWearablesChanged();
	return true;
}

void UJTSWearableEquipmentComponent::RestoreWearables(const TArray<FJTSItemInstance>& NewWearables)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}
	WearableSlots = NewWearables;
	EnsureSlotCount();
	for (FJTSItemInstance& Item : WearableSlots)
	{
		if (Item.IsEmpty())
		{
			Item.Clear();
		}
	}
	NotifyWearablesChanged();
}

void UJTSWearableEquipmentComponent::EnsureSlotCount()
{
	WearableSlots.SetNum(4, EAllowShrinking::No);
}

void UJTSWearableEquipmentComponent::NotifyWearablesChanged()
{
	OnWearablesChanged.Broadcast(GetEquippedWearableCount());
}

void UJTSWearableEquipmentComponent::OnRep_WearableSlots()
{
	EnsureSlotCount();
	NotifyWearablesChanged();
}

void UJTSWearableEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSWearableEquipmentComponent, WearableSlots);
}
