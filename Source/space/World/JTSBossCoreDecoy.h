#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/JTSMeleeTarget.h"
#include "JTSBossCoreDecoy.generated.h"
class UStaticMeshComponent;
class AJTSPlanetBoss;
UCLASS(Blueprintable)
class SPACE_API AJTSBossCoreDecoy : public AActor, public IJTSMeleeTarget
{
    GENERATED_BODY()
public:
    AJTSBossCoreDecoy();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(Replicated) TObjectPtr<AJTSPlanetBoss> Boss;
    UPROPERTY(Replicated) bool bOriginal=false;
    UPROPERTY(EditAnywhere) float ExplosionDamage=16;
    bool IsNameplateLit(const APawn* Viewer) const;
    virtual float TakeDamage(float Damage,const FDamageEvent& Event,AController* Controller,AActor* Causer) override;
    virtual bool CanReceiveMeleeHit_Implementation(APawn* P) const override;
    virtual void ReceiveMeleeHit_Implementation(APawn* P,EJTSMeleeAttackType Type) override;
    virtual FText GetMeleeTargetDisplayName_Implementation() const override;
    virtual FText GetMeleeTargetPrompt_Implementation(APawn*) const override;
    virtual FVector GetMeleeTargetAnchorWorldLocation_Implementation() const override { return GetActorLocation(); }
protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
};
