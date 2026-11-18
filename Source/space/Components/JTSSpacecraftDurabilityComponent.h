#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Items/JTSResourceType.h"
#include "JTSSpacecraftDurabilityComponent.generated.h"
/** Hull damage is authoritative; disabled ships preserve inertial velocity. */
UCLASS(ClassGroup=(Ship),meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSSpacecraftDurabilityComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UJTSSpacecraftDurabilityComponent();
    UPROPERTY(EditAnywhere) float MaximumHull=600;
    UPROPERTY(EditAnywhere) TMap<EJTSResourceType,int32> ReconstructionCosts;
    UPROPERTY(Replicated,BlueprintReadOnly) float Hull=600;
    UPROPERTY(Replicated,BlueprintReadOnly) bool bDisabled=false;
    UPROPERTY(Replicated,BlueprintReadOnly) float EstimatedSurvivalSeconds=0;
    UFUNCTION(BlueprintCallable,BlueprintAuthorityOnly) bool Reconstruct(APawn* Pawn);
    void RestoreHull(float Value);
protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
    UFUNCTION() void OnDamage(AActor* Actor,float Damage,const UDamageType* Type,AController* Instigator,AActor* Causer);
    double LastDamageTime=-100;
    float RecentDamageRate=0;
};
