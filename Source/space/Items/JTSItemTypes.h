// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "space/Items/JTSResourceType.h"

#include "JTSItemTypes.generated.h"

/** Stable, save-friendly identity for a Jump to Space item definition. */
UENUM(BlueprintType)
enum class EJTSItemId : uint8
{
	None UMETA(DisplayName = "Empty"),
	Fuel UMETA(DisplayName = "Fuel"),
	Water UMETA(DisplayName = "Water"),
	Food UMETA(DisplayName = "Food"),
	Rock UMETA(DisplayName = "Rock"),
	Ore UMETA(DisplayName = "Ore"),
	MoonAntCorpse UMETA(DisplayName = "Moon Ant Corpse"),
	/** Retired test weapons. Values stay reserved so old saves deserialize; they are no longer obtainable. */
	Pickaxe UMETA(Hidden),
	/** Retired serialized value kept only so old saves/world assets can be safely ignored. */
	Backpack UMETA(Hidden),
	Knife UMETA(Hidden),
	Pistol UMETA(Hidden),
	MachineGun UMETA(Hidden),
	Axe UMETA(Hidden),
	/** Stable append-only identity for the long-range precision weapon. */
	Sniper UMETA(Hidden),
	/** White belt lamp. Worn at the waist, toggled with F from the quickbar. */
	WaistLamp UMETA(DisplayName = "Waist Lamp"),
	/** Serialized ice-axe value repurposed as dual pistols to preserve existing saves. */
	IceAxe UMETA(Hidden),
	/** Text-only stellar loot identity; the specific entry remains data driven. */
	StellarText UMETA(DisplayName = "Stellar Item"),
	/** Assembled from one core and its matching attachment in the ship locker. */
	StellarWeapon UMETA(DisplayName = "Stellar Weapon"),

	/** Normal weapon line. Append-only; each is upgradeable per instance. */
	ShortBlade UMETA(DisplayName = "Alloy Short Blade"),
	ShockPole UMETA(DisplayName = "Shock Pole"),
	PowerHammer UMETA(DisplayName = "Power Hammer"),
	RailPistol UMETA(DisplayName = "Rail Pistol"),
	AssaultRifle UMETA(DisplayName = "Assault Rifle"),
	Shotgun UMETA(DisplayName = "Shotgun"),
	RailSniper UMETA(DisplayName = "Rail Sniper"),
	HeavyMachineGun UMETA(DisplayName = "Heavy Machine Gun"),
	GrenadeLauncher UMETA(DisplayName = "Grenade Launcher"),
	ArcGun UMETA(DisplayName = "Arc Gun")
};

/** Item behavior is composed from these capability bits instead of mutually exclusive item classes. */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EJTSItemCapability : uint8
{
	None = 0 UMETA(Hidden),
	Holdable = 1 << 0 UMETA(DisplayName = "Holdable"),
	/** Retained as a serialized bit only; wearable equipment no longer exists. */
	Wearable = 1 << 1 UMETA(Hidden),
	Mining = 1 << 2 UMETA(DisplayName = "Mining"),
	MeleeOverride = 1 << 3 UMETA(DisplayName = "Melee Override"),
	RangedWeapon = 1 << 4 UMETA(DisplayName = "Ranged Weapon"),
	StackableResource = 1 << 5 UMETA(DisplayName = "Stackable Resource"),
	ShopPurchasable = 1 << 6 UMETA(DisplayName = "Shop Purchasable")
};
ENUM_CLASS_FLAGS(EJTSItemCapability);

/** Presentation and catalogue classification. A definition may also appear in secondary shop filters. */
UENUM(BlueprintType)
enum class EJTSItemCategory : uint8
{
	Resources UMETA(DisplayName = "Resources / Materials"),
	Weapons UMETA(DisplayName = "Weapons"),
	Mining UMETA(DisplayName = "Mining"),
	Utility UMETA(DisplayName = "Utility"),
	/** Retained for old data assets; new items are regular inventory items. */
	Wearables UMETA(Hidden)
};

