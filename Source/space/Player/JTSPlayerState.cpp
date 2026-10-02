// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Player/JTSPlayerState.h"

#include "GameFramework/OnlineReplStructs.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Items/JTSStellarLootTable.h"

namespace
{
	FJTSItemInstance MakeLockerCarriedItem(const FJTSShipLockerSlot& Slot,
		const UJTSStellarLootTable* LootTable)
	{
		if (!Slot.StandardItem.IsEmpty()) return Slot.StandardItem;
		return !Slot.StellarItemId.IsNone() && IsValid(LootTable)
			? LootTable->MakeTextItem(Slot.StellarItemId) : FJTSItemInstance();
	}
}

AJTSPlayerState::AJTSPlayerState()
{
	bReplicates = true;
	ShipLockerSlots.SetNum(ShipLockerCapacity);
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
		Slot.bPendingStellarReveal = false;
		Slot.SlotToken = FGuid::NewGuid();
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
		Slot.bPendingStellarReveal = true;
		Slot.SlotToken = FGuid::NewGuid();
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

bool AJTSPlayerState::TryStoreCarriedItemAtSlot(int32 LockerSlotIndex, UJTSInventoryComponent* Inventory,
	int32 CarriedSlotIndex, FGuid ExpectedItemId)
{
	if (!HasAuthority() || !IsValid(Inventory) || !ShipLockerSlots.IsValidIndex(LockerSlotIndex)
		|| !ShipLockerSlots[LockerSlotIndex].IsEmpty()) return false;
	FJTSItemInstance MovedItem;
	if (!Inventory->TryExtractItemAtSlot(CarriedSlotIndex, ExpectedItemId, MovedItem)) return false;
	FJTSShipLockerSlot& Slot = ShipLockerSlots[LockerSlotIndex];
	Slot.StandardItem = MovedItem.ItemId == EJTSItemId::StellarText ? FJTSItemInstance() : MovedItem;
	Slot.StellarItemId = MovedItem.ItemId == EJTSItemId::StellarText
		? MovedItem.StellarItemId : NAME_None;
	Slot.bPendingStellarReveal = false;
	Slot.SlotToken = FGuid::NewGuid();
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
	if (IncomingItem.IsEmpty()) return false;
	FJTSItemInstance ReplacedItem;
	if (!Inventory->TryExchangeItemAtSlot(CarriedSlotIndex, ExpectedCarriedItemId,
		IncomingItem, ReplacedItem)) return false;
	if (ReplacedItem.IsEmpty())
	{
		LockerEntry.Clear();
	}
	else
	{
		LockerEntry.StandardItem = ReplacedItem.ItemId == EJTSItemId::StellarText
			? FJTSItemInstance() : ReplacedItem;
		LockerEntry.StellarItemId = ReplacedItem.ItemId == EJTSItemId::StellarText
			? ReplacedItem.StellarItemId : NAME_None;
		LockerEntry.SlotToken = FGuid::NewGuid();
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
	if (Item.IsEmpty() || !Inventory->CanAddItem(Item.ItemId, Item.StackCount)) return false;
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
		if (!ShipLockerSlots[Index].IsEmpty() && !ShipLockerSlots[Index].SlotToken.IsValid())
		{
			ShipLockerSlots[Index].SlotToken = FGuid::NewGuid();
		}
	}
	OnRep_ShipLockerSlots();
	ForceNetUpdate();
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
		for (FJTSShipLockerSlot& Slot : ShipLockerSlots) Slot.bPendingStellarReveal = false;
	}
}
