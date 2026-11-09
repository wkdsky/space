#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/IInteractable.h"
#include "JTSExpeditionKeyPickup.generated.h"
class USphereComponent;
class UStaticMeshComponent;

/** Manual pickup of shared expedition progression; deliberately has no inventory item instance. */
UCLASS(Blueprintable)
class SPACE_API AJTSExpeditionKeyPickup : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	AJTSExpeditionKeyPickup();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Important Item") FName ImportantItemId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Important Item") FText DisplayName;
	UPROPERTY(EditDefaultsOnly, Category="Important Item") FText PickupNotice;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USphereComponent> InteractionVolume;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
	virtual bool CanInteract_Implementation(APawn* Pawn) const override;
	virtual FText GetInteractionPrompt_Implementation(APawn* Pawn) const override;
	FVector GetInteractionAnchorWorldLocation() const { return GetActorLocation() + GetActorUpVector() * 30.f; }
	virtual void Interact_Implementation(APawn* Pawn) override;
private:
	bool bCollected = false;
};
