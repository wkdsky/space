#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JTSPlanetEnemySettlement.generated.h"

class AJTSPlanetAnchor;
class USphereComponent;

/** Level-authored settlement recipe. The active surface controller supplies its Planet at runtime. */
UCLASS(Blueprintable)
class SPACE_API AJTSPlanetEnemySettlement : public AActor
{
	GENERATED_BODY()

public:
	AJTSPlanetEnemySettlement();

	UFUNCTION(BlueprintCallable, Category = "Planet|Enemy Settlement")
	void Activate(AJTSPlanetAnchor* Planet, AActor* SurfaceController);
	UFUNCTION(BlueprintCallable, Category = "Planet|Enemy Settlement")
	void Deactivate();
	UFUNCTION(BlueprintPure, Category = "Planet|Enemy Settlement")
	int32 GetSpawnedEnemyCount() const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Override in a planet-specific Blueprint for daylight, biome or POI filters. */
	UFUNCTION(BlueprintNativeEvent, Category = "Planet|Enemy Settlement")
	bool AcceptSpawnPoint(FVector GroundPoint, AJTSPlanetAnchor* Planet) const;
	virtual bool AcceptSpawnPoint_Implementation(FVector GroundPoint, AJTSPlanetAnchor* Planet) const;

	/** Optional per-species presentation setup, after the Actor has been spawned. */
	UFUNCTION(BlueprintNativeEvent, Category = "Planet|Enemy Settlement")
	void ConfigureSpawnedEnemy(AActor* Enemy);
	virtual void ConfigureSpawnedEnemy_Implementation(AActor* Enemy);

	UPROPERTY(VisibleAnywhere, Category = "Planet|Enemy Settlement")
	TObjectPtr<USphereComponent> EditorRadius;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Enemy Settlement")
	TSubclassOf<AActor> EnemyClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Enemy Settlement", meta = (ClampMin = "0", ClampMax = "512"))
	int32 EnemyCount = 20;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Enemy Settlement", meta = (ClampMin = "0"))
	float SpawnRadius = 670.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Enemy Settlement", meta = (ClampMin = "0"))
	float MinimumSpacing = 95.0f;

	TArray<TWeakObjectPtr<AActor>> SpawnedEnemies;

private:
	bool bActive = false;
};
