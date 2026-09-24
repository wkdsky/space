// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSInventoryComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Player/JTSPlayerState.h"

UJTSInventoryComponent::UJTSInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UJTSInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		EnsureSlotCount();
	}
}

int32 UJTSInventoryComponent::GetBaseInventoryCapacity() const
{
	return FMath::Max(2, BaseInventoryCapacity);
}

int32 UJTSInventoryComponent::GetInventoryCapacity() const
{
	const APawn* const OwnerPawn = Cast<APawn>(GetOwner());
	const AJTSPlayerState* const PlayerState = IsValid(OwnerPawn) ? OwnerPawn->GetPlayerState<AJTSPlayerState>() : nullptr;
	return GetBaseInventoryCapacity() + (IsValid(PlayerState) ? PlayerState->GetInventorySlotCapacityBonus() : 0);
}

int32 UJTSInventoryComponent::GetUsedSlotCount() const
{
	int32 UsedSlotCount = 0;
	for (const FJTSItemInstance& Item : ItemSlots)
	{
		UsedSlotCount += Item.IsEmpty() ? 0 : 1;
	}
	return UsedSlotCount;
}

bool UJTSInventoryComponent::HasAvailableSlot() const
{
	const int32 Capacity = GetInventoryCapacity();
	for (int32 Index = 0; Index < Capacity; ++Index)
	{
		if (!ItemSlots.IsValidIndex(Index) || ItemSlots[Index].IsEmpty())
		{
			return true;
		}
	}
	return false;
}

FJTSItemInstance UJTSInventoryComponent::GetItemAtSlot(int32 SlotIndex) const
{
	return ItemSlots.IsValidIndex(SlotIndex) ? ItemSlots[SlotIndex] : FJTSItemInstance();
}

int32 UJTSInventoryComponent::GetSelectedQuickbarSlot() const
{
	return FMath::Clamp(SelectedQuickbarSlot, 0, FMath::Max(0, GetInventoryCapacity() - 1));
}

int32 UJTSInventoryComponent::GetQuickbarSlotCount() const
{
	return FMath::Clamp(GetInventoryCapacity() - GetQuickbarPageStart(), 0, MaximumQuickbarSlots);
}

int32 UJTSInventoryComponent::GetQuickbarPageCount() const
{
	return FMath::Max(1, (GetInventoryCapacity() + MaximumQuickbarSlots - 1) / MaximumQuickbarSlots);
}

int32 UJTSInventoryComponent::GetQuickbarPageIndex() const
{
	return FMath::Clamp(QuickbarPageIndex, 0, GetQuickbarPageCount() - 1);
}

int32 UJTSInventoryComponent::GetQuickbarPageStart() const
{
	return GetQuickbarPageIndex() * MaximumQuickbarSlots;
}

FJTSItemInstance UJTSInventoryComponent::GetActiveItem() const
{
	return GetItemAtSlot(GetSelectedQuickbarSlot());
}

EJTSItemId UJTSInventoryComponent::GetActiveItemId() const
{
	return GetActiveItem().ItemId;
}

bool UJTSInventoryComponent::SelectQuickbarSlot(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= GetInventoryCapacity())
	{
		return false;
	}
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		ServerSelectQuickbarSlot(SlotIndex);
		return true;
	}

	const int32 NewPageIndex = SlotIndex / MaximumQuickbarSlots;
	if (SelectedQuickbarSlot != SlotIndex || QuickbarPageIndex != NewPageIndex)
	{
		SelectedQuickbarSlot = SlotIndex;
		QuickbarPageIndex = NewPageIndex;
		NotifyInventoryChanged();
	}
	return true;
}

void UJTSInventoryComponent::ServerSelectQuickbarSlot_Implementation(int32 SlotIndex)
{
	SelectQuickbarSlot(SlotIndex);
}

bool UJTSInventoryComponent::SelectQuickbarPage(int32 PageIndex)
{
	if (PageIndex < 0 || PageIndex >= GetQuickbarPageCount())
	{
		return false;
	}
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		ServerSelectQuickbarPage(PageIndex);
		return true;
	}
	if (QuickbarPageIndex != PageIndex)
	{
		// Never leave selection on a hidden page: preserve the physical 1-9 position where
		// possible, then clamp the last short page to its final visible slot.
		const int32 SlotOffset = GetSelectedQuickbarSlot() % MaximumQuickbarSlots;
		const int32 NewPageStart = PageIndex * MaximumQuickbarSlots;
		SelectedQuickbarSlot = FMath::Min(
			NewPageStart + SlotOffset,
			FMath::Max(0, GetInventoryCapacity() - 1));
		QuickbarPageIndex = PageIndex;
		NotifyInventoryChanged();
	}
	return true;
}

