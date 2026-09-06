#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/IInteractable.h"
#include "space/Items/JTSResourceType.h"

#include "JTSMoonResourceActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UJTSMoonWrappedActorComponent;

UCLASS()
class SPACE_API AJTSMoonResourceActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AJTSMoonResourceActor();

	UFUNCTION(BlueprintPure, Category = "Moon|Resource")
	EJTSResourceType GetResourceType() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Resource")
	int32 GetTotalYieldUnits() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Resource")
	int32 GetRemainingYieldUnits() const;

	/** Name and mesh-top anchor used by the native world interaction prompt. */
	UFUNCTION(BlueprintPure, Category = "Moon|Interaction")
	FText GetInteractionDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Interaction")
	FVector GetInteractionAnchorWorldLocation() const;

	FVector GetVisualBoundsExtent() const;
	void AdjustToGround(const FVector& GroundHitLocation);

	/** Initializes a multi-use Large Rock or Ore Deposit mining node. */
	void InitializeMiningNode(EJTSResourceType NewResourceType, int32 NewTotalYieldUnits);

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	FText GetMiningPrompt(APawn* InteractingPawn) const;
	void ConfigureResourceMesh();
	void ApplyResourceAppearance();

	UPROPERTY(VisibleAnywhere, Category = "Moon|Resource")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Resource", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ResourceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Wrapping", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSMoonWrappedActorComponent> MoonWrappedActorComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Rendering", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> FakeMoonBendMaterial;

	/** Engine primitive used by all rock variants. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> RockMesh;

	/** Engine primitive used by every ore deposit. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> OreMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resource", meta = (AllowPrivateAccess = "true"))
	EJTSResourceType ResourceType = EJTSResourceType::Rock;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Mining", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 TotalYieldUnits = 6;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Mining", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 RemainingYieldUnits = 6;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ResourceMaterial;

	bool bMiningInProgress = false;
};
