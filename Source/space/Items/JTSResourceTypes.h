// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "JTSResourceTypes.generated.h"

/** Types of world resources used in the Earth-stage collection slice. */
UENUM(BlueprintType)
enum class EJTSResourceType : uint8
{
	Fuel UMETA(DisplayName = "Fuel"),
	Water UMETA(DisplayName = "Water"),
	Food UMETA(DisplayName = "Food")
};
