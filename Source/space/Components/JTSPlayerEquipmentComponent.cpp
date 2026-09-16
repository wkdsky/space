// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSPlayerEquipmentComponent.h"

#include "space/Components/JTSInventoryComponent.h"

UJTSPlayerEquipmentComponent::UJTSPlayerEquipmentComponent()
{
	LegacyQuickbarSlots.Init(EJTSEquipmentType::None, 4);
}

void UJTSPlayerEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	OnWearablesChanged.AddDynamic(this, &UJTSPlayerEquipmentComponent::HandleWearablesChanged);
	if (UJTSInventoryComponent* const Inventory = GetInventory())
	{
		Inventory->OnInventoryChanged.AddDynamic(this, &UJTSPlayerEquipmentComponent::HandleInventoryChanged);
	}
	NotifyEquipmentChanged();
}

EJTSItemId UJTSPlayerEquipmentComponent::ToItemId(EJTSEquipmentType EquipmentType)
{
	switch (EquipmentType)
	{
	case EJTSEquipmentType::Pickaxe: return EJTSItemId::Pickaxe;
	case EJTSEquipmentType::Backpack: return EJTSItemId::Backpack;
	case EJTSEquipmentType::Knife: return EJTSItemId::Knife;
	case EJTSEquipmentType::Axe: return EJTSItemId::Axe;
	default: return EJTSItemId::None;
	}
}

EJTSEquipmentType UJTSPlayerEquipmentComponent::ToEquipmentType(EJTSItemId ItemId)
{
	switch (ItemId)
	{
	case EJTSItemId::Pickaxe: return EJTSEquipmentType::Pickaxe;
	case EJTSItemId::Backpack: return EJTSEquipmentType::Backpack;
	case EJTSItemId::Knife: return EJTSEquipmentType::Knife;
	case EJTSItemId::Axe: return EJTSEquipmentType::Axe;
	default: return EJTSEquipmentType::None;
	}
}

UJTSInventoryComponent* UJTSPlayerEquipmentComponent::GetInventory() const
{
	return GetOwner() != nullptr ? GetOwner()->FindComponentByClass<UJTSInventoryComponent>() : nullptr;
}

bool UJTSPlayerEquipmentComponent::HasEquippedItem(EJTSEquipmentType EquipmentType) const
{
	if (EquipmentType == EJTSEquipmentType::Backpack)
	{
		return HasWearable(EJTSWearableSlot::Backpack);
	}
	if (const UJTSInventoryComponent* const Inventory = GetInventory())
	{
		return Inventory->GetItemCount(ToItemId(EquipmentType)) > 0;
	}
	return false;
}

bool UJTSPlayerEquipmentComponent::TryEquipItem(EJTSEquipmentType EquipmentType)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return false;
	}
	const EJTSItemId ItemId = ToItemId(EquipmentType);
	if (ItemId == EJTSItemId::None)
	{
		return false;
	}
	if (EquipmentType == EJTSEquipmentType::Backpack)
	{
		FJTSItemInstance Backpack;
		Backpack.ItemId = EJTSItemId::Backpack;
		Backpack.StackCount = 1;
		return UJTSWearableEquipmentComponent::TryEquipItem(Backpack);
	}
	return GetInventory() != nullptr && GetInventory()->TryAddItemById(ItemId);
}

bool UJTSPlayerEquipmentComponent::CanUnequipItem(EJTSEquipmentType EquipmentType) const
{
	return HasEquippedItem(EquipmentType);
}

bool UJTSPlayerEquipmentComponent::UnequipItem(EJTSEquipmentType EquipmentType)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return false;
	}
	if (EquipmentType == EJTSEquipmentType::Backpack)
	{
		return UJTSWearableEquipmentComponent::TryUnequipItem(EJTSWearableSlot::Backpack);
	}
	return GetInventory() != nullptr && GetInventory()->TryRemoveItem(ToItemId(EquipmentType), 1);
}

bool UJTSPlayerEquipmentComponent::DropEquippedItem(EJTSEquipmentType EquipmentType)
{
	if (EquipmentType == EJTSEquipmentType::Backpack)
	{
		return DropWearableItem(EJTSWearableSlot::Backpack);
	}
	return DropEquippedItemAtSlot(GetEquipmentSlotIndex(EquipmentType));
}

