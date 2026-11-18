#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/World/JTSNavigationStage.h"
#include "JTSNavigationStageComponent.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FJTSNavigationStageChanged,EJTSNavigationMapStage,Stage,FName,SystemId);
UCLASS(ClassGroup=(World),meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSNavigationStageComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UJTSNavigationStageComponent();
    UPROPERTY(ReplicatedUsing=OnRep_Stage,BlueprintReadOnly) EJTSNavigationMapStage Stage=EJTSNavigationMapStage::Interstellar;
    UPROPERTY(ReplicatedUsing=OnRep_Stage,BlueprintReadOnly) FName SystemId;
    UPROPERTY(BlueprintAssignable) FJTSNavigationStageChanged OnStageChanged;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type R) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
    void Update();
    UFUNCTION() void OnRep_Stage();
    FTimerHandle StageTimer;
};
