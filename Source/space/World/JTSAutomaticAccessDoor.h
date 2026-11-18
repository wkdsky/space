#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/IInteractable.h"
#include "space/Items/JTSItemTypes.h"
#include "JTSAutomaticAccessDoor.generated.h"

class UStaticMeshComponent;
class UJTSFacilityConditionComponent;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EJTSAccessReaderState : uint8
{
    Idle,
    Reading,
    Denied
};

/** Server authenticates nearby expedition members; local-space leaves also work on spherical worlds. */
UCLASS(Blueprintable)
class SPACE_API AJTSAutomaticAccessDoor : public AActor, public IInteractable
{
    GENERATED_BODY()
public:
    AJTSAutomaticAccessDoor();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Frame;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> LeftLeaf;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> RightLeaf;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Reader;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> StatusLight;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UJTSFacilityConditionComponent> Conditions;

    UPROPERTY(EditAnywhere, Category="Door") FVector DetectionCenter=FVector(0,0,180);
    UPROPERTY(EditAnywhere, Category="Door") FVector ActivationHalfExtents=FVector(320,210,200);
    UPROPERTY(EditAnywhere, Category="Door") FVector SafetyHalfExtents=FVector(130,180,200);
    UPROPERTY(EditAnywhere, Category="Door") FVector LeafTravel=FVector(0,150,0);
    UPROPERTY(EditAnywhere, Category="Door", meta=(ClampMin="0.1")) float SlideSeconds=.8f;
    UPROPERTY(EditAnywhere, Category="Door", meta=(ClampMin="0")) float CloseDelay=1.5f;
    UPROPERTY(EditAnywhere, Category="Door") FText LockedPrompt;
    UPROPERTY(EditAnywhere, Category="Door") FText DisplayName;
    UPROPERTY(EditAnywhere, Category="Door") FText AuthorizedPrompt;
    UPROPERTY(EditAnywhere, Category="Door|Access") bool bRequireManualActivation=false;
    UPROPERTY(EditAnywhere, Category="Door|Access") EJTSItemId RequiredHeldItem=EJTSItemId::None;
    UPROPERTY(EditAnywhere, Category="Door|Access") FName RequiredCredentialId;
    UPROPERTY(EditAnywhere, Category="Door|Access") FName UnlockedFlag;
    UPROPERTY(EditAnywhere, Category="Door|Access") FText ActivationPrompt;
    UPROPERTY(EditAnywhere, Category="Door|Access") FText MissingDevicePrompt;
    UPROPERTY(EditAnywhere, Category="Door|Access") FText MissingCredentialPrompt;
    UPROPERTY(EditAnywhere, Category="Door|Access") float SwipeSeconds=.9f;
    UPROPERTY(EditAnywhere, Category="Door|Access") TObjectPtr<UMaterialInterface> ReadingMaterial;
    UPROPERTY(EditAnywhere, Category="Door|Access") TObjectPtr<UMaterialInterface> DeniedMaterial;
    UPROPERTY(ReplicatedUsing=OnRep_ReaderState, BlueprintReadOnly, Category="Door|Access") EJTSAccessReaderState ReaderState=EJTSAccessReaderState::Idle;
    UPROPERTY(EditAnywhere, Category="Door") TObjectPtr<UMaterialInterface> LockedMaterial;
    UPROPERTY(EditAnywhere, Category="Door") TObjectPtr<UMaterialInterface> AuthorizedMaterial;
    UPROPERTY(ReplicatedUsing=OnRep_DoorState, BlueprintReadOnly) bool bOpen=false;
    UPROPERTY(ReplicatedUsing=OnRep_DoorState, BlueprintReadOnly) bool bAuthorized=false;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual bool CanInteract_Implementation(APawn* Pawn) const override;
    virtual bool CanExecuteInteraction_Implementation(APawn* Pawn) const override;
    virtual FText GetInteractionPrompt_Implementation(APawn*) const override;
    virtual FText GetInteractionLabel_Implementation() const override { return DisplayName; }
    virtual FVector GetInteractionPromptAnchor_Implementation() const override;
    virtual void Interact_Implementation(APawn* Pawn) override;
    virtual float GetInteractionHoldDuration_Implementation(APawn*) const override { return bRequireManualActivation && RequiredHeldItem==EJTSItemId::None && RequiredCredentialId.IsNone()?SwipeSeconds:0; }
    FVector GetInteractionTargetWorldLocation() const;
protected:
    virtual void PostInitializeComponents() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void InitializePresentation();
    void AdvanceSlide();
    void Refresh();
    bool HasHeldCredential(APawn* Pawn) const;
    bool HasRequiredDevice(APawn* Pawn) const;
    void CompleteReaderScan();
    void ActivateDoor();
    void ResetReader();
    UFUNCTION() void OnRep_ReaderState();
    UFUNCTION() void OnRep_DoorState();
    FVector LeftClosed, RightClosed;
    float OpenFraction=0;
    double LastPresenceTime=0;
    double LastSlideTime=0;
    bool bPresentationReady=false;
    FTimerHandle PresenceTimer;
    FTimerHandle SlideTimer;
    FTimerHandle ReaderTimer;
    TWeakObjectPtr<APawn> ScanningPawn;
    FGuid ScanningItemId;
};
