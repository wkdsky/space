#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSPlanetSurfaceCutComponent.generated.h"

class UStaticMesh;
class UDynamicMeshComponent;
struct FJTSPlanetSurfaceHit;
struct FJTSPlanetSurfaceCutState;
struct FJTSPlanetSurfaceCutStateDeleter { void operator()(FJTSPlanetSurfaceCutState* Value) const; };
DECLARE_MULTICAST_DELEGATE(FJTSSurfaceCutsRebuilt);

/** Reversible Boolean cuts on a private runtime copy. Never writes to the authored mesh asset. */
UCLASS(ClassGroup=(Planet), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSPlanetSurfaceCutComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSPlanetSurfaceCutComponent();
	virtual ~UJTSPlanetSurfaceCutComponent() override;
	/** A CPU-readable copy of the configured surface, selected in Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface Cuts") TObjectPtr<UStaticMesh> CutSourceMesh;
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Surface Cuts") TObjectPtr<UDynamicMeshComponent> RuntimeSurface;
	void RegisterCut(AActor* CutOwner, UStaticMesh* Volume, const FTransform& WorldTransform);
	void RemoveCut(AActor* CutOwner);
	bool ProjectOriginalSurface(FVector Point, FJTSPlanetSurfaceHit& Hit) const;
	UFUNCTION(BlueprintCallable, Category="Surface Cuts") bool RebuildNow();
	UFUNCTION(BlueprintPure, Category="Surface Cuts") int32 GetCutCount() const;
	bool IsCutApplied(const AActor* CutOwner) const;
	FJTSSurfaceCutsRebuilt OnCutsRebuilt;
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	TUniquePtr<FJTSPlanetSurfaceCutState, FJTSPlanetSurfaceCutStateDeleter> State;
	FTimerHandle RebuildTimer;
	bool EnsureSource();
	void QueueRebuild();
	void RestoreOriginal();
};
