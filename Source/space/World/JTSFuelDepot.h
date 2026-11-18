#pragma once
#include "CoreMinimal.h"
#include "space/World/JTSFacilityController.h"
#include "JTSFuelDepot.generated.h"
class AJTSPlanetAnchor;
UCLASS(Blueprintable)
class SPACE_API AJTSFuelDepot : public AJTSFacilityController
{
    GENERATED_BODY()
public:
    UPROPERTY(EditInstanceOnly) TObjectPtr<AJTSPlanetAnchor> Planet;
    UPROPERTY(EditAnywhere) int32 FuelAmount=80;
    UPROPERTY(Replicated) bool bUsedThisLanding=false;
    virtual bool CanInteract_Implementation(APawn* Pawn) const override;
    virtual bool CanExecuteInteraction_Implementation(APawn* Pawn) const override { return bActive?CanInteract_Implementation(Pawn):Super::CanExecuteInteraction_Implementation(Pawn); }
    virtual FText GetInteractionPrompt_Implementation(APawn* Pawn) const override;
    virtual float GetInteractionHoldDuration_Implementation(APawn* Pawn) const override { return bActive?0:RepairSeconds; }
    virtual void Interact_Implementation(APawn* Pawn) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type R) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
    void CheckLanding();
    FTimerHandle LandingTimer;
};
