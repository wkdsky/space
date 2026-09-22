// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Items/JTSItemDefinitionLibrary.h"

#include "UObject/UObjectGlobals.h"
#include "space/Items/JTSItemDefinition.h"

namespace
{
	TMap<EJTSItemId, TObjectPtr<UJTSItemDefinition>> GFallbackDefinitions;
	TMap<EJTSItemId, TObjectPtr<UJTSItemDefinition>> GLoadedDefinitions;

	int32 CapabilityMask(std::initializer_list<EJTSItemCapability> Capabilities)
	{
		int32 Result = 0;
		for (const EJTSItemCapability Capability : Capabilities)
		{
			Result |= static_cast<int32>(Capability);
		}
		return Result;
	}

	FJTSItemCost Cost(EJTSResourceType Type, int32 Amount)
	{
		FJTSItemCost Result;
		Result.ResourceType = Type;
		Result.Amount = Amount;
		return Result;
	}

	FString AssetPathFor(EJTSItemId ItemId)
	{
		switch (ItemId)
		{
		case EJTSItemId::Pickaxe: return TEXT("/Game/Space/Data/Items/DA_Item_Pickaxe.DA_Item_Pickaxe");
		case EJTSItemId::Knife: return TEXT("/Game/Space/Data/Items/DA_Item_Knife.DA_Item_Knife");
		case EJTSItemId::Pistol: return TEXT("/Game/Space/Data/Items/DA_Item_Pistol.DA_Item_Pistol");
		case EJTSItemId::MachineGun: return TEXT("/Game/Space/Data/Items/DA_Item_MachineGun.DA_Item_MachineGun");
		case EJTSItemId::Sniper: return TEXT("/Game/Space/Data/Items/DA_Item_Sniper.DA_Item_Sniper");
		case EJTSItemId::Backpack: return TEXT("/Game/Space/Data/Items/DA_Item_Backpack.DA_Item_Backpack");
		case EJTSItemId::Rock: return TEXT("/Game/Space/Data/Items/DA_Item_Rock.DA_Item_Rock");
		case EJTSItemId::Ore: return TEXT("/Game/Space/Data/Items/DA_Item_Ore.DA_Item_Ore");
		case EJTSItemId::Fuel: return TEXT("/Game/Space/Data/Items/DA_Item_Fuel.DA_Item_Fuel");
		case EJTSItemId::Water: return TEXT("/Game/Space/Data/Items/DA_Item_Water.DA_Item_Water");
		case EJTSItemId::Food: return TEXT("/Game/Space/Data/Items/DA_Item_Food.DA_Item_Food");
		case EJTSItemId::MoonAntCorpse: return TEXT("/Game/Space/Data/Items/DA_Item_MoonAntCorpse.DA_Item_MoonAntCorpse");
		case EJTSItemId::Axe: return TEXT("/Game/Space/Data/Items/DA_Item_Axe.DA_Item_Axe");
		default: return FString();
		}
	}

