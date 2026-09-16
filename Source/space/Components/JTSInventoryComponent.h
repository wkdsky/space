// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Items/JTSItemTypes.h"

#include "JTSInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnJTSInventoryChanged, int32, UsedSlots, int32, Capacity);

/**
 * Replicated item-slot inventory. Up to the first four physical slots form the player's quickbar;
 * a new character exposes two, while wearable capacity can unlock the remaining quickbar slots.
 * Other slots are ordinary carried inventory. Item definitions decide stacking and capabilities.
 */
UCLASS(ClassGroup = (Items), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSInventoryComponent();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetInventoryCapacity() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetBaseInventoryCapacity() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetUsedSlotCount() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasAvailableSlot() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FJTSItemInstance GetItemAtSlot(int32 SlotIndex) const;

	const TArray<FJTSItemInstance>& GetItemSlots() const { return ItemSlots; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Quickbar")
	int32 GetSelectedQuickbarSlot() const;

	/** Number of currently exposed quickbar slots. A backpack can extend this up to four. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Quickbar")
	int32 GetQuickbarSlotCount() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Quickbar")
	FJTSItemInstance GetActiveItem() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Quickbar")
	EJTSItemId GetActiveItemId() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory|Quickbar")
	bool SelectQuickbarSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerSelectQuickbarSlot(int32 SlotIndex);

	/** Adds as much of an item payload as the inventory can contain. OutRemaining is zero on full success. */
	bool TryAddItem(const FJTSItemInstance& Item, int32& OutRemaining);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryAddItemById(EJTSItemId ItemId, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanAddItem(EJTSItemId ItemId, int32 Count = 1) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(EJTSItemId ItemId) const;

	bool TryRemoveItem(EJTSItemId ItemId, int32 Count);
	bool TryTakeAllResources(TMap<EJTSResourceType, int32>& OutResources);
	bool TryAddResources(const TMap<EJTSResourceType, int32>& Resources);
	int32 GetResourceAmount(EJTSResourceType ResourceType) const;
	TMap<EJTSResourceType, int32> GetResourceAmounts() const;

	/** Drops the complete slot payload after the server successfully creates a matching world pickup. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool DropItemAtSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerDropItemAtSlot(int32 SlotIndex);

	/** Used by the wearable layer before reducing capacity. */
	bool GetOverflowItemsForCapacity(int32 NewCapacity, TArray<FJTSItemInstance>& OutOverflowItems) const;
	bool CommitOverflowRemovalForCapacity(int32 NewCapacity, const TArray<FJTSItemInstance>& ExpectedOverflowItems);

	/**
	 * Server-side seamless-travel/save restore. Old oversized stacks are split to the current item
	 * rules. Items beyond the active capacity remain serialized in protected overflow slots until a
	 * wearable expands capacity, never silently discarded.
	 */
	void RestoreItems(const TArray<FJTSItemInstance>& NewSlots, int32 NewSelectedQuickbarSlot);
	void RestoreLegacyResources(const TArray<EJTSResourceType>& Resources);

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnJTSInventoryChanged OnInventoryChanged;

private:
	bool InsertIntoSlots(TArray<FJTSItemInstance>& InOutSlots, const FJTSItemInstance& Item, int32 Capacity, int32& OutRemaining) const;
	void EnsureSlotCount();
	void NotifyInventoryChanged();

	UFUNCTION()
	void OnRep_ItemSlots();

	UFUNCTION()
	void OnRep_SelectedQuickbarSlot();

	/** New characters start with exactly two physical inventory slots. Wearables can add capacity. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory", meta = (ClampMin = "2", UIMin = "2"))
	int32 BaseInventoryCapacity = 2;

	UPROPERTY(ReplicatedUsing = OnRep_ItemSlots, VisibleAnywhere, Category = "Inventory")
	TArray<FJTSItemInstance> ItemSlots;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedQuickbarSlot, VisibleAnywhere, Category = "Inventory|Quickbar")
	int32 SelectedQuickbarSlot = 0;
};
