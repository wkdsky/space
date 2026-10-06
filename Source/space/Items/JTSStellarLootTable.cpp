// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Items/JTSStellarLootTable.h"
#include "Misc/Crc.h"

const FJTSStellarLootEntry* UJTSStellarLootTable::FindEntry(FName ItemId) const
{
	return Entries.FindByPredicate([ItemId](const FJTSStellarLootEntry& Entry)
	{
		return Entry.ItemId == ItemId && !ItemId.IsNone();
	});
}

FLinearColor UJTSStellarLootTable::GetCoreActivationColor(FName CoreId) const
{
	const auto* Entry = FindEntry(CoreId);
	if (!Entry || !Entry->bCore) return FLinearColor::Transparent;
	if (Entry->ActivationColor.A > 0.0f) return Entry->ActivationColor.CopyWithNewOpacity(1.0f);
	const uint32 Hue = FCrc::StrCrc32(*CoreId.ToString()) % 360;
	return FLinearColor(static_cast<float>(Hue), 0.72f, 1.0f).HSVToLinearRGB();
}

FText UJTSStellarLootTable::GetItemDescription(FName ItemId, FName CoreId) const
{
	const FJTSStellarLootEntry* const Entry = FindEntry(ItemId);
	const FJTSStellarLootEntry* const Core = FindEntry(CoreId);
	const FText ItemDescription = Entry ? Entry->Description : FText::GetEmpty();
	if (!Core || Core->Description.IsEmpty()) return ItemDescription;
	if (ItemDescription.IsEmpty()) return Core->Description;
	return FText::Format(NSLOCTEXT("JTSStellarLoot", "CombinedDescription", "{0}\n{1}"),
		Core->Description, ItemDescription);
}

FJTSItemInstance UJTSStellarLootTable::MakeTextItem(FName ItemId) const
{
	FJTSItemInstance Item;
	if (ItemId.IsNone()) return Item;
	const FJTSStellarLootEntry* const Entry = FindEntry(ItemId);
	Item.ItemId = EJTSItemId::StellarText;
	Item.StackCount = 1;
	Item.InstanceId = FGuid::NewGuid();
	Item.StellarItemId = ItemId;
	Item.CustomDisplayName = Entry ? Entry->DisplayName : FText::FromName(ItemId);
	return Item;
}

bool UJTSStellarLootTable::CanCombine(FName CoreId, FName AttachmentId) const
{
	const FJTSStellarLootEntry* const Core = FindEntry(CoreId);
	const FJTSStellarLootEntry* const Attachment = FindEntry(AttachmentId);
	return Core && Core->bCore && Attachment && !Attachment->bCore
		&& !Attachment->CompatibleCoreId.IsNone() && Attachment->CompatibleCoreId == CoreId;
}

FText UJTSStellarLootTable::GetWeaponDisplayName(FName CoreId, FName AttachmentId) const
{
	if (CoreId.IsNone() || AttachmentId.IsNone()) return FText::GetEmpty();
	const FJTSStellarLootEntry* const Core = FindEntry(CoreId);
	const FJTSStellarLootEntry* const Attachment = FindEntry(AttachmentId);
	return FText::Format(FText::FromString(TEXT("{0}·{1}")),
		Core ? Core->DisplayName : FText::FromName(CoreId),
		Attachment ? Attachment->DisplayName : FText::FromName(AttachmentId));
}

FJTSItemInstance UJTSStellarLootTable::MakeWeaponItem(FName CoreId, FName AttachmentId) const
{
	FJTSItemInstance Item;
	if (CoreId.IsNone() || AttachmentId.IsNone()) return Item;
	Item.ItemId = EJTSItemId::StellarWeapon;
	Item.StackCount = 1;
	Item.InstanceId = FGuid::NewGuid();
	Item.StellarCoreId = CoreId;
	Item.StellarItemId = AttachmentId;
	Item.CustomDisplayName = GetWeaponDisplayName(CoreId, AttachmentId);
	return Item;
}

int32 UJTSStellarLootTable::CountDiscoveredCoreKinds(const TArray<FName>& DiscoveredCoreIds) const
{
	TSet<FName> Kinds;
	for (const FName CoreId : DiscoveredCoreIds)
	{
		if (const FJTSStellarLootEntry* const Entry = FindEntry(CoreId); Entry && Entry->bCore)
		{
			Kinds.Add(CoreId);
		}
	}
	return Kinds.Num();
}

double UJTSStellarLootTable::BuildRollWeights(const TArray<FName>& DiscoveredCoreIds,
	TArray<double>& OutWeights) const
{
	const int32 HistoryKinds = CountDiscoveredCoreKinds(DiscoveredCoreIds);
	const double Ratio = FMath::Clamp(GradeWeightRatio, 0.01f, 0.99f);
	OutWeights.Reset();
	OutWeights.Reserve(Entries.Num());
	double TotalWeight = 0.0;
	for (const FJTSStellarLootEntry& Entry : Entries)
	{
		double Weight = 0.0;
		if (!Entry.ItemId.IsNone() && !Entry.DisplayName.IsEmpty() && Entry.WeightScale > 0.0f)
		{
			const int32 EffectiveRank = FMath::Clamp(Entry.RarityRank, 0, 25) + (Entry.bCore ? HistoryKinds : 0);
			Weight = FMath::Pow(Ratio, EffectiveRank) * Entry.WeightScale;
		}
		OutWeights.Add(Weight);
		TotalWeight += Weight;
	}
	return TotalWeight;
}

bool UJTSStellarLootTable::GetRollProbabilities(const TArray<FName>& DiscoveredCoreIds,
	TArray<double>& OutProbabilities) const
{
	const double TotalWeight = BuildRollWeights(DiscoveredCoreIds, OutProbabilities);
	if (TotalWeight <= 0.0) return false;
	for (double& Probability : OutProbabilities) Probability /= TotalWeight;
	return true;
}

bool UJTSStellarLootTable::Roll(const TArray<FName>& DiscoveredCoreIds, FName& OutItemId) const
{
	OutItemId = NAME_None;
	TArray<double> Weights;
	const double TotalWeight = BuildRollWeights(DiscoveredCoreIds, Weights);
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
