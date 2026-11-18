#pragma once
#include "CoreMinimal.h"
#include "space/World/JTSPassivePlanetSurfaceController.h"
#include "Engine/DataAsset.h"
#include "JTSPlanetSurfaceController.generated.h"
class AJTSPlanetEnemySettlement;
class AJTSMoonResourceSpawner;
UCLASS(BlueprintType)
class SPACE_API UJTSPlanetSurfaceGameplayData : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FName PlanetId;
};
/** Activates level-owned content. Configuration and actor selection stay in the level. */
UCLASS(Blueprintable)
class SPACE_API AJTSPlanetSurfaceController : public AJTSPassivePlanetSurfaceController
{
    GENERATED_BODY()
public:
    virtual bool InitializeSurfaceGameplay(const FJTSSurfaceGameplayContext& Context) override;
    virtual void RegisterSurfaceRuntimeActor(AActor* Actor) override;
    virtual void ShutdownSurfaceGameplay() override;
private:
    TArray<TWeakObjectPtr<AActor>> RuntimeActors;
    TArray<TWeakObjectPtr<AJTSPlanetEnemySettlement>> Settlements;
};
