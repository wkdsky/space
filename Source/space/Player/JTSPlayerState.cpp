// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Player/JTSPlayerState.h"

#include "GameFramework/OnlineReplStructs.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSStellarLoadoutComponent.h"
#include "space/Items/JTSStellarLootTable.h"
#include "space/Items/JTSStellarProgression.h"
#include "space/Items/JTSWeaponProgression.h"

namespace
{
	FJTSItemInstance MakeLockerCarriedItem(const FJTSShipLockerSlot& Slot,
		const UJTSStellarLootTable* LootTable)
	{
		if (!Slot.StandardItem.IsEmpty()) return Slot.StandardItem;
		if (Slot.StellarItemId.IsNone() || !IsValid(LootTable)) return FJTSItemInstance();
		FJTSItemInstance Carried = Slot.StellarCoreId.IsNone()
			? LootTable->MakeTextItem(Slot.StellarItemId)
			: LootTable->MakeWeaponItem(Slot.StellarCoreId, Slot.StellarItemId);
		Carried.StellarCoreLevel = Slot.StellarCoreLevel;
		Carried.InstanceId = Slot.StellarInstanceId.IsValid() ? Slot.StellarInstanceId : Slot.SlotToken;
		Carried.StellarAttachmentInstanceId = Slot.StellarAttachmentInstanceId;
		Carried.StellarPoints = Slot.StellarPoints;
		return Carried;
	}

	void AssignItemToLockerSlot(FJTSShipLockerSlot& Slot, const FJTSItemInstance& Item)
	{
		const bool bStellar = Item.ItemId == EJTSItemId::StellarText
			|| Item.ItemId == EJTSItemId::StellarWeapon;
		Slot.StandardItem = bStellar ? FJTSItemInstance() : Item;
		Slot.StellarItemId = bStellar ? Item.StellarItemId : NAME_None;
		Slot.StellarCoreId = Item.ItemId == EJTSItemId::StellarWeapon
			? Item.StellarCoreId : NAME_None;
		Slot.StellarCoreLevel = bStellar ? FMath::Max(1, Item.StellarCoreLevel) : 1;
		Slot.StellarPoints = bStellar ? Item.StellarPoints : TArray<uint8>();
		Slot.bPendingStellarReveal = false;
		Slot.SlotToken = FGuid::NewGuid();
		Slot.StellarInstanceId = bStellar ? Item.InstanceId : FGuid();
		Slot.StellarAttachmentInstanceId = Item.StellarAttachmentInstanceId;
	}
}

AJTSPlayerState::AJTSPlayerState()
{
	bReplicates = true;
	ShipLockerSlots.SetNum(ShipLockerCapacity);
	StellarLoadout = CreateDefaultSubobject<UJTSStellarLoadoutComponent>(TEXT("StellarLoadout"));
}

bool AJTSPlayerState::TryReplaceStellarLockerSlot(int32 Index, FGuid ExpectedToken, const FJTSItemInstance& Replacement)
{
	if (!HasAuthority() || !ShipLockerSlots.IsValidIndex(Index)
		|| ShipLockerSlots[Index].SlotToken != ExpectedToken || ShipLockerSlots[Index].bPendingStellarReveal
		|| !ShipLockerSlots[Index].StandardItem.IsEmpty()
		|| (!Replacement.IsEmpty() && Replacement.ItemId != EJTSItemId::StellarText)) return false;
	if (Replacement.IsEmpty()) ShipLockerSlots[Index].Clear();
	else AssignItemToLockerSlot(ShipLockerSlots[Index], Replacement);
	OnRep_ShipLockerSlots();
	ForceNetUpdate();
	return true;
}

FJTSShipLockerSlot AJTSPlayerState::GetShipLockerSlot(int32 SlotIndex) const
{
	return ShipLockerSlots.IsValidIndex(SlotIndex) ? ShipLockerSlots[SlotIndex] : FJTSShipLockerSlot();
}

bool AJTSPlayerState::HasFreeShipLockerSlot() const
{
	for (int32 Index = 0; Index < ShipLockerCapacity; ++Index)
	{
		if (!ShipLockerSlots.IsValidIndex(Index) || ShipLockerSlots[Index].IsEmpty()) return true;
	}
	return false;
}

bool AJTSPlayerState::HasPendingStellarReveal() const
{
	return ShipLockerSlots.ContainsByPredicate([](const FJTSShipLockerSlot& Slot)
	{
		return Slot.bPendingStellarReveal;
	});
}

bool AJTSPlayerState::TryStorePurchasedItem(const FJTSItemInstance& Item)
{
	if (!HasAuthority() || Item.IsEmpty()) return false;
	ShipLockerSlots.SetNum(ShipLockerCapacity);
	for (FJTSShipLockerSlot& Slot : ShipLockerSlots)
	{
		if (!Slot.IsEmpty()) continue;
		Slot.StandardItem = Item;
		Slot.StellarItemId = NAME_None;
		Slot.StellarCoreLevel = 1;
		Slot.StellarPoints.Reset();
		Slot.StellarCoreId = NAME_None;
		Slot.bPendingStellarReveal = false;
		Slot.SlotToken = FGuid::NewGuid();
		OnRep_ShipLockerSlots();
		ForceNetUpdate();
		return true;
	}
	return false;
}

