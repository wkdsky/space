// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/IInteractable.h"
#include "space/Items/JTSItemTypes.h"

#include "JTSShopTerminalActor.generated.h"

class AJTSCharacter;
class AJTSSpacecraftActor;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/** A replicated, physical SpaceWorld supply terminal backed by the active ship's shared storage. */
UCLASS()
class SPACE_API AJTSShopTerminalActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AJTSShopTerminalActor();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeTerminal(AJTSSpacecraftActor* InSharedSpacecraft);

	UFUNCTION(BlueprintPure, Category = "Shop")
	AJTSSpacecraftActor* GetSharedSpacecraft() const;

	UFUNCTION(BlueprintPure, Category = "Shop")
	int32 GetSharedResourceAmount(EJTSResourceType ResourceType) const;

	bool TryDepositPlayerMaterials(AJTSCharacter* Player);
	EJTSShopPurchaseResult TryPurchase(AJTSCharacter* Player, EJTSItemId ItemId);

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

private:
	bool IsPlayerInTerminalRange(const APawn* Player) const;
	bool BuildCosts(EJTSItemId ItemId, TMap<EJTSResourceType, int32>& OutCosts) const;
	bool DeliverPurchase(AJTSCharacter* Player, const FJTSItemInstance& Item, bool& bOutDropped);
	void RefreshVisuals();

	UPROPERTY(VisibleAnywhere, Category = "Shop")
	TObjectPtr<UStaticMeshComponent> TerminalMesh;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Shop")
	TObjectPtr<AJTSSpacecraftActor> SharedSpacecraft;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TerminalMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Shop", meta = (ClampMin = "100.0", UIMin = "100.0"))
	float InteractionDistance = 360.0f;
};
