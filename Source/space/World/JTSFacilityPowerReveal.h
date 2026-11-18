#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JTSFacilityPowerReveal.generated.h"
class AJTSFacilityController;
class UCameraComponent;
class APlayerController;

/** Server-triggered camera move; restored saves never replay it. All framing is content configuration. */
UCLASS(Blueprintable)
class SPACE_API AJTSFacilityPowerReveal : public AActor
{
    GENERATED_BODY()
public:
    AJTSFacilityPowerReveal();
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(EditInstanceOnly) TObjectPtr<AJTSFacilityController> Source;
    UPROPERTY(EditAnywhere) FVector CameraStart=FVector(700,850,500);
    UPROPERTY(EditAnywhere) FVector CameraEnd=FVector(380,520,360);
    UPROPERTY(EditAnywhere) FVector Focus=FVector(0,0,400);
    UPROPERTY(EditAnywhere) float Duration=4.f;
    UPROPERTY(EditAnywhere) float AudienceRadius=18000;
    UPROPERTY(EditAnywhere) FText Notice;
    UPROPERTY(EditAnywhere) TObjectPtr<class USoundBase> PowerOnSound;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void OnPowerChanged(bool bActive);
    UFUNCTION(NetMulticast, Reliable) void MulticastReveal();
    void Advance();
    void Restore();
    FTimerHandle CameraTimer;
    double Started=0;
    bool bInitialStateReceived=false;
    TWeakObjectPtr<APlayerController> LocalController;
    TWeakObjectPtr<AActor> PreviousView;
    TWeakObjectPtr<APawn> OriginalPawn;
};
