#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/IInteractable.h"
#include "space/Items/JTSItemTypes.h"
#include "JTSItemSupplyRack.generated.h"

class UJTSFacilityConditionComponent;
class UStaticMeshComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FJTSRackSlotsChanged);

/** Content-configured expedition supplies. Inventory, flags and refill decisions are server owned. */
UCLASS(Blueprintable)
class SPACE_API AJTSItemSupplyRack : public AActor, public IInteractable
{
    GENERATED_BODY()
public:
    AJTSItemSupplyRack();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UJTSFacilityConditionComponent> RefillConditions;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Supply") EJTSItemId ItemId=EJTSItemId::None;
    UPROPERTY(EditAnywhere, Category="Supply", meta=(ClampMin="1",ClampMax="32")) int32 SlotCount=4;
    UPROPERTY(EditAnywhere, Category="Supply", meta=(ClampMin="0")) int32 InitialCount=4;
    UPROPERTY(EditAnywhere, Category="Supply", meta=(ClampMin="0.1")) float RefillDelay=10;
    UPROPERTY(EditAnywhere, Category="Supply") FName StopFlag;
    /** On world restore, stock = incomplete flags + reserve, clamped to slot capacity. */
    UPROPERTY(EditAnywhere, Category="Supply|Restore") TArray<FName> CompletionFlags;
    UPROPERTY(EditAnywhere, Category="Supply|Restore", meta=(ClampMin="0")) int32 ReserveCount=1;
    /** Model-local occupied slot centres, also used to resolve the player's aimed slot. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Supply") TArray<FVector> SlotLocations;
    UPROPERTY(ReplicatedUsing=OnRep_Slots, BlueprintReadOnly, Category="Supply") TArray<uint8> Slots;
    UPROPERTY(BlueprintAssignable) FJTSRackSlotsChanged OnSlotsChanged;
    UFUNCTION(BlueprintPure) int32 GetSupplyCount() const;
    UFUNCTION(BlueprintPure) int32 GetAimedSlot(APawn* Pawn) const;
    virtual bool CanInteract_Implementation(APawn* Pawn) const override;
    virtual bool CanExecuteInteraction_Implementation(APawn* Pawn) const override;
    virtual FText GetInteractionPrompt_Implementation(APawn* Pawn) const override;
    virtual FText GetInteractionLabel_Implementation() const override;
    virtual void Interact_Implementation(APawn* Pawn) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    void Refresh();
    void Refill();
    void RemoveSupplies();
    UFUNCTION() void OnRep_Slots();
    FTimerHandle RefreshTimer;
    FTimerHandle RefillTimer;
};
