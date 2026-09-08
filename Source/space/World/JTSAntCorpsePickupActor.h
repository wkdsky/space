#pragma once

#include "CoreMinimal.h"
#include "space/Items/JTSWorldPickupActor.h"

#include "JTSAntCorpsePickupActor.generated.h"

class UMaterialInterface;
class UPrimitiveComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * A manually collected Moon Ant corpse. The gameplay root remains on the logical Moon surface;
 * the copied Ant visual owns its short pop and Fake Moon bend separately.
 */
UCLASS()
class SPACE_API AJTSAntCorpsePickupActor : public AJTSWorldPickupActor
{
	GENERATED_BODY()

public:
	AJTSAntCorpsePickupActor();

	/**
	 * Copies the dead Ant's already-final visual data before FinishSpawning. Ground support and the
	 * short pop are intentionally initialized only after the components have registered in BeginPlay.
	 */
	void InitializeFromAnt(
		USkeletalMesh* SourceSkeletalMesh,
		UStaticMesh* SourceFallbackMesh,
		const TArray<UMaterialInterface*>& SourceMaterials,
		const FVector& SourceVisualScale,
		const FRotator& SourceVisualRotation,
		const FVector& InDeathGroundLocation);

	/** Runtime diagnostics used by the one-shot Ant death logs. */
	bool HasVisibleCorpseVisual() const;
	FString GetCorpseVisualDebugName() const;
	FVector GetCorpseVisualScale() const;
	bool IsCorpseVisualHidden() const;

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FVector GetInteractionTargetWorldLocation() const override;
	virtual FVector GetInteractionAnchorWorldLocation() const override;
	virtual FVector GetVisualBoundsExtent() const override;
	virtual void AdjustToGround(const FVector& GroundHitLocation) override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual UMaterialInterface* GetMoonBendMaterialForPickup() const override;

private:
	UPrimitiveComponent* GetActiveCorpseVisual() const;
	void ConfigureCorpseVisual();
	void RecalculateGroundSupport();
	void ResolveInitialSettledGroundLocation();
	void ResolveFinalSettledGroundLocation();
	void PlaceAtSettledGroundLocation();
	void UpdateCorpseVisualTransform(float PopAlpha);
	FVector GetPopVisualWorldOffset(float PopAlpha) const;
	void UpdateInteractionCollider();
	void SetCorpseInteractionEnabled(bool bEnabled);

	/** Dedicated visual only; it never provides interaction collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Ant Corpse|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> CorpseMesh;

	/** Always has an Engine primitive assigned when copying a SkeletalMesh fails. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Ant Corpse|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> CorpseFallbackMesh;

	/** The only collision component. It is enabled only once the visual has settled. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Ant Corpse|Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> PickupInteractionCollider;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant Corpse|Visual", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float CorpseSideRollDegrees = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant Corpse|Pop", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float CorpsePopHeight = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant Corpse|Pop", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float CorpsePopHorizontalDistanceMin = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant Corpse|Pop", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float CorpsePopHorizontalDistanceMax = 45.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant Corpse|Pop", meta = (AllowPrivateAccess = "true", ClampMin = "0.05", UIMin = "0.05"))
	float CorpsePopDurationMin = 0.30f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant Corpse|Pop", meta = (AllowPrivateAccess = "true", ClampMin = "0.05", UIMin = "0.05"))
	float CorpsePopDurationMax = 0.45f;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMesh> SourceSkeletalMeshAsset;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> SourceFallbackMeshAsset;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> DebugFallbackMeshAsset;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DebugFallbackMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> SourceVisualMaterials;

	FVector SourceVisualScale = FVector::OneVector;
	FRotator CorpseBaseRelativeRotation = FRotator::ZeroRotator;
	FVector CorpseBaseRelativeLocation = FVector::ZeroVector;
	FVector DeathGroundLocation = FVector::ZeroVector;
	FVector SettledGroundLocation = FVector::ZeroVector;
	FVector2D DeathLogicalPosition = FVector2D::ZeroVector;
	FVector2D SettledLogicalPosition = FVector2D::ZeroVector;
	float GroundSupportHeight = 1.0f;
	float CorpsePopElapsed = 0.0f;
	float CorpsePopDuration = 0.35f;
	bool bUseSkeletalCorpseVisual = false;
	bool bUseDebugFallbackVisual = true;
	bool bVisualUsesMaterialMoonBend = false;
	bool bCorpseSettled = false;
	bool bInitializedFromAnt = false;
	bool bUsingMoonWrapForPop = false;
};
