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
class UPrimitiveComponent;
class UJTSNestEntranceMeshComponent;
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
	/** Aligns the nest with real planet collision. */
	void PlaceOnPlanetSurface(
		AJTSPlanetAnchor* Planet,
		const FVector& GroundLocation,
		const FVector& PreferredForward);
	void SetMoonAntNestVisualScale(float InVisualScale);
	/** Receives the GameMode-configured MoonAnt class once; null intentionally selects the native fallback at spawn time. */
	void SetMoonAntActorClass(TSubclassOf<AJTSMoonAntActor> InMoonAntActorClass);

	/** Activates a level-authored entrance without replacing its fitted transform. */
	bool ActivateAuthoredPlanetNest(AJTSPlanetAnchor* Planet, TSubclassOf<AJTSMoonAntActor> InMoonAntActorClass);
	void DeactivateSurfaceNest();
	void NotifyMoonAntEnded(AJTSMoonAntActor* Ant);
	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Population")
	int32 GetActiveMoonAntCount() const;
	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt|Population")
	float GetPopulationSpawnInterval() const;

	virtual bool CanReceiveMeleeHit_Implementation(APawn* AttackingPawn) const override;
	virtual void ReceiveMeleeHit_Implementation(APawn* AttackingPawn, EJTSMeleeAttackType AttackType) override;
	virtual FText GetMeleeTargetDisplayName_Implementation() const override;
	virtual FText GetMeleeTargetPrompt_Implementation(APawn* AttackingPawn) const override;
	virtual FVector GetMeleeTargetAnchorWorldLocation_Implementation() const override;

	AJTSPlanetAnchor* GetSurfacePlanet() const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_NestPresentation();

	const IJTSMoonSurfaceGameplaySettings* GetMoonGameMode() const;
	FVector GetVisualBoundsExtent() const;
	UPrimitiveComponent* GetNestVisual() const;
	float ChooseMoonAntSpawnDistance(const IJTSMoonSurfaceGameplaySettings& MoonGameMode) const;
	void ScheduleNextMoonAntSpawn();
	void StartSurfaceActivity();
	void TrySpawnMoonAnt();
	bool IsUsingRealPlanetSurface() const;

	UPROPERTY(VisibleAnywhere, Category = "Moon|MoonAnt|Nest")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|MoonAnt|Nest", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> NestMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|MoonAnt|Nest", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSNestEntranceMeshComponent> EntranceShape;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MoonAntNestMaterial;

	/** Explicitly supplied by the active Moon surface controller when this runtime Nest is created. */
	UPROPERTY(Transient)
	TSubclassOf<AJTSMoonAntActor> MoonAntActorClass;

	FTimerHandle MoonAntSpawnTimerHandle;
	TArray<TWeakObjectPtr<AJTSMoonAntActor>> ActiveMoonAnts;
	UPROPERTY(Replicated)
	TObjectPtr<AJTSPlanetAnchor> SurfacePlanet;

	UPROPERTY(ReplicatedUsing = OnRep_NestPresentation)
	FVector SurfaceUp = FVector::UpVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nest", meta = (AllowPrivateAccess = "true"))
	FVector BaseMoonAntNestMeshScale = FVector(0.68f, 0.68f, 0.20f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Nest", meta = (AllowPrivateAccess = "true"))
	FLinearColor NestTint = FLinearColor(0.18f, 0.055f, 0.025f, 1.0f);

	bool bSurfaceActivityStarted = false;
	/** Uses the configured active cap as a target and replenishes faster when more ants are missing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Population", meta = (AllowPrivateAccess = "true"))
	bool bMaintainPopulation = false;

	UPROPERTY(ReplicatedUsing = OnRep_NestPresentation)
	float NestVisualScale = 1.0f;

	UPROPERTY(Replicated)
	int32 PunchHitsRemaining = 3;

	UPROPERTY(ReplicatedUsing = OnRep_NestPresentation)
	bool bUsesRealPlanetSurface = false;
};
