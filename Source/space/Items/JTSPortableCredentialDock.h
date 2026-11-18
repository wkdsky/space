#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/IInteractable.h"
#include "space/Items/JTSItemTypes.h"
#include "JTSPortableCredentialDock.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UMaterialInterface;

/** Physical ship-mounted issuer. Only the server can copy earned expedition permits into an item. */
UCLASS(Blueprintable)
class SPACE_API AJTSPortableCredentialDock : public AActor, public IInteractable
{
    GENERATED_BODY()
public:
    AJTSPortableCredentialDock();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mount;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Device;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Status;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> InteractionBounds;
    UFUNCTION(BlueprintPure) FVector GetInteractionTargetWorldLocation() const;
    UPROPERTY(EditDefaultsOnly) EJTSItemId DeviceItemId = EJTSItemId::PortableCommunicator;
    UPROPERTY(EditDefaultsOnly) TArray<FName> ScannablePermitIds;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UMaterialInterface> EmptyMaterial;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UMaterialInterface> ReadyMaterial;
    UPROPERTY(ReplicatedUsing=OnRep_Available, BlueprintReadOnly) bool bAvailable = true;
    virtual bool CanInteract_Implementation(APawn* Pawn) const override;
    virtual bool CanExecuteInteraction_Implementation(APawn* Pawn) const override;
    virtual FText GetInteractionPrompt_Implementation(APawn* Pawn) const override;
    virtual FText GetInteractionLabel_Implementation() const override;
    virtual FVector GetInteractionPromptAnchor_Implementation() const override;
    virtual float GetInteractionHoldDuration_Implementation(APawn*) const override { return 0.f; }
    virtual void Interact_Implementation(APawn* Pawn) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) FJTSItemInstance DockedItem;
    TArray<FName> EarnedPermits() const;
    void Refresh();
    UFUNCTION() void OnRep_Available();
    FTimerHandle RefreshTimer;
};