	UJTSItemDefinition* MakeFallback(EJTSItemId ItemId)
	{
		if (const TObjectPtr<UJTSItemDefinition>* Existing = GFallbackDefinitions.Find(ItemId))
		{
			return Existing->Get();
		}

		UJTSItemDefinition* Definition = NewObject<UJTSItemDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Definition->AddToRoot();
		Definition->ItemId = ItemId;
		Definition->DisplayName = FText::FromString(TEXT("Unknown Item"));
		Definition->Description = FText::FromString(TEXT("Prototype item definition."));
		Definition->PrimaryCategory = EJTSItemCategory::Utility;
		Definition->MaxStackSize = 1;
		Definition->CombatDamage = 1.0f;
		Definition->MeleeAttackInterval = 0.45f;
		Definition->RangedFireInterval = 0.35f;
		Definition->RangedRange = 8000.0f;
		Definition->RangedAimFOV = 60.0f;
		Definition->AccentColor = FLinearColor(0.38f, 0.70f, 0.95f, 1.0f);

		auto SetResource = [Definition](const TCHAR* Name, EJTSItemId Id, EJTSItemCategory Category, const FLinearColor& Color)
		{
			Definition->ItemId = Id;
			Definition->DisplayName = FText::FromString(Name);
			Definition->Description = FText::FromString(TEXT("Single-unit expedition material. It can be held as an improvised melee item."));
			Definition->PrimaryCategory = Category;
			Definition->ShopCategories = { EJTSShopCategory::Resources };
			Definition->AffinityTags = { TEXT("Material"), TEXT("Improvised") };
			Definition->CapabilityMask = CapabilityMask({ EJTSItemCapability::Holdable });
			Definition->MaxStackSize = 1;
			Definition->CombatDamage = 1.0f;
			Definition->AccentColor = Color;
		};

		switch (ItemId)
		{
		case EJTSItemId::Rock:
			SetResource(TEXT("Rock"), ItemId, EJTSItemCategory::Resources, FLinearColor(0.48f, 0.50f, 0.56f, 1.0f));
			break;
		case EJTSItemId::Ore:
			SetResource(TEXT("Ore"), ItemId, EJTSItemCategory::Resources, FLinearColor(0.10f, 0.72f, 0.95f, 1.0f));
			Definition->AffinityTags = { TEXT("Material"), TEXT("Conductive"), TEXT("Improvised") };
			break;
		case EJTSItemId::Fuel:
			SetResource(TEXT("Fuel"), ItemId, EJTSItemCategory::Resources, FLinearColor(1.0f, 0.44f, 0.12f, 1.0f));
			break;
		case EJTSItemId::Water:
			SetResource(TEXT("Water"), ItemId, EJTSItemCategory::Resources, FLinearColor(0.14f, 0.55f, 1.0f, 1.0f));
			break;
		case EJTSItemId::Food:
			SetResource(TEXT("Food"), ItemId, EJTSItemCategory::Resources, FLinearColor(0.34f, 0.90f, 0.38f, 1.0f));
			break;
		case EJTSItemId::MoonAntCorpse:
			SetResource(TEXT("Moon Ant Corpse"), ItemId, EJTSItemCategory::Resources, FLinearColor(0.70f, 0.30f, 0.72f, 1.0f));
			Definition->MaxStackSize = 1;
			break;
		case EJTSItemId::Pickaxe:
			Definition->DisplayName = FText::FromString(TEXT("Pickaxe"));
			Definition->Description = FText::FromString(TEXT("A dependable lunar tool. Strong mining work, steady melee impact."));
			Definition->PrimaryCategory = EJTSItemCategory::Mining;
			Definition->ShopCategories = { EJTSShopCategory::Mining, EJTSShopCategory::Utility };
			Definition->AffinityTags = { TEXT("Mining"), TEXT("Melee"), TEXT("Utility") };
			Definition->CapabilityMask = CapabilityMask({ EJTSItemCapability::Holdable, EJTSItemCapability::Mining, EJTSItemCapability::MeleeOverride, EJTSItemCapability::ShopPurchasable });
			Definition->CombatDamage = 2.0f;
			Definition->MiningWork = 4.0f;
			Definition->MeleeAttackInterval = 0.62f;
			Definition->ShopCosts = { Cost(EJTSResourceType::Rock, 4) };
			Definition->AccentColor = FLinearColor(1.0f, 0.70f, 0.18f, 1.0f);
			break;
		case EJTSItemId::Knife:
			Definition->DisplayName = FText::FromString(TEXT("Field Knife"));
			Definition->Description = FText::FromString(TEXT("A fast close-quarters blade. It can chip rock, but it is not a mining tool."));
			Definition->PrimaryCategory = EJTSItemCategory::Weapons;
			Definition->ShopCategories = { EJTSShopCategory::Weapons, EJTSShopCategory::Utility };
			Definition->AffinityTags = { TEXT("Weapon"), TEXT("Melee"), TEXT("Lightweight") };
			Definition->CapabilityMask = CapabilityMask({ EJTSItemCapability::Holdable, EJTSItemCapability::Mining, EJTSItemCapability::MeleeOverride, EJTSItemCapability::ShopPurchasable });
			Definition->CombatDamage = 3.0f;
			Definition->MiningWork = 0.75f;
			Definition->MeleeAttackInterval = 0.28f;
			Definition->ShopCosts = { Cost(EJTSResourceType::Rock, 3), Cost(EJTSResourceType::Ore, 1) };
			Definition->AccentColor = FLinearColor(0.82f, 0.89f, 1.0f, 1.0f);
			break;
		case EJTSItemId::Pistol:
			Definition->DisplayName = FText::FromString(TEXT("Service Pistol"));
			Definition->Description = FText::FromString(TEXT("Accurate sidearm prototype. Infinite test-cell ammunition; low mining work per shot."));
			Definition->PrimaryCategory = EJTSItemCategory::Weapons;
			Definition->ShopCategories = { EJTSShopCategory::Weapons };
			Definition->AffinityTags = { TEXT("Weapon"), TEXT("Ranged"), TEXT("Sidearm") };
			Definition->CapabilityMask = CapabilityMask({ EJTSItemCapability::Holdable, EJTSItemCapability::Mining, EJTSItemCapability::RangedWeapon, EJTSItemCapability::ShopPurchasable });
			Definition->CombatDamage = 1.0f;
			Definition->MiningWork = 0.35f;
			Definition->RangedDamage = 2.5f;
			Definition->RangedFireInterval = 0.42f;
			Definition->RangedRange = 9000.0f;
			Definition->RangedAimFOV = 68.0f;
			Definition->ShopCosts = { Cost(EJTSResourceType::Rock, 2), Cost(EJTSResourceType::Ore, 4) };
			Definition->AccentColor = FLinearColor(0.42f, 0.72f, 1.0f, 1.0f);
			break;
		case EJTSItemId::MachineGun:
			Definition->DisplayName = FText::FromString(TEXT("Machine Gun"));
			Definition->Description = FText::FromString(TEXT("Sustained-fire prototype. Infinite test-cell ammunition; weak mining work accumulates through rate of fire."));
			Definition->PrimaryCategory = EJTSItemCategory::Weapons;
			Definition->ShopCategories = { EJTSShopCategory::Weapons };
			Definition->AffinityTags = { TEXT("Weapon"), TEXT("Ranged"), TEXT("Sustained Fire") };
			Definition->CapabilityMask = CapabilityMask({ EJTSItemCapability::Holdable, EJTSItemCapability::Mining, EJTSItemCapability::RangedWeapon, EJTSItemCapability::ShopPurchasable });
			Definition->CombatDamage = 1.0f;
			Definition->MiningWork = 0.25f;
			Definition->RangedDamage = 0.85f;
			Definition->RangedFireInterval = 0.12f;
			Definition->RangedRange = 8500.0f;
			Definition->RangedAimFOV = 62.0f;
			Definition->bAutomaticFire = true;
			Definition->ShopCosts = { Cost(EJTSResourceType::Rock, 6), Cost(EJTSResourceType::Ore, 10) };
			Definition->AccentColor = FLinearColor(1.0f, 0.34f, 0.18f, 1.0f);
			break;
		case EJTSItemId::Sniper:
			Definition->DisplayName = FText::FromString(TEXT("Sniper Rifle"));
			Definition->Description = FText::FromString(TEXT("A deliberate long-range rifle. Slow follow-up shots, high precision damage and the strongest aim zoom."));
			Definition->PrimaryCategory = EJTSItemCategory::Weapons;
			Definition->ShopCategories = { EJTSShopCategory::Weapons };
			Definition->AffinityTags = { TEXT("Weapon"), TEXT("Ranged"), TEXT("Precision") };
			Definition->CapabilityMask = CapabilityMask({ EJTSItemCapability::Holdable, EJTSItemCapability::Mining, EJTSItemCapability::RangedWeapon, EJTSItemCapability::ShopPurchasable });
			Definition->CombatDamage = 1.0f;
			Definition->MiningWork = 1.0f;
			Definition->RangedDamage = 8.0f;
			Definition->RangedFireInterval = 1.15f;
			Definition->RangedRange = 16000.0f;
			Definition->RangedAimFOV = 36.0f;
			Definition->bAutomaticFire = false;
			Definition->ShopCosts = { Cost(EJTSResourceType::Rock, 10), Cost(EJTSResourceType::Ore, 18) };
			Definition->AccentColor = FLinearColor(0.72f, 0.42f, 1.0f, 1.0f);
			break;
		case EJTSItemId::Backpack:
			Definition->DisplayName = FText::FromString(TEXT("Expedition Backpack"));
			Definition->Description = FText::FromString(TEXT("Wearable expedition pack. Adds eight inventory slots; only one can occupy the backpack slot."));
			Definition->PrimaryCategory = EJTSItemCategory::Wearables;
			Definition->ShopCategories = { EJTSShopCategory::Wearables, EJTSShopCategory::Utility };
			Definition->AffinityTags = { TEXT("Wearable"), TEXT("Capacity"), TEXT("Utility") };
			Definition->CapabilityMask = CapabilityMask({ EJTSItemCapability::Wearable, EJTSItemCapability::ShopPurchasable });
			Definition->WearableSlot = EJTSWearableSlot::Backpack;
			Definition->InventoryCapacityBonus = 8;
			Definition->CombatDamage = 0.0f;
			Definition->ShopCosts = { Cost(EJTSResourceType::Rock, 5), Cost(EJTSResourceType::Ore, 2) };
			Definition->AccentColor = FLinearColor(0.22f, 0.94f, 0.55f, 1.0f);
			break;
		case EJTSItemId::Axe:
			Definition->DisplayName = FText::FromString(TEXT("Legacy Axe"));
			Definition->Description = FText::FromString(TEXT("Compatibility item from the first Moon prototype."));
			Definition->PrimaryCategory = EJTSItemCategory::Utility;
			Definition->ShopCategories = { EJTSShopCategory::Utility };
			Definition->AffinityTags = { TEXT("Melee"), TEXT("Legacy") };
			Definition->CapabilityMask = CapabilityMask({ EJTSItemCapability::Holdable, EJTSItemCapability::Mining, EJTSItemCapability::MeleeOverride });
			Definition->CombatDamage = 3.0f;
			Definition->MiningWork = 1.0f;
			Definition->AccentColor = FLinearColor(0.90f, 0.34f, 0.14f, 1.0f);
			break;
		default:
			break;
		}

		GFallbackDefinitions.Add(ItemId, Definition);
		return Definition;
	}
}

