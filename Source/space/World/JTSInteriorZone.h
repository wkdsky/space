#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JTSInteriorZone.generated.h"
class UBoxComponent;
class AJTSPlanetAnchor;
struct FJTSPlanetSurfaceHit;
struct FJTSPlanetSurfaceFrame;
/** Authored pocket volume; its floor is separate geometry, never the planet's mesh. */
UCLASS(Blueprintable)
class SPACE_API AJTSInteriorZone : public AActor
{
    GENERATED_BODY()
public:
    AJTSInteriorZone();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> Bounds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ZoneId;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite) TObjectPtr<AJTSPlanetAnchor> Planet;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere) TArray<TObjectPtr<AActor>> FloorActors;
    bool Contains(const FVector& Point) const;
    static AJTSInteriorZone* FindAt(const UWorld* World,const FVector& Point);
    static bool ProjectFloor(AJTSPlanetAnchor* Planet,const FVector& Point,FJTSPlanetSurfaceHit& Out);
    static bool ProjectGameplayGround(AJTSPlanetAnchor* Planet,const FVector& Point,FJTSPlanetSurfaceHit& Out);
    static bool GroundFrame(AJTSPlanetAnchor* Planet,const FVector& Point,const FVector& Forward,FJTSPlanetSurfaceFrame& Out);
};
