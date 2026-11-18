#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JTSFacilityHazard.generated.h"
class UBoxComponent;
class UStaticMeshComponent;
class UJTSFacilityConditionComponent;
UENUM(BlueprintType)
enum class EJTSFacilityHazardKind : uint8 { SporeFog, IceLoad, DustSweep, Slag, FallingIce };
UCLASS(Blueprintable)
class SPACE_API AJTSFacilityHazard : public AActor
{
    GENERATED_BODY()
public:
    AJTSFacilityHazard();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBoxComponent> Bounds;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UJTSFacilityConditionComponent> Conditions;
    UPROPERTY(EditAnywhere) EJTSFacilityHazardKind Kind=EJTSFacilityHazardKind::SporeFog;
    UPROPERTY(EditAnywhere) float DamagePerSecond=4;
    UPROPERTY(EditAnywhere) int32 MaximumOccupants=2;
    UPROPERTY(EditAnywhere) float WarningSeconds=1.25f;
    UPROPERTY(EditAnywhere) float RecoverySeconds=12;
    UPROPERTY(EditAnywhere) FName CompletedFlag;
    UPROPERTY(EditAnywhere) TArray<FName> SweepFlags;
    UPROPERTY(EditInstanceOnly) TArray<TObjectPtr<AActor>> AffectedFloors;
    UPROPERTY(EditAnywhere) TObjectPtr<class USoundBase> WarningSound;
    UPROPERTY(EditAnywhere) TObjectPtr<class USoundBase> TriggerSound;
    UPROPERTY(ReplicatedUsing=OnRep_Triggered,BlueprintReadOnly) bool bTriggered=false;
    UPROPERTY(ReplicatedUsing=OnRep_Warning,BlueprintReadOnly) bool bWarning=false;
    /** Creature/environment events use the same warning and recovery cycle as occupancy. */
    void Arm();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type R) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
    void Update();
    UFUNCTION() void OnRep_Triggered();
    UFUNCTION() void OnRep_Warning();
    FTimerHandle UpdateTimer;
    double TriggerAt=0,ResetAt=0;
    bool bForcedTrigger=false;
};
