#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JTSMoonCorpseActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class AJTSPlanetSurfaceAnchor;

/** Server-owned Moon event landmark. Blueprint selects the complete posed corpse mesh. */
UCLASS()
class SPACE_API AJTSMoonCorpseActor : public AActor
{
	GENERATED_BODY()

public:
	AJTSMoonCorpseActor();

	/** Places the complete figure on an authored real-planet surface anchor. */
	UFUNCTION(BlueprintCallable, Category = "Moon|Real Surface")
	bool SnapToPlanetSurfaceAnchor(AJTSPlanetSurfaceAnchor* SurfaceAnchor);

	UFUNCTION(BlueprintPure, Category = "Moon|Real Surface")
	bool IsUsingRealPlanetSurfacePlacement() const;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Moon|MoonAnt Event")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Asset and material choices belong to the corpse Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|MoonAnt Event", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> CorpseMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Real Surface", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float PlanetSurfaceClearance = 2.0f;

	UPROPERTY(Replicated)
	bool bUsesRealPlanetSurfacePlacement = false;
};