void UJTSInventoryComponent::ServerSelectQuickbarPage_Implementation(int32 PageIndex)
{
	SelectQuickbarPage(PageIndex);
}

bool UJTSInventoryComponent::TryAddItem(const FJTSItemInstance& Item, int32& OutRemaining)
{
	OutRemaining = FMath::Max(0, Item.StackCount);
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || Item.IsEmpty()
		|| !UJTSItemDefinitionLibrary::IsGameplayItemAvailable(Item.ItemId))
	{
		return false;
	}

	EnsureSlotCount();
	const int32 InitialRemaining = OutRemaining;
	if (!InsertIntoSlots(ItemSlots, Item, GetInventoryCapacity(), OutRemaining))
	{
		return false;
	}
	if (OutRemaining != InitialRemaining)
	{
		NotifyInventoryChanged();
	}
	return OutRemaining == 0;
}

bool UJTSInventoryComponent::TryAddItemById(EJTSItemId ItemId, int32 Count)
{
	int32 Remaining = Count;
	return TryAddItem(UJTSItemDefinitionLibrary::MakeInstance(ItemId, Count), Remaining) && Remaining == 0;
}

bool UJTSInventoryComponent::CanAddItem(EJTSItemId ItemId, int32 Count) const
{
	if (!UJTSItemDefinitionLibrary::IsGameplayItemAvailable(ItemId) || Count <= 0)
	{
		return false;
	}
	TArray<FJTSItemInstance> SimulatedSlots = ItemSlots;
	SimulatedSlots.SetNum(FMath::Max(SimulatedSlots.Num(), GetInventoryCapacity()));
	int32 Remaining = Count;
	return InsertIntoSlots(
		SimulatedSlots,
		UJTSItemDefinitionLibrary::MakeInstance(ItemId, Count),
		GetInventoryCapacity(),
		Remaining)
		&& Remaining == 0;
}

int32 UJTSInventoryComponent::GetItemCount(EJTSItemId ItemId) const
{
	int32 Result = 0;
	for (const FJTSItemInstance& Item : ItemSlots)
	{
		if (Item.ItemId == ItemId && !Item.IsEmpty())
		{
			Result += Item.StackCount;
		}
	}
	return Result;
}

int32 UJTSInventoryComponent::GetEffectiveStackLimit(const EJTSItemId ItemId) const
{
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
	if (!IsValid(Definition) || !Definition->IsStackable())
	{
		return 1;
	}

	const APawn* const OwnerPawn = Cast<APawn>(GetOwner());
	const AJTSPlayerState* const PlayerState = IsValid(OwnerPawn) ? OwnerPawn->GetPlayerState<AJTSPlayerState>() : nullptr;
	return IsValid(PlayerState) ? FMath::Max(1, PlayerState->GetItemStackLimit()) : 1;
}

bool UJTSInventoryComponent::TryRemoveItem(EJTSItemId ItemId, int32 Count)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || ItemId == EJTSItemId::None || Count <= 0 || GetItemCount(ItemId) < Count)
	{
		return false;
	}

	int32 Remaining = Count;
	for (FJTSItemInstance& Item : ItemSlots)
	{
		if (Item.ItemId != ItemId || Item.IsEmpty() || Remaining <= 0)
		{
			continue;
		}
		const int32 Removed = FMath::Min(Item.StackCount, Remaining);
		Item.StackCount -= Removed;
		Remaining -= Removed;
		if (Item.StackCount <= 0)
		{
			Item.Clear();
		}
	}
	NotifyInventoryChanged();
	return true;
}

bool UJTSInventoryComponent::TryTakeAllResources(TMap<EJTSResourceType, int32>& OutResources)
{
	OutResources.Reset();
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return false;
	}

	for (const FJTSItemInstance& Item : ItemSlots)
	{
		EJTSResourceType ResourceType = EJTSResourceType::Rock;
		if (!Item.IsEmpty() && UJTSItemDefinitionLibrary::TryGetResourceType(Item.ItemId, ResourceType))
		{
			OutResources.FindOrAdd(ResourceType) += Item.StackCount;
		}
	}
	if (OutResources.IsEmpty())
	{
		return false;
	}

	for (FJTSItemInstance& Item : ItemSlots)
	{
		EJTSResourceType ResourceType = EJTSResourceType::Rock;
		if (!Item.IsEmpty() && UJTSItemDefinitionLibrary::TryGetResourceType(Item.ItemId, ResourceType))
		{
			Item.Clear();
		}
	}
	NotifyInventoryChanged();
	return true;
}

