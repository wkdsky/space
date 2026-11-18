#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/IInteractable.h"
#include "space/Items/JTSResourceType.h"
#include "space/Items/JTSItemTypes.h"
#include "JTSFacilityController.generated.h"
class UJTSFacilityConditionComponent;
class UStaticMeshComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJTSFacilityChanged, bool, bActive);
/** A server-committed repair/automatic latch. Mesh and all progression IDs are content configuration. */
UCLASS(Blueprintable)
class SPACE_API AJTSFacilityController : public AActor, public IInteractable
{
    GENERATED_BODY()
public:
    AJTSFacilityController();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UJTSFacilityConditionComponent> Conditions;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> StatusLight;
    UPROPERTY(EditAnywhere, Category="Presentation") TObjectPtr<class UMaterialInterface> InactiveMaterial;
    UPROPERTY(EditAnywhere, Category="Presentation") TObjectPtr<class UMaterialInterface> ActiveMaterial;
    UPROPERTY(EditAnywhere, Category="Presentation") FText LockedPrompt;
    UPROPERTY(EditAnywhere, Category="Presentation") FText CompletedPrompt;
    UPROPERTY(EditAnywhere, Category="Presentation", meta=(ClampMin="0")) float StatusActivationDelay=0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CompletedFlag;
    UPROPERTY(EditAnywhere) FText DisplayName;
    UPROPERTY(EditAnywhere) bool bShowProgressOnHUD = false;
    UPROPERTY(EditAnywhere) FName FollowupFlag;
    UPROPERTY(EditAnywhere) bool bAutomatic = false;
    UPROPERTY(EditAnywhere) bool bHideMeshWhenActive = false;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0")) float RepairSeconds = 3;
    UPROPERTY(EditAnywhere) TMap<EJTSResourceType,int32> RepairCosts;
    UPROPERTY(EditAnywhere, Category="Repair") EJTSItemId RequiredHeldItem=EJTSItemId::None;
    UPROPERTY(EditAnywhere, Category="Repair", meta=(ClampMin="1")) int32 RequiredHeldItemCount=1;
    UPROPERTY(EditAnywhere, Category="Repair") FText MissingHeldItemPrompt;
    UPROPERTY(EditAnywhere, Category="Repair") FText HeldItemActionPrompt;
    UPROPERTY(EditAnywhere, Category="Interaction", meta=(ClampMin="1")) float InteractionDistance=350;
    /** Optional one-time recovery/harvest, committed with CompletedFlag on the server. */
    UPROPERTY(EditAnywhere) TMap<EJTSResourceType,int32> CompletionRewards;
    UPROPERTY(EditInstanceOnly) TArray<TObjectPtr<AActor>> PoweredActors;
    UPROPERTY(ReplicatedUsing=OnRep_Active, BlueprintReadOnly) bool bActive=false;
    UPROPERTY(BlueprintAssignable) FJTSFacilityChanged OnStateChanged;
    /** Replicated new activation; restoring a completed flag never emits this event. */
    UPROPERTY(BlueprintAssignable) FJTSFacilityChanged OnRuntimeActivated;
    virtual bool CanInteract_Implementation(APawn* Pawn) const override;
    virtual bool CanExecuteInteraction_Implementation(APawn* Pawn) const override;
    virtual FText GetInteractionPrompt_Implementation(APawn* Pawn) const override;
    virtual FText GetInteractionLabel_Implementation() const override { return DisplayName; }
    virtual float GetInteractionHoldDuration_Implementation(APawn*) const override { return RequiredHeldItem==EJTSItemId::None ? RepairSeconds : 0; }
    virtual void Interact_Implementation(APawn* Pawn) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
    void Refresh();
    bool HasRequiredHeldItem(APawn* Pawn) const;
    UFUNCTION(NetMulticast, Reliable) void MulticastRuntimeActivated();
    UFUNCTION() void OnRep_Active();
    void ApplyStatusPresentation();
    bool bPresentationInitialized=false;
    FTimerHandle StatusTimer;
    FTimerHandle RefreshTimer;
};
