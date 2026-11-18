#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JTSSurfaceWaypoint.generated.h"
class AJTSPlanetAnchor;
UCLASS(Blueprintable)
class SPACE_API AJTSSurfaceWaypoint : public AActor
{
    GENERATED_BODY()
public:
    AJTSSurfaceWaypoint();
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite) TObjectPtr<AJTSPlanetAnchor> Planet;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName CoordinateId;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FText DisplayName;
    UFUNCTION(BlueprintPure) bool IsDiscovered() const;
};
