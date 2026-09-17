#pragma once

#include "CoreMinimal.h"
#include "space/Items/JTSWorldPickupActor.h"

#include "JTSMoonAntCorpsePickupActor.generated.h"

class UMaterialInterface;
class UPrimitiveComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;
class AJTSPlanetAnchor;

/**
 * A manually collected MoonAnt corpse placed on the active Moon PlanetAnchor surface.
 */
UCLASS()
class SPACE_API AJTSMoonAntCorpsePickupActor : public AJTSWorldPickupActor
{
	GENERATED_BODY()

public:
	AJTSMoonAntCorpsePickupActor();

	/**
	 * Copies the dead MoonAnt's already-final visual data before FinishSpawning. Ground support and the
	 * short pop are intentionally initialized only after the components have registered in BeginPlay.
	 */
	void InitializeFromMoonAnt(
		USkeletalMesh* SourceSkeletalMesh,
		UStaticMesh* SourceFallbackMesh,
		const TArray<UMaterialInterface*>& SourceMaterials,
		const FVector& SourceVisualScale,
		const FRotator& SourceVisualRotation,
		const FVector& InDeathGroundLocation);

	/** Runtime diagnostics used by the one-shot MoonAnt death logs. */
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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_CorpseVisualData();

	UFUNCTION()
	void OnRep_CorpseSettled();

	UPrimitiveComponent* GetActiveCorpseVisual() const;
	void ConfigureCorpseVisual();
	void RecalculateGroundSupport();
	void ResolveInitialSettledGroundLocation();
	void ResolveFinalSettledGroundLocation();
	void PlaceAtSettledGroundLocation();
	void UpdateCorpseVisualTransform(float PopAlpha);
	void UpdateInteractionCollider();
	void SetCorpseInteractionEnabled(bool bEnabled);
	AJTSPlanetAnchor* GetRealSurfacePlanet() const;
	FVector GetCorpseSurfaceUp(const FVector& SurfaceLocation) const;

	/** Dedicated visual only; it never provides interaction collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|MoonAnt Corpse|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> CorpseMesh;

	/** Always has an Engine primitive assigned when copying a SkeletalMesh fails. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|MoonAnt Corpse|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> CorpseFallbackMesh;

	/** The only collision component. It is enabled only once the visual has settled. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|MoonAnt Corpse|Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> PickupInteractionCollider;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt Corpse|Visual", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float CorpseSideRollDegrees = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt Corpse|Pop", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float CorpsePopHeight = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt Corpse|Pop", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float CorpsePopHorizontalDistanceMin = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt Corpse|Pop", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float CorpsePopHorizontalDistanceMax = 45.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt Corpse|Pop", meta = (AllowPrivateAccess = "true", ClampMin = "0.05", UIMin = "0.05"))
	float CorpsePopDurationMin = 0.30f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt Corpse|Pop", meta = (AllowPrivateAccess = "true", ClampMin = "0.05", UIMin = "0.05"))
	float CorpsePopDurationMax = 0.45f;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_CorpseVisualData)
	TObjectPtr<USkeletalMesh> SourceSkeletalMeshAsset;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_CorpseVisualData)
	TObjectPtr<UStaticMesh> SourceFallbackMeshAsset;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> DebugFallbackMeshAsset;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DebugFallbackMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> SourceVisualMaterials;

	UPROPERTY(ReplicatedUsing = OnRep_CorpseVisualData)
	FVector SourceVisualScale = FVector::OneVector;

	UPROPERTY(ReplicatedUsing = OnRep_CorpseVisualData)
	FRotator CorpseBaseRelativeRotation = FRotator::ZeroRotator;

	FVector CorpseBaseRelativeLocation = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_CorpseVisualData)
	FVector DeathGroundLocation = FVector::ZeroVector;
	FVector SettledGroundLocation = FVector::ZeroVector;
	TWeakObjectPtr<AJTSPlanetAnchor> RealSurfacePlanet;
	float GroundSupportHeight = 1.0f;
	float CorpsePopElapsed = 0.0f;
	float CorpsePopDuration = 0.35f;
	bool bUseSkeletalCorpseVisual = false;
	bool bUseDebugFallbackVisual = true;
	UPROPERTY(ReplicatedUsing = OnRep_CorpseSettled)
	bool bCorpseSettled = false;

	UPROPERTY(ReplicatedUsing = OnRep_CorpseVisualData)
	bool bInitializedFromMoonAnt = false;
};
