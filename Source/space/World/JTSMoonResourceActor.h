#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/IInteractable.h"
#include "space/Interaction/JTSMeleeTarget.h"
#include "space/Items/JTSItemTypes.h"
#include "space/Items/JTSResourceType.h"

#include "JTSMoonResourceActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class AJTSPlanetAnchor;

/** Physical Moon resource forms. Small rocks are loose pickups; the other forms are mineable nodes. */
UENUM(BlueprintType)
enum class EJTSMoonResourceNodeSize : uint8
{
	MediumRock UMETA(DisplayName = "Medium Rock"),
	LargeRock UMETA(DisplayName = "Large Rock"),
	OreVein UMETA(DisplayName = "Ore Vein")
};

UCLASS()
class SPACE_API AJTSMoonResourceActor : public AActor, public IInteractable, public IJTSMeleeTarget
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

	UFUNCTION(BlueprintPure, Category = "Moon|Resource")
	EJTSMoonResourceNodeSize GetNodeSize() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Mining")
	float GetRemainingMiningWork() const;

	/** Name and mesh-top anchor used by the native world interaction prompt. */
	UFUNCTION(BlueprintPure, Category = "Moon|Interaction")
	FText GetInteractionDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Interaction")
	FVector GetInteractionAnchorWorldLocation() const;

	FVector GetVisualBoundsExtent() const;
	void AdjustToGround(const FVector& GroundHitLocation);

	/** Places this node against real planet mesh collision and aligns local Z with its surface normal. */
	void PlaceOnPlanetSurface(
		AJTSPlanetAnchor* Planet,
		const FVector& GroundLocation,
		const FVector& PreferredForward);

	/** Initializes a Medium Rock, Large Rock, or Ore Vein mining node. */
	void InitializeMiningNode(EJTSResourceType NewResourceType, int32 NewTotalYieldUnits, EJTSMoonResourceNodeSize NewNodeSize = EJTSMoonResourceNodeSize::LargeRock);

	/** Server-authoritative work application shared by melee and hitscan weapons. */
	bool ApplyMiningWork(APawn* Miner, EJTSItemId SourceItemId, float WorkAmount);

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	virtual bool CanReceiveMeleeHit_Implementation(APawn* AttackingPawn) const override;
	virtual void ReceiveMeleeHit_Implementation(APawn* AttackingPawn, EJTSMeleeAttackType AttackType) override;
	virtual FText GetMeleeTargetDisplayName_Implementation() const override;
	virtual FText GetMeleeTargetPrompt_Implementation(APawn* AttackingPawn) const override;
	virtual FVector GetMeleeTargetAnchorWorldLocation_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_ResourceData();

	UFUNCTION()
	void OnRep_SurfacePresentation();

	FText GetMiningPrompt(APawn* InteractingPawn) const;
	bool ResolveHeldMiningWork(APawn* Miner, EJTSItemId& OutItemId, float& OutWork) const;
	bool SpawnAllResourceDrops(APawn* Miner);
	void ConfigureResourceMesh();
	void ApplySurfacePresentationMaterial();
	void ApplyResourceAppearance();

	UPROPERTY(VisibleAnywhere, Category = "Moon|Resource")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Resource", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ResourceMesh;

	/** Surface material used by every resource node. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Rendering", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> RealPlanetSurfaceMaterial;

	/** Engine primitive used by all rock variants. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> RockMesh;

	/** Engine primitive used by every ore deposit. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> OreMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_ResourceData, Category = "Moon|Resource", meta = (AllowPrivateAccess = "true"))
	EJTSResourceType ResourceType = EJTSResourceType::Rock;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_ResourceData, Category = "Moon|Mining", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 TotalYieldUnits = 6;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_ResourceData, Category = "Moon|Mining", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 RemainingYieldUnits = 6;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_ResourceData, Category = "Moon|Resource", meta = (AllowPrivateAccess = "true"))
	EJTSMoonResourceNodeSize NodeSize = EJTSMoonResourceNodeSize::LargeRock;

	/** Work is intentionally separate from output count and combat damage. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_ResourceData, Category = "Moon|Mining", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float TotalMiningWork = 24.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_ResourceData, Category = "Moon|Mining", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float RemainingMiningWork = 24.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ResourceMaterial;

	/** Base material currently selected before the dynamic color instance is created. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AppliedPresentationMaterial;

	/** Cached normal used by interaction UI after a real-surface placement. */
	UPROPERTY(ReplicatedUsing = OnRep_SurfacePresentation)
	FVector SurfaceUp = FVector::UpVector;

	UPROPERTY(ReplicatedUsing = OnRep_SurfacePresentation)
	bool bUsesRealPlanetSurface = false;

	bool bMiningInProgress = false;
};
