#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "JTSMoonCorpseActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class AJTSPlanetSurfaceAnchor;

/**
 * Runtime-only Moon event landmark: a visibly human, clothed skeleton assembled from prototype primitives.
 * It intentionally has no interaction behavior; it is the center of the nearby MoonAnt Nest distribution.
 */
UCLASS()
class SPACE_API AJTSMoonCorpseActor : public AActor
{
	GENERATED_BODY()

public:
	AJTSMoonCorpseActor();

	/** Aligns the complete lying figure to a validated Moon ground hit. */
	void AdjustToGround(const FVector& GroundLocation);

	/** Places this landmark once on an authored real-planet surface anchor. */
	bool SnapToPlanetSurfaceAnchor(AJTSPlanetSurfaceAnchor* SurfaceAnchor);

	UFUNCTION(BlueprintPure, Category = "Moon|Real Surface")
	bool IsUsingRealPlanetSurfacePlacement() const;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_RealSurfacePlacement();

	void ApplyPrototypeMaterials();
	void ApplySurfacePresentation();

	UPROPERTY(VisibleAnywhere, Category = "Moon|MoonAnt Event")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Moon|MoonAnt Event")
	TObjectPtr<UStaticMeshComponent> SkullMesh;

	UPROPERTY(VisibleAnywhere, Category = "Moon|MoonAnt Event")
	TObjectPtr<UStaticMeshComponent> TorsoClothingMesh;

	UPROPERTY(VisibleAnywhere, Category = "Moon|MoonAnt Event")
	TObjectPtr<UStaticMeshComponent> HipClothingMesh;

	UPROPERTY(VisibleAnywhere, Category = "Moon|MoonAnt Event")
	TObjectPtr<UStaticMeshComponent> LeftArmBoneMesh;

	UPROPERTY(VisibleAnywhere, Category = "Moon|MoonAnt Event")
	TObjectPtr<UStaticMeshComponent> RightArmBoneMesh;

	UPROPERTY(VisibleAnywhere, Category = "Moon|MoonAnt Event")
	TObjectPtr<UStaticMeshComponent> LeftLegBoneMesh;

	UPROPERTY(VisibleAnywhere, Category = "Moon|MoonAnt Event")
	TObjectPtr<UStaticMeshComponent> RightLegBoneMesh;

	/** The material used by the gameplay-planet placement path. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BasicPrototypeMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Real Surface", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float PlanetSurfaceClearance = 2.0f;

	UPROPERTY(ReplicatedUsing = OnRep_RealSurfacePlacement)
	bool bUsesRealPlanetSurfacePlacement = false;
};
