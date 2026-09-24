#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "JTSProjectileActor.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

/** Brief, local-only hitscan streak. Damage and hit validation live in the ranged component. */
UCLASS()
class SPACE_API AJTSProjectileActor : public AActor
{
	GENERATED_BODY()

public:
	AJTSProjectileActor();
	virtual void Tick(float DeltaSeconds) override;

	void InitializeTracer(const FVector& Start, const FVector& End, UMaterialInterface* GlowMaterial,
		const FLinearColor& Color);

private:
	UPROPERTY(VisibleAnywhere, Category = "Tracer")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Tracer")
	TObjectPtr<UStaticMeshComponent> HaloMesh;

	UPROPERTY(VisibleAnywhere, Category = "Tracer")
	TObjectPtr<UStaticMeshComponent> CoreMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Tracer", meta = (ClampMin = "100.0"))
	float StreakLength = 140.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Tracer", meta = (ClampMin = "1000.0"))
	float TravelSpeed = 80000.0f;

	FVector TraceStart = FVector::ZeroVector;
	FVector TraceDirection = FVector::ForwardVector;
	float TraceDistance = 0.0f;
	float TravelledDistance = 0.0f;
};
