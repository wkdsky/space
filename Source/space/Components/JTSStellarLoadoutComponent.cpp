#include "space/Components/JTSStellarLoadoutComponent.h"


#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "space/Core/JTSGameState.h"
#include "space/Items/JTSStellarLootTable.h"
#include "space/Items/JTSStellarProgression.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"

UJTSStellarLoadoutComponent::UJTSStellarLoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	Slots.SetNum(FJTSStellarLoadoutRules::Capacity);
}

void UJTSStellarLoadoutComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority())
	{
		LastUpdateTime = GetWorld()->GetTimeSeconds();
		GetWorld()->GetTimerManager().SetTimer(RuntimeTimer, this, &ThisClass::UpdateRuntime, 0.2f, true);
	}
}

void UJTSStellarLoadoutComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(RuntimeTimer);
	Super::EndPlay(Reason);
}

FJTSItemInstance UJTSStellarLoadoutComponent::GetSlot(int32 Index) const
{
	return Slots.IsValidIndex(Index) ? Slots[Index] : FJTSItemInstance();
}

const UJTSStellarLootTable* UJTSStellarLoadoutComponent::GetLootTable() const
{
	if (CachedLootTable) return CachedLootTable;
	const AJTSGameState* GS = GetWorld() ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
	return GS && GS->GetActiveSpacecraft() ? GS->GetActiveSpacecraft()->GetStellarLootTable() : nullptr;
}

void UJTSStellarLoadoutComponent::ConfigureLootTable(UJTSStellarLootTable* Table)
{
	if (CachedLootTable == Table) return;
	CachedLootTable = Table;
	OnRep_State();
}

TArray<FJTSStellarWeaponBinding> UJTSStellarLoadoutComponent::GetWeapons() const
{
	return FJTSStellarLoadoutRules::Assemble(Slots, AvailableSlotCount, GetLootTable());
}

int32 UJTSStellarLoadoutComponent::GetAvailableFirmwareUnits() const
{
	const AJTSPlayerState* PS = Cast<AJTSPlayerState>(GetOwner());
	const UJTSStellarLootTable* Table = GetLootTable();
	if (!PS || !Table) return 0;
	int32 Units = PS->GetStellarFirmwareUnits();
	for (int32 Index = 0; Index < AvailableSlotCount; ++Index)
	{
		const auto* Entry = Table->FindEntry(Slots[Index].StellarItemId);
		if (!Slots[Index].IsEmpty() && Entry && Entry->FirmwareUnits > 0) Units += Entry->FirmwareUnits;
	}
	const AJTSCharacter* Character = Cast<AJTSCharacter>(PS->GetPawn());
	const UJTSInventoryComponent* Inventory = Character ? Character->GetInventoryComponent() : nullptr;
	if (Inventory) for (int32 Index = 0; Index < Inventory->GetInventoryCapacity(); ++Index)
	{
		const auto Item = Inventory->GetItemAtSlot(Index);
		const auto* Entry = Table->FindEntry(Item.StellarItemId);
		if (!Item.IsEmpty() && Item.ItemId == EJTSItemId::StellarText && Item.StackCount == 1
			&& Entry && Entry->FirmwareUnits > 0) Units += Entry->FirmwareUnits;
	}
	return Units;
}

bool UJTSStellarLoadoutComponent::GetActiveWeapon(FJTSStellarWeaponBinding& Out) const
{
	for (const auto& Weapon : GetWeapons()) if (Weapon.CoreSlot == ActiveCoreSlot) { Out = Weapon; return true; }
	return false;
}

void UJTSStellarLoadoutComponent::SelectWeapon(int32 CoreSlot)
{
	if (GetOwner()->HasAuthority()) ServerSelectWeapon_Implementation(CoreSlot);
	else ServerSelectWeapon(CoreSlot);
}

void UJTSStellarLoadoutComponent::ServerSelectWeapon_Implementation(int32 CoreSlot)
{
	RefreshParticipants();
	if (CoreSlot != INDEX_NONE && !GetWeapons().ContainsByPredicate(
		[&](const auto& Weapon) { return Weapon.CoreSlot == CoreSlot; })) return;
	if (ActiveCoreSlot != CoreSlot) { ActiveCoreSlot = CoreSlot; Changed(); }
}

