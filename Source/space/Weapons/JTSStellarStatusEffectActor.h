#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JTSStellarStatusEffectActor.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Local enemy feedback only. Status rules and replication stay on the target component. */
UCLASS(Blueprintable)
class SPACE_API AJTSStellarStatusEffectActor : public AActor
{
	GENERATED_BODY()
public:
	AJTSStellarStatusEffectActor();
	virtual void Tick(float DeltaSeconds) override;
	UFUNCTION(BlueprintCallable, Category="Status") void UpdateStatus(bool Frozen, bool Burning, bool LightBurning, bool Poisoned);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UInstancedStaticMeshComponent> IceShards;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UInstancedStaticMeshComponent> Flames;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UInstancedStaticMeshComponent> Embers;
	/** Stains the actual body surface, including animated skeletal meshes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status") TObjectPtr<UMaterialInterface> BodyTintMaterial;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status|Attachments") FName HeadSocket;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status|Attachments") TArray<FName> IceSockets;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status") FLinearColor IceColor = FLinearColor(.28f, .72f, 1.f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status") FLinearColor FireColor = FLinearColor(1.f, .19f, .015f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status") FLinearColor LightFireColor = FLinearColor(1.f, .68f, .12f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status") FLinearColor PoisonColor = FLinearColor(.22f, .8f, .035f);
private:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void FindBodyMesh();
	void SetLayer(UStaticMeshComponent* Mesh, bool Visible, FLinearColor Color);
	void UpdateInstances(UInstancedStaticMeshComponent* Mesh, const TArray<FTransform>& Transforms);
	bool bFrozen = false, bBurning = false, bLightBurning = false, bPoisoned = false;
	TWeakObjectPtr<UMeshComponent> BodyMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> OriginalOverlay;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BodyTint;
};
