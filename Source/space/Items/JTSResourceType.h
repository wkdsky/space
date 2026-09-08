#pragma once

#include "CoreMinimal.h"

#include "JTSResourceType.generated.h"

UENUM(BlueprintType)
enum class EJTSResourceType : uint8
{
	Fuel = 0 UMETA(DisplayName = "Fuel"),
	Water = 1 UMETA(DisplayName = "Water"),
	Food = 2 UMETA(DisplayName = "Food"),
	Rock = 3 UMETA(DisplayName = "Rock"),
	Ore = 4 UMETA(DisplayName = "Ore"),
	/** A processed ship resource produced by submitting an Ant Corpse. */
	Organic = 5 UMETA(DisplayName = "Organic"),
	/** A carried Moon item. It occupies one inventory slot and is converted to Organic only by the spacecraft. */
	AntCorpse = 6 UMETA(DisplayName = "Ant Corpse")
};

USTRUCT(BlueprintType)
struct SPACE_API FJTSResourceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	EJTSResourceType ResourceType = EJTSResourceType::Rock;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource", meta = (ClampMin = "1", UIMin = "1"))
	int32 StackSize = 1;
};
