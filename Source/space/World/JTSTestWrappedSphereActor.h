#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "JTSTestWrappedSphereActor.generated.h"

class UJTSMoonWrappedActorComponent;
class UMaterialInterface;
class UStaticMeshComponent;

/** Minimal sphere actor used to verify Moon wrapping and material assignment. */
UCLASS()
class SPACE_API AJTSTestWrappedSphereActor : public AActor
{
	GENERATED_BODY()

public:
	AJTSTestWrappedSphereActor();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void ApplyMeshMaterial();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Test|Wrapped Sphere", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Test|Wrapped Sphere", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSMoonWrappedActorComponent> MoonWrappedActorComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test|Wrapped Sphere", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> MeshMaterial;
};
