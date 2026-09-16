// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "space/Components/JTSWearableEquipmentComponent.h"

#include "JTSPlayerEquipmentComponent.generated.h"

/**
 * Compatibility names retained for existing Blueprint classes and the retired Moon workshop.
 * New gameplay must use EJTSItemId / UJTSInventoryComponent for held items and this component
 * only for body-worn equipment.
 */
UENUM(BlueprintType)
enum class EJTSEquipmentType : uint8
{
	None UMETA(DisplayName = "Empty"),
	Pickaxe UMETA(DisplayName = "Pickaxe"),
	Backpack UMETA(DisplayName = "Backpack"),
	Knife UMETA(DisplayName = "Knife"),
	Axe UMETA(DisplayName = "Axe")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipmentChanged, int32, EquippedItemCount);

/**
 * Deprecated facade preserving the old component class identity.  Its replicated storage now
 * comes from UJTSWearableEquipmentComponent; tools are redirected into UJTSInventoryComponent.
 */
UCLASS(ClassGroup = (Equipment), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSPlayerEquipmentComponent : public UJTSWearableEquipmentComponent
{
	GENERATED_BODY()

public:
	UJTSPlayerEquipmentComponent();
	virtual void BeginPlay() override;
	using UJTSWearableEquipmentComponent::TryEquipItem;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool HasEquippedItem(EJTSEquipmentType EquipmentType) const;

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool TryEquipItem(EJTSEquipmentType EquipmentType);

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool CanUnequipItem(EJTSEquipmentType EquipmentType) const;

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool UnequipItem(EJTSEquipmentType EquipmentType);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool DropEquippedItem(EJTSEquipmentType EquipmentType);

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

	UFUNCTION(BlueprintPure, Category = "Equipment")
	EJTSEquipmentType GetEquipmentSlot(int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool SelectEquipmentSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerSelectEquipmentSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerDropEquipmentSlot(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetSelectedEquipmentSlotIndex() const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetEquipmentSlotIndex(EJTSEquipmentType EquipmentType) const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool HasActiveTool(EJTSEquipmentType EquipmentType) const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool HasActiveWeapon() const;

	/** Legacy visual view. It mirrors the four quickbar positions, not wearable slots. */
	const TArray<EJTSEquipmentType>& GetEquipmentSlots() const;

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FOnEquipmentChanged OnEquipmentChanged;

private:
	static EJTSItemId ToItemId(EJTSEquipmentType EquipmentType);
	static EJTSEquipmentType ToEquipmentType(EJTSItemId ItemId);
	class UJTSInventoryComponent* GetInventory() const;
	void NotifyEquipmentChanged();

	UFUNCTION()
	void HandleWearablesChanged(int32 EquippedWearableCount);

	UFUNCTION()
	void HandleInventoryChanged(int32 UsedSlots, int32 Capacity);

	mutable TArray<EJTSEquipmentType> LegacyQuickbarSlots;
};
