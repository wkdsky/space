#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSMiningImpactComponent.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USoundBase;
class UNiagaraSystem;
class UMaterialInterface;
class UDecalComponent;
class USoundAttenuation;

/** Target-owned contact sounds, scars and short-lived surface fragments. No gameplay damage. */
UCLASS(ClassGroup=(Presentation), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSMiningImpactComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSMiningImpactComponent();
	UPROPERTY(EditAnywhere, Category="Mining|Feedback") TArray<TObjectPtr<USoundBase>> ImpactSounds;
	UPROPERTY(EditAnywhere, Category="Mining|Feedback") TObjectPtr<USoundBase> BreakSound;
	UPROPERTY(EditAnywhere, Category="Mining|Feedback") TObjectPtr<USoundAttenuation> ImpactAttenuation;
	UPROPERTY(EditAnywhere, Category="Mining|Feedback") TObjectPtr<UNiagaraSystem> ImpactDust;
	UPROPERTY(EditAnywhere, Category="Mining|Feedback") TObjectPtr<UStaticMesh> FragmentMesh;
	UPROPERTY(EditAnywhere, Category="Mining|Feedback") TObjectPtr<UStaticMesh> SparkMesh;
	UPROPERTY(EditAnywhere, Category="Mining|Feedback") TObjectPtr<UMaterialInterface> SparkMaterial;
	UPROPERTY(EditAnywhere, Category="Mining|Feedback", meta=(ClampMin="0",ClampMax="16")) int32 SparkCount = 10;
	UPROPERTY(EditAnywhere, Category="Mining|Feedback") TObjectPtr<UMaterialInterface> ScarMaterial;
	UPROPERTY(EditAnywhere, Category="Mining|Feedback") FLinearColor DustColor = FLinearColor(.55f,.50f,.42f);
	UPROPERTY(EditAnywhere, Category="Mining|Feedback") float DustScale = .22f;
	UStaticMeshComponent* GetSurfaceMesh() const;
	void ConfirmImpact(const FHitResult& Hit, bool bBreaking = false);
	void ConfirmBreak();
	UFUNCTION(BlueprintPure, Category="Mining") int32 GetImpactCount() const { return ImpactCount; }
	UFUNCTION(NetMulticast, Unreliable) void MulticastImpact(FVector_NetQuantize Point, FVector_NetQuantizeNormal Normal, bool bBreaking);
protected:
	virtual void TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	struct FFragment { TWeakObjectPtr<UStaticMeshComponent> Mesh; FVector Start; FVector Velocity; FRotator Spin; double Born = 0; float Lifetime = .65f; bool bSpark = false; };
	TArray<FFragment> Fragments;
	UPROPERTY(Transient) TArray<TObjectPtr<UDecalComponent>> Scars;
	UPROPERTY(VisibleInstanceOnly, Category="Mining") int32 ImpactCount = 0;
	FVector LastPoint = FVector::ZeroVector;
	FVector LastNormal = FVector::UpVector;
};
