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
	Pickaxe UMETA(DisplayName = "Pickaxe"),
	Backpack UMETA(DisplayName = "Backpack"),
	Knife UMETA(DisplayName = "Knife"),
	Pistol UMETA(DisplayName = "Pistol"),
	MachineGun UMETA(DisplayName = "Machine Gun"),
	Axe UMETA(DisplayName = "Axe")
};

/** Item behavior is composed from these capability bits instead of mutually exclusive item classes. */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EJTSItemCapability : uint8
{
	None = 0 UMETA(Hidden),
	Holdable = 1 << 0 UMETA(DisplayName = "Holdable"),
	Wearable = 1 << 1 UMETA(DisplayName = "Wearable"),
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
	Wearables UMETA(DisplayName = "Wearables")
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
	Wearables UMETA(DisplayName = "Wearables")
};

/** Authoritative outcome returned to the requesting client after a shop transaction. */
UENUM(BlueprintType)
enum class EJTSShopPurchaseResult : uint8
{
	Succeeded UMETA(DisplayName = "Succeeded"),
	SucceededDropped UMETA(DisplayName = "Succeeded - Dropped"),
	InvalidItem UMETA(DisplayName = "Invalid Item"),
	InsufficientResources UMETA(DisplayName = "Insufficient Resources"),
	DeliveryFailed UMETA(DisplayName = "Delivery Failed")
};

/** A wearable's one valid body location. Weapons and tools never use this container. */
UENUM(BlueprintType)
enum class EJTSWearableSlot : uint8
{
	None UMETA(DisplayName = "None"),
	Backpack UMETA(DisplayName = "Backpack"),
	Body UMETA(DisplayName = "Suit / Body"),
	Head UMETA(DisplayName = "Helmet / Head"),
	Accessory UMETA(DisplayName = "Accessory")
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

	bool IsEmpty() const { return ItemId == EJTSItemId::None || StackCount <= 0; }
	void Clear()
	{
		ItemId = EJTSItemId::None;
		StackCount = 0;
		Durability = -1.0f;
		InstanceId.Invalidate();
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
