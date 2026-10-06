// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "space/Core/JTSExpeditionTypes.h"
#include "space/Player/JTSPlayerProgressionTypes.h"
#include "space/Items/JTSShipLockerTypes.h"

#include "JTSPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJTSPlayerNetworkStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJTSPlayerProgressionChanged);

/** Replicated, player-specific state for the four-player expedition lobby and runtime. */
UCLASS()
class SPACE_API AJTSPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AJTSPlayerState();

	UFUNCTION(BlueprintPure, Category = "Stellar")
	class UJTSStellarLoadoutComponent* GetStellarLoadout() const { return StellarLoadout; }
	/** Atomic loadout/locker transfer; an empty replacement extracts the old item. */
	bool TryReplaceStellarLockerSlot(int32 Index, FGuid ExpectedToken, const FJTSItemInstance& Replacement);

	UFUNCTION(BlueprintPure, Category = "Expedition")
	bool IsReady() const { return bReady; }

	UFUNCTION(BlueprintPure, Category = "Expedition")
	bool IsExpeditionHost() const { return bIsExpeditionHost; }

	UFUNCTION(BlueprintPure, Category = "Expedition")
	EJTSAvatarColor GetAvatarColor() const { return AvatarColor; }

	UFUNCTION(BlueprintPure, Category = "Expedition")
	EJTSPlayerExpeditionStatus GetExpeditionStatus() const { return ExpeditionStatus; }

	UFUNCTION(BlueprintPure, Category = "Expedition")
	bool IsBoarded() const { return bIsBoarded; }

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetProgressionLevel() const { return ProgressionLevel; }

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetExperienceInCurrentLevel() const { return ExperienceInCurrentLevel; }

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetExperienceRequiredForNextLevel() const;

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetUnspentAbilityPoints() const { return UnspentAbilityPoints; }

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetAbilityRank(EJTSPlayerAbility Ability) const;

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetInventorySlotCapacityBonus() const;

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetItemStackLimit() const;

	UFUNCTION(BlueprintPure, Category = "Progression")
	float GetRunSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetCriticalChancePercent() const;

	/** Incremented with every server-authoritative progression mutation for UI pending-state reconciliation. */
	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetProgressionRevision() const { return ProgressionRevision; }

	/** Stable online identity when an OSS provider supplies one; falls back to the replicated player id for LAN. */
	UFUNCTION(BlueprintPure, Category = "Online")
	FString GetOnlineIdentityString() const;

	/** Voice providers use the same stable identity mapping as lobby/player presentation. */
	UFUNCTION(BlueprintPure, Category = "Voice")
	FString GetVoiceIdentityString() const { return GetOnlineIdentityString(); }

	UFUNCTION(BlueprintPure, Category = "Expedition")
	FLinearColor GetAvatarLinearColor() const;

	void SetReady(bool bNewReady);
	void SetExpeditionHost(bool bNewHost);
	void SetAvatarColor(EJTSAvatarColor NewAvatarColor);
	void SetExpeditionStatus(EJTSPlayerExpeditionStatus NewStatus);
	void SetBoarded(bool bNewBoarded);

	/** Enemy reward components call this only on the server. Each completed level grants one ability point. */
	void GrantExperience(int32 Amount);
	/** Terminal testing action: advance ten levels and grant the matching ability points. */
	bool GrantDebugLevels();

	/** Commits client-side pending upgrades. No refund path exists after this succeeds. */
	bool CommitAbilityAllocation(const FJTSAbilityAllocation& Allocation);

	/** Authoritative save/seamless-travel restore. */
	void RestoreProgression(
		int32 NewLevel,
		int32 NewExperienceInCurrentLevel,
		int32 NewUnspentAbilityPoints,
		int32 NewInventorySlotRank,
		int32 NewStackLimitRank,
		int32 NewRunSpeedRank,
		int32 NewStaminaRank = 0,
		int32 NewCriticalChanceRank = 0);

	static constexpr int32 ShipLockerCapacity = 30;
	const TArray<FJTSShipLockerSlot>& GetShipLockerSlots() const { return ShipLockerSlots; }
	FJTSShipLockerSlot GetShipLockerSlot(int32 SlotIndex) const;
	bool HasFreeShipLockerSlot() const;
	bool HasPendingStellarReveal() const;
	/** Server-only mutations. The controller validates ship terminal access before calling these. */
	bool TryStorePurchasedItem(const FJTSItemInstance& Item);
	bool TryStoreStellarItem(FName ItemId);
	bool TryStorePendingStellarItem(FName ItemId, int32& OutSlotIndex, FGuid& OutSlotToken);
	bool TryRevealStellarItem(int32 SlotIndex, FGuid ExpectedToken);
	bool TryMoveShipLockerSlot(int32 FromSlotIndex, FGuid ExpectedFromToken,
		int32 ToSlotIndex, FGuid ExpectedToToken);
	bool TryCombineStellarSlots(int32 CoreSlotIndex, FGuid ExpectedCoreToken,
		FGuid ExpectedAttachmentToken, const class UJTSStellarLootTable* LootTable);
	bool TryStoreCarriedItemAtSlot(int32 LockerSlotIndex, class UJTSInventoryComponent* Inventory,
		int32 CarriedSlotIndex, FGuid ExpectedItemId);
	bool TryExchangeShipLockerItemWithCarriedSlot(int32 LockerSlotIndex, FGuid ExpectedLockerToken,
		class UJTSInventoryComponent* Inventory, int32 CarriedSlotIndex, FGuid ExpectedCarriedItemId,
		const class UJTSStellarLootTable* LootTable);
	bool TryDeleteShipLockerSlot(int32 SlotIndex, FGuid ExpectedToken);
	bool TryTakeShipLockerItem(int32 SlotIndex, FGuid ExpectedToken, class UJTSInventoryComponent* Inventory,
		const class UJTSStellarLootTable* LootTable);
	void RestoreShipLockerSlots(const TArray<FJTSShipLockerSlot>& SavedSlots);

	/** Core kinds this player has ever rolled (design U). Persisted per player; trades never add to it. */
	const TArray<FName>& GetDiscoveredStellarCoreIds() const { return DiscoveredStellarCoreIds; }
	void RestoreDiscoveredStellarCoreIds(const TArray<FName>& SavedCoreIds);
	/** Server-only. Idempotent: a repeated core kind does not grow U. Returns true when the kind is new. */
	bool RecordDiscoveredStellarCore(FName CoreId);

	/** Server-only. Validates ownership, slot token and budget, then records the six attachment points. */
	bool TryApplyStellarPoints(int32 SlotIndex, FGuid ExpectedToken, const TArray<uint8>& NewPoints,
		const class UJTSStellarLootTable* LootTable);
	/** Server-only. Records a normal weapon's six skill points; they must fit the body-level budget (lowering is always allowed). */
	bool TryApplyWeaponPoints(int32 SlotIndex, FGuid ExpectedToken, const TArray<uint8>& NewPoints);
	/** Server-only. Raises a locker weapon one body level. The caller has already charged the cost. */
	bool TryRaiseWeaponBodyLevel(int32 SlotIndex, FGuid ExpectedToken);
	/** Server-only. Raises a core one level, consuming the specified dragged firmware first when supplied. */
	bool TryUpgradeStellarCore(int32 SlotIndex, FGuid ExpectedToken, const class UJTSStellarLootTable* LootTable,
		int32 FirmwareSlotIndex = INDEX_NONE, FGuid ExpectedFirmwareToken = FGuid());
	int32 GetStellarFirmwareUnits() const { return StellarFirmwareUnits; }
	void RestoreStellarFirmwareUnits(int32 SavedUnits);

	/** Server-only idempotency log so a resent roll request returns the stored result and never charges twice. */
	const FJTSStellarRollRecord* FindStellarRollRecord(const FGuid& RequestId) const;
	void AddStellarRollRecord(const FJTSStellarRollRecord& Record);

	UPROPERTY(BlueprintAssignable, Category = "Expedition")
	FOnJTSPlayerNetworkStateChanged OnNetworkStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Progression")
	FOnJTSPlayerProgressionChanged OnProgressionChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OverrideWith(APlayerState* PlayerState) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stellar", meta=(AllowPrivateAccess="true"))
	TObjectPtr<class UJTSStellarLoadoutComponent> StellarLoadout;
	UFUNCTION()
	void OnRep_NetworkState();

	UFUNCTION()
	void OnRep_Progression();
	UFUNCTION()
	void OnRep_ShipLockerSlots();

	void NotifyProgressionChanged();

	UPROPERTY(ReplicatedUsing = OnRep_NetworkState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Expedition", meta = (AllowPrivateAccess = "true"))
	bool bReady = false;

	UPROPERTY(ReplicatedUsing = OnRep_NetworkState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Expedition", meta = (AllowPrivateAccess = "true"))
	bool bIsExpeditionHost = false;

	UPROPERTY(ReplicatedUsing = OnRep_NetworkState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Expedition", meta = (AllowPrivateAccess = "true"))
	EJTSAvatarColor AvatarColor = EJTSAvatarColor::Blue;

	UPROPERTY(ReplicatedUsing = OnRep_NetworkState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Expedition", meta = (AllowPrivateAccess = "true"))
	EJTSPlayerExpeditionStatus ExpeditionStatus = EJTSPlayerExpeditionStatus::InLobby;

	UPROPERTY(ReplicatedUsing = OnRep_NetworkState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Expedition", meta = (AllowPrivateAccess = "true"))
	bool bIsBoarded = false;

	/** Starts at level one. The first ability point is earned for reaching level two. */
	UPROPERTY(ReplicatedUsing = OnRep_Progression, VisibleInstanceOnly, BlueprintReadOnly, Category = "Progression", meta = (AllowPrivateAccess = "true"))
	int32 ProgressionLevel = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Progression, VisibleInstanceOnly, BlueprintReadOnly, Category = "Progression", meta = (AllowPrivateAccess = "true"))
	int32 ExperienceInCurrentLevel = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Progression, VisibleInstanceOnly, BlueprintReadOnly, Category = "Progression", meta = (AllowPrivateAccess = "true"))
	int32 UnspentAbilityPoints = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Progression, VisibleInstanceOnly, BlueprintReadOnly, Category = "Progression", meta = (AllowPrivateAccess = "true"))
	int32 InventorySlotAbilityRank = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Progression, VisibleInstanceOnly, BlueprintReadOnly, Category = "Progression", meta = (AllowPrivateAccess = "true"))
	int32 StackLimitAbilityRank = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Progression, VisibleInstanceOnly, BlueprintReadOnly, Category = "Progression", meta = (AllowPrivateAccess = "true"))
	int32 RunSpeedAbilityRank = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Progression, VisibleInstanceOnly, BlueprintReadOnly, Category = "Progression", meta = (AllowPrivateAccess = "true"))
	int32 StaminaAbilityRank = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Progression, VisibleInstanceOnly, BlueprintReadOnly, Category = "Progression", meta = (AllowPrivateAccess = "true"))
	int32 CriticalChanceAbilityRank = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Progression, VisibleInstanceOnly, BlueprintReadOnly, Category = "Progression", meta = (AllowPrivateAccess = "true"))
	int32 ProgressionRevision = 0;

	/** Owner-only contents; other expedition members cannot inspect another player's terminal items. */
	UPROPERTY(ReplicatedUsing = OnRep_ShipLockerSlots, VisibleInstanceOnly, Category = "Ship Locker")
	TArray<FJTSShipLockerSlot> ShipLockerSlots;

	/** Owner-only roll history that drives the personal drop table. */
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Ship Locker")
	TArray<FName> DiscoveredStellarCoreIds;

	/** Firmware units left over from broken-down firmware items; spent on core upgrades. */
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Ship Locker")
	int32 StellarFirmwareUnits = 0;

	TArray<FJTSStellarRollRecord> RecentStellarRollRecords;
};
