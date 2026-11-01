#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/JTSMeleeTarget.h"
#include "space/World/JTSPlanetSettlementEnemy.h"
#include "space/Systems/JTSReplicatedMotionBuffer.h"
#include "JTSPlanetSurfaceCreature.generated.h"

class AJTSPlanetAnchor;
class AJTSPlanetEcologyHabitat;
class UJTSHealthComponent;
class UJTSPlanetSurfaceSteeringComponent;
class USphereComponent;
class USkeletalMeshComponent;
class UAnimSequence;

UENUM(BlueprintType)
enum class EJTSForagerState : uint8
{
	Idle, Walk, Feed, Startle, Flee, Burrow, Hidden, Emerge, Attack, Hit, Dead
};

/** Small server-owned surface animal. Species, assets and tuning are Blueprint configuration. */
UCLASS(Blueprintable)
class SPACE_API AJTSPlanetSurfaceCreature : public AActor, public IJTSPlanetSettlementEnemy, public IJTSMeleeTarget
{
	GENERATED_BODY()
public:
	AJTSPlanetSurfaceCreature();
	virtual bool InitializeForSettlement_Implementation(AJTSPlanetAnchor* Planet, FVector HomeLocation, FVector GroundLocation) override;
	virtual void OnSettlementAttackStarted_Implementation() override;
	virtual void OnSettlementForceImpact_Implementation(FVector Direction, float Speed) override;
	void SetHabitat(AJTSPlanetEcologyHabitat* InHabitat, const FVector& RefugeLocation);
	virtual bool CanReceiveMeleeHit_Implementation(APawn* AttackingPawn) const override;
	virtual void ReceiveMeleeHit_Implementation(APawn* AttackingPawn, EJTSMeleeAttackType AttackType) override;
	virtual FText GetMeleeTargetDisplayName_Implementation() const override;
	virtual FText GetMeleeTargetPrompt_Implementation(APawn* AttackingPawn) const override;
	virtual FVector GetMeleeTargetAnchorWorldLocation_Implementation() const override;
	virtual void Tick(float DeltaSeconds) override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnRep_ReplicatedMovement() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Creature") TObjectPtr<USphereComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Creature") TObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Creature") TObjectPtr<UJTSHealthComponent> Health;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Creature") TObjectPtr<UJTSPlanetSurfaceSteeringComponent> Steering;
	UPROPERTY(EditDefaultsOnly, Category="Creature") FText DisplayName;
	UPROPERTY(EditDefaultsOnly, Category="Creature|Motion", meta=(ClampMin="1")) float WalkSpeed = 65;
	UPROPERTY(EditDefaultsOnly, Category="Creature|Motion", meta=(ClampMin="1")) float FleeSpeed = 210;
	UPROPERTY(EditDefaultsOnly, Category="Creature|Behavior", meta=(ClampMin="0")) float RoamRadius = 1400;
	UPROPERTY(EditDefaultsOnly, Category="Creature|Behavior", meta=(ClampMin="0")) float StartleDistance = 450;
	/** Start the dive at the entrance lip, before the body reaches the mouth's origin. */
	UPROPERTY(EditDefaultsOnly, Category="Creature|Behavior", meta=(ClampMin="1")) float RefugeEntryDistance = 110;
	UPROPERTY(EditDefaultsOnly, Category="Creature|Behavior", meta=(ClampMin="1")) float HomeArrivalDistance = 120;
	UPROPERTY(EditDefaultsOnly, Category="Creature|Behavior") bool bDefendsNest = false;
	UPROPERTY(EditDefaultsOnly, Category="Creature|Behavior") bool bRetaliates = false;
	UPROPERTY(EditDefaultsOnly, Category="Creature|Behavior") bool bRepairsRefuge = false;
	UPROPERTY(EditDefaultsOnly, Category="Creature|Behavior", meta=(ClampMin="0")) float AttackDamage = 6;
	UPROPERTY(EditDefaultsOnly, Category="Creature|Behavior", meta=(ClampMin="1")) float AttackReach = 100;
	UPROPERTY(EditDefaultsOnly, Category="Creature|Behavior", meta=(ClampMin="0")) float LeashRadius = 1800;
	UPROPERTY(EditDefaultsOnly, Category="Creature|Animation") TMap<EJTSForagerState, TObjectPtr<UAnimSequence>> Animations;
	UPROPERTY(ReplicatedUsing=OnRep_State, VisibleInstanceOnly, BlueprintReadOnly, Category="Creature") EJTSForagerState State = EJTSForagerState::Idle;
private:
	void UpdateBehavior();
	void SetState(EJTSForagerState NewState, float Duration);
	bool MoveTowards(const FVector& Destination, float Speed, float DeltaSeconds);
	APawn* FindThreat() const;
	void ChooseNextActivity();
	UFUNCTION() void OnRep_State();
	UFUNCTION() void OnDamaged(float Current, float Maximum, float Damage, AActor* Causer);
	UFUNCTION() void OnDied(AController* InstigatorController, AActor* Causer);
	TWeakObjectPtr<AJTSPlanetAnchor> ActivePlanet;
	UPROPERTY(VisibleInstanceOnly, Category="Creature|Behavior") TWeakObjectPtr<AJTSPlanetEcologyHabitat> Habitat;
	TWeakObjectPtr<APawn> Aggressor;
	UPROPERTY(VisibleInstanceOnly, Category="Creature|Behavior") TWeakObjectPtr<AActor> RefugeActor;
	FVector AggressionOrigin = FVector::ZeroVector;
	FVector Home = FVector::ZeroVector;
	UPROPERTY(VisibleInstanceOnly, Category="Creature|Behavior") FVector Refuge = FVector::ZeroVector;
	FVector Goal = FVector::ZeroVector;
	double StateEnd = 0;
	double NextAttack = 0;
	double LastUpdate = 0;
	FTimerHandle BehaviorTimer;
	FJTSReplicatedMotionBuffer MotionBuffer;
};
