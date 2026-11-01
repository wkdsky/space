#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSPlanetSurfaceScatterComponent.generated.h"
class AJTSPlanetAnchor;
class AJTSPlanetEcologyHabitat;
class UStaticMesh;

USTRUCT(BlueprintType)
struct FJTSPlanetSurfaceScatterEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) TArray<TObjectPtr<UStaticMesh>> Meshes;
	/** Optional closed Boolean volumes, paired by mesh index. Replicated with the independent attachment. */
	UPROPERTY(EditAnywhere) TArray<TObjectPtr<UStaticMesh>> CutVolumes;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0",ClampMax="128")) int32 Count = 1;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0")) float MinimumSpacing = 350;
	UPROPERTY(EditAnywhere) FVector2D ScaleRange = FVector2D(0.8, 1.2);
	UPROPERTY(EditAnywhere) bool bConformToSurface = false;
	UPROPERTY(EditAnywhere) bool bCastShadow = false;
	UPROPERTY(EditAnywhere) bool bRefuge = false;
	UPROPERTY(EditAnywhere) bool bDestructible = false;
	UPROPERTY(EditAnywhere) bool bFood = false;
	UPROPERTY(EditAnywhere) float MinimumSunDot = -1;
	UPROPERTY(EditAnywhere) float MaximumSunDot = 1;
	UPROPERTY(EditAnywhere) TSubclassOf<AJTSPlanetEcologyHabitat> ResidentHabitatClass;
};

/** Runtime placement only; assets, density and population recipes belong to Blueprint/level configuration. */
UCLASS(ClassGroup=(Planet), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSPlanetSurfaceScatterComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSPlanetSurfaceScatterComponent();
	void Generate(AJTSPlanetAnchor* Planet, AActor* Controller);
	void Clear();
	UPROPERTY(EditAnywhere, Category="Surface Attachments") TArray<FJTSPlanetSurfaceScatterEntry> Entries;
	UPROPERTY(EditInstanceOnly, Category="Surface Attachments") TArray<TObjectPtr<AActor>> ExclusionActors;
	UPROPERTY(EditAnywhere, Category="Surface Attachments") float ExclusionRadius = 700;
	/** Optional clear space around the recipe marker, e.g. between a feeding patch and its refuge. */
	UPROPERTY(EditAnywhere, Category="Surface Attachments", meta=(ClampMin="0")) float OwnerClearance = 0;
	UPROPERTY(EditInstanceOnly, Category="Surface Attachments") TObjectPtr<AJTSPlanetEcologyHabitat> PlacementRegion;
	UPROPERTY(EditInstanceOnly, Category="Surface Attachments") TArray<TObjectPtr<AActor>> RelocateWithOwner;
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	bool bGenerated = false;
	TArray<TWeakObjectPtr<AActor>> GeneratedActors;
};
