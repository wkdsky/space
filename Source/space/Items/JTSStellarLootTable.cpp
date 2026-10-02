// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Items/JTSStellarLootTable.h"

const FJTSStellarLootEntry* UJTSStellarLootTable::FindEntry(FName ItemId) const
{
	return Entries.FindByPredicate([ItemId](const FJTSStellarLootEntry& Entry)
	{
		return Entry.ItemId == ItemId && !ItemId.IsNone();
	});
}

FJTSItemInstance UJTSStellarLootTable::MakeTextItem(FName ItemId) const
{
	FJTSItemInstance Item;
	const FJTSStellarLootEntry* const Entry = FindEntry(ItemId);
	if (!Entry) return Item;
	Item.ItemId = EJTSItemId::StellarText;
	Item.StackCount = 1;
	Item.InstanceId = FGuid::NewGuid();
	Item.StellarItemId = Entry->ItemId;
	Item.CustomDisplayName = Entry->DisplayName;
	return Item;
}

double UJTSStellarLootTable::BuildRollWeights(const TArray<FJTSShipLockerSlot>& OwnedSlots,
	const TArray<FJTSItemInstance>& CarriedItems,
	TArray<double>& OutWeights) const
{
	TMap<FName, int32> CoreCopies;
	auto CountOwnedCore = [this, &CoreCopies](FName OwnedItemId)
	{
		if (const FJTSStellarLootEntry* const Owned = FindEntry(OwnedItemId); Owned && Owned->bCore)
		{
			++CoreCopies.FindOrAdd(Owned->ItemId);
		}
	};
	for (const FJTSShipLockerSlot& Slot : OwnedSlots)
	{
		CountOwnedCore(!Slot.StellarItemId.IsNone()
			? Slot.StellarItemId : Slot.StandardItem.StellarItemId);
	}
	for (const FJTSItemInstance& Item : CarriedItems)
	{
		if (Item.ItemId == EJTSItemId::StellarText && !Item.IsEmpty())
		{
			CountOwnedCore(Item.StellarItemId);
		}
	}

	OutWeights.Reset();
	OutWeights.Reserve(Entries.Num());
	double TotalWeight = 0.0;
	for (const FJTSStellarLootEntry& Entry : Entries)
	{
		double Weight = 0.0;
		if (!Entry.ItemId.IsNone() && !Entry.DisplayName.IsEmpty() && Entry.WeightScale > 0.0f)
		{
			Weight = FMath::Pow(static_cast<double>(FMath::Clamp(GradeWeightRatio, 0.01f, 0.99f)),
				FMath::Clamp(Entry.RarityRank, 0, 25)) * Entry.WeightScale;
			if (Entry.bCore)
			{
				const int32 SameCopies = CoreCopies.FindRef(Entry.ItemId);
				const int32 OtherKinds = CoreCopies.Num() - (SameCopies > 0 ? 1 : 0);
				const double CoreFactor = FMath::Pow(static_cast<double>(SameCoreCopyMultiplier), SameCopies)
					* FMath::Pow(static_cast<double>(OtherCoreKindMultiplier), OtherKinds);
				Weight *= FMath::Max(static_cast<double>(MinimumCoreMultiplier), CoreFactor);
			}
		}
		OutWeights.Add(Weight);
		TotalWeight += Weight;
	}
	return TotalWeight;
}

bool UJTSStellarLootTable::GetRollProbabilities(const TArray<FJTSShipLockerSlot>& OwnedSlots,
	TArray<double>& OutProbabilities, const TArray<FJTSItemInstance>& CarriedItems) const
{
	const double TotalWeight = BuildRollWeights(OwnedSlots, CarriedItems, OutProbabilities);
	if (TotalWeight <= 0.0) return false;
	for (double& Probability : OutProbabilities) Probability /= TotalWeight;
	return true;
}

bool UJTSStellarLootTable::Roll(const TArray<FJTSShipLockerSlot>& OwnedSlots, FName& OutItemId,
	const TArray<FJTSItemInstance>& CarriedItems) const
{
	OutItemId = NAME_None;
	TArray<double> Weights;
	const double TotalWeight = BuildRollWeights(OwnedSlots, CarriedItems, Weights);
	if (TotalWeight <= 0.0)
	{
		return false;
	}

	double Ticket = FMath::FRand() * TotalWeight;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Ticket -= Weights[Index];
		if (Ticket < 0.0 && Weights[Index] > 0.0)
		{
			OutItemId = Entries[Index].ItemId;
			return true;
		}
	}
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		if (Weights[Index] > 0.0)
		{
			OutItemId = Entries[Index].ItemId;
			return true;
		}
	}
	return false;
}
