#pragma once
#include "CoreMinimal.h"
#include "space/Items/JTSItemTypes.h"
#include "space/Ships/JTSShipDockActor.h"
#include "JTSStellarExchangeDock.generated.h"

class UWidgetComponent;
class USoundBase;

/** Physical lever and independent adjacent display. The ship component owns rolls and private delivery. */
UCLASS(Blueprintable)
class SPACE_API AJTSStellarExchangeDock : public AJTSShipDockActor
{
	GENERATED_BODY()
public:
	AJTSStellarExchangeDock();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FVector GetInteractionWorldLocation(const FVector& ReferenceLocation) const override;
	virtual FText GetInteractionPrompt_Implementation(APawn* Pawn) const override;
	virtual void Interact_Implementation(APawn* Pawn) override;
	bool IsLookingAtLever(const APawn* Pawn) const;
	bool EjectPrize(const FJTSItemInstance& Item, APawn* Recipient);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> PrizeEjectAnchor;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float PrizeUpwardSpeed = 250;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float PrizeForwardSpeed = 180;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> LeverPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Lever;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> LeverTarget;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UWidgetComponent> Display;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float PullAngle = 62;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<USoundBase> PullSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<USoundBase> LockSound;
private:
	int32 HeardSequence = 0;
	bool bWasRolling = false;
};