bool UJTSPlayerEquipmentComponent::DropEquippedItemAtSlot(int32 SlotIndex)
{
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		ServerDropEquipmentSlot(SlotIndex);
		return SlotIndex >= 0 && SlotIndex < 4;
	}
	return GetInventory() != nullptr && GetInventory()->DropItemAtSlot(SlotIndex);
}

void UJTSPlayerEquipmentComponent::UnequipAll()
{
	if (GetOwner() != nullptr && GetOwner()->HasAuthority() && HasWearable(EJTSWearableSlot::Backpack))
	{
		DropWearableItem(EJTSWearableSlot::Backpack);
	}
}

int32 UJTSPlayerEquipmentComponent::GetEquippedItemCount() const
{
	return GetEquippedWearableCount();
}

int32 UJTSPlayerEquipmentComponent::GetEquipmentCapacity() const
{
	return 4;
}

bool UJTSPlayerEquipmentComponent::HasAvailableSlot() const
{
	const UJTSInventoryComponent* const Inventory = GetInventory();
	return (Inventory != nullptr && Inventory->HasAvailableSlot()) || !HasWearable(EJTSWearableSlot::Backpack);
}

EJTSEquipmentType UJTSPlayerEquipmentComponent::GetEquipmentSlot(int32 SlotIndex) const
{
	if (const UJTSInventoryComponent* const Inventory = GetInventory())
	{
		return ToEquipmentType(Inventory->GetItemAtSlot(SlotIndex).ItemId);
	}
	return EJTSEquipmentType::None;
}

bool UJTSPlayerEquipmentComponent::SelectEquipmentSlot(int32 SlotIndex)
{
	return GetInventory() != nullptr && GetInventory()->SelectQuickbarSlot(SlotIndex);
}

void UJTSPlayerEquipmentComponent::ServerSelectEquipmentSlot_Implementation(int32 SlotIndex)
{
	SelectEquipmentSlot(SlotIndex);
}

void UJTSPlayerEquipmentComponent::ServerDropEquipmentSlot_Implementation(int32 SlotIndex)
{
	DropEquippedItemAtSlot(SlotIndex);
}

int32 UJTSPlayerEquipmentComponent::GetSelectedEquipmentSlotIndex() const
{
	return GetInventory() != nullptr ? GetInventory()->GetSelectedQuickbarSlot() : 0;
}

int32 UJTSPlayerEquipmentComponent::GetEquipmentSlotIndex(EJTSEquipmentType EquipmentType) const
{
	if (const UJTSInventoryComponent* const Inventory = GetInventory())
	{
		const TArray<FJTSItemInstance>& Slots = Inventory->GetItemSlots();
		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			if (Slots[SlotIndex].ItemId == ToItemId(EquipmentType))
			{
				return SlotIndex;
			}
		}
	}
	return INDEX_NONE;
}

bool UJTSPlayerEquipmentComponent::HasActiveTool(EJTSEquipmentType EquipmentType) const
{
	return GetInventory() != nullptr && GetInventory()->GetActiveItemId() == ToItemId(EquipmentType);
}

bool UJTSPlayerEquipmentComponent::HasActiveWeapon() const
{
	const EJTSItemId ActiveItemId = GetInventory() != nullptr ? GetInventory()->GetActiveItemId() : EJTSItemId::None;
	return ActiveItemId == EJTSItemId::Knife || ActiveItemId == EJTSItemId::Axe;
}

const TArray<EJTSEquipmentType>& UJTSPlayerEquipmentComponent::GetEquipmentSlots() const
{
	LegacyQuickbarSlots.Init(EJTSEquipmentType::None, 4);
	if (const UJTSInventoryComponent* const Inventory = GetInventory())
	{
		for (int32 SlotIndex = 0; SlotIndex < LegacyQuickbarSlots.Num(); ++SlotIndex)
		{
			LegacyQuickbarSlots[SlotIndex] = ToEquipmentType(Inventory->GetItemAtSlot(SlotIndex).ItemId);
		}
	}
	return LegacyQuickbarSlots;
}

void UJTSPlayerEquipmentComponent::HandleWearablesChanged(int32 EquippedWearableCount)
{
	NotifyEquipmentChanged();
}

void UJTSPlayerEquipmentComponent::HandleInventoryChanged(int32 UsedSlots, int32 Capacity)
{
	NotifyEquipmentChanged();
}

void UJTSPlayerEquipmentComponent::NotifyEquipmentChanged()
{
	OnEquipmentChanged.Broadcast(GetEquippedItemCount());
}
