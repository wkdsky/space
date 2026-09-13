#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/JTSMeleeTarget.h"

#include "JTSMoonAntNestActor.generated.h"

class AJTSMoonAntActor;
class APawn;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;
class UJTSMoonWrappedActorComponent;
class IJTSMoonSurfaceGameplaySettings;
class AJTSPlanetAnchor;

/** Destructible MoonAnt Nest. The legacy native class name is retained for local asset compatibility. */
UCLASS()
class SPACE_API AJTSMoonAntNestActor : public AActor, public IJTSMeleeTarget
{
	GENERATED_BODY()

public:
	AJTSMoonAntNestActor();

	void AdjustToGround(const FVector& GroundLocation);
	/** Aligns the nest with real planet collision and disables Fake Moon wrapping for this instance. */
	void PlaceOnPlanetSurface(
		AJTSPlanetAnchor* Planet,
		const FVector& GroundLocation,
		const FVector& PreferredForward);
	void SetMoonAntNestVisualScale(float InVisualScale);
	/** Receives the GameMode-configured MoonAnt class once; null intentionally selects the native fallback at spawn time. */
	void SetMoonAntActorClass(TSubclassOf<AJTSMoonAntActor> InMoonAntActorClass);

	virtual bool CanReceiveMeleeHit_Implementation(APawn* AttackingPawn) const override;
	virtual void ReceiveMeleeHit_Implementation(APawn* AttackingPawn, EJTSMeleeAttackType AttackType) override;
	virtual FText GetMeleeTargetDisplayName_Implementation() const override;
	virtual FText GetMeleeTargetPrompt_Implementation(APawn* AttackingPawn) const override;
	virtual FVector GetMeleeTargetAnchorWorldLocation_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	const IJTSMoonSurfaceGameplaySettings* GetMoonGameMode() const;
	FVector GetVisualBoundsExtent() const;
	bool ResolveMoonAntGroundLocation(const FVector& CandidateLocation, FVector& OutGroundLocation) const;
	float ChooseMoonAntSpawnDistance(const IJTSMoonSurfaceGameplaySettings& MoonGameMode) const;
	void ScheduleNextMoonAntSpawn();
	void TrySpawnMoonAnt();
	void UpdateMoonWrappedLogicalPosition();
	AJTSPlanetAnchor* GetSurfacePlanet() const;
	bool IsUsingRealPlanetSurface() const;

	UPROPERTY(VisibleAnywhere, Category = "Moon|MoonAnt|Nest")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|MoonAnt|Nest", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> NestMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Wrapping", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSMoonWrappedActorComponent> MoonWrappedActorComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MoonAntNestMaterial;

	/** Explicitly supplied by AJTSMoonGameMode when this runtime Nest is created. */
	UPROPERTY(Transient)
	TSubclassOf<AJTSMoonAntActor> MoonAntActorClass;

	FTimerHandle MoonAntSpawnTimerHandle;
	TArray<TWeakObjectPtr<AJTSMoonAntActor>> ActiveMoonAnts;
	TWeakObjectPtr<AJTSPlanetAnchor> SurfacePlanet;
	FVector SurfaceUp = FVector::UpVector;
	FVector BaseMoonAntNestMeshScale = FVector(0.68f, 0.68f, 0.20f);
	int32 PunchHitsRemaining = 3;
	bool bUsesRealPlanetSurface = false;
};
