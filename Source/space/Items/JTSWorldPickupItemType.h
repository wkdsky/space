#pragma once

#include "CoreMinimal.h"

#include "JTSWorldPickupItemType.generated.h"

/** The single-item payload represented by AJTSWorldPickupActor. */
UENUM(BlueprintType)
enum class EJTSWorldPickupItemType : uint8
{
	Fuel UMETA(DisplayName = "Fuel"),
	Water UMETA(DisplayName = "Water"),
	Food UMETA(DisplayName = "Food"),
	Rock UMETA(DisplayName = "Rock"),
	Ore UMETA(DisplayName = "Ore"),
	MoonAntCorpse UMETA(DisplayName = "MoonAnt Corpse"),
	Pickaxe UMETA(DisplayName = "Pickaxe"),
	Backpack UMETA(Hidden),
	Knife UMETA(DisplayName = "Knife"),
	Pistol UMETA(DisplayName = "Pistol"),
	MachineGun UMETA(DisplayName = "Machine Gun"),
	Axe UMETA(DisplayName = "Axe")
};
