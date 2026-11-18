#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSCreatureTacticsComponent.generated.h"
class AJTSPlanetAnchor;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
UENUM(BlueprintType)
enum class EJTSCreatureTactic : uint8 { None, Roll, LightFlock, IceAmbush, BurrowBite, HeavyMiner, CeilingRunner, FeignWreck };
UENUM(BlueprintType)
enum class EJTSCreaturePosture : uint8 { Rest, Warning, Attack, Retreat, Stunned };
/** Optional species mechanics layered over the existing settlement/creature lifecycle. */
UCLASS(ClassGroup=(Creature),meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSCreatureTacticsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UJTSCreatureTacticsComponent();
    UPROPERTY(EditAnywhere) EJTSCreatureTactic Tactic=EJTSCreatureTactic::None;
    UPROPERTY(EditAnywhere) bool bPassive=false;
    UPROPERTY(EditAnywhere) float DetectRadius=1000;
    UPROPERTY(EditAnywhere) float AttackSpeed=450;
    UPROPERTY(EditAnywhere) float Damage=12;
    UPROPERTY(EditAnywhere) TObjectPtr<class UStaticMesh> BodyMesh;
    UPROPERTY(EditAnywhere) TObjectPtr<class UMaterialInterface> BodyMaterial;
    UPROPERTY(EditAnywhere) FVector BodySize=FVector(1);
    UPROPERTY(EditAnywhere) FVector BodyOffset=FVector(0,0,45);
    UPROPERTY(EditAnywhere) FVector StunnedBodyOffset=FVector::ZeroVector;
    UPROPERTY(ReplicatedUsing=OnRep_Posture,BlueprintReadOnly) EJTSCreaturePosture Posture=EJTSCreaturePosture::Rest;
    bool Update(AJTSPlanetAnchor* Planet,float Dt);
    float DamageMultiplier() const;
    void RefreshPresentation();
protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
    void SetPosture(EJTSCreaturePosture P,float Duration);
    UFUNCTION() void OnRep_Posture();
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Flock;
    double StateUntil=0,NextHit=0;
    FVector Home=FVector::ZeroVector,LockedHeading=FVector::ForwardVector;
    bool bInitialized=false,bAwake=false,bHit=false;
};