bool AJTSPlayerState::TryStoreStellarItem(FName ItemId)
{
	if (!HasAuthority() || ItemId.IsNone()) return false;
	ShipLockerSlots.SetNum(ShipLockerCapacity);
	for (FJTSShipLockerSlot& Slot : ShipLockerSlots)
	{
		if (!Slot.IsEmpty()) continue;
		Slot.StandardItem.Clear();
		Slot.StellarItemId = ItemId;
		Slot.StellarCoreLevel = 1;
		Slot.StellarPoints.Reset();
		Slot.StellarCoreId = NAME_None;
		Slot.bPendingStellarReveal = false;
		Slot.SlotToken = FGuid::NewGuid();
		Slot.StellarInstanceId = FGuid::NewGuid();
		OnRep_ShipLockerSlots();
		ForceNetUpdate();
		return true;
	}
	return false;
}

bool AJTSPlayerState::TryStorePendingStellarItem(FName ItemId, int32& OutSlotIndex, FGuid& OutSlotToken)
{
	OutSlotIndex = INDEX_NONE;
	OutSlotToken.Invalidate();
	if (!HasAuthority() || ItemId.IsNone()) return false;
	ShipLockerSlots.SetNum(ShipLockerCapacity);
	for (int32 Index = 0; Index < ShipLockerCapacity; ++Index)
	{
		FJTSShipLockerSlot& Slot = ShipLockerSlots[Index];
		if (!Slot.IsEmpty()) continue;
		Slot.StandardItem.Clear();
		Slot.StellarItemId = ItemId;
		Slot.StellarCoreLevel = 1;
		Slot.StellarPoints.Reset();
		Slot.StellarCoreId = NAME_None;
		Slot.bPendingStellarReveal = true;
		Slot.SlotToken = FGuid::NewGuid();
		Slot.StellarInstanceId = FGuid::NewGuid();
		OutSlotIndex = Index;
		OutSlotToken = Slot.SlotToken;
		OnRep_ShipLockerSlots();
		ForceNetUpdate();
		return true;
	}
	return false;
}

bool AJTSPlayerState::TryRevealStellarItem(int32 SlotIndex, FGuid ExpectedToken)
{
	if (!HasAuthority() || !ShipLockerSlots.IsValidIndex(SlotIndex)) return false;
	FJTSShipLockerSlot& Slot = ShipLockerSlots[SlotIndex];
	if (!Slot.bPendingStellarReveal || Slot.StellarItemId.IsNone() || !ExpectedToken.IsValid()
		|| Slot.SlotToken != ExpectedToken) return false;
	Slot.bPendingStellarReveal = false;
	OnRep_ShipLockerSlots();
	ForceNetUpdate();
	return true;
}

bool AJTSPlayerState::TryMoveShipLockerSlot(int32 FromSlotIndex, FGuid ExpectedFromToken,
	int32 ToSlotIndex, FGuid ExpectedToToken)
{
	if (!HasAuthority() || FromSlotIndex == ToSlotIndex
		|| !ShipLockerSlots.IsValidIndex(FromSlotIndex) || !ShipLockerSlots.IsValidIndex(ToSlotIndex))
	{
		return false;
	}
	FJTSShipLockerSlot& From = ShipLockerSlots[FromSlotIndex];
	FJTSShipLockerSlot& To = ShipLockerSlots[ToSlotIndex];
	if (From.IsEmpty() || From.bPendingStellarReveal || To.bPendingStellarReveal
		|| !ExpectedFromToken.IsValid() || From.SlotToken != ExpectedFromToken
		|| To.SlotToken != ExpectedToToken)
	{
		return false;
	}
	Swap(From, To);
	From.SlotToken = From.IsEmpty() ? FGuid() : FGuid::NewGuid();
	To.SlotToken = To.IsEmpty() ? FGuid() : FGuid::NewGuid();
	OnRep_ShipLockerSlots();
	ForceNetUpdate();
	return true;
}

bool AJTSPlayerState::TryCombineStellarSlots(int32 CoreSlotIndex, FGuid ExpectedCoreToken,
	FGuid ExpectedAttachmentToken, const UJTSStellarLootTable* LootTable)
{
	if (!HasAuthority() || !IsValid(LootTable)
		|| !ShipLockerSlots.IsValidIndex(CoreSlotIndex)
		|| !ShipLockerSlots.IsValidIndex(CoreSlotIndex + 1))
	{
		return false;
	}
	FJTSShipLockerSlot& Core = ShipLockerSlots[CoreSlotIndex];
	FJTSShipLockerSlot& Attachment = ShipLockerSlots[CoreSlotIndex + 1];
	if (Core.IsEmpty() || Attachment.IsEmpty()
		|| Core.bPendingStellarReveal || Attachment.bPendingStellarReveal
		|| !Core.StandardItem.IsEmpty() || !Attachment.StandardItem.IsEmpty()
		|| !Core.StellarCoreId.IsNone() || !Attachment.StellarCoreId.IsNone()
		|| !ExpectedCoreToken.IsValid() || !ExpectedAttachmentToken.IsValid()
		|| Core.SlotToken != ExpectedCoreToken
		|| Attachment.SlotToken != ExpectedAttachmentToken
		|| !LootTable->CanCombine(Core.StellarItemId, Attachment.StellarItemId))
	{
		return false;
	}
	Core.StellarPoints = Attachment.StellarPoints;
	Core.StellarInstanceId = Core.StellarInstanceId.IsValid() ? Core.StellarInstanceId : Core.SlotToken;
	Core.StellarAttachmentInstanceId = Attachment.StellarInstanceId.IsValid() ? Attachment.StellarInstanceId : Attachment.SlotToken;
	Core.StellarCoreId = Core.StellarItemId;
	Core.StellarItemId = Attachment.StellarItemId;
	Core.SlotToken = FGuid::NewGuid();
	Attachment.Clear();
	OnRep_ShipLockerSlots();
	ForceNetUpdate();
	return true;
}

