#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSInteriorStateComponent.generated.h"
class AJTSInteriorZone;
UCLASS(ClassGroup=(Planet), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSInteriorStateComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UJTSInteriorStateComponent();
    UPROPERTY(Replicated, BlueprintReadOnly) TObjectPtr<AJTSInteriorZone> Zone;
    UFUNCTION(BlueprintPure) bool IsIndoors() const { return Zone!=nullptr; }
    UFUNCTION(Client,Reliable) void ClientFade(bool bOut);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type R) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
    void UpdateZone();
    FTimerHandle ZoneTimer;
};
