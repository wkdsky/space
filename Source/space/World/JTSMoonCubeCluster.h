#pragma once

#include "CoreMinimal.h"
#include "space/World/JTSPlanetEnemySettlement.h"
#include "JTSMoonCubeCluster.generated.h"

class ADirectionalLight;
class AJTSPlanetAnchor;
class UMaterialInterface;
class UStaticMesh;

/** Moon-specific daylight rule and art recipe for the reusable planet settlement spawner. */
UCLASS(Blueprintable)
class SPACE_API AJTSMoonCubeCluster : public AJTSPlanetEnemySettlement
{
	GENERATED_BODY()

public:
	AJTSMoonCubeCluster();

protected:
	virtual bool AcceptSpawnPoint_Implementation(FVector GroundPoint, AJTSPlanetAnchor* Planet) const override;
	virtual void ConfigureSpawnedEnemy_Implementation(AActor* Enemy) override;

private:
	UPROPERTY(EditInstanceOnly, Category = "Moon|Cube Cluster")
	TObjectPtr<ADirectionalLight> SunLight;
	UPROPERTY(EditAnywhere, Category = "Moon|Cube Cluster|Visual")
	TObjectPtr<UStaticMesh> BodyMeshAsset;
	UPROPERTY(EditAnywhere, Category = "Moon|Cube Cluster|Visual")
	TObjectPtr<UStaticMesh> WeakPointMeshAsset;
	UPROPERTY(EditAnywhere, Category = "Moon|Cube Cluster|Visual")
	TObjectPtr<UMaterialInterface> BodyMaterial;
	UPROPERTY(EditAnywhere, Category = "Moon|Cube Cluster|Visual")
	TObjectPtr<UMaterialInterface> WeakPointMaterial;
	UPROPERTY(EditAnywhere, Category = "Moon|Cube Cluster", meta = (ClampMin = "0", ClampMax = "1"))
	float MinimumSunDot = 0.20f;
};
