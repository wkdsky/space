// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "space/Items/JTSItemTypes.h"
#include "space/Items/JTSShipLockerTypes.h"
#include "JTSStellarLootTable.generated.h"

/** A to Z is an ordering, not a fixed percentage. The server normalizes all current weights. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSStellarLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	FName ItemId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	FText DisplayName;

	/** 0=A (most common), 25=Z (least common). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0", ClampMax = "25"))
	int32 RarityRank = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	bool bCore = false;

	/** An attachment combines only with this core in the preceding locker slot. Empty for cores and materials. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	FName CompatibleCoreId;

	/** Fine tuning within the same grade. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.01"))
	float WeightScale = 1.0f;
};

/** Blueprint-authored stellar item pool and economy knobs for a ship terminal. */
UCLASS(BlueprintType)
class SPACE_API UJTSStellarLootTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static constexpr float RevealDurationSeconds = 2.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TArray<FJTSStellarLootEntry> Entries;

	/** Every step toward Z multiplies weight by this ratio (design q = 0.65). The total is normalized each roll. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Balance", meta = (ClampMin = "0.01", ClampMax = "0.99"))
	float GradeWeightRatio = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Economy")
	TArray<FJTSItemCost> SpinCosts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Economy", meta = (ClampMin = "0.0"))
	float SpinCooldownSeconds = 2.0f;

	const FJTSStellarLootEntry* FindEntry(FName ItemId) const;
	FJTSItemInstance MakeTextItem(FName ItemId) const;
	bool CanCombine(FName CoreId, FName AttachmentId) const;
	FText GetWeaponDisplayName(FName CoreId, FName AttachmentId) const;
	FJTSItemInstance MakeWeaponItem(FName CoreId, FName AttachmentId) const;
	/**
	 * U: distinct core kinds this player has ever rolled. A core's weight is q^(rank + U), so every
	 * core slides U grades toward Z while other entries keep q^rank.
	 */
	int32 CountDiscoveredCoreKinds(const TArray<FName>& DiscoveredCoreIds) const;
	/** Probabilities in the same order as Entries, using the exact server roll weights. */
	bool GetRollProbabilities(const TArray<FName>& DiscoveredCoreIds, TArray<double>& OutProbabilities) const;
	bool Roll(const TArray<FName>& DiscoveredCoreIds, FName& OutItemId) const;

private:
	double BuildRollWeights(const TArray<FName>& DiscoveredCoreIds, TArray<double>& OutWeights) const;
};
