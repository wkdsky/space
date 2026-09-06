#include "space/Components/JTSPlayerEquipmentComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "space/Components/JTSCarryComponent.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Items/JTSWorldPickupItemType.h"

namespace
{
	bool TryGetPickupItemType(EJTSResourceType ResourceType, EJTSWorldPickupItemType& OutPickupItemType)
	{
		switch (ResourceType)
		{
		case EJTSResourceType::Fuel:
			OutPickupItemType = EJTSWorldPickupItemType::Fuel;
			return true;

		case EJTSResourceType::Water:
			OutPickupItemType = EJTSWorldPickupItemType::Water;
			return true;

		case EJTSResourceType::Food:
			OutPickupItemType = EJTSWorldPickupItemType::Food;
			return true;

		case EJTSResourceType::Rock:
			OutPickupItemType = EJTSWorldPickupItemType::Rock;
			return true;

		case EJTSResourceType::Ore:
			OutPickupItemType = EJTSWorldPickupItemType::Ore;
			return true;

		default:
			return false;
		}
	}

	bool TryGetPickupItemType(EJTSEquipmentType EquipmentType, EJTSWorldPickupItemType& OutPickupItemType)
	{
		switch (EquipmentType)
		{
		case EJTSEquipmentType::Pickaxe:
			OutPickupItemType = EJTSWorldPickupItemType::Pickaxe;
			return true;

		case EJTSEquipmentType::Backpack:
			OutPickupItemType = EJTSWorldPickupItemType::Backpack;
			return true;

		default:
			return false;
		}
	}

	void DestroySpawnedPickups(TArray<AJTSWorldPickupActor*>& SpawnedPickups)
	{
		for (AJTSWorldPickupActor* const Pickup : SpawnedPickups)
		{
			if (IsValid(Pickup))
			{
				Pickup->Destroy();
			}
		}
		SpawnedPickups.Reset();
	}
}

UJTSPlayerEquipmentComponent::UJTSPlayerEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	EquipmentSlots.Init(EJTSEquipmentType::None, EquipmentCapacity);
}

bool UJTSPlayerEquipmentComponent::HasEquippedItem(EJTSEquipmentType EquipmentType) const
{
	return EquipmentType != EJTSEquipmentType::None && EquipmentSlots.Contains(EquipmentType);
}

bool UJTSPlayerEquipmentComponent::TryEquipItem(EJTSEquipmentType EquipmentType)
{
	if (EquipmentType == EJTSEquipmentType::None || HasEquippedItem(EquipmentType))
	{
		return false;
	}

	const int32 EmptySlotIndex = EquipmentSlots.IndexOfByKey(EJTSEquipmentType::None);
	if (EmptySlotIndex == INDEX_NONE)
	{
		return false;
	}

	EquipmentSlots[EmptySlotIndex] = EquipmentType;
	if (EquipmentType == EJTSEquipmentType::Pickaxe
		&& !HasActiveTool(EJTSEquipmentType::Pickaxe))
	{
		SelectedEquipmentSlotIndex = EmptySlotIndex;
	}
	NotifyEquipmentChanged();
	return true;
}

bool UJTSPlayerEquipmentComponent::CanUnequipItem(EJTSEquipmentType EquipmentType) const
{
	return EquipmentType != EJTSEquipmentType::None && HasEquippedItem(EquipmentType);
}

bool UJTSPlayerEquipmentComponent::UnequipItem(EJTSEquipmentType EquipmentType)
{
	return TryUnequipItemInternal(EquipmentType, false);
}

bool UJTSPlayerEquipmentComponent::DropEquippedItem(EJTSEquipmentType EquipmentType)
{
	return TryUnequipItemInternal(EquipmentType, true);
}

