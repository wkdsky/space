#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Items/JTSStellarLoadoutTypes.h"
#include "JTSStellarLoadoutComponent.generated.h"

class AJTSSpacecraftActor;
class UJTSStellarLootTable;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FJTSOnStellarLoadoutChanged);

/** PlayerState-owned equipment and one shared energy pool. No combat or asset presentation here. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSStellarLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSStellarLoadoutComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	const TArray<FJTSItemInstance>& GetSlots() const { return Slots; }
	UFUNCTION(BlueprintPure) FJTSItemInstance GetSlot(int32 Index) const;
	UFUNCTION(BlueprintPure) int32 GetAvailableSlots() const { return AvailableSlotCount; }
	UFUNCTION(BlueprintPure) int32 GetActiveCoreSlot() const { return ActiveCoreSlot; }
	UFUNCTION(BlueprintPure) float GetEnergy() const { return Energy; }
	UFUNCTION(BlueprintPure) TArray<FJTSStellarWeaponBinding> GetWeapons() const;
	/** Spendable banked, equipped and legacy inventory firmware; sealed equipment is excluded. */
	UFUNCTION(BlueprintPure) int32 GetAvailableFirmwareUnits() const;
	bool GetActiveWeapon(FJTSStellarWeaponBinding& Out) const;
	const UJTSStellarLootTable* GetLootTable() const;
	void ConfigureLootTable(UJTSStellarLootTable* Table);
	bool ConsumeEnergy(float Amount);
	void RefundEnergy(float Amount);
	void SetChannelActive(bool bActive) { bChannelActive = bActive; }
	void StopStellarWeapon();
	void RefreshParticipants();
	void RestoreState(const TArray<FJTSItemInstance>& SavedSlots, float SavedEnergy, int32 SavedActiveSlot = INDEX_NONE);
	UFUNCTION(BlueprintCallable) void SelectWeapon(int32 CoreSlot);
	UFUNCTION(Server, Reliable) void ServerSelectWeapon(int32 CoreSlot);
	UFUNCTION(Server, Reliable) void ServerMoveSlot(int32 From, FGuid ExpectedFrom, int32 To, FGuid ExpectedTo);
	UFUNCTION(Server, Reliable) void ServerExchangeLocker(AJTSSpacecraftActor* Ship, int32 LockerIndex,
		FGuid LockerToken, int32 LoadoutIndex, FGuid ExpectedItem);
	UFUNCTION(Server, Reliable) void ServerExchangeInventory(int32 InventoryIndex, FGuid ExpectedInventoryItem,
		int32 LoadoutIndex, FGuid ExpectedItem);
	UFUNCTION(Server, Reliable) void ServerUpgradeCore(int32 CoreSlot, FGuid ExpectedItem, int32 ExpectedRevision);
	UFUNCTION(Server, Reliable) void ServerApplyPoints(int32 AttachmentSlot, FGuid ExpectedItem,
		int32 ExpectedRevision, const TArray<uint8>& Points);
	UFUNCTION(BlueprintPure) int32 GetRevision() const { return Revision; }
	FString GetLastActionMessage() const { return LastActionMessage; }
	int32 GetActionResponseCount() const { return ActionResponseCount; }
	UPROPERTY(BlueprintAssignable) FJTSOnStellarLoadoutChanged OnLoadoutChanged;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void UpdateRuntime();
	void Changed();
	UFUNCTION() void OnRep_State();
	UFUNCTION(Client, Reliable) void ClientActionResult(bool bSucceeded);
	UPROPERTY(ReplicatedUsing=OnRep_State) TArray<FJTSItemInstance> Slots;
	UPROPERTY(ReplicatedUsing=OnRep_State) int32 AvailableSlotCount = 9;
	UPROPERTY(ReplicatedUsing=OnRep_State) int32 ActiveCoreSlot = INDEX_NONE;
	UPROPERTY(ReplicatedUsing=OnRep_State) int32 Revision = 0;
	UPROPERTY(Replicated) float Energy = 100.0f;
	UPROPERTY(Transient) TObjectPtr<UJTSStellarLootTable> CachedLootTable;
	bool bChannelActive = false;
	double LastEnergyUse = -1.0;
	double LastUpdateTime = 0.0;
	FTimerHandle RuntimeTimer;
	FString LastActionMessage;
	int32 ActionResponseCount = 0;
};
