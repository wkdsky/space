#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Components/JTSBossCombatComponent.h"
#include "JTSBossPhaseComponent.generated.h"
class USkeletalMesh;
class AJTSBossCoreDecoy;
USTRUCT(BlueprintType)
struct FJTSBossPhase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) float EnterBelowHealth=1;
    UPROPERTY(EditAnywhere) TMap<EJTSBossAction,FJTSBossAttack> Attacks;
    UPROPERTY(EditAnywhere) float WalkSpeed=190;
    UPROPERTY(EditAnywhere) float ChargeSpeed=1150;
    UPROPERTY(EditAnywhere) float DamageMultiplier=1;
    UPROPERTY(EditAnywhere) TObjectPtr<USkeletalMesh> Form;
    UPROPERTY(EditAnywhere) TSubclassOf<AActor> SummonClass;
    UPROPERTY(EditAnywhere) int32 SummonCount=0;
    UPROPERTY(EditAnywhere) float SummonInterval=12;
    UPROPERTY(EditAnywhere) TSubclassOf<AActor> AssemblyPointClass;
    UPROPERTY(EditAnywhere) float AssemblySeconds=4;
    UPROPERTY(EditAnywhere) bool bCoreCopies=false;
    UPROPERTY(EditAnywhere) TSubclassOf<AJTSBossCoreDecoy> CoreClass;
    UPROPERTY(EditAnywhere) TSubclassOf<AActor> TrailHazardClass;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJTSBossPhaseChanged,int32,PhaseIndex);
/** Monotonic phases, authored on each boss Blueprint. No project encounter IDs. */
UCLASS(ClassGroup=(Combat),meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSBossPhaseComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UJTSBossPhaseComponent();
    UPROPERTY(EditAnywhere) TArray<FJTSBossPhase> Phases;
    UPROPERTY(ReplicatedUsing=OnRep_Phase,BlueprintReadOnly) int32 PhaseIndex=INDEX_NONE;
    UPROPERTY(BlueprintAssignable) FJTSBossPhaseChanged OnPhaseChanged;
    UPROPERTY(Replicated,BlueprintReadOnly) float EncounterArmor=1;
    UPROPERTY(EditAnywhere) float ArmorIntegrity=250;
    UPROPERTY() TSubclassOf<AActor> BonusSummonClass;
    UPROPERTY() int32 BonusSummonCount=0;
    bool AllowsDamageFrom(AActor* Causer) const;
    void DamageArmor(float Damage);
    UPROPERTY(EditAnywhere) bool bCooperativeFlip=false;
    UPROPERTY(ReplicatedUsing=OnRep_Flip,BlueprintReadOnly) double FlippedUntil=0;
    /** Mesh-space correction for a foot-origin model when rolled onto its back. */
    UPROPERTY(EditAnywhere) FVector FlippedMeshOffset=FVector::ZeroVector;
    bool CanFlip(APawn* Pawn) const;
    float FlipHoldSeconds() const;
    void ContributeFlip(APawn* Pawn);
    float GetDamageMultiplier() const;
    static int32 SelectPhase(const TArray<FJTSBossPhase>& InPhases,float Fraction,int32 Previous);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type R) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
    void Update();
    void Summon();
    void SpawnCores();
    UFUNCTION() void OnRep_Phase();
    UFUNCTION() void OnRep_Flip();
    void RefreshFlipPresentation();
    FTransform RestMeshTransform=FTransform::Identity;
    bool bFlipPresented=false;
    FTimerHandle FlipPresentationTimer;
    double NextSummon=0;
    double NextHazard=0;
    double AssemblyReadyAt=0;
    TWeakObjectPtr<AActor> AssemblyPoint;
    bool bCoresSpawned=false;
    TArray<TWeakObjectPtr<AJTSBossCoreDecoy>> Cores;
    FTimerHandle PhaseTimer;
    TArray<TWeakObjectPtr<AActor>> Summons;
    TMap<TWeakObjectPtr<APawn>,double> Contributions;
};
