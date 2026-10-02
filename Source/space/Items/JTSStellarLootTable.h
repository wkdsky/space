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

	/** Every step toward Z multiplies weight by this ratio. The total is normalized each roll. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Balance", meta = (ClampMin = "0.01", ClampMax = "0.99"))
	float GradeWeightRatio = 0.72f;

	/** Applied to a core for each copy of that same core in this player's locker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Balance", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float SameCoreCopyMultiplier = 0.55f;

	/** Applied to a core for each other core kind already owned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Balance", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float OtherCoreKindMultiplier = 0.82f;

	/** Prevents a core from becoming impossible to win. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Balance", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float MinimumCoreMultiplier = 0.03f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Economy")
	TArray<FJTSItemCost> SpinCosts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Economy", meta = (ClampMin = "0.0"))
	float SpinCooldownSeconds = 2.0f;

	const FJTSStellarLootEntry* FindEntry(FName ItemId) const;
	FJTSItemInstance MakeTextItem(FName ItemId) const;
	/** Probabilities in the same order as Entries, using the exact server roll weights. */
	bool GetRollProbabilities(const TArray<FJTSShipLockerSlot>& OwnedSlots,
		TArray<double>& OutProbabilities,
		const TArray<FJTSItemInstance>& CarriedItems = {}) const;
	bool Roll(const TArray<FJTSShipLockerSlot>& OwnedSlots, FName& OutItemId,
		const TArray<FJTSItemInstance>& CarriedItems = {}) const;

private:
	double BuildRollWeights(const TArray<FJTSShipLockerSlot>& OwnedSlots,
		const TArray<FJTSItemInstance>& CarriedItems,
		TArray<double>& OutWeights) const;
};
