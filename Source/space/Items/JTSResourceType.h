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
	Ore = 4 UMETA(DisplayName = "Ore")
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