UJTSItemDefinition* UJTSItemDefinitionLibrary::GetItemDefinition(const UObject* /*WorldContextObject*/, EJTSItemId ItemId)
{
	if (ItemId == EJTSItemId::None)
	{
		return nullptr;
	}
	if (const TObjectPtr<UJTSItemDefinition>* Existing = GLoadedDefinitions.Find(ItemId))
	{
		return Existing->Get();
	}

	const FString AssetPath = AssetPathFor(ItemId);
	if (!AssetPath.IsEmpty())
	{
		if (UJTSItemDefinition* const Loaded = LoadObject<UJTSItemDefinition>(nullptr, *AssetPath);
			IsValid(Loaded) && Loaded->ItemId == ItemId)
		{
			GLoadedDefinitions.Add(ItemId, Loaded);
			return Loaded;
		}
	}

	UJTSItemDefinition* const Fallback = MakeFallback(ItemId);
	GLoadedDefinitions.Add(ItemId, Fallback);
	return Fallback;
}

FText UJTSItemDefinitionLibrary::GetItemDisplayName(EJTSItemId ItemId)
{
	if (const UJTSItemDefinition* const Definition = GetItemDefinition(nullptr, ItemId))
	{
		return Definition->DisplayName;
	}
	return FText::FromString(TEXT("Empty"));
}

