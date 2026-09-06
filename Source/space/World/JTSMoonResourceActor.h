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
	int32 GetResourceAmount() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Resource")
	bool CanBePickedUp() const;

	FVector GetVisualBoundsExtent() const;
	void AdjustToGround(const FVector& GroundHitLocation);

	void InitializeResource(
		EJTSResourceType NewResourceType,
		int32 NewResourceAmount,
		bool bNewCanPickup,
		const FText& NewPickupText = FText::GetEmpty());

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	FText MakeDefaultPickupText() const;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resource", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 ResourceAmount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resource", meta = (AllowPrivateAccess = "true"))
	FText PickupText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resource", meta = (AllowPrivateAccess = "true"))
	bool bCanPickup = true;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ResourceMaterial;

	bool bResourceConsumed = false;
};
