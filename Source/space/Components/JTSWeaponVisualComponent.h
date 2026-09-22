#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "JTSWeaponVisualComponent.generated.h"

class UInventoryComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Builds a small primitive-mesh firearm presentation from the active item definition.
 * It deliberately owns no combat rules; the ranged weapon component remains authoritative.
 */
UCLASS(ClassGroup = (Presentation), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSWeaponVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSWeaponVisualComponent();
	virtual void BeginPlay() override;

	/** Refreshes the mesh profile after inventory replication or quickbar selection. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Presentation")
	void RefreshWeaponVisual();

	/** Allows the character to keep the mesh presentation in sync with its ADS transition. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Presentation")
	void SetAimAlpha(float NewAimAlpha);

private:
	UFUNCTION()
	void HandleInventoryChanged(int32 UsedSlots, int32 Capacity);

	void EnsureMeshComponents();
	void ConfigureAttachment();
	void SetVisible(bool bVisible);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WeaponBody;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WeaponBarrel;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WeaponSight;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CylinderMesh;

	FVector DefaultBodyLocation = FVector::ZeroVector;
	FVector DefaultBarrelLocation = FVector::ZeroVector;
	FVector DefaultSightLocation = FVector::ZeroVector;
	float AimAlpha = 0.0f;
};
