#include "space/Items/JTSStellarLoadoutTypes.h"
#include "space/Items/JTSStellarLootTable.h"
#include "space/Items/JTSStellarProgression.h"

bool FJTSStellarLoadoutRules::CanStore(const FJTSItemInstance& Item, const UJTSStellarLootTable* Table)
{
	if (Item.IsEmpty()) return true;
	if (!IsValid(Table) || Item.ItemId != EJTSItemId::StellarText || Item.StackCount != 1
		|| !Item.InstanceId.IsValid()) return false;
	const auto* Entry = Table->FindEntry(Item.StellarItemId);
	return Entry && Entry->FirmwareUnits == 0 && (Entry->bCore || !Entry->CompatibleCoreId.IsNone());
}

TArray<FJTSStellarWeaponBinding> FJTSStellarLoadoutRules::Assemble(
	const TArray<FJTSItemInstance>& Slots, int32 Available, const UJTSStellarLootTable* Table)
{
	TArray<FJTSStellarWeaponBinding> Result;
	if (!IsValid(Table)) return Result;
	const int32 End = FMath::Min3(Available, Slots.Num(), Capacity);
	for (int32 Index = 1; Index + 1 < End; Index += 2)
	{
		const FJTSItemInstance& Core = Slots[Index];
		const FJTSItemInstance& Attachment = Slots[Index + 1];
		if (Core.IsEmpty() || Attachment.IsEmpty() || !CanStore(Core, Table) || !CanStore(Attachment, Table)
			|| !Table->CanCombine(Core.StellarItemId, Attachment.StellarItemId)) continue;
		FJTSStellarWeaponBinding& Binding = Result.AddDefaulted_GetRef();
		Binding.CoreSlot = Index;
		Binding.CoreInstanceId = Core.InstanceId;
		Binding.AttachmentInstanceId = Attachment.InstanceId;
		Binding.CoreId = Core.StellarItemId;
		Binding.AttachmentId = Attachment.StellarItemId;
		FJTSStellarProgression::ComputeEffectiveLevels(Attachment.StellarPoints,
			FJTSStellarProgression::GetCoreBudget(Core.StellarCoreLevel), Binding.EffectiveLevels);
	}
	return Result;
}
