#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/JTSMeleeTarget.h"

#include "JTSRoachActor.generated.h"

class AJTSRoachNestActor;
class APawn;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;
class UJTSMoonWrappedActorComponent;

/** The brief, tangible life cycle of a single roach after it emerges from a nest. */
UENUM(BlueprintType)
enum class EJTSRoachState : uint8
{
	Emerging UMETA(DisplayName = "Emerging"),
	Crawling UMETA(DisplayName = "Crawling"),
	ReactingToHit UMETA(DisplayName = "Reacting To Hit"),
	Escaping UMETA(DisplayName = "Escaping"),
	Burrowing UMETA(DisplayName = "Burrowing")
};

/** Runtime-only roach actor. It exists only after a nest has emitted it and destroys itself when it burrows. */
UCLASS()
class SPACE_API AJTSRoachActor : public AActor, public IJTSMeleeTarget
{
	GENERATED_BODY()

public:
	AJTSRoachActor();

	void InitializeRoach(AJTSRoachNestActor* InOriginNest, const FVector& InGroundLocation, const FVector& InCrawlDirection);

	UFUNCTION(BlueprintPure, Category = "Moon|Roach")
	EJTSRoachState GetRoachState() const;

	virtual bool CanReceiveMeleeHit_Implementation(APawn* AttackingPawn) const override;
	virtual void ReceiveMeleeHit_Implementation(APawn* AttackingPawn, EJTSMeleeAttackType AttackType) override;
	virtual FText GetMeleeTargetDisplayName_Implementation() const override;
	virtual FText GetMeleeTargetPrompt_Implementation(APawn* AttackingPawn) const override;
	virtual FVector GetMeleeTargetAnchorWorldLocation_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	const class AJTSMoonGameMode* GetMoonGameMode() const;
	void AdvanceHorizontal(const FVector& Direction, float Speed, float DeltaSeconds);
	void BeginEscape(APawn* AttackingPawn);
	void BeginBurrow();
	void SetRoachState(EJTSRoachState NewState);
	void UpdateAppearanceForState(float StateProgress = 0.0f);
	void UpdateMoonWrappedLogicalPosition();

	UPROPERTY(VisibleAnywhere, Category = "Moon|Roach")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> RoachMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Wrapping", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSMoonWrappedActorComponent> MoonWrappedActorComponent;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Moon|Roach", meta = (AllowPrivateAccess = "true"))
	EJTSRoachState RoachState = EJTSRoachState::Emerging;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RoachMaterial;

	TWeakObjectPtr<AJTSRoachNestActor> OriginNest;
	FVector GroundLocation = FVector::ZeroVector;
	FVector CrawlDirection = FVector::ForwardVector;
	FVector EscapeDirection = FVector::ForwardVector;
	FVector BaseMeshScale = FVector(0.22f, 0.34f, 0.09f);
	float StateElapsed = 0.0f;
	float LifeElapsed = 0.0f;
	int32 PunchHitsRemaining = 3;
	bool bInitialized = false;
};
