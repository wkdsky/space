// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Items/JTSItemTypes.h"

#include "JTSInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnJTSInventoryChanged, int32, UsedSlots, int32, Capacity);

/**
 * Replicated item-slot inventory. The current quickbar page presents up to nine physical slots in
 * one horizontal HUD row; every carried item, including former equipment, belongs here.
 */
UCLASS(ClassGroup = (Items), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr int32 MaximumQuickbarSlots = 9;

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

	/** Number of positions visible on the current quickbar page (never more than keys 1-9). */
	UFUNCTION(BlueprintPure, Category = "Inventory|Quickbar")
	int32 GetQuickbarSlotCount() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Quickbar")
	int32 GetQuickbarPageCount() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Quickbar")
	int32 GetQuickbarPageIndex() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Quickbar")
	int32 GetQuickbarPageStart() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Quickbar")
	FJTSItemInstance GetActiveItem() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Quickbar")
	EJTSItemId GetActiveItemId() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory|Quickbar")
	bool SelectQuickbarSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerSelectQuickbarSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Quickbar")
	bool SelectQuickbarPage(int32 PageIndex);

	UFUNCTION(Server, Reliable)
	void ServerSelectQuickbarPage(int32 PageIndex);

	/** Adds as much of an item payload as the inventory can contain. OutRemaining is zero on full success. */
	bool TryAddItem(const FJTSItemInstance& Item, int32& OutRemaining);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryAddItemById(EJTSItemId ItemId, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanAddItem(EJTSItemId ItemId, int32 Count = 1) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(EJTSItemId ItemId) const;

	/** The current progression cap for a stackable item; non-stackable items always return one. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetEffectiveStackLimit(EJTSItemId ItemId) const;

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

	/** Drops only Count units. The server validates the selected slot and preserves the remainder. */
	bool DropItemQuantityAtSlot(int32 SlotIndex, int32 Count);

	UFUNCTION(Server, Reliable)
	void ServerDropItemQuantityAtSlot(int32 SlotIndex, int32 Count);

	/** Permanently destroys only Count units. There is deliberately no refund path. */
	bool DestroyItemQuantityAtSlot(int32 SlotIndex, int32 Count);

	UFUNCTION(Server, Reliable)
	void ServerDestroyItemQuantityAtSlot(int32 SlotIndex, int32 Count);

	/** Called after a server-authoritative progression update that may have increased capacity. */
	void RefreshCapacityFromProgression();

	/**
	 * Server-side seamless-travel/save restore. Old oversized stacks are split to the current item
	 * rules. Items beyond the active capacity remain serialized as migration overflow until a
	 * progression capacity upgrade exposes them, never silently discarded.
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

	/** New characters start with exactly two physical inventory slots; progression adds capacity. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory", meta = (ClampMin = "2", UIMin = "2"))
	int32 BaseInventoryCapacity = 2;

	UPROPERTY(ReplicatedUsing = OnRep_ItemSlots, VisibleAnywhere, Category = "Inventory")
	TArray<FJTSItemInstance> ItemSlots;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedQuickbarSlot, VisibleAnywhere, Category = "Inventory|Quickbar")
	int32 SelectedQuickbarSlot = 0;

	/** Current page is owner-only presentation state; the selected absolute slot remains replicated for held-item visuals. */
	UPROPERTY(ReplicatedUsing = OnRep_QuickbarPageIndex, VisibleAnywhere, Category = "Inventory|Quickbar")
	int32 QuickbarPageIndex = 0;

	UFUNCTION()
	void OnRep_QuickbarPageIndex();
};
