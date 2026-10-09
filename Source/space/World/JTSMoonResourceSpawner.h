#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Items/JTSResourceType.h"
#include "space/World/JTSMoonResourceActor.h"
#include "space/World/JTSMoonResourceDistribution.h"

#include "JTSMoonResourceSpawner.generated.h"

class USceneComponent;
class ULineBatchComponent;
class FJTSMoonResourceCoverage;
class UJTSMoonSurfaceGameplayData;
class AJTSMoonCorpseActor;
class AJTSMoonSurfaceController;
class AJTSPlanetAnchor;
class AJTSMoonAntNestActor;
class AJTSSpacecraftActor;
class AJTSWorldPickupActor;

/** Shared resource content defaults. Legacy count/weights are retained only for asset migration. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSMoonResourceSpawnSettings
{
	GENERATED_BODY()

	UPROPERTY()
	int32 TotalResourceCount = 75;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpawnRadius = 10000.0f;

	/** Minimum centre-to-centre separation along the Moon surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinimumResourceSpacing = 200.0f;

	UPROPERTY()
	int32 SmallRockWeight = 55;

	UPROPERTY()
	int32 MediumRockWeight = 20;

	UPROPERTY()
	int32 LargeRockWeight = 10;

	UPROPERTY()
	int32 OreWeight = 15;

	UPROPERTY()
	int32 MediumMetalRockWeight = 0;

	/** Secondary yield for medium metal-bearing rocks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (ClampMin = "1", UIMin = "1"))
	int32 MediumMetalYieldUnits = 2;

	/** Extra surface-distance clearance added around the spacecraft's physical mesh bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpacecraftExclusionPadding = 500.0f;

	/** Extra surface-distance clearance added around fixed corpse and random MoonAnt Nest landmark bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LandmarkExclusionPadding = 400.0f;

	/** Real SpaceWorld Moon generation must be reproducible from its configured content data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources|Random")
	bool bUseDeterministicSeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources|Random", meta = (EditCondition = "bUseDeterministicSeed"))
	int32 RandomSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (DisplayName = "Resource Kinds"))
	FJTSMoonResourceKinds ResourceKinds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (ClampMin = "1"))
	int32 MediumCrystalYieldUnits = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (ClampMin = "1"))
	int32 LargeCrystalYieldUnits = 6;
};

UCLASS()
class SPACE_API AJTSMoonResourceSpawner : public AActor
{
	GENERATED_BODY()

public:
	AJTSMoonResourceSpawner();

	/** Refreshes content availability and the editor-only surface ring without replacing instance abundance. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Moon|Resources")
	void RefreshResourceConfiguration();

	UFUNCTION(BlueprintPure, Category = "Moon|Resources")
	int32 GetTargetResourceCount() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Resources")
	float GetCoverageAreaSquareMeters() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Resources|Coverage")
	int32 GetCoveragePreviewSegmentCount() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moon|Resources|Content")
	TObjectPtr<UJTSMoonSurfaceGameplayData> ResourceGameplayData;

	/** Number of resource objects per 100 square metres of spherical coverage. Zero disables spawning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moon|Resources|Distribution", meta = (DisplayName = "Resource Density (per 100 m2)", ClampMin = "0", UIMin = "0"))
	float ResourceDensity = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moon|Resources|Distribution")
	FJTSMoonResourceDistribution ResourceRatios;

	/** Compare priority whenever ratios or density differ; lower numbers cover higher numbers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moon|Resources|Distribution")
	int32 OverlapPriority = 0;

	UPROPERTY(EditInstanceOnly, Category = "Moon|Resources|Coverage")
	bool bOverrideCoverageRadius = false;

	/** Surface arc radius. The PDA supplies the base value unless this instance opts into an override. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moon|Resources|Coverage", meta = (ClampMin = "0", UIMin = "0", EditCondition = "bOverrideCoverageRadius"))
	float Radius = 2000;

	/** Level-authored planet used to project the editor coverage circle onto real mesh collision. */
	UPROPERTY(EditInstanceOnly, Category = "Moon|Resources|Coverage")
	TObjectPtr<AJTSPlanetAnchor> CoveragePlanet;


	UFUNCTION(BlueprintCallable, Category = "Moon|Resources")
	int32 GenerateResources();

	/** Regenerates a planet's configured regions together, clearing old winners and sharing spacing. */
	static int32 GenerateResourceGroup(const TArray<AJTSMoonResourceSpawner*>& Spawners);

	/** Applies Moon surface runtime balance values before this spawner generates its resources. */
	UFUNCTION(BlueprintCallable, Category = "Moon|Resources")
	void ApplyMoonSpawnSettings(const FJTSMoonResourceSpawnSettings& Settings);

	/** Caches the Moon initialization landmarks used to exclude procedural resource locations. */
	void SetLandmarkExclusions(
		AJTSSpacecraftActor* InSpacecraft,
		const TArray<TWeakObjectPtr<AJTSMoonCorpseActor>>& InCorpseLandmarks,
		const TArray<TWeakObjectPtr<AJTSMoonAntNestActor>>& InMoonAntNestLandmarks);

	/** Enables real spherical surface sampling when this spawner belongs to a PlanetAnchor. */
	void SetOwningPlanet(AJTSPlanetAnchor* InOwningPlanet);

	/** Planet this spawner was bound to. Empty until surface gameplay initializes it. */
	AJTSPlanetAnchor* GetOwningPlanet() const;

	/** Registers generated resources and pickups with the active real-surface controller. */
	void SetSurfaceGameplayController(AJTSMoonSurfaceController* InSurfaceGameplayController);

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
	virtual void PostEditUndo() override;
