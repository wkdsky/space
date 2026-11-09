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
	/** Legacy processed organic material used by equipment recipes. */
	Organic = 5 UMETA(DisplayName = "Organic"),
	/** Generic biological feedstock; retain the serialized enum name/value for existing saves and assets. */
	MoonAntCorpse = 6 UMETA(DisplayName = "Biological Corpse"),
	Crystal = 7 UMETA(DisplayName = "Crystal"),
	InorganicCarbon = 8 UMETA(DisplayName = "Inorganic Carbon")
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
