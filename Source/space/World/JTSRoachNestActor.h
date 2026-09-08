#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/JTSMeleeTarget.h"

#include "JTSRoachNestActor.generated.h"

class AJTSRoachActor;
class APawn;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;
class UJTSMoonWrappedActorComponent;

/** Destructible Moon Ant Nest. The legacy native class name is retained for local asset compatibility. */
UCLASS()
class SPACE_API AJTSRoachNestActor : public AActor, public IJTSMeleeTarget
{
	GENERATED_BODY()

public:
	AJTSRoachNestActor();

	void AdjustToGround(const FVector& GroundLocation);
	void SetAntNestVisualScale(float InVisualScale);
	/** Receives the GameMode-configured Ant class once; null intentionally selects the native fallback at spawn time. */
	void SetAntActorClass(TSubclassOf<AJTSRoachActor> InAntActorClass);

	virtual bool CanReceiveMeleeHit_Implementation(APawn* AttackingPawn) const override;
	virtual void ReceiveMeleeHit_Implementation(APawn* AttackingPawn, EJTSMeleeAttackType AttackType) override;
	virtual FText GetMeleeTargetDisplayName_Implementation() const override;
	virtual FText GetMeleeTargetPrompt_Implementation(APawn* AttackingPawn) const override;
	virtual FVector GetMeleeTargetAnchorWorldLocation_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	const class AJTSMoonGameMode* GetMoonGameMode() const;
	FVector GetVisualBoundsExtent() const;
	bool ResolveAntGroundLocation(const FVector& CandidateLocation, FVector& OutGroundLocation) const;
	float ChooseAntSpawnDistance(const class AJTSMoonGameMode& MoonGameMode) const;
	void ScheduleNextAntSpawn();
	void TrySpawnAnt();
	void UpdateMoonWrappedLogicalPosition();

	UPROPERTY(VisibleAnywhere, Category = "Moon|Ant|Nest")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Ant|Nest", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> NestMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Wrapping", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSMoonWrappedActorComponent> MoonWrappedActorComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> AntNestMaterial;

	/** Explicitly supplied by AJTSMoonGameMode when this runtime Nest is created. */
	UPROPERTY(Transient)
	TSubclassOf<AJTSRoachActor> AntActorClass;

	FTimerHandle AntSpawnTimerHandle;
	TArray<TWeakObjectPtr<AJTSRoachActor>> ActiveAnts;
	FVector BaseAntNestMeshScale = FVector(0.68f, 0.68f, 0.20f);
	int32 PunchHitsRemaining = 3;
};