void UJTSStellarLoadoutComponent::StopStellarWeapon()
{
	if (GetOwner()->HasAuthority() && ActiveCoreSlot != INDEX_NONE)
	{
		ActiveCoreSlot = INDEX_NONE;
		Changed();
	}
}

void UJTSStellarLoadoutComponent::RefreshParticipants()
{
	if (!GetOwner()->HasAuthority()) return;
	const AJTSGameState* GS = GetWorld()->GetGameState<AJTSGameState>();
	int32 Count = 0;
	if (GS) for (const APlayerState* Raw : GS->PlayerArray)
	{
		const auto* PS = Cast<AJTSPlayerState>(Raw);
		if (PS && (PS->GetExpeditionStatus() == EJTSPlayerExpeditionStatus::Active
			|| PS->GetExpeditionStatus() == EJTSPlayerExpeditionStatus::Boarded
			|| PS->GetExpeditionStatus() == EJTSPlayerExpeditionStatus::Dead)) ++Count;
	}
	const int32 NewAvailable = FJTSStellarLoadoutRules::AvailableSlots(Count);
	if (AvailableSlotCount != NewAvailable)
	{
		AvailableSlotCount = NewAvailable;
		FJTSStellarWeaponBinding Binding;
		if (!GetActiveWeapon(Binding)) ActiveCoreSlot = INDEX_NONE;
		Changed();
	}
}

void UJTSStellarLoadoutComponent::UpdateRuntime()
{
	RefreshParticipants();
	const double Now = GetWorld()->GetTimeSeconds();
	if (!bChannelActive && Now > LastEnergyUse + 1.0)
	{
		const double Delta = Now - FMath::Max(LastUpdateTime, LastEnergyUse + 1.0);
		Energy = FMath::Clamp(Energy + static_cast<float>(Delta) * 16.0f, 0.0f, 100.0f);
	}
	LastUpdateTime = Now;
}

bool UJTSStellarLoadoutComponent::ConsumeEnergy(float Amount)
{
	if (!GetOwner()->HasAuthority() || !FMath::IsFinite(Amount) || Amount <= 0 || Energy + KINDA_SMALL_NUMBER < Amount) return false;
	Energy = FMath::Max(0.0f, Energy - Amount);
	LastEnergyUse = GetWorld()->GetTimeSeconds();
	return true;
}

void UJTSStellarLoadoutComponent::ServerMoveSlot_Implementation(int32 From, FGuid ExpectedFrom, int32 To, FGuid ExpectedTo)
{
	RefreshParticipants();
	if (From == To || !Slots.IsValidIndex(From) || To < 0 || To >= AvailableSlotCount
		|| Slots[From].IsEmpty() || Slots[From].InstanceId != ExpectedFrom || Slots[To].InstanceId != ExpectedTo
		|| (From >= AvailableSlotCount && !Slots[To].IsEmpty())) { ClientActionResult(false); return; }
	if (!FJTSStellarLoadoutRules::CanStore(Slots[From], GetLootTable())
		|| !FJTSStellarLoadoutRules::CanStore(Slots[To], GetLootTable())) { ClientActionResult(false); return; }
	Swap(Slots[From], Slots[To]);
	ActiveCoreSlot = INDEX_NONE;
	Changed();
	ClientActionResult(true);
}

