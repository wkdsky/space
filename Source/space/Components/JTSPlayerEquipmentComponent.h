#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "JTSPlayerEquipmentComponent.generated.h"

/** Equipment is intentionally separate from the player's resource carry inventory. */
UENUM(BlueprintType)
enum class EJTSEquipmentType : uint8
{
	None UMETA(DisplayName = "Empty"),
	Pickaxe UMETA(DisplayName = "Pickaxe"),
	Backpack UMETA(DisplayName = "Backpack")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipmentChanged, int32, EquippedItemCount);

/** Stores the fixed four-slot equipment loadout for one player character. */
UCLASS(ClassGroup = (Equipment), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSPlayerEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSPlayerEquipmentComponent();

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool HasEquippedItem(EJTSEquipmentType EquipmentType) const;

	/** Equips a unique equipment item in the first empty fixed slot. */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool TryEquipItem(EJTSEquipmentType EquipmentType);

	/** Returns whether this item is currently equipped; Backpack overflow is preflighted during the operation. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool CanUnequipItem(EJTSEquipmentType EquipmentType) const;

	/** Removes equipment without creating an item pickup. Backpack overflow resources are safely dropped first. */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool UnequipItem(EJTSEquipmentType EquipmentType);

	/** Drops equipped gear as a World Pickup. Backpack overflow resources are committed only after every pickup spawns. */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool DropEquippedItem(EJTSEquipmentType EquipmentType);

	/** Drops the item in one explicit visual slot. Empty or invalid slots do nothing. */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool DropEquippedItemAtSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void UnequipAll();

	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetEquippedItemCount() const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetEquipmentCapacity() const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool HasAvailableSlot() const;

	/** Backpack contributes five slots to the existing Carry component; no second inventory is created. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetInventoryCapacityBonus() const;

	/** Returns one read-only visual slot value, or None for an invalid slot index. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	EJTSEquipmentType GetEquipmentSlot(int32 SlotIndex) const;

	/** Changes the selected equipment slot. Empty slots are valid selections. */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool SelectEquipmentSlot(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetSelectedEquipmentSlotIndex() const;

	/** Returns the slot currently containing a unique equipment type, or INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetEquipmentSlotIndex(EJTSEquipmentType EquipmentType) const;

	/** Pickaxe is active only while its containing slot is selected. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool HasActiveTool(EJTSEquipmentType EquipmentType) const;

	/** C++ read-only view for native presentation code. */
	const TArray<EJTSEquipmentType>& GetEquipmentSlots() const;

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FOnEquipmentChanged OnEquipmentChanged;

private:
	static constexpr int32 EquipmentCapacity = 4;
	static constexpr int32 BackpackInventoryCapacityBonus = 5;

	bool TryUnequipItemInternal(EJTSEquipmentType EquipmentType, bool bDropEquipmentPickup);
	void NotifyEquipmentChanged();

	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	TArray<EJTSEquipmentType> EquipmentSlots;

	/** The selected slot controls the active tool but never disables passive equipment. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	int32 SelectedEquipmentSlotIndex = 0;
};