bool AJTSPlayerState::TryStoreCarriedItemAtSlot(int32 LockerSlotIndex, UJTSInventoryComponent* Inventory,
	int32 CarriedSlotIndex, FGuid ExpectedItemId)
{
	if (!HasAuthority() || !IsValid(Inventory) || !ShipLockerSlots.IsValidIndex(LockerSlotIndex)
		|| !ShipLockerSlots[LockerSlotIndex].IsEmpty()) return false;
	FJTSItemInstance MovedItem;
	if (!Inventory->TryExtractItemAtSlot(CarriedSlotIndex, ExpectedItemId, MovedItem)) return false;
	FJTSShipLockerSlot& Slot = ShipLockerSlots[LockerSlotIndex];
	AssignItemToLockerSlot(Slot, MovedItem);
	OnRep_ShipLockerSlots();
	ForceNetUpdate();
	return true;
}

bool AJTSPlayerState::TryExchangeShipLockerItemWithCarriedSlot(int32 LockerSlotIndex,
	FGuid ExpectedLockerToken, UJTSInventoryComponent* Inventory, int32 CarriedSlotIndex,
	FGuid ExpectedCarriedItemId, const UJTSStellarLootTable* LootTable)
{
	if (!HasAuthority() || !IsValid(Inventory) || !ShipLockerSlots.IsValidIndex(LockerSlotIndex)
		|| !ExpectedLockerToken.IsValid()) return false;
	FJTSShipLockerSlot& LockerEntry = ShipLockerSlots[LockerSlotIndex];
	if (LockerEntry.bPendingStellarReveal || LockerEntry.SlotToken != ExpectedLockerToken
		|| LockerEntry.IsEmpty()) return false;
	const FJTSItemInstance IncomingItem = MakeLockerCarriedItem(LockerEntry, LootTable);
	if (IncomingItem.IsEmpty() || IncomingItem.ItemId == EJTSItemId::StellarText
		|| IncomingItem.ItemId == EJTSItemId::StellarWeapon) return false;
	FJTSItemInstance ReplacedItem;
	if (!Inventory->TryExchangeItemAtSlot(CarriedSlotIndex, ExpectedCarriedItemId,
		IncomingItem, ReplacedItem)) return false;
	if (ReplacedItem.IsEmpty())
	{
		LockerEntry.Clear();
	}
	else
	{
		AssignItemToLockerSlot(LockerEntry, ReplacedItem);
	}
	OnRep_ShipLockerSlots();
	ForceNetUpdate();
	return true;
}

bool AJTSPlayerState::TryDeleteShipLockerSlot(int32 SlotIndex, FGuid ExpectedToken)
{
	if (!HasAuthority() || !ShipLockerSlots.IsValidIndex(SlotIndex) || ShipLockerSlots[SlotIndex].IsEmpty()
		|| ShipLockerSlots[SlotIndex].bPendingStellarReveal
		|| !ExpectedToken.IsValid() || ShipLockerSlots[SlotIndex].SlotToken != ExpectedToken)
	{
		return false;
	}
	ShipLockerSlots[SlotIndex].Clear();
	OnRep_ShipLockerSlots();
	ForceNetUpdate();
	return true;
}

bool AJTSPlayerState::TryTakeShipLockerItem(int32 SlotIndex, FGuid ExpectedToken,
	UJTSInventoryComponent* Inventory, const UJTSStellarLootTable* LootTable)
{
	if (!HasAuthority() || !IsValid(Inventory) || !ShipLockerSlots.IsValidIndex(SlotIndex)) return false;
	const FJTSShipLockerSlot& Slot = ShipLockerSlots[SlotIndex];
	if (!ExpectedToken.IsValid() || Slot.SlotToken != ExpectedToken || Slot.bPendingStellarReveal) return false;
	const FJTSItemInstance Item = MakeLockerCarriedItem(Slot, LootTable);
	if (Item.IsEmpty() || Item.ItemId == EJTSItemId::StellarText || Item.ItemId == EJTSItemId::StellarWeapon
		|| !Inventory->CanAddItem(Item.ItemId, Item.StackCount)) return false;
	int32 Remaining = Item.StackCount;
	if (!Inventory->TryAddItem(Item, Remaining) || Remaining != 0) return false;
	ShipLockerSlots[SlotIndex].Clear();
	OnRep_ShipLockerSlots();
	ForceNetUpdate();
	return true;
}