void UJTSStellarLoadoutComponent::ServerExchangeLocker_Implementation(AJTSSpacecraftActor* Ship,
	int32 LockerIndex, FGuid LockerToken, int32 LoadoutIndex, FGuid ExpectedItem)
{
	AJTSPlayerState* PS = Cast<AJTSPlayerState>(GetOwner());
	if (!PS || !IsValid(Ship) || !Ship->CanUseShipTerminal(PS->GetPawn()) || !Slots.IsValidIndex(LoadoutIndex)) { ClientActionResult(false); return; }
	RefreshParticipants();
	const FJTSShipLockerSlot Locker = PS->GetShipLockerSlot(LockerIndex);
	if (Slots[LoadoutIndex].InstanceId != ExpectedItem || Locker.SlotToken != LockerToken
		|| Locker.bPendingStellarReveal || !Locker.StandardItem.IsEmpty()) { ClientActionResult(false); return; }
	const UJTSStellarLootTable* Table = Ship->GetStellarLootTable();
	if (!IsValid(Table)) { ClientActionResult(false); return; }
	CachedLootTable = const_cast<UJTSStellarLootTable*>(Table);
	// Sealed positions permit extraction to an empty locker, never insertion or swapping.
	if (LoadoutIndex >= AvailableSlotCount && !Locker.IsEmpty()) { ClientActionResult(false); return; }
	FJTSItemInstance Incoming;
	FJTSItemInstance IncomingAttachment;
	if (!Locker.IsEmpty())
	{
		const auto* Entry = Table->FindEntry(Locker.StellarItemId);
		if (!Entry) { ClientActionResult(false); return; }
		Incoming = Table->MakeTextItem(Locker.StellarCoreId.IsNone() ? Locker.StellarItemId : Locker.StellarCoreId);
		Incoming.InstanceId = Locker.StellarInstanceId.IsValid() ? Locker.StellarInstanceId : Locker.SlotToken;
		Incoming.StellarCoreLevel = Locker.StellarCoreLevel;
		if (Locker.StellarCoreId.IsNone()) Incoming.StellarPoints = Locker.StellarPoints;
		else
		{
			// One-time migration of old combined items into two independent physical slots.
			if (!FJTSStellarLoadoutRules::IsCoreSlot(LoadoutIndex) || !Slots[LoadoutIndex].IsEmpty() || LoadoutIndex + 1 >= AvailableSlotCount
				|| !Slots[LoadoutIndex + 1].IsEmpty() || !Table->CanCombine(Locker.StellarCoreId, Locker.StellarItemId)) { ClientActionResult(false); return; }
			IncomingAttachment = Table->MakeTextItem(Locker.StellarItemId);
			IncomingAttachment.InstanceId = Locker.StellarAttachmentInstanceId.IsValid()
				? Locker.StellarAttachmentInstanceId : FGuid::NewGuid();
			IncomingAttachment.StellarPoints = Locker.StellarPoints;
		}
	}
	if ((Incoming.IsEmpty() && Slots[LoadoutIndex].IsEmpty())
		|| !FJTSStellarLoadoutRules::CanStore(Incoming, Table)
		|| !FJTSStellarLoadoutRules::CanStore(IncomingAttachment, Table)) { ClientActionResult(false); return; }
	if (!PS->TryReplaceStellarLockerSlot(LockerIndex, LockerToken, Slots[LoadoutIndex])) { ClientActionResult(false); return; }
	Slots[LoadoutIndex] = Incoming;
	if (!IncomingAttachment.IsEmpty()) Slots[LoadoutIndex + 1] = IncomingAttachment;
	ActiveCoreSlot = INDEX_NONE;
	Changed();
	ClientActionResult(true);
}

void UJTSStellarLoadoutComponent::ServerExchangeInventory_Implementation(int32 InventoryIndex,
	FGuid ExpectedInventoryItem, int32 LoadoutIndex, FGuid ExpectedItem)
{
	const AJTSPlayerState* PS = Cast<AJTSPlayerState>(GetOwner());
	AJTSCharacter* Character = PS ? Cast<AJTSCharacter>(PS->GetPawn()) : nullptr;
	UJTSInventoryComponent* Inventory = Character ? Character->GetInventoryComponent() : nullptr;
	RefreshParticipants();
	// Only migrate legacy carried stellar items into empty equipment slots. Stellar equipment
	// can never be extracted or swapped back into the character's ordinary inventory.
	if (!Inventory || !Slots.IsValidIndex(LoadoutIndex) || Slots[LoadoutIndex].InstanceId != ExpectedItem
		|| !Slots[LoadoutIndex].IsEmpty()
		|| InventoryIndex < 0 || InventoryIndex >= Inventory->GetInventoryCapacity()) { ClientActionResult(false); return; }
	const FJTSItemInstance Incoming = Inventory->GetItemAtSlot(InventoryIndex);
	const UJTSStellarLootTable* Table = GetLootTable();
	if (Incoming.InstanceId != ExpectedInventoryItem || (!Incoming.IsEmpty()
		&& (LoadoutIndex >= AvailableSlotCount || !Table || Incoming.StackCount != 1
			|| (Incoming.ItemId != EJTSItemId::StellarText && Incoming.ItemId != EJTSItemId::StellarWeapon)
			|| !Table->FindEntry(Incoming.StellarItemId)))) { ClientActionResult(false); return; }
	FJTSItemInstance Core = Incoming, Attachment;
	if (!Incoming.IsEmpty() && Incoming.ItemId == EJTSItemId::StellarWeapon)
	{
		if (!FJTSStellarLoadoutRules::IsCoreSlot(LoadoutIndex)
			|| !Table->CanCombine(Incoming.StellarCoreId, Incoming.StellarItemId) || !Slots[LoadoutIndex].IsEmpty()
			|| LoadoutIndex + 1 >= AvailableSlotCount || !Slots[LoadoutIndex + 1].IsEmpty()) { ClientActionResult(false); return; }
		Core = Table->MakeTextItem(Incoming.StellarCoreId); Core.InstanceId = Incoming.InstanceId;
		Core.StellarCoreLevel = Incoming.StellarCoreLevel;
		Attachment = Table->MakeTextItem(Incoming.StellarItemId);
		Attachment.InstanceId = Incoming.StellarAttachmentInstanceId.IsValid() ? Incoming.StellarAttachmentInstanceId : FGuid::NewGuid();
		Attachment.StellarPoints = Incoming.StellarPoints;
	}
	if (!FJTSStellarLoadoutRules::CanStore(Core, Table)
		|| !FJTSStellarLoadoutRules::CanStore(Attachment, Table)) { ClientActionResult(false); return; }
	FJTSItemInstance Replaced;
	const bool bExchanged = !Incoming.IsEmpty()
		&& Inventory->TryExtractItemAtSlot(InventoryIndex, ExpectedInventoryItem, Replaced);
	if (!bExchanged) { ClientActionResult(false); return; }
	Slots[LoadoutIndex] = Core;
	if (!Attachment.IsEmpty()) Slots[LoadoutIndex + 1] = Attachment;
	ActiveCoreSlot = INDEX_NONE;
	Changed(); ClientActionResult(true);
}