bool UJTSInventoryComponent::TryAddResources(const TMap<EJTSResourceType, int32>& Resources)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || Resources.IsEmpty())
	{
		return false;
	}
	for (const TPair<EJTSResourceType, int32>& Resource : Resources)
	{
		if (!CanAddItem(UJTSItemDefinitionLibrary::GetItemIdForResource(Resource.Key), Resource.Value))
		{
			return false;
		}
	}

	for (const TPair<EJTSResourceType, int32>& Resource : Resources)
	{
		int32 Remaining = Resource.Value;
		TryAddItem(UJTSItemDefinitionLibrary::MakeInstance(UJTSItemDefinitionLibrary::GetItemIdForResource(Resource.Key), Resource.Value), Remaining);
	}
	return true;
}

int32 UJTSInventoryComponent::GetResourceAmount(EJTSResourceType ResourceType) const
{
	return GetItemCount(UJTSItemDefinitionLibrary::GetItemIdForResource(ResourceType));
}

TMap<EJTSResourceType, int32> UJTSInventoryComponent::GetResourceAmounts() const
{
	TMap<EJTSResourceType, int32> Result;
	for (const FJTSItemInstance& Item : ItemSlots)
	{
		EJTSResourceType ResourceType = EJTSResourceType::Rock;
		if (!Item.IsEmpty() && UJTSItemDefinitionLibrary::TryGetResourceType(Item.ItemId, ResourceType))
		{
			Result.FindOrAdd(ResourceType) += Item.StackCount;
		}
	}
	return Result;
}

bool UJTSInventoryComponent::DropItemAtSlot(int32 SlotIndex)
{
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		ServerDropItemAtSlot(SlotIndex);
		return true;
	}
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || !ItemSlots.IsValidIndex(SlotIndex) || ItemSlots[SlotIndex].IsEmpty())
	{
		return false;
	}
	return DropItemQuantityAtSlot(SlotIndex, ItemSlots[SlotIndex].StackCount);
}

bool UJTSInventoryComponent::DropItemQuantityAtSlot(int32 SlotIndex, int32 Count)
{
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		ServerDropItemQuantityAtSlot(SlotIndex, Count);
		return Count > 0;
	}
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || !ItemSlots.IsValidIndex(SlotIndex)
		|| ItemSlots[SlotIndex].IsEmpty() || Count <= 0 || Count > ItemSlots[SlotIndex].StackCount)
	{
		return false;
	}

	APawn* const OwnerPawn = Cast<APawn>(GetOwner());
	FJTSItemInstance DroppedItem = ItemSlots[SlotIndex];
	DroppedItem.StackCount = Count;
	if (Count != ItemSlots[SlotIndex].StackCount)
	{
		DroppedItem.InstanceId = FGuid::NewGuid();
	}
	AJTSWorldPickupActor* const Pickup = IsValid(OwnerPawn)
		? AJTSWorldPickupActor::SpawnGameplayDrop(GetWorld(), DroppedItem, OwnerPawn->GetActorLocation(), OwnerPawn, OwnerPawn, OwnerPawn->GetActorForwardVector())
		: nullptr;
	if (!IsValid(Pickup))
	{
		return false;
	}
	ItemSlots[SlotIndex].StackCount -= Count;
	if (ItemSlots[SlotIndex].StackCount <= 0)
	{
		ItemSlots[SlotIndex].Clear();
	}
	NotifyInventoryChanged();
	return true;
}

void UJTSInventoryComponent::ServerDropItemAtSlot_Implementation(int32 SlotIndex)
{
	if (SlotIndex == GetSelectedQuickbarSlot())
	{
		DropItemAtSlot(SlotIndex);
	}
}

void UJTSInventoryComponent::ServerDropItemQuantityAtSlot_Implementation(int32 SlotIndex, int32 Count)
{
	if (SlotIndex == GetSelectedQuickbarSlot())
	{
		DropItemQuantityAtSlot(SlotIndex, Count);
	}
}

bool UJTSInventoryComponent::DestroyItemQuantityAtSlot(int32 SlotIndex, int32 Count)
{
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		ServerDestroyItemQuantityAtSlot(SlotIndex, Count);
		return Count > 0;
	}
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || !ItemSlots.IsValidIndex(SlotIndex)
		|| ItemSlots[SlotIndex].IsEmpty() || Count <= 0 || Count > ItemSlots[SlotIndex].StackCount)
	{
		return false;
	}

	ItemSlots[SlotIndex].StackCount -= Count;
	if (ItemSlots[SlotIndex].StackCount <= 0)
	{
		ItemSlots[SlotIndex].Clear();
	}
	NotifyInventoryChanged();
	return true;
}