void AJTSPlayerState::RestoreShipLockerSlots(const TArray<FJTSShipLockerSlot>& SavedSlots)
{
	if (!HasAuthority()) return;
	ShipLockerSlots.SetNum(ShipLockerCapacity);
	for (int32 Index = 0; Index < ShipLockerCapacity; ++Index)
	{
		ShipLockerSlots[Index] = SavedSlots.IsValidIndex(Index) ? SavedSlots[Index] : FJTSShipLockerSlot();
		// A level transition can interrupt the local reel; the earned item is still delivered.
		ShipLockerSlots[Index].bPendingStellarReveal = false;
		if (!ShipLockerSlots[Index].StellarItemId.IsNone() && !ShipLockerSlots[Index].StellarInstanceId.IsValid())
			ShipLockerSlots[Index].StellarInstanceId = ShipLockerSlots[Index].SlotToken.IsValid()
				? ShipLockerSlots[Index].SlotToken : FGuid::NewGuid();
		if (!ShipLockerSlots[Index].IsEmpty() && !ShipLockerSlots[Index].SlotToken.IsValid())
		{
			ShipLockerSlots[Index].SlotToken = FGuid::NewGuid();
		}
	}
	OnRep_ShipLockerSlots();
	ForceNetUpdate();
}

void AJTSPlayerState::RestoreStellarFirmwareUnits(int32 SavedUnits)
{
	if (!HasAuthority()) return;
	StellarFirmwareUnits = FMath::Max(0, SavedUnits);
	ForceNetUpdate();
}

bool AJTSPlayerState::TryApplyStellarPoints(int32 SlotIndex, FGuid ExpectedToken,
	const TArray<uint8>& NewPoints, const UJTSStellarLootTable* LootTable)
{
	if (!HasAuthority() || !IsValid(LootTable) || !ShipLockerSlots.IsValidIndex(SlotIndex)
		|| NewPoints.Num() > FJTSStellarProgression::SkillCount) return false;
	FJTSShipLockerSlot& Slot = ShipLockerSlots[SlotIndex];
	if (Slot.IsEmpty() || Slot.bPendingStellarReveal || !Slot.StandardItem.IsEmpty()
		|| !ExpectedToken.IsValid() || Slot.SlotToken != ExpectedToken) return false;
	const FJTSStellarLootEntry* const Attachment = LootTable->FindEntry(Slot.StellarItemId);
	if (!Attachment || Attachment->bCore || Attachment->CompatibleCoreId.IsNone()) return false;
	if (Slot.StellarCoreId.IsNone() || !LootTable->CanCombine(Slot.StellarCoreId, Slot.StellarItemId)) return false;
	for (const uint8 Point : NewPoints)
	{
		if (Point > FJTSStellarProgression::MaxPointsPerSkill) return false;
	}
	TArray<uint8> Requested = NewPoints;
	FJTSStellarProgression::NormalizePoints(Requested);
	TArray<uint8> Recorded = Slot.StellarPoints;
	FJTSStellarProgression::NormalizePoints(Recorded);
	// Existing over-budget allocations can be reduced, but only while the weapon remains assembled.
	bool bOnlyLowering = true;
	for (int32 Index = 0; Index < FJTSStellarProgression::SkillCount; ++Index)
	{
		bOnlyLowering &= Requested[Index] <= Recorded[Index];
	}
	if (!bOnlyLowering
		&& FJTSStellarProgression::SumPoints(Requested) > FJTSStellarProgression::GetCoreBudget(Slot.StellarCoreLevel))
	{
		return false;
	}
	Slot.StellarPoints = FJTSStellarProgression::SumPoints(Requested) > 0 ? Requested : TArray<uint8>();
	OnRep_ShipLockerSlots();
	ForceNetUpdate();
	return true;
}