void UJTSStellarLoadoutComponent::ServerUpgradeCore_Implementation(int32 CoreSlot, FGuid ExpectedItem, int32 ExpectedRevision)
{
	RefreshParticipants();
	AJTSPlayerState* PS = Cast<AJTSPlayerState>(GetOwner());
	const auto* Table = GetLootTable();
	if (!PS || !Table || ExpectedRevision != Revision || CoreSlot < 0 || CoreSlot >= AvailableSlotCount
		|| Slots[CoreSlot].InstanceId != ExpectedItem || Slots[CoreSlot].IsEmpty()
		|| Slots[CoreSlot].StellarCoreLevel >= FJTSStellarProgression::MaxCoreLevel) { ClientActionResult(false); return; }
	const auto* Core = Table->FindEntry(Slots[CoreSlot].StellarItemId);
	if (!Core || !Core->bCore) { ClientActionResult(false); return; }
	const int32 Cost = FJTSStellarProgression::GetUpgradeUnitCost(Slots[CoreSlot].StellarCoreLevel);
	int32 Units = PS->GetStellarFirmwareUnits();
	TArray<int32> SpendSlots;
	for (int32 Index = 0; Index < AvailableSlotCount && Units < Cost; ++Index)
	{
		const auto* Entry = Table->FindEntry(Slots[Index].StellarItemId);
		if (Index != CoreSlot && !Slots[Index].IsEmpty() && Entry && Entry->FirmwareUnits > 0)
		{ Units += Entry->FirmwareUnits; SpendSlots.Add(Index); }
	}
	// Legacy saves may retain firmware in the ordinary inventory; consume it without allowing new transfers there.
	AJTSCharacter* Character = Cast<AJTSCharacter>(PS->GetPawn());
	UJTSInventoryComponent* Inventory = Character ? Character->GetInventoryComponent() : nullptr;
	TArray<int32> SpendInventorySlots;
	if (Inventory) for (int32 Index = 0; Index < Inventory->GetInventoryCapacity() && Units < Cost; ++Index)
	{
		const auto Item = Inventory->GetItemAtSlot(Index);
		const auto* Entry = Table->FindEntry(Item.StellarItemId);
		if (!Item.IsEmpty() && Item.ItemId == EJTSItemId::StellarText && Item.StackCount == 1
			&& Entry && Entry->FirmwareUnits > 0)
		{ Units += Entry->FirmwareUnits; SpendInventorySlots.Add(Index); }
	}
	if (Units < Cost) { ClientActionResult(false); return; }
	for (int32 Index : SpendInventorySlots)
	{
		FJTSItemInstance Spent;
		Inventory->TryExtractItemAtSlot(Index, Inventory->GetItemAtSlot(Index).InstanceId, Spent);
	}
	for (int32 Index : SpendSlots) Slots[Index].Clear();
	PS->RestoreStellarFirmwareUnits(Units - Cost);
	++Slots[CoreSlot].StellarCoreLevel;
	Changed(); ClientActionResult(true);
}

