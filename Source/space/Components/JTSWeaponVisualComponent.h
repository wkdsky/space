#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Items/JTSItemTypes.h"

#include "JTSWeaponVisualComponent.generated.h"

class UInventoryComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Refreshes the mesh profile after inventory replication or quickbar selection. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Presentation")
	void RefreshWeaponVisual();

	/** Allows the character to keep the mesh presentation in sync with its ADS transition. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Presentation")
	void SetAimAlpha(float NewAimAlpha);

	/** Native prototype presentation used when a held melee attack starts. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Presentation")
	void PlayMeleeSwingPresentation();

	/** Cosmetic-only shot kick and a pooled muzzle flash; called for local prediction and remote shots. */
	void PlayShotPresentation(UMaterialInterface* GlowMaterial, const FLinearColor& Color);

	/** Returns the visible muzzle point used to start a replicated projectile tracer. */
	bool GetMuzzleWorldLocation(FVector& OutLocation) const;

private:
	UFUNCTION()
	void HandleInventoryChanged(int32 UsedSlots, int32 Capacity);

	void EnsureMeshComponents();
	void ConfigureAttachment();
	void AttachToRootFallback();
	/** Places the grip in the palm from the posed wrist and finger bones, in world centimetres. */
	void UpdatePalmAnchor();
	void ValidateAttachmentAfterPose();
	bool IsCurrentAttachmentPlausible() const;
	void SetVisible(bool bVisible);
	void ApplyPresentationTransform();
	void HideShotFlash();

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> HandAttachmentAnchor;

	/** Carries transient aim and melee pose offsets without moving the authored grip pivot. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> WeaponPresentationRoot;

	/** Model-local root.  Its transform is the inverse of the active item's grip transform. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> WeaponModelRoot;

	/** Non-rendered, item-local muzzle point used for projectile starts and tracers. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> WeaponMuzzle;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> MuzzleFlash;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> MuzzleLight;

	/** Visible handle reference used by the primitive prototype visual. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WeaponGrip;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WeaponBody;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WeaponBarrel;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WeaponSight;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BasicShapeMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WeaponBodyMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WeaponBarrelMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WeaponSightMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WeaponGripMaterial;

	/**
	 * Character-asset-specific correction between a hand socket's local coordinate system and
	 * the held-item grip. Keep this in the player Blueprint for each skeleton instead of baking
	 * a skeleton convention into the runtime weapon system.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation|Attachment", meta = (AllowPrivateAccess = "true"))
	FVector HandSocketRelativeLocation = FVector::ZeroVector;

	/** See HandSocketRelativeLocation. Applied by the anchor, before all item-specific offsets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation|Attachment", meta = (AllowPrivateAccess = "true"))
	FRotator HandSocketRelativeRotation = FRotator::ZeroRotator;

	/** Used only when the character has no usable hand socket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation|Attachment", meta = (AllowPrivateAccess = "true"))
	FVector FallbackHandRelativeLocation = FVector(0.0f, 30.0f, 35.0f);

	/** Diagnostic distance used to warn about an unexpectedly distant hand socket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation|Attachment", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MaximumTrustedSocketDistance = 350.0f;

	FVector DefaultBodyLocation = FVector::ZeroVector;
	FVector DefaultBarrelLocation = FVector::ZeroVector;
	FVector DefaultSightLocation = FVector::ZeroVector;
	FTransform DefaultGripTransform = FTransform::Identity;
	FTransform DefaultMuzzleTransform = FTransform::Identity;
	FVector DefaultGripScale = FVector(0.16f, 0.12f, 0.32f);
	/** Ranged leaves mesh +X on the palm forward. Melee pitches that axis onto palm up. */
	FRotator DefaultCarryRotation = FRotator::ZeroRotator;
	float AimAlpha = 0.0f;
	float ShotKickAlpha = 0.0f;
	bool bRangedVisible = false;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Presentation|Shot", meta = (ClampMin = "1.0"))
	float ShotKickRecoverySpeed = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Presentation|Shot", meta = (ClampMin = "0.01"))
	float MuzzleFlashSeconds = 0.055f;

	FTimerHandle MuzzleFlashTimerHandle;
	FTimerHandle AttachmentValidationTimerHandle;
	bool bMeleeSwingPresentationActive = false;
	bool bMeleeSwingReverse = false;
	float MeleeSwingElapsed = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Presentation|Melee", meta = (ClampMin = "0.15"))
	float MeleeSwingPresentationSeconds = 0.42f;
	bool bUsingFallbackAttachment = false;
	bool bMeshComponentsInitialized = false;
};
