#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "JTSCoordinateSettings.generated.h"
USTRUCT()
struct FJTSCoordinateGrant
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) TArray<FName> CoordinateIds;
};
/** Project configuration for defaults and legacy key-to-coordinate migration. */
UCLASS(Config=Game,DefaultConfig)
class SPACE_API UJTSCoordinateSettings : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(Config,EditAnywhere) TArray<FName> DefaultCoordinateIds;
    UPROPERTY(Config,EditAnywhere) TArray<FName> CoordinateGatedPlanetIds;
    UPROPERTY(Config,EditAnywhere) TMap<FName,FJTSCoordinateGrant> KeyCoordinateGrants;
};
