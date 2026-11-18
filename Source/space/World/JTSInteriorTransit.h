#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/IInteractable.h"
#include "JTSInteriorTransit.generated.h"
class UStaticMeshComponent;
class UJTSFacilityConditionComponent;
UCLASS(Blueprintable)
class SPACE_API AJTSInteriorTransit : public AActor, public IInteractable
{
    GENERATED_BODY()
public:
    AJTSInteriorTransit();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UJTSFacilityConditionComponent> Conditions;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite) TObjectPtr<AActor> Destination;
    UPROPERTY(EditAnywhere) FText DisplayName;
    UPROPERTY(EditAnywhere, Category="Presentation") FText PromptLabel;
    UPROPERTY(EditAnywhere) FText LockedPrompt;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0")) float TransitSeconds=.3f;
    UPROPERTY(EditAnywhere,meta=(ClampMin="0")) float RideSeconds=0;
    UPROPERTY(EditInstanceOnly) TArray<TObjectPtr<AActor>> RideWalls;
    UPROPERTY(EditInstanceOnly) TObjectPtr<class AJTSPlanetEnemySettlement> RideGuards;
    UPROPERTY(EditAnywhere) TObjectPtr<class USoundBase> RideSound;
    UPROPERTY(ReplicatedUsing=OnRep_Riding) bool bRiding=false;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual bool CanInteract_Implementation(APawn* Pawn) const override;
    virtual bool CanExecuteInteraction_Implementation(APawn* Pawn) const override;
    virtual FText GetInteractionPrompt_Implementation(APawn*) const override;
    virtual FText GetInteractionLabel_Implementation() const override { return PromptLabel.IsEmpty() ? DisplayName : PromptLabel; }
    virtual void Interact_Implementation(APawn* Pawn) override;
private:
    virtual void EndPlay(const EEndPlayReason::Type R) override;
    UFUNCTION() void OnRep_Riding();
    TMap<TWeakObjectPtr<AActor>,FVector> WallOrigins;
    void CompleteTransit(TWeakObjectPtr<APawn> Pawn);
    TSet<TWeakObjectPtr<APawn>> Pending;
};
