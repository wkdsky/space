// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Items/JTSItemTypes.h"

#include "JTSWearableEquipmentComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJTSWearablesChanged, int32, EquippedWearableCount);

/** Dedicated body-worn equipment container. It does not store weapons, tools, or general items. */
UCLASS(ClassGroup = (Equipment), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSWearableEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSWearableEquipmentComponent();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Wearables")
	FJTSItemInstance GetWearable(EJTSWearableSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Wearables")
	bool HasWearable(EJTSWearableSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Wearables")
	int32 GetEquippedWearableCount() const;

	UFUNCTION(BlueprintPure, Category = "Wearables")
	int32 GetInventoryCapacityBonus() const;

	bool CanEquipItem(const FJTSItemInstance& Item) const;
	bool TryEquipItem(const FJTSItemInstance& Item);
	bool TryUnequipItem(EJTSWearableSlot Slot);
	bool DropWearableItem(EJTSWearableSlot Slot);

	const TArray<FJTSItemInstance>& GetWearableSlots() const { return WearableSlots; }
	void RestoreWearables(const TArray<FJTSItemInstance>& NewWearables);

	UPROPERTY(BlueprintAssignable, Category = "Wearables")
	FOnJTSWearablesChanged OnWearablesChanged;

protected:
	static int32 ToIndex(EJTSWearableSlot Slot);
	int32 GetInventoryCapacityBonusForSlot(int32 SlotIndex) const;
	void EnsureSlotCount();
	void NotifyWearablesChanged();

	UFUNCTION()
	void OnRep_WearableSlots();

	UPROPERTY(ReplicatedUsing = OnRep_WearableSlots, VisibleAnywhere, Category = "Wearables")
	TArray<FJTSItemInstance> WearableSlots;
};