#endif

private:
	int32 GenerateResourcesInCoverage(FJTSMoonResourceCoverage& Coverage);
	void ClearGeneratedResources();
	AJTSMoonResourceActor* SpawnMiningNode(
		EJTSResourceType ResourceType,
		int32 TotalYieldUnits,
		EJTSMoonResourceNodeSize NodeSize,
		int32 MetalYieldUnits,
		int32 VisualVariantIndex,
		const FVector& ResourceScale,
		const FRotator& ResourceRotation,
		const FVector& GroundLocation);
	AJTSWorldPickupActor* SpawnInitialPickup(const FVector& GroundLocation);
	bool ResolveGroundLocation(const FVector& CandidateLocation, FVector& OutGroundLocation) const;
	bool IsCandidateExcludedByLandmarks(const FVector& CandidateLocation) const;
	bool IsUsingRealPlanetSurface() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moon|Resources", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSMoonResourceActor> ResourceActorClass;

	UPROPERTY(VisibleAnywhere, Category = "Moon|Resources|Coverage")
	TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Moon|Resources|Coverage")
	TObjectPtr<ULineBatchComponent> CoverageOutline;
#endif

	UPROPERTY(VisibleInstanceOnly, Category = "Moon|Resources|Distribution")
	int32 ResourceCount = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Moon|Resources")
	float MinimumResourceSpacing = 200;

	void UpdateCoveragePreview();

	UPROPERTY(VisibleInstanceOnly, Category = "Moon|Resources")
	int32 MediumMetalYieldUnits = 2;

	int32 MediumCrystalYieldUnits = 2;
	int32 LargeCrystalYieldUnits = 6;

	/** Runtime copy of the Moon surface settings' spacecraft exclusion setting. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Moon|Resources", meta = (AllowPrivateAccess = "true"))
	float SpacecraftExclusionPadding = 0.0f;

	/** Runtime copy of the Moon surface settings' corpse/nest landmark exclusion setting. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Moon|Resources", meta = (AllowPrivateAccess = "true"))
	float LandmarkExclusionPadding = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources|Ground", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float GroundTraceStartHeight = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources|Ground", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float GroundTraceDistance = 3000.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Moon|Resources|Random", meta = (AllowPrivateAccess = "true"))
	bool bUseRandomSeed = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Moon|Resources|Random", meta = (AllowPrivateAccess = "true"))
	int32 RandomSeed = 1337;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AJTSMoonResourceActor>> GeneratedResources;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AJTSWorldPickupActor>> GeneratedPickups;

	/** Initialization-time landmark cache supplied by AJTSMoonSurfaceController before GenerateResources. */
	TWeakObjectPtr<AJTSSpacecraftActor> SpacecraftLandmark;
	TWeakObjectPtr<AJTSPlanetAnchor> OwningPlanet;
	TWeakObjectPtr<AJTSMoonSurfaceController> SurfaceGameplayController;
	TArray<TWeakObjectPtr<AJTSMoonCorpseActor>> CorpseLandmarks;
	TArray<TWeakObjectPtr<AJTSMoonAntNestActor>> MoonAntNestLandmarks;
};
