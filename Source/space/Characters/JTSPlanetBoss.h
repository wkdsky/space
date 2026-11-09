#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/JTSMeleeTarget.h"
#include "space/Components/JTSBossCombatComponent.h"
#include "space/Systems/JTSReplicatedMotionBuffer.h"
#include "JTSPlanetBoss.generated.h"

class UJTSHealthComponent;
class UJTSPlanetSurfaceSteeringComponent;
class USkeletalMeshComponent;
class USceneComponent;
class UBoxComponent;
class UJTSBossHealthWidget;

USTRUCT(BlueprintType)
struct FJTSBossHitZone
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName Bone;
	/** Mesh reference-pose coordinates in centimetres. */
	UPROPERTY(EditAnywhere) FVector Center = FVector::ZeroVector;
	UPROPERTY(EditAnywhere) FVector HalfExtent = FVector(50);
	UPROPERTY(EditAnywhere) float DamageMultiplier = 1;
	UPROPERTY(EditAnywhere) bool bBlocksMovement = false;
};

/** Asset-independent surface boss presentation and damage receiver. */
UCLASS(Blueprintable)
class SPACE_API AJTSPlanetBoss : public AActor, public IJTSMeleeTarget
{
	GENERATED_BODY()
public:
	AJTSPlanetBoss();
	void InitializeOnPlanet(AJTSPlanetAnchor* Planet);
	void RefreshAction();
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float Damage, const FDamageEvent& Event, AController* EventInstigator, AActor* Causer) override;
	virtual bool CanReceiveMeleeHit_Implementation(APawn* Pawn) const override;
	virtual void ReceiveMeleeHit_Implementation(APawn* Pawn, EJTSMeleeAttackType Type) override;
	virtual FText GetMeleeTargetDisplayName_Implementation() const override { return DisplayName; }
	virtual FText GetMeleeTargetPrompt_Implementation(APawn* Pawn) const override;
	virtual FVector GetMeleeTargetAnchorWorldLocation_Implementation() const override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UJTSHealthComponent> Health;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UJTSBossCombatComponent> Combat;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UJTSPlanetSurfaceSteeringComponent> Steering;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss") FText DisplayName;
	UPROPERTY(EditDefaultsOnly, Category="Boss") float HudRange = 8000;
	UPROPERTY(EditDefaultsOnly, Category="Boss") float ExtraPlayerHealth = 180;
	UPROPERTY(EditDefaultsOnly, Category="Boss|Damage") TArray<FJTSBossHitZone> HitZones;
	UPROPERTY(EditDefaultsOnly, Category="Boss|Animation") TMap<EJTSBossAction, TObjectPtr<UAnimSequence>> Animations;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnRep_ReplicatedMovement() override;
private:
	UFUNCTION() void OnDied(AController* InstigatorController, AActor* Causer);
	UFUNCTION() void OnDamaged(float Current, float Maximum, float Damage, AActor* Causer);
	UPROPERTY() TArray<TObjectPtr<UBoxComponent>> DamageBodies;
	UPROPERTY() TArray<TObjectPtr<UJTSBossHealthWidget>> HudWidgets;
	FJTSReplicatedMotionBuffer MotionBuffer;
};
