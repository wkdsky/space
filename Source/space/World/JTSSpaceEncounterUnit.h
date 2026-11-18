#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Items/JTSResourceType.h"
#include "JTSSpaceEncounterUnit.generated.h"
class UStaticMeshComponent;
class UJTSHealthComponent;
class AJTSSpacecraftActor;
UCLASS(Blueprintable)
class SPACE_API AJTSSpaceEncounterUnit : public AActor
{
    GENERATED_BODY()
public:
    UPROPERTY(ReplicatedUsing=OnRep_Team) int32 TeamId=0;
    UFUNCTION() void OnRep_Team();
public:
    AJTSSpaceEncounterUnit();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UJTSHealthComponent> Health;
    UPROPERTY(EditAnywhere) bool bLightSailRay=false;
    UPROPERTY(EditAnywhere,ReplicatedUsing=OnRep_Visual) int32 SizeTier=2;
    UPROPERTY(EditAnywhere) float ContactDamage=30;
    UPROPERTY(EditAnywhere) float HeatDamage=12;
    UPROPERTY(EditAnywhere) float WarningSeconds=1.25f;
    UPROPERTY(EditAnywhere) float HeatRadius=1500;
    UPROPERTY(EditAnywhere) float WingRootHalfWidth=12;
    UPROPERTY(EditAnywhere) float WingDamageMultiplier=3;
    UPROPERTY(EditAnywhere) TMap<EJTSResourceType,int32> Salvage;
    UPROPERTY(ReplicatedUsing=OnRep_Team,BlueprintReadOnly) bool bHostile=false;
    UPROPERTY(Replicated,BlueprintReadOnly) bool bHeating=false;
    UPROPERTY(Replicated,BlueprintReadOnly) FVector HeatPoint=FVector::ZeroVector;
    UPROPERTY(Replicated,BlueprintReadOnly) FVector DriftVelocity=FVector::ZeroVector;
    UPROPERTY(ReplicatedUsing=OnRep_Visual,BlueprintReadOnly) bool bIsSalvage=false;
    void Advance(float Dt,AJTSSpacecraftActor* Ship);
    virtual float TakeDamage(float Damage,const FDamageEvent& Event,AController* Controller,AActor* Causer) override;
protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
    UFUNCTION() void OnDeath(AController* InstigatorController,AActor* Causer);
    UFUNCTION() void OnRep_Visual();
    FVector AuthoredMeshScale=FVector::OneVector;
    double AttackAt=0;
    double CollisionReady=0;
};