/** Shop navigation category. All is deliberately a UI filter, not an item type. */
UENUM(BlueprintType)
enum class EJTSShopCategory : uint8
{
	All UMETA(DisplayName = "All"),
	Weapons UMETA(DisplayName = "Weapons"),
	Mining UMETA(DisplayName = "Mining"),
	Utility UMETA(DisplayName = "Utility"),
	Resources UMETA(DisplayName = "Resources / Materials"),
	/** Retained for old data assets; this shop filter is no longer exposed. */
	Wearables UMETA(Hidden)
};

/** Authoritative outcome returned to the requesting client after a shop transaction. */
UENUM(BlueprintType)
enum class EJTSShopPurchaseResult : uint8
{
	Succeeded UMETA(DisplayName = "Succeeded"),
	SucceededDropped UMETA(DisplayName = "Succeeded - Dropped"),
	InvalidItem UMETA(DisplayName = "Invalid Item"),
	InsufficientResources UMETA(DisplayName = "Insufficient Resources"),
	DeliveryFailed UMETA(DisplayName = "Delivery Failed"),
	InventoryFull UMETA(DisplayName = "Ship Locker Full")
};

UENUM(BlueprintType)
enum class EJTSStellarRollResult : uint8
{
	Succeeded,
	InventoryFull,
	InsufficientResources,
	NotAvailable,
	CoolingDown
};

/** Server-side log of one resolved roll request; a resend with the same RequestId replays it without a second charge. */
struct FJTSStellarRollRecord
{
	FGuid RequestId;
	EJTSStellarRollResult Result = EJTSStellarRollResult::NotAvailable;
	FName ItemId = NAME_None;
	int32 SlotIndex = INDEX_NONE;
};

/** Legacy serialized values from the retired wearable system. */
UENUM(BlueprintType)
enum class EJTSWearableSlot : uint8
{
	None UMETA(DisplayName = "None"),
	Backpack UMETA(Hidden),
	Body UMETA(Hidden),
	Head UMETA(Hidden),
	Accessory UMETA(Hidden)
};

/** Runtime payload: definition identity stays data driven, while count/durability remain per-instance state. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSItemInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	EJTSItemId ItemId = EJTSItemId::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "0"))
	int32 StackCount = 0;

	/** Negative means this item currently has no durability system enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	float Durability = -1.0f;

	/** Kept even in v1 so future per-item state does not require replacing inventory serialization. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FGuid InstanceId;

	/** Set only for StellarText items so they survive quickbar transfers and saves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FName StellarItemId;

	/** Set for an assembled StellarWeapon; StellarItemId identifies its attachment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FName StellarCoreId;

	/** Identity of the second physical item in a legacy combined weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FGuid StellarAttachmentInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FText CustomDisplayName;

	/** Stellar core level (1..61). Meaningful for a core or an assembled weapon; the budget is level - 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "1", ClampMax = "61"))
	int32 StellarCoreLevel = 1;

	/** Six recorded attachment skill points (0..10 each). Empty means all zero. Never rewritten by scaling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TArray<uint8> StellarPoints;

	/** Normal-weapon body level (1..10). Each level grants four skill points and +12% base damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "1", ClampMax = "10"))
	int32 WeaponBodyLevel = 1;

	/** Normal-weapon skill points, six entries of 0..10 (empty = all zero). Spent from the body-level budget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TArray<uint8> WeaponPoints;

	bool IsEmpty() const
	{
		return ItemId == EJTSItemId::None || StackCount <= 0
			|| (ItemId == EJTSItemId::StellarText && StellarItemId.IsNone())
			|| (ItemId == EJTSItemId::StellarWeapon && (StellarCoreId.IsNone() || StellarItemId.IsNone()));
	}
	void Clear()
	{
		ItemId = EJTSItemId::None;
		StackCount = 0;
		Durability = -1.0f;
		InstanceId.Invalidate();
		StellarItemId = NAME_None;
		StellarCoreId = NAME_None;
		StellarAttachmentInstanceId.Invalidate();
		CustomDisplayName = FText::GetEmpty();
		StellarCoreLevel = 1;
		StellarPoints.Reset();
		WeaponBodyLevel = 1;
		WeaponPoints.Reset();
	}
};

/** A resource cost in the shared spacecraft / expedition wallet. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSItemCost
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cost")
	EJTSResourceType ResourceType = EJTSResourceType::Rock;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cost", meta = (ClampMin = "0"))
	int32 Amount = 0;
};
