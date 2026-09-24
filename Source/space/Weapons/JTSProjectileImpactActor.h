#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "JTSProjectileImpactActor.generated.h"

class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

UCLASS()
class SPACE_API AJTSProjectileImpactActor : public AActor
{
	GENERATED_BODY()

public:
	AJTSProjectileImpactActor();

	void InitializeImpact(const FVector& ImpactNormal, UMaterialInterface* GlowMaterial,
		const FLinearColor& Color, bool bShowMark);

protected:
	virtual void BeginPlay() override;

private:
	void HideImpactFlash();

	UPROPERTY(VisibleAnywhere, Category = "Impact")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Impact")
	TObjectPtr<UStaticMeshComponent> ImpactMark;

	UPROPERTY(VisibleAnywhere, Category = "Impact")
	TObjectPtr<UStaticMeshComponent> ImpactFlashA;

	UPROPERTY(VisibleAnywhere, Category = "Impact")
	TObjectPtr<UStaticMeshComponent> ImpactFlashB;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ImpactMarkMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ImpactFlashMaterial;

	FTimerHandle ImpactFlashTimerHandle;
};