bool AJTSPlayerState::TryUpgradeStellarCore(int32 SlotIndex, FGuid ExpectedToken, const UJTSStellarLootTable* LootTable,
	int32 FirmwareSlotIndex, FGuid ExpectedFirmwareToken)
{
	if (!HasAuthority() || !IsValid(LootTable) || !ShipLockerSlots.IsValidIndex(SlotIndex)) return false;
	FJTSShipLockerSlot& Slot = ShipLockerSlots[SlotIndex];
	if (Slot.IsEmpty() || Slot.bPendingStellarReveal || !Slot.StandardItem.IsEmpty()
		|| !ExpectedToken.IsValid() || Slot.SlotToken != ExpectedToken
		|| Slot.StellarCoreLevel >= FJTSStellarProgression::MaxCoreLevel) return false;
	const FName CoreId = Slot.StellarCoreId.IsNone() ? Slot.StellarItemId : Slot.StellarCoreId;
	const FJTSStellarLootEntry* const Core = LootTable->FindEntry(CoreId);
	if (!Core || !Core->bCore) return false;

	const int32 Cost = FJTSStellarProgression::GetUpgradeUnitCost(Slot.StellarCoreLevel);
	auto FirmwareUnitsOf = [LootTable](const FJTSShipLockerSlot& Candidate)
	{
		if (Candidate.bPendingStellarReveal || !Candidate.StandardItem.IsEmpty()
			|| !Candidate.StellarCoreId.IsNone()) return 0;
		const FJTSStellarLootEntry* const Entry = LootTable->FindEntry(Candidate.StellarItemId);
		return Entry ? Entry->FirmwareUnits : 0;
	};
	if (FirmwareSlotIndex != INDEX_NONE
		&& (!ShipLockerSlots.IsValidIndex(FirmwareSlotIndex) || FirmwareSlotIndex == SlotIndex
			|| !ExpectedFirmwareToken.IsValid() || ShipLockerSlots[FirmwareSlotIndex].SlotToken != ExpectedFirmwareToken
			|| FirmwareUnitsOf(ShipLockerSlots[FirmwareSlotIndex]) <= 0)) return false;
	int32 Available = StellarFirmwareUnits;
	for (int32 Index = 0; Index < ShipLockerSlots.Num(); ++Index)
	{
		if (Index != SlotIndex) Available += FirmwareUnitsOf(ShipLockerSlots[Index]);
	}
	if (Available < Cost) return false;
	if (FirmwareSlotIndex != INDEX_NONE)
	{
		StellarFirmwareUnits += FirmwareUnitsOf(ShipLockerSlots[FirmwareSlotIndex]);
		ShipLockerSlots[FirmwareSlotIndex].Clear();
	}
	for (int32 Index = 0; Index < ShipLockerSlots.Num() && StellarFirmwareUnits < Cost; ++Index)
	{
		if (Index == SlotIndex) continue;
		if (const int32 Units = FirmwareUnitsOf(ShipLockerSlots[Index]); Units > 0)
		{
			StellarFirmwareUnits += Units;
			ShipLockerSlots[Index].Clear();
		}
	}
	StellarFirmwareUnits -= Cost;
	++Slot.StellarCoreLevel;
	OnRep_ShipLockerSlots();
	ForceNetUpdate();
	return true;
}

void AJTSPlayerState::RestoreDiscoveredStellarCoreIds(const TArray<FName>& SavedCoreIds)
{
	if (!HasAuthority()) return;
	DiscoveredStellarCoreIds.Reset();
	for (const FName CoreId : SavedCoreIds)
	{
		if (!CoreId.IsNone()) DiscoveredStellarCoreIds.AddUnique(CoreId);
	}
	ForceNetUpdate();
}

bool AJTSPlayerState::RecordDiscoveredStellarCore(FName CoreId)
{
	if (!HasAuthority() || CoreId.IsNone() || DiscoveredStellarCoreIds.Contains(CoreId)) return false;
	DiscoveredStellarCoreIds.Add(CoreId);
	ForceNetUpdate();
	return true;
}

const FJTSStellarRollRecord* AJTSPlayerState::FindStellarRollRecord(const FGuid& RequestId) const
{
	return RequestId.IsValid()
		? RecentStellarRollRecords.FindByPredicate([&RequestId](const FJTSStellarRollRecord& Record)
		{
			return Record.RequestId == RequestId;
		})
		: nullptr;
}

void AJTSPlayerState::AddStellarRollRecord(const FJTSStellarRollRecord& Record)
{
	constexpr int32 MaxRecords = 32;
	if (!HasAuthority() || !Record.RequestId.IsValid()) return;
	RecentStellarRollRecords.Add(Record);
	if (RecentStellarRollRecords.Num() > MaxRecords)
	{
		RecentStellarRollRecords.RemoveAt(0, RecentStellarRollRecords.Num() - MaxRecords);
	}
}

void AJTSPlayerState::OnRep_ShipLockerSlots()
{
	OnNetworkStateChanged.Broadcast();
}

int32 AJTSPlayerState::GetExperienceRequiredForNextLevel() const
{
	return ProgressionLevel >= FJTSPlayerProgressionRules::MaximumLevel
		? 0
		: FJTSPlayerProgressionRules::GetExperienceRequiredForNextLevel(ProgressionLevel);
}

int32 AJTSPlayerState::GetAbilityRank(const EJTSPlayerAbility Ability) const
{
	switch (Ability)
	{
	case EJTSPlayerAbility::InventorySlots: return InventorySlotAbilityRank;
	case EJTSPlayerAbility::StackLimit: return StackLimitAbilityRank;
	case EJTSPlayerAbility::RunSpeed: return RunSpeedAbilityRank;
	case EJTSPlayerAbility::Stamina: return StaminaAbilityRank;
	case EJTSPlayerAbility::CriticalChance: return CriticalChanceAbilityRank;
	default: return 0;
	}
}

int32 AJTSPlayerState::GetInventorySlotCapacityBonus() const
{
	return FJTSPlayerProgressionRules::GetInventorySlotBonus(InventorySlotAbilityRank);
}

int32 AJTSPlayerState::GetItemStackLimit() const
{
	return FJTSPlayerProgressionRules::GetStackLimit(StackLimitAbilityRank);
}

float AJTSPlayerState::GetRunSpeedMultiplier() const
{
	return FJTSPlayerProgressionRules::GetRunSpeedMultiplier(RunSpeedAbilityRank);
}