void UJTSStellarLoadoutComponent::ServerApplyPoints_Implementation(int32 AttachmentSlot, FGuid ExpectedItem,
	int32 ExpectedRevision, const TArray<uint8>& Points)
{
	RefreshParticipants();
	if (ExpectedRevision != Revision || AttachmentSlot < 0 || AttachmentSlot >= AvailableSlotCount
		|| Slots[AttachmentSlot].InstanceId != ExpectedItem || Points.Num() != 6) { ClientActionResult(false); return; }
	const auto* Table = GetLootTable();
	const auto* Entry = Table ? Table->FindEntry(Slots[AttachmentSlot].StellarItemId) : nullptr;
	if (!Entry || Entry->bCore || Entry->CompatibleCoreId.IsNone()) { ClientActionResult(false); return; }
	const TArray<FJTSStellarWeaponBinding> Weapons = GetWeapons();
	const auto* Binding = Weapons.FindByPredicate([AttachmentSlot](const auto& Weapon)
		{ return Weapon.CoreSlot + 1 == AttachmentSlot; });
	// An inactive attachment cannot mutate its allocation, even by submitting an all-zero reset.
	if (!Binding) { ClientActionResult(false); return; }
	const int32 Budget = FJTSStellarProgression::GetCoreBudget(Slots[Binding->CoreSlot].StellarCoreLevel);
	int32 Total = 0;
	for (uint8 Point : Points) { if (Point > 10) { ClientActionResult(false); return; } Total += Point; }
	if (Total > Budget) { ClientActionResult(false); return; }
	Slots[AttachmentSlot].StellarPoints = Points;
	Changed();
	ClientActionResult(true);
}

void UJTSStellarLoadoutComponent::ClientActionResult_Implementation(bool bSucceeded)
{
	++ActionResponseCount;
	LastActionMessage = bSucceeded ? TEXT("服务器已保存") : TEXT("未保存：物品、预算或可用格已变化，请重新选择。");
}

void UJTSStellarLoadoutComponent::RestoreState(const TArray<FJTSItemInstance>& SavedSlots,
	float SavedEnergy, int32 SavedActiveSlot)
{
	if (!GetOwner()->HasAuthority()) return;
	Slots = SavedSlots;
	Slots.SetNum(FJTSStellarLoadoutRules::Capacity);
	for (auto& Item : Slots) if (!Item.IsEmpty())
	{
		if (!Item.InstanceId.IsValid()) Item.InstanceId = FGuid::NewGuid();
		FJTSStellarProgression::NormalizePoints(Item.StellarPoints);
		Item.StellarCoreLevel = FMath::Clamp(Item.StellarCoreLevel, 1, 61);
	}
	Energy = FMath::IsFinite(SavedEnergy) ? FMath::Clamp(SavedEnergy, 0.0f, 100.0f) : 100.0f;
	// Fields and channels never resume from save/travel. Require an explicit selection.
	ActiveCoreSlot = INDEX_NONE;
	bChannelActive = false;
	LastEnergyUse = GetWorld()->GetTimeSeconds();
	Changed();
}

void UJTSStellarLoadoutComponent::Changed()
{
	++Revision;
	OnRep_State();
	GetOwner()->ForceNetUpdate();
}

void UJTSStellarLoadoutComponent::OnRep_State() { OnLoadoutChanged.Broadcast(); }

void UJTSStellarLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UJTSStellarLoadoutComponent, Slots, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UJTSStellarLoadoutComponent, AvailableSlotCount, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UJTSStellarLoadoutComponent, ActiveCoreSlot, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UJTSStellarLoadoutComponent, Revision, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UJTSStellarLoadoutComponent, Energy, COND_OwnerOnly);
}

void UJTSStellarLoadoutComponent::RefundEnergy(float Amount)
{
	if (GetOwner()->HasAuthority() && FMath::IsFinite(Amount) && Amount > 0) Energy = FMath::Min(100.f, Energy + Amount);
}