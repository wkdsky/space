#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "space/Components/JTSPlanetSurfaceScatterComponent.h"
#include "space/World/JTSMoonResourceActor.h"
#include "JTSPlanetDistributionData.generated.h"

class AJTSMoonResourceSpawner;

USTRUCT(BlueprintType)
struct FJTSFrozenSurfaceProp
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FTransform Pose;
	UPROPERTY(EditAnywhere) FJTSPlanetSurfaceScatterEntry Recipe;
	UPROPERTY(EditAnywhere) int32 Variant = 0;
	UPROPERTY(EditAnywhere) int32 Population = -1;
	UPROPERTY(EditAnywhere) FName ResidentKey;
};

USTRUCT(BlueprintType)
struct FJTSFrozenScatterLayout
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName Key;
	UPROPERTY(EditAnywhere) FTransform OwnerPose;
	UPROPERTY(EditAnywhere) TArray<FJTSFrozenSurfaceProp> Props;
};

USTRUCT(BlueprintType)
struct FJTSFrozenResource
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FTransform Pose;
	UPROPERTY(EditAnywhere) bool bPickup = false;
	UPROPERTY(EditAnywhere) EJTSResourceType ResourceType = EJTSResourceType::Rock;
	UPROPERTY(EditAnywhere) EJTSMoonResourceNodeSize Size = EJTSMoonResourceNodeSize::MediumRock;
	UPROPERTY(EditAnywhere) int32 Yield = 2;
	UPROPERTY(EditAnywhere) int32 MetalYield = 0;
	UPROPERTY(EditAnywhere) int32 Variant = 0;
};

USTRUCT(BlueprintType)
struct FJTSFrozenResourceLayout
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName Key;
	UPROPERTY(EditAnywhere) TArray<FJTSFrozenResource> Resources;
};

USTRUCT(BlueprintType)
struct FJTSFrozenActorPose
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName ActorName;
	UPROPERTY(EditAnywhere) FTransform Pose;
};

/** Authored placement manifest. Runtime consumes it without choosing new resource/refuge locations. */
UCLASS(BlueprintType)
class SPACE_API UJTSPlanetDistributionData : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere) TArray<FJTSFrozenActorPose> ActorPoses;
	UPROPERTY(EditAnywhere) TArray<FJTSFrozenScatterLayout> ScatterLayouts;
	UPROPERTY(EditAnywhere) TArray<FJTSFrozenResourceLayout> ResourceLayouts;
	const FJTSFrozenScatterLayout* FindScatter(FName Key) const;
	const FJTSFrozenResourceLayout* FindResources(FName Key) const;
	void ApplyActorPoses(UWorld* World) const;
	bool IsCapturing() const { return bCapturing; }
	void RecordScatter(FName Key, const FTransform& Pose, const TArray<FJTSFrozenSurfaceProp>& Props);
	void RecordResource(FName Key, const FJTSFrozenResource& Resource);
	/** Explicit editor operation only. Never bakes or edits the manifest during play. */
	UFUNCTION(BlueprintCallable, Category="Planet|Distribution")
	bool BakeDistribution(AJTSPlanetAnchor* Planet, const TArray<AJTSPlanetEcologyHabitat*>& Habitats,
		const TArray<AJTSMoonResourceSpawner*>& Spawners);
private:
	bool bCapturing = false;
};