int32 AJTSPlayerState::GetCriticalChancePercent() const
{
	return FJTSPlayerProgressionRules::GetCriticalChancePercent(CriticalChanceAbilityRank);
}

FLinearColor AJTSPlayerState::GetAvatarLinearColor() const
{
	switch (AvatarColor)
	{
	case EJTSAvatarColor::Orange:
		return FLinearColor(1.0f, 0.34f, 0.06f, 1.0f);
	case EJTSAvatarColor::Green:
		return FLinearColor(0.18f, 0.85f, 0.28f, 1.0f);
	case EJTSAvatarColor::Purple:
		return FLinearColor(0.58f, 0.25f, 0.90f, 1.0f);
	case EJTSAvatarColor::Blue:
	default:
		return FLinearColor(0.10f, 0.45f, 1.0f, 1.0f);
	}
}

void AJTSPlayerState::SetReady(bool bNewReady)
{
	if (HasAuthority())
	{
		bReady = bNewReady;
		if (ExpeditionStatus == EJTSPlayerExpeditionStatus::InLobby || ExpeditionStatus == EJTSPlayerExpeditionStatus::Ready)
		{
			ExpeditionStatus = bReady ? EJTSPlayerExpeditionStatus::Ready : EJTSPlayerExpeditionStatus::InLobby;
		}
		OnRep_NetworkState();
	}
}

void AJTSPlayerState::SetExpeditionHost(bool bNewHost)
{
	if (HasAuthority())
	{
		bIsExpeditionHost = bNewHost;
		OnRep_NetworkState();
	}
}

void AJTSPlayerState::SetAvatarColor(EJTSAvatarColor NewAvatarColor)
{
	if (HasAuthority())
	{
		AvatarColor = NewAvatarColor;
		OnRep_NetworkState();
	}
}

void AJTSPlayerState::SetExpeditionStatus(EJTSPlayerExpeditionStatus NewStatus)
{
	if (HasAuthority())
	{
		ExpeditionStatus = NewStatus;
		OnRep_NetworkState();
	}
}

void AJTSPlayerState::SetBoarded(bool bNewBoarded)
{
	if (HasAuthority())
	{
		bIsBoarded = bNewBoarded;
		if (bNewBoarded && ExpeditionStatus == EJTSPlayerExpeditionStatus::Active)
		{
			ExpeditionStatus = EJTSPlayerExpeditionStatus::Boarded;
		}
		else if (!bNewBoarded && ExpeditionStatus == EJTSPlayerExpeditionStatus::Boarded)
		{
			ExpeditionStatus = EJTSPlayerExpeditionStatus::Active;
		}
		OnRep_NetworkState();
	}
}

void AJTSPlayerState::GrantExperience(const int32 Amount)
{
	if (!HasAuthority() || Amount <= 0 || ProgressionLevel >= FJTSPlayerProgressionRules::MaximumLevel)
	{
		return;
	}

	ExperienceInCurrentLevel = FMath::Min(MAX_int32 - Amount, ExperienceInCurrentLevel) + Amount;
	while (ProgressionLevel < FJTSPlayerProgressionRules::MaximumLevel)
	{
		const int32 RequiredExperience = GetExperienceRequiredForNextLevel();
		if (RequiredExperience <= 0 || ExperienceInCurrentLevel < RequiredExperience)
		{
			break;
		}

		ExperienceInCurrentLevel -= RequiredExperience;
		++ProgressionLevel;
		++UnspentAbilityPoints;
	}

	if (ProgressionLevel >= FJTSPlayerProgressionRules::MaximumLevel)
	{
		ExperienceInCurrentLevel = 0;
	}

	++ProgressionRevision;
	NotifyProgressionChanged();
}

bool AJTSPlayerState::GrantDebugLevels()
{
	if (!HasAuthority() || ProgressionLevel + 10 > FJTSPlayerProgressionRules::MaximumLevel)
	{
		return false;
	}

	ProgressionLevel += 10;
	UnspentAbilityPoints += 10;
	++ProgressionRevision;
	NotifyProgressionChanged();
	return true;
}

bool AJTSPlayerState::CommitAbilityAllocation(const FJTSAbilityAllocation& Allocation)
{
	if (!HasAuthority()
		|| Allocation.InventorySlotRanks < 0
		|| Allocation.StackLimitRanks < 0
		|| Allocation.RunSpeedRanks < 0
		|| Allocation.StaminaRanks < 0
		|| Allocation.CriticalChanceRanks < 0)
	{
		return false;
	}

	const int32 TotalCost = Allocation.GetTotalPointCost();
	if (TotalCost <= 0 || TotalCost > UnspentAbilityPoints
		|| InventorySlotAbilityRank + Allocation.InventorySlotRanks > FJTSPlayerProgressionRules::MaximumAbilityRank
		|| StackLimitAbilityRank + Allocation.StackLimitRanks > FJTSPlayerProgressionRules::MaximumAbilityRank
		|| RunSpeedAbilityRank + Allocation.RunSpeedRanks > FJTSPlayerProgressionRules::MaximumAbilityRank
		|| StaminaAbilityRank + Allocation.StaminaRanks > FJTSPlayerProgressionRules::MaximumAbilityRank
		|| CriticalChanceAbilityRank + Allocation.CriticalChanceRanks > FJTSPlayerProgressionRules::MaximumAbilityRank)
	{
		return false;
	}

	InventorySlotAbilityRank += Allocation.InventorySlotRanks;
	StackLimitAbilityRank += Allocation.StackLimitRanks;
	RunSpeedAbilityRank += Allocation.RunSpeedRanks;
	StaminaAbilityRank += Allocation.StaminaRanks;
	CriticalChanceAbilityRank += Allocation.CriticalChanceRanks;
	UnspentAbilityPoints -= TotalCost;
	++ProgressionRevision;
	NotifyProgressionChanged();
	return true;
}