bool UJTSPlayerEquipmentComponent::DropEquippedItemAtSlot(int32 SlotIndex)
{
	if (!EquipmentSlots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	const EJTSEquipmentType EquipmentType = EquipmentSlots[SlotIndex];
	return EquipmentType != EJTSEquipmentType::None && TryUnequipItemInternal(EquipmentType, true);
}

void UJTSPlayerEquipmentComponent::UnequipAll()
{
	for (int32 SlotIndex = 0; SlotIndex < EquipmentSlots.Num(); ++SlotIndex)
	{
		const EJTSEquipmentType EquipmentType = EquipmentSlots[SlotIndex];
		if (EquipmentType != EJTSEquipmentType::None)
		{
			UnequipItem(EquipmentType);
		}
	}
}

int32 UJTSPlayerEquipmentComponent::GetEquippedItemCount() const
{
	int32 EquippedItemCount = 0;
	for (const EJTSEquipmentType EquipmentType : EquipmentSlots)
	{
		if (EquipmentType != EJTSEquipmentType::None)
		{
			++EquippedItemCount;
		}
	}

	return EquippedItemCount;
}

int32 UJTSPlayerEquipmentComponent::GetEquipmentCapacity() const
{
	return EquipmentCapacity;
}

bool UJTSPlayerEquipmentComponent::HasAvailableSlot() const
{
	return EquipmentSlots.Contains(EJTSEquipmentType::None);
}

int32 UJTSPlayerEquipmentComponent::GetInventoryCapacityBonus() const
{
	return HasEquippedItem(EJTSEquipmentType::Backpack) ? BackpackInventoryCapacityBonus : 0;
}

EJTSEquipmentType UJTSPlayerEquipmentComponent::GetEquipmentSlot(int32 SlotIndex) const
{
	return EquipmentSlots.IsValidIndex(SlotIndex)
		? EquipmentSlots[SlotIndex]
		: EJTSEquipmentType::None;
}

bool UJTSPlayerEquipmentComponent::SelectEquipmentSlot(int32 SlotIndex)
{
	if (!EquipmentSlots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	SelectedEquipmentSlotIndex = SlotIndex;
	NotifyEquipmentChanged();
	return true;
}

int32 UJTSPlayerEquipmentComponent::GetSelectedEquipmentSlotIndex() const
{
	return EquipmentSlots.IsValidIndex(SelectedEquipmentSlotIndex) ? SelectedEquipmentSlotIndex : 0;
}

int32 UJTSPlayerEquipmentComponent::GetEquipmentSlotIndex(EJTSEquipmentType EquipmentType) const
{
	return EquipmentType == EJTSEquipmentType::None ? INDEX_NONE : EquipmentSlots.IndexOfByKey(EquipmentType);
}

bool UJTSPlayerEquipmentComponent::HasActiveTool(EJTSEquipmentType EquipmentType) const
{
	return EquipmentType != EJTSEquipmentType::None
		&& GetEquipmentSlot(GetSelectedEquipmentSlotIndex()) == EquipmentType;
}

const TArray<EJTSEquipmentType>& UJTSPlayerEquipmentComponent::GetEquipmentSlots() const
{
	return EquipmentSlots;
}

bool UJTSPlayerEquipmentComponent::TryUnequipItemInternal(EJTSEquipmentType EquipmentType, bool bDropEquipmentPickup)
{
	if (!CanUnequipItem(EquipmentType))
	{
		return false;
	}

	const int32 EquippedSlotIndex = EquipmentSlots.IndexOfByKey(EquipmentType);
	if (EquippedSlotIndex == INDEX_NONE)
	{
		return false;
	}

	UJTSCarryComponent* const CarryComponent = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UJTSCarryComponent>()
		: nullptr;
	TArray<EJTSResourceType> OverflowItems;
	if (EquipmentType == EJTSEquipmentType::Backpack && IsValid(CarryComponent)
		&& !CarryComponent->GetOverflowItemsForCapacity(CarryComponent->GetBaseCapacity(), OverflowItems))
	{
		return false;
	}

	const bool bNeedsWorldPickup = bDropEquipmentPickup || !OverflowItems.IsEmpty();
	APawn* const OwnerPawn = Cast<APawn>(GetOwner());
	UWorld* const World = GetWorld();
	if (bNeedsWorldPickup && (!IsValid(OwnerPawn) || World == nullptr))
	{
		return false;
	}

	TArray<AJTSWorldPickupActor*> SpawnedPickups;
	auto SpawnPickup = [World, OwnerPawn, &SpawnedPickups](EJTSWorldPickupItemType PickupItemType)
	{
		AJTSWorldPickupActor* const Pickup = AJTSWorldPickupActor::SpawnGroundedPickup(
			World,
			PickupItemType,
			OwnerPawn->GetActorLocation(),
			OwnerPawn,
			OwnerPawn,
			OwnerPawn->GetActorForwardVector());
		if (!IsValid(Pickup))
		{
			return false;
		}

		SpawnedPickups.Add(Pickup);
		return true;
	};

	// Backpack overflow remains in the existing Carry slots until all resource drops and the backpack drop exist.
	for (const EJTSResourceType OverflowItem : OverflowItems)
	{
		EJTSWorldPickupItemType ResourcePickupItemType = EJTSWorldPickupItemType::Rock;
		if (!TryGetPickupItemType(OverflowItem, ResourcePickupItemType) || !SpawnPickup(ResourcePickupItemType))
		{
			DestroySpawnedPickups(SpawnedPickups);
			return false;
		}
	}

	if (bDropEquipmentPickup)
	{
		EJTSWorldPickupItemType EquipmentPickupItemType = EJTSWorldPickupItemType::Pickaxe;
		if (!TryGetPickupItemType(EquipmentType, EquipmentPickupItemType) || !SpawnPickup(EquipmentPickupItemType))
		{
			DestroySpawnedPickups(SpawnedPickups);
			return false;
		}
	}

	if (!OverflowItems.IsEmpty()
		&& (!IsValid(CarryComponent)
			|| !CarryComponent->CommitOverflowRemovalForCapacity(CarryComponent->GetBaseCapacity(), OverflowItems)))
	{
		DestroySpawnedPickups(SpawnedPickups);
		return false;
	}

	EquipmentSlots[EquippedSlotIndex] = EJTSEquipmentType::None;
	NotifyEquipmentChanged();

	if (EquipmentType == EJTSEquipmentType::Backpack)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("JumpToSpace Backpack Unequip: OverflowDropped=%d BackpackDropped=%s"),
			OverflowItems.Num(),
			bDropEquipmentPickup ? TEXT("true") : TEXT("false"));
	}
	else if (bDropEquipmentPickup)
	{
		UE_LOG(LogTemp, Log, TEXT("JumpToSpace Equipment Drop: Item=Pickaxe"));
	}

	return true;
}

void UJTSPlayerEquipmentComponent::NotifyEquipmentChanged()
{
	OnEquipmentChanged.Broadcast(GetEquippedItemCount());

	if (UJTSCarryComponent* const CarryComponent = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UJTSCarryComponent>()
		: nullptr)
	{
		CarryComponent->NotifyCapacityChanged();
	}
}