bool UJTSItemDefinitionLibrary::TryGetResourceType(EJTSItemId ItemId, EJTSResourceType& OutResourceType)
{
	switch (ItemId)
	{
	case EJTSItemId::Fuel: OutResourceType = EJTSResourceType::Fuel; return true;
	case EJTSItemId::Water: OutResourceType = EJTSResourceType::Water; return true;
	case EJTSItemId::Food: OutResourceType = EJTSResourceType::Food; return true;
	case EJTSItemId::Rock: OutResourceType = EJTSResourceType::Rock; return true;
	case EJTSItemId::Ore: OutResourceType = EJTSResourceType::Ore; return true;
	case EJTSItemId::MoonAntCorpse: OutResourceType = EJTSResourceType::MoonAntCorpse; return true;
	default: return false;
	}
}

EJTSItemId UJTSItemDefinitionLibrary::GetItemIdForResource(EJTSResourceType ResourceType)
{
	switch (ResourceType)
	{
	case EJTSResourceType::Fuel: return EJTSItemId::Fuel;
	case EJTSResourceType::Water: return EJTSItemId::Water;
	case EJTSResourceType::Food: return EJTSItemId::Food;
	case EJTSResourceType::Rock: return EJTSItemId::Rock;
	case EJTSResourceType::Ore: return EJTSItemId::Ore;
	case EJTSResourceType::MoonAntCorpse: return EJTSItemId::MoonAntCorpse;
	default: return EJTSItemId::None;
	}
}

FJTSItemInstance UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId ItemId, int32 Count)
{
	FJTSItemInstance Result;
	if (ItemId == EJTSItemId::None || Count <= 0)
	{
		return Result;
	}

	Result.ItemId = ItemId;
	Result.StackCount = Count;
	if (const UJTSItemDefinition* const Definition = GetItemDefinition(nullptr, ItemId))
	{
		Result.Durability = Definition->DefaultDurability;
	}
	Result.InstanceId = FGuid::NewGuid();
	return Result;
}

const TArray<EJTSItemId>& UJTSItemDefinitionLibrary::GetDefaultShopCatalog()
{
	static const TArray<EJTSItemId> Catalog = {
		EJTSItemId::Pickaxe,
		EJTSItemId::Knife,
		EJTSItemId::Pistol,
		EJTSItemId::MachineGun,
		EJTSItemId::Sniper,
		EJTSItemId::Backpack
	};
	return Catalog;
}
