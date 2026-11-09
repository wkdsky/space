#pragma once

#include "CoreMinimal.h"
#include "space/World/JTSPlanetEnemySettlement.h"
#include "space/Interaction/JTSMeleeTarget.h"
#include "space/Interaction/IInteractable.h"
#include "JTSPlanetEcologyHabitat.generated.h"

class UStaticMeshComponent;
class UJTSNestBlockadeComponent;
class UJTSMiningImpactComponent;
class AJTSPlanetSurfaceCreature;
class UStaticMesh;
class UJTSHealthComponent;
class UBoxComponent;
class UJTSPlanetSurfaceScatterComponent;
class AJTSPlanetSurfaceAttachment;

UENUM(BlueprintType)
enum class EJTSHabitatLight : uint8 { Any, Sunlit, Shadow };

/** An irregular core and a much rarer fringe, using the shared settlement spawning template. */
UCLASS(Blueprintable)
class SPACE_API AJTSPlanetEcologyHabitat : public AJTSPlanetEnemySettlement, public IJTSMeleeTarget, public IInteractable
{
	GENERATED_BODY()
public:
	AJTSPlanetEcologyHabitat();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Habitat|Blocking") TObjectPtr<UJTSNestBlockadeComponent> Blockade;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Habitat|Blocking") TObjectPtr<UJTSMiningImpactComponent> MiningImpact;
	virtual bool CanExecuteInteraction_Implementation(APawn* Pawn) const override;
	virtual bool UsesSharedInteractionProgress_Implementation() const override { return false; }
	virtual float GetSharedInteractionProgress_Implementation() const override;
	virtual void SetInteractionHeld_Implementation(APawn* Pawn, bool bHeld) override;
	void CommitEntranceBlockade(bool bAnimate = false);
	void ShowBlockedRubble();
	AActor* GetEntranceAttachment() const;
	void ReleaseEntranceAttachment();
	AJTSPlanetSurfaceCreature* SpawnEntranceDefender(int32 Index);
	virtual void Deactivate() override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Habitat") TObjectPtr<UJTSPlanetSurfaceScatterComponent> SurfaceScatter;
	virtual bool CanReceiveMeleeHit_Implementation(APawn* Pawn) const override;
	virtual void ReceiveMeleeHit_Implementation(APawn* Pawn, EJTSMeleeAttackType Type) override;
	virtual FText GetMeleeTargetDisplayName_Implementation() const override;
	virtual FText GetMeleeTargetPrompt_Implementation(APawn* Pawn) const override;
	virtual FVector GetMeleeTargetAnchorWorldLocation_Implementation() const override;
	UPROPERTY(EditAnywhere, Category="Habitat") bool bDestructibleEntrance = false;
	/** Authored only on the special level entrances; scatter recipes and Blueprint defaults leave this off. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Habitat|Blocking") bool bCanBlockEntrance = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Habitat|Blocking") FVector EntranceInteractionOffset = FVector::ZeroVector;
	/** Mesh-local ground points, configured with the entrance artwork. */
	UPROPERTY(EditAnywhere, Category="Habitat|Blocking") FVector EntranceEmergenceOffset = FVector(-40, 0, 0);
	UPROPERTY(EditAnywhere, Category="Habitat|Blocking") FVector EntranceExitOffset = FVector(-150, 0, 0);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Habitat|Blocking") TObjectPtr<UStaticMeshComponent> EntranceBlocker;
	virtual bool CanInteract_Implementation(APawn* Pawn) const override;
	virtual FText GetInteractionPrompt_Implementation(APawn* Pawn) const override;
	virtual float GetInteractionHoldDuration_Implementation(APawn* Pawn) const override;
	virtual void Interact_Implementation(APawn* Pawn) override;
	UFUNCTION(BlueprintPure, Category="Habitat|Blocking") FVector GetInteractionAnchorWorldLocation() const;
	UFUNCTION(BlueprintPure, Category="Habitat|Blocking") bool IsEntranceBlocked() const { return bCollapsed; }
	/** Do not create residents unless a live refuge for their species exists. */
	UPROPERTY(EditAnywhere, Category="Habitat") bool bRequireRefuge = false;
	bool ContainsPoint(FVector Point, const AJTSPlanetAnchor* Planet, bool bCoreOnly = false) const;
	FVector FindRefuge(FVector Point) const;
	static FVector GetRefugeSurfaceLocation(const AActor* Refuge);
	AActor* FindRefugeActor(FVector Point) const;
	bool IsRefugeAvailable() const { return !bCollapsed; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Habitat") TObjectPtr<UStaticMeshComponent> RefugeMesh;
	/** Optional closed cutter paired with this level-authored entrance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Habitat") TObjectPtr<UStaticMesh> RefugeCutVolume;
	AJTSPlanetSurfaceAttachment* CreateEntranceAttachment(AJTSPlanetAnchor* Planet);
	UPROPERTY(EditInstanceOnly, Category="Habitat") TArray<TObjectPtr<AActor>> Refuges;
	UPROPERTY(EditInstanceOnly, Category="Habitat") TArray<TObjectPtr<AJTSPlanetEcologyHabitat>> RefugeProviders;
	UPROPERTY(EditInstanceOnly, Category="Habitat") TArray<TObjectPtr<AActor>> FoodPoints;
	/** Resource spectators use their first food point without consuming it or changing inventory. */
	UPROPERTY(EditAnywhere, Category="Habitat") bool bStationaryFeeding = false;
	UPROPERTY(EditInstanceOnly, Category="Habitat") TObjectPtr<AActor> MaintenanceTarget;
	UPROPERTY(EditAnywhere, Category="Habitat", meta=(ClampMin="0")) float CoreRadius = 3500;
	UPROPERTY(EditAnywhere, Category="Habitat", meta=(ClampMin="0.3", ClampMax="1")) float MinorAxisRatio = 0.8f;
	UPROPERTY(EditAnywhere, Category="Habitat") float BoundaryPhase = 0;
	/** A probability that the entire sparse fringe cohort appears on this expedition. */
	UPROPERTY(EditAnywhere, Category="Habitat", meta=(ClampMin="0", ClampMax="1")) float ExpeditionChance = 1;
	UPROPERTY(EditAnywhere, Category="Habitat") bool bFringeOnly = false;
	UPROPERTY(EditAnywhere, Category="Habitat") EJTSHabitatLight LightPreference = EJTSHabitatLight::Any;
	UPROPERTY(EditInstanceOnly, Category="Habitat") TObjectPtr<AActor> SunLight;
	UPROPERTY(EditAnywhere, Category="Habitat", meta=(ClampMin="0", ClampMax="0.5")) float TerminatorMargin = 0.08f;
	UFUNCTION(BlueprintCallable, Category="Habitat") void ActivateEcology(AJTSPlanetAnchor* Planet, AActor* Controller);
protected:
	virtual AActor* SpawnSettlementEnemy(const FTransform& Pose, const FVector& Ground) override;
	virtual FVector GetSettlementLocation() const override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	virtual bool AcceptSpawnPoint_Implementation(FVector GroundPoint, AJTSPlanetAnchor* Planet) const override;
	virtual void ConfigureSpawnedEnemy_Implementation(AActor* Enemy) override;
private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UJTSHealthComponent> EntranceHealth;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> EntranceTarget;
	UPROPERTY(Transient) TObjectPtr<AJTSPlanetSurfaceAttachment> EntranceAttachment;
	UPROPERTY(ReplicatedUsing=ApplyCollapsed) bool bEntranceManaged = false;
	UPROPERTY(ReplicatedUsing=ApplyCollapsed) bool bEntrancePlacementRejected = false;
	UPROPERTY(ReplicatedUsing=ApplyCollapsed) bool bCollapsed = false;
	UPROPERTY(ReplicatedUsing=ApplyCollapsed) bool bRubbleVisible = true;
	UFUNCTION() void ApplyCollapsed();
	UFUNCTION() void OnEntranceDestroyed(AController* InstigatorController, AActor* Causer);
	float NormalizedDistance(FVector Point, const AJTSPlanetAnchor* Planet) const;
	TWeakObjectPtr<AJTSPlanetAnchor> EcologyPlanet;
};
