#pragma once

#include "CoreMinimal.h"
#include "space/Items/JTSItemTypes.h"
#include "JTSStellarLoadoutTypes.generated.h"

class UJTSStellarLootTable;

USTRUCT(BlueprintType)
struct SPACE_API FJTSStellarWeaponBinding
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 CoreSlot = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FGuid CoreInstanceId;
	UPROPERTY(BlueprintReadOnly) FGuid AttachmentInstanceId;
	UPROPERTY(BlueprintReadOnly) FName CoreId;
	UPROPERTY(BlueprintReadOnly) FName AttachmentId;
	UPROPERTY(BlueprintReadOnly) TArray<double> EffectiveLevels;
};

/** Slot 0 is the spare diamond; (1,2), (3,4), (5,6), (7,8) are vertical weapon pairs. */
struct SPACE_API FJTSStellarLoadoutRules
{
	static constexpr int32 Capacity = 9;
	static constexpr int32 SpareSlot = 0;
	static bool IsCoreSlot(int32 Index) { return Index > SpareSlot && Index < Capacity && Index % 2 == 1; }
	static int32 PairCount(int32 Available) { return FMath::Clamp((Available - 1) / 2, 0, 4); }
	static int32 AvailableSlots(int32 Participants) { return 11 - 2 * FMath::Clamp(Participants, 1, 4); }
	static bool CanStore(const FJTSItemInstance& Item, const UJTSStellarLootTable* Table);
	static TArray<FJTSStellarWeaponBinding> Assemble(const TArray<FJTSItemInstance>& Slots,
		int32 Available, const UJTSStellarLootTable* Table);
};
