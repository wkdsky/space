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

/** A destructible Moon landmark that occasionally emits one real, short-lived roach actor. */
UCLASS()
class SPACE_API AJTSRoachNestActor : public AActor, public IJTSMeleeTarget
{
	GENERATED_BODY()

public:
	AJTSRoachNestActor();

	void AdjustToGround(const FVector& GroundLocation);

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
	bool ResolveRoachGroundLocation(const FVector& CandidateLocation, FVector& OutGroundLocation) const;
	void ScheduleNextRoachSpawn();
	void TrySpawnRoach();
	void UpdateMoonWrappedLogicalPosition();

	UPROPERTY(VisibleAnywhere, Category = "Moon|Roach")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> NestMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Wrapping", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSMoonWrappedActorComponent> MoonWrappedActorComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> NestMaterial;

	FTimerHandle RoachSpawnTimerHandle;
	TWeakObjectPtr<AJTSRoachActor> ActiveRoach;
	int32 PunchHitsRemaining = 3;
};