void AJTSPlayerState::RestoreProgression(
	const int32 NewLevel,
	const int32 NewExperienceInCurrentLevel,
	const int32 NewUnspentAbilityPoints,
	const int32 NewInventorySlotRank,
	const int32 NewStackLimitRank,
	const int32 NewRunSpeedRank,
	const int32 NewStaminaRank,
	const int32 NewCriticalChanceRank)
{
	if (!HasAuthority())
	{
		return;
	}

	ProgressionLevel = FMath::Clamp(NewLevel, 1, FJTSPlayerProgressionRules::MaximumLevel);
	ExperienceInCurrentLevel = ProgressionLevel >= FJTSPlayerProgressionRules::MaximumLevel
		? 0
		: FMath::Clamp(NewExperienceInCurrentLevel, 0, FMath::Max(0, GetExperienceRequiredForNextLevel() - 1));
	UnspentAbilityPoints = FMath::Max(0, NewUnspentAbilityPoints);
	InventorySlotAbilityRank = FMath::Clamp(NewInventorySlotRank, 0, FJTSPlayerProgressionRules::MaximumAbilityRank);
	StackLimitAbilityRank = FMath::Clamp(NewStackLimitRank, 0, FJTSPlayerProgressionRules::MaximumAbilityRank);
	RunSpeedAbilityRank = FMath::Clamp(NewRunSpeedRank, 0, FJTSPlayerProgressionRules::MaximumAbilityRank);
	StaminaAbilityRank = FMath::Clamp(NewStaminaRank, 0, FJTSPlayerProgressionRules::MaximumAbilityRank);
	CriticalChanceAbilityRank = FMath::Clamp(NewCriticalChanceRank, 0, FJTSPlayerProgressionRules::MaximumAbilityRank);
	++ProgressionRevision;
	NotifyProgressionChanged();
}

FString AJTSPlayerState::GetOnlineIdentityString() const
{
	const FUniqueNetIdRepl& NetId = GetUniqueId();
	if (NetId.IsValid())
	{
		return NetId->ToString();
	}
	return FString::Printf(TEXT("LocalPlayer-%d"), GetPlayerId());
}

void AJTSPlayerState::OnRep_NetworkState()
{
	OnNetworkStateChanged.Broadcast();
}

void AJTSPlayerState::OnRep_Progression()
{
	NotifyProgressionChanged();
}

void AJTSPlayerState::NotifyProgressionChanged()
{
	OnProgressionChanged.Broadcast();
	// Existing character/view bindings already listen to the network-state delegate.  Reuse that
	// notification path rather than requiring every presentation consumer to duplicate bindings.
	OnNetworkStateChanged.Broadcast();
}

void AJTSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSPlayerState, bReady);
	DOREPLIFETIME(AJTSPlayerState, bIsExpeditionHost);
	DOREPLIFETIME(AJTSPlayerState, AvatarColor);
	DOREPLIFETIME(AJTSPlayerState, ExpeditionStatus);
	DOREPLIFETIME(AJTSPlayerState, bIsBoarded);
	DOREPLIFETIME(AJTSPlayerState, ProgressionLevel);
	DOREPLIFETIME(AJTSPlayerState, ExperienceInCurrentLevel);
	DOREPLIFETIME(AJTSPlayerState, UnspentAbilityPoints);
	DOREPLIFETIME(AJTSPlayerState, InventorySlotAbilityRank);
	DOREPLIFETIME(AJTSPlayerState, StackLimitAbilityRank);
	DOREPLIFETIME(AJTSPlayerState, RunSpeedAbilityRank);
	DOREPLIFETIME(AJTSPlayerState, StaminaAbilityRank);
	DOREPLIFETIME(AJTSPlayerState, CriticalChanceAbilityRank);
	DOREPLIFETIME(AJTSPlayerState, ProgressionRevision);
	DOREPLIFETIME_CONDITION(AJTSPlayerState, ShipLockerSlots, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AJTSPlayerState, DiscoveredStellarCoreIds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AJTSPlayerState, StellarFirmwareUnits, COND_OwnerOnly);
}

void AJTSPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	if (AJTSPlayerState* const Target = Cast<AJTSPlayerState>(PlayerState))
	{
		Target->bReady = bReady;
		Target->bIsExpeditionHost = bIsExpeditionHost;
		Target->AvatarColor = AvatarColor;
		Target->ExpeditionStatus = ExpeditionStatus;
		Target->bIsBoarded = bIsBoarded;
		Target->ProgressionLevel = ProgressionLevel;
		Target->ExperienceInCurrentLevel = ExperienceInCurrentLevel;
		Target->UnspentAbilityPoints = UnspentAbilityPoints;
		Target->InventorySlotAbilityRank = InventorySlotAbilityRank;
		Target->StackLimitAbilityRank = StackLimitAbilityRank;
		Target->RunSpeedAbilityRank = RunSpeedAbilityRank;
		Target->StaminaAbilityRank = StaminaAbilityRank;
		Target->CriticalChanceAbilityRank = CriticalChanceAbilityRank;
		Target->ProgressionRevision = ProgressionRevision;
		Target->ShipLockerSlots = ShipLockerSlots;
		Target->DiscoveredStellarCoreIds = DiscoveredStellarCoreIds;
		Target->StellarFirmwareUnits = StellarFirmwareUnits;
		Target->RecentStellarRollRecords = RecentStellarRollRecords;
		Target->StellarLoadout->RestoreState(StellarLoadout->GetSlots(), StellarLoadout->GetEnergy());
		for (FJTSShipLockerSlot& Slot : Target->ShipLockerSlots) Slot.bPendingStellarReveal = false;
	}
}

void AJTSPlayerState::OverrideWith(APlayerState* PlayerState)
{
	Super::OverrideWith(PlayerState);
	if (const AJTSPlayerState* const Source = Cast<AJTSPlayerState>(PlayerState))
	{
		bReady = Source->bReady;
		bIsExpeditionHost = Source->bIsExpeditionHost;
		AvatarColor = Source->AvatarColor;
		ExpeditionStatus = Source->ExpeditionStatus;
		bIsBoarded = Source->bIsBoarded;
		ProgressionLevel = Source->ProgressionLevel;
		ExperienceInCurrentLevel = Source->ExperienceInCurrentLevel;
		UnspentAbilityPoints = Source->UnspentAbilityPoints;
		InventorySlotAbilityRank = Source->InventorySlotAbilityRank;
		StackLimitAbilityRank = Source->StackLimitAbilityRank;
		RunSpeedAbilityRank = Source->RunSpeedAbilityRank;
		StaminaAbilityRank = Source->StaminaAbilityRank;
		CriticalChanceAbilityRank = Source->CriticalChanceAbilityRank;
		ProgressionRevision = Source->ProgressionRevision;
		ShipLockerSlots = Source->ShipLockerSlots;
		StellarLoadout->RestoreState(Source->StellarLoadout->GetSlots(), Source->StellarLoadout->GetEnergy());
		DiscoveredStellarCoreIds = Source->DiscoveredStellarCoreIds;
		StellarFirmwareUnits = Source->StellarFirmwareUnits;
		RecentStellarRollRecords = Source->RecentStellarRollRecords;
		for (FJTSShipLockerSlot& Slot : ShipLockerSlots) Slot.bPendingStellarReveal = false;
	}
}

namespace
{
	/** The locker slot must still hold the exact upgradeable normal weapon the client opened. */
	bool IsUpgradeableWeaponSlot(const FJTSShipLockerSlot& Slot, const FGuid& ExpectedToken)
	{
		return !Slot.bPendingStellarReveal && !Slot.StandardItem.IsEmpty()
			&& FJTSWeaponProgression::IsUpgradeable(Slot.StandardItem.ItemId)
			&& ExpectedToken.IsValid() && Slot.SlotToken == ExpectedToken;
	}
}

bool AJTSPlayerState::TryApplyWeaponPoints(int32 SlotIndex, FGuid ExpectedToken, const TArray<uint8>& NewPoints)
{
	if (!HasAuthority() || !ShipLockerSlots.IsValidIndex(SlotIndex)
		|| !IsUpgradeableWeaponSlot(ShipLockerSlots[SlotIndex], ExpectedToken)) return false;
	FJTSItemInstance& Weapon = ShipLockerSlots[SlotIndex].StandardItem;
	if (!FJTSWeaponProgression::IsValidAllocation(NewPoints, Weapon.WeaponBodyLevel)) return false;
	TArray<uint8> Points = NewPoints;
	FJTSWeaponProgression::NormalizePoints(Points);
	Weapon.WeaponPoints = FJTSWeaponProgression::SumPoints(Points) > 0 ? Points : TArray<uint8>();
	OnRep_ShipLockerSlots();
	ForceNetUpdate();
	return true;
}

bool AJTSPlayerState::TryRaiseWeaponBodyLevel(int32 SlotIndex, FGuid ExpectedToken)
{
	if (!HasAuthority() || !ShipLockerSlots.IsValidIndex(SlotIndex)
		|| !IsUpgradeableWeaponSlot(ShipLockerSlots[SlotIndex], ExpectedToken)) return false;
	FJTSItemInstance& Weapon = ShipLockerSlots[SlotIndex].StandardItem;
	if (Weapon.WeaponBodyLevel >= FJTSWeaponProgression::MaxBodyLevel) return false;
	++Weapon.WeaponBodyLevel;
	OnRep_ShipLockerSlots();
	ForceNetUpdate();
	return true;
}