void UJTSInventoryComponent::ServerDestroyItemQuantityAtSlot_Implementation(int32 SlotIndex, int32 Count)
{
	if (SlotIndex == GetSelectedQuickbarSlot())
	{
		DestroyItemQuantityAtSlot(SlotIndex, Count);
	}
}

void UJTSInventoryComponent::RefreshCapacityFromProgression()
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		NotifyInventoryChanged();
		return;
	}

	const int32 PreviousSlotCount = ItemSlots.Num();
	EnsureSlotCount();
	if (ItemSlots.Num() != PreviousSlotCount)
	{
		NotifyInventoryChanged();
	}
}

void UJTSInventoryComponent::RestoreItems(const TArray<FJTSItemInstance>& NewSlots, int32 NewSelectedQuickbarSlot)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}

	auto FinalizeRestore = [this, NewSelectedQuickbarSlot]()
	{
		for (FJTSItemInstance& Item : ItemSlots)
		{
			if (Item.IsEmpty())
			{
				Item.Clear();
			}
		}
		SelectedQuickbarSlot = FMath::Clamp(NewSelectedQuickbarSlot, 0, FMath::Max(0, GetInventoryCapacity() - 1));
		QuickbarPageIndex = SelectedQuickbarSlot / MaximumQuickbarSlots;
		NotifyInventoryChanged();
	};

	// Save files written before the two-slot, one-resource-per-slot rules can contain both
	// oversized inventories and resource stacks. Normalize them without throwing away any saved
	// payload: only active-capacity slots are exposed now, while migration overflow stays
	// serialized and becomes visible again after a progression capacity upgrade.
	const int32 Capacity = GetInventoryCapacity();
	TArray<FJTSItemInstance> RestoredSlots;
	RestoredSlots.SetNum(Capacity);
	TArray<FJTSItemInstance> OverflowItems;
	for (int32 SavedSlotIndex = 0; SavedSlotIndex < NewSlots.Num(); ++SavedSlotIndex)
	{
		const FJTSItemInstance& SavedItem = NewSlots[SavedSlotIndex];
		if (SavedItem.IsEmpty())
		{
			continue;
		}

		if (!UJTSItemDefinitionLibrary::IsGameplayItemAvailable(SavedItem.ItemId))
		{
			continue;
		}
		const int32 StackLimit = GetEffectiveStackLimit(SavedItem.ItemId);
		const bool bSplitSavedStack = SavedItem.StackCount > StackLimit;
		int32 Remaining = SavedItem.StackCount;
		bool bPlacedFirstChunk = false;
		while (Remaining > 0)
		{
			FJTSItemInstance NormalizedItem = SavedItem;
			NormalizedItem.StackCount = FMath::Min(StackLimit, Remaining);
			if (bSplitSavedStack || !NormalizedItem.InstanceId.IsValid())
			{
				NormalizedItem.InstanceId = FGuid::NewGuid();
			}

			if (!bPlacedFirstChunk && SavedSlotIndex < Capacity)
			{
				// Preserve the visible order of existing slot zero/one items. A stack's
				// remaining units deliberately move behind the active capacity boundary.
				RestoredSlots[SavedSlotIndex] = NormalizedItem;
				bPlacedFirstChunk = true;
			}
			else
			{
				OverflowItems.Add(NormalizedItem);
			}
			Remaining -= NormalizedItem.StackCount;
		}
	}

	RestoredSlots.Append(OverflowItems);
	ItemSlots = MoveTemp(RestoredSlots);
	FinalizeRestore();
}

void UJTSInventoryComponent::RestoreLegacyResources(const TArray<EJTSResourceType>& Resources)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}
	TArray<FJTSItemInstance> LegacyItems;
	for (const EJTSResourceType Resource : Resources)
	{
		LegacyItems.Add(UJTSItemDefinitionLibrary::MakeInstance(UJTSItemDefinitionLibrary::GetItemIdForResource(Resource)));
	}
	RestoreItems(LegacyItems, 0);
}

