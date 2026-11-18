#pragma once
#include "CoreMinimal.h"
#include "space/World/JTSFacilityController.h"
#include "space/Interaction/JTSMeleeTarget.h"
#include "JTSBreakableObstacle.generated.h"
class UJTSHealthComponent;
class AJTSInteriorZone;
/** Authored breakable wall or local prop; never edits a celestial mesh. */
UCLASS(Blueprintable)
class SPACE_API AJTSBreakableObstacle : public AJTSFacilityController, public IJTSMeleeTarget
{
    GENERATED_BODY()
public:
    AJTSBreakableObstacle();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UJTSHealthComponent> Health;
    UPROPERTY(EditAnywhere) bool bRequiresMiningTool=true;
    UPROPERTY(EditAnywhere) TMap<EJTSResourceType,int32> Salvage;
    UPROPERTY(ReplicatedUsing=OnRep_Broken) bool bPropBroken=false;
    /** A camera resets only when the room transitions from empty to occupied. */
    UPROPERTY(EditInstanceOnly) TObjectPtr<AJTSInteriorZone> RefreshOnEntry;
    /** Optional brief CRT-style collapse before the destroyed prop disappears. */
    UPROPERTY(EditAnywhere) float ShutdownSeconds=0;
    UPROPERTY(EditAnywhere) TObjectPtr<class USoundBase> ShutdownSound;
    virtual float TakeDamage(float Damage,const FDamageEvent& Event,AController* Controller,AActor* Causer) override;
    virtual bool BlocksAttackInitiation_Implementation(APawn* Pawn) const override;
    virtual bool CanInteract_Implementation(APawn*) const override { return false; }
    virtual bool CanReceiveMeleeHit_Implementation(APawn* Pawn) const override;
    virtual void ReceiveMeleeHit_Implementation(APawn* Pawn,EJTSMeleeAttackType Type) override;
    virtual FText GetMeleeTargetDisplayName_Implementation() const override { return DisplayName; }
    virtual FText GetMeleeTargetPrompt_Implementation(APawn*) const override;
    virtual FVector GetMeleeTargetAnchorWorldLocation_Implementation() const override { return GetActorLocation(); }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type R) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
    UFUNCTION() void Broken(AController* Controller,AActor* Causer);
    UFUNCTION() void OnRep_Broken();
    void CheckEntry();
    void AnimateShutdown();
    bool bOccupied=false;
    FVector IntactMeshScale=FVector::OneVector;
    double ShutdownAt=0;
    FTimerHandle EntryTimer,ShutdownTimer;
};
