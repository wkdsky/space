// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "space/Items/JTSItemTypes.h"
#include "JTSShipLockerTypes.generated.h"

/** One terminal slot. Stellar items are text-only prototypes until their gameplay definitions exist. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSShipLockerSlot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship Locker")
	FJTSItemInstance StandardItem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship Locker")
	FName StellarItemId = NAME_None;

	/** Nonempty only for an assembled weapon; StellarItemId is then its attachment. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship Locker")
	FName StellarCoreId = NAME_None;

	/** Reserved on the server while the reel spins. The UI must keep this slot visually empty. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship Locker")
	bool bPendingStellarReveal = false;

	/** Core level of a core or assembled weapon stored here (budget = level - 1). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship Locker")
	int32 StellarCoreLevel = 1;

	/** Recorded attachment skill points, six entries of 0..10 (empty = all zero). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship Locker")
	TArray<uint8> StellarPoints;

	/** Guards a drag started before the server changes this slot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship Locker")
	FGuid SlotToken;

	/** Persistent item identities; slot tokens may change on edit/move. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship Locker")
	FGuid StellarInstanceId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship Locker")
	FGuid StellarAttachmentInstanceId;

	bool IsEmpty() const { return StandardItem.IsEmpty() && StellarItemId.IsNone() && !bPendingStellarReveal; }
	void Clear()
	{
		StandardItem.Clear();
		StellarItemId = NAME_None;
		StellarCoreId = NAME_None;
		bPendingStellarReveal = false;
		StellarCoreLevel = 1;
		StellarPoints.Reset();
		SlotToken.Invalidate();
		StellarInstanceId.Invalidate();
		StellarAttachmentInstanceId.Invalidate();
	}
};