bool UJTSInventoryComponent::InsertIntoSlots(TArray<FJTSItemInstance>& InOutSlots, const FJTSItemInstance& Item, int32 Capacity, int32& OutRemaining) const
{
	OutRemaining = FMath::Max(0, Item.StackCount);
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, Item.ItemId);
	if (!IsValid(Definition) || Item.IsEmpty() || Capacity <= 0)
	{
		return false;
	}
	InOutSlots.SetNum(FMath::Max(InOutSlots.Num(), Capacity));
	const int32 StackLimit = GetEffectiveStackLimit(Item.ItemId);

	if (Definition->IsStackable())
	{
		for (int32 SlotIndex = 0; SlotIndex < Capacity && OutRemaining > 0; ++SlotIndex)
		{
			FJTSItemInstance& Existing = InOutSlots[SlotIndex];
			if (Existing.ItemId != Item.ItemId || Existing.IsEmpty() || Existing.StackCount >= StackLimit)
			{
				continue;
			}
			const int32 Added = FMath::Min(StackLimit - Existing.StackCount, OutRemaining);
			Existing.StackCount += Added;
			OutRemaining -= Added;
		}
	}

	const int32 QuickbarCapacity = FMath::Min(MaximumQuickbarSlots, Capacity);
	const bool bPreferQuickbar = Definition->IsHoldable() && !Definition->IsStackable();
	auto FillEmptySlotRange = [&InOutSlots, &Item, &OutRemaining, StackLimit](const int32 StartIndex, const int32 EndIndex)
	{
		for (int32 SlotIndex = StartIndex; SlotIndex < EndIndex && OutRemaining > 0; ++SlotIndex)
		{
			if (!InOutSlots[SlotIndex].IsEmpty())
			{
				continue;
			}
			FJTSItemInstance NewItem = Item;
			NewItem.StackCount = FMath::Min(StackLimit, OutRemaining);
			if (!NewItem.InstanceId.IsValid() || NewItem.StackCount != Item.StackCount)
			{
				NewItem.InstanceId = FGuid::NewGuid();
			}
			InOutSlots[SlotIndex] = NewItem;
			OutRemaining -= NewItem.StackCount;
		}
	};

	// Combat/tools become immediately usable through the exposed quickbar. Stackable materials stay
	// out of those slots while normal inventory space exists, so a manual pickup never silently
	// hides a new tool. An empty selected slot is explicit player intent, so fill it first: the
	// inventory notification then refreshes the held-item visual without changing the player's
	// selected slot on their behalf.
	if (bPreferQuickbar)
	{
		const int32 PreferredQuickbarSlot = FMath::Clamp(GetSelectedQuickbarSlot(), 0, QuickbarCapacity - 1);
		FillEmptySlotRange(PreferredQuickbarSlot, PreferredQuickbarSlot + 1);
		FillEmptySlotRange(0, QuickbarCapacity);
		FillEmptySlotRange(QuickbarCapacity, Capacity);
	}
	else
	{
		FillEmptySlotRange(QuickbarCapacity, Capacity);
		FillEmptySlotRange(0, QuickbarCapacity);
	}
	return true;
}

void UJTSInventoryComponent::EnsureSlotCount()
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}
	const int32 Capacity = GetInventoryCapacity();
	if (ItemSlots.Num() < Capacity)
	{
		ItemSlots.SetNum(Capacity);
	}
	else if (ItemSlots.Num() > Capacity)
	{
		for (int32 SlotIndex = Capacity; SlotIndex < ItemSlots.Num(); ++SlotIndex)
		{
			// Compatibility overflow is intentionally retained until it is either made
			// visible by an expanded capacity or removed through the explicit safe
			// migration overflow path, preserved until progression grows capacity.
			if (!ItemSlots[SlotIndex].IsEmpty())
			{
				return;
			}
		}
		ItemSlots.SetNum(Capacity, EAllowShrinking::No);
	}
}

void UJTSInventoryComponent::NotifyInventoryChanged()
{
	OnInventoryChanged.Broadcast(GetUsedSlotCount(), GetInventoryCapacity());
}

void UJTSInventoryComponent::OnRep_ItemSlots()
{
	NotifyInventoryChanged();
}

void UJTSInventoryComponent::OnRep_SelectedQuickbarSlot()
{
	NotifyInventoryChanged();
}

void UJTSInventoryComponent::OnRep_QuickbarPageIndex()
{
	NotifyInventoryChanged();
}

void UJTSInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSInventoryComponent, ItemSlots);
	DOREPLIFETIME(UJTSInventoryComponent, SelectedQuickbarSlot);
	DOREPLIFETIME_CONDITION(UJTSInventoryComponent, QuickbarPageIndex, COND_OwnerOnly);
}
