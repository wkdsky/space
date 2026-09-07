#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "JTSMoonCorpseActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UJTSMoonWrappedActorComponent;

/**
 * Runtime-only Moon event landmark: a visibly human, clothed skeleton assembled from prototype primitives.
 * It intentionally has no interaction behavior; it is the center of the nearby roach-nest distribution.
 */
UCLASS()
class SPACE_API AJTSMoonCorpseActor : public AActor
{
	GENERATED_BODY()

public:
	AJTSMoonCorpseActor();

	/** Aligns the complete lying figure to a validated Moon ground hit. */
	void AdjustToGround(const FVector& GroundLocation);

protected:
	virtual void BeginPlay() override;

private:
	void ApplyPrototypeMaterials();
	void UpdateMoonWrappedLogicalPosition();

	UPROPERTY(VisibleAnywhere, Category = "Moon|Roach Event")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Moon|Roach Event")
	TObjectPtr<UStaticMeshComponent> SkullMesh;

	UPROPERTY(VisibleAnywhere, Category = "Moon|Roach Event")
	TObjectPtr<UStaticMeshComponent> TorsoClothingMesh;

	UPROPERTY(VisibleAnywhere, Category = "Moon|Roach Event")
	TObjectPtr<UStaticMeshComponent> HipClothingMesh;

	UPROPERTY(VisibleAnywhere, Category = "Moon|Roach Event")
	TObjectPtr<UStaticMeshComponent> LeftArmBoneMesh;

	UPROPERTY(VisibleAnywhere, Category = "Moon|Roach Event")
	TObjectPtr<UStaticMeshComponent> RightArmBoneMesh;

	UPROPERTY(VisibleAnywhere, Category = "Moon|Roach Event")
	TObjectPtr<UStaticMeshComponent> LeftLegBoneMesh;

	UPROPERTY(VisibleAnywhere, Category = "Moon|Roach Event")
	TObjectPtr<UStaticMeshComponent> RightLegBoneMesh;

	/** Keeps the event landmark at the nearest periodic physical image in Moon Wrap worlds. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Wrapping", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSMoonWrappedActorComponent> MoonWrappedActorComponent;
};
