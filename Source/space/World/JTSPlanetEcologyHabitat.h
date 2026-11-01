#pragma once

#include "CoreMinimal.h"
#include "space/World/JTSPlanetEnemySettlement.h"
#include "space/Interaction/JTSMeleeTarget.h"
#include "JTSPlanetEcologyHabitat.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UJTSHealthComponent;
class UBoxComponent;
class UJTSPlanetSurfaceScatterComponent;
class AJTSPlanetSurfaceAttachment;

UENUM(BlueprintType)
enum class EJTSHabitatLight : uint8 { Any, Sunlit, Shadow };

/** An irregular core and a much rarer fringe, using the shared settlement spawning template. */
UCLASS(Blueprintable)
class SPACE_API AJTSPlanetEcologyHabitat : public AJTSPlanetEnemySettlement, public IJTSMeleeTarget
{
	GENERATED_BODY()
public:
	AJTSPlanetEcologyHabitat();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Habitat") TObjectPtr<UJTSPlanetSurfaceScatterComponent> SurfaceScatter;
	virtual bool CanReceiveMeleeHit_Implementation(APawn* Pawn) const override;
	virtual void ReceiveMeleeHit_Implementation(APawn* Pawn, EJTSMeleeAttackType Type) override;
	virtual FText GetMeleeTargetDisplayName_Implementation() const override;
	virtual FText GetMeleeTargetPrompt_Implementation(APawn* Pawn) const override;
	virtual FVector GetMeleeTargetAnchorWorldLocation_Implementation() const override;
	UPROPERTY(EditAnywhere, Category="Habitat") bool bDestructibleEntrance = false;
	/** Do not create residents unless a live refuge for their species exists. */
	UPROPERTY(EditAnywhere, Category="Habitat") bool bRequireRefuge = false;
	bool ContainsPoint(FVector Point, const AJTSPlanetAnchor* Planet, bool bCoreOnly = false) const;
	FVector FindRefuge(FVector Point) const;
	AActor* FindRefugeActor(FVector Point) const;
	bool IsRefugeAvailable() const { return !bCollapsed; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Habitat") TObjectPtr<UStaticMeshComponent> RefugeMesh;
	/** Optional closed cutter paired with this level-authored entrance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Habitat") TObjectPtr<UStaticMesh> RefugeCutVolume;
	AJTSPlanetSurfaceAttachment* CreateEntranceAttachment(AJTSPlanetAnchor* Planet);
	UPROPERTY(EditInstanceOnly, Category="Habitat") TArray<TObjectPtr<AActor>> Refuges;
	UPROPERTY(EditInstanceOnly, Category="Habitat") TArray<TObjectPtr<AJTSPlanetEcologyHabitat>> RefugeProviders;
	UPROPERTY(EditInstanceOnly, Category="Habitat") TArray<TObjectPtr<AActor>> FoodPoints;
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
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	virtual bool AcceptSpawnPoint_Implementation(FVector GroundPoint, AJTSPlanetAnchor* Planet) const override;
	virtual void ConfigureSpawnedEnemy_Implementation(AActor* Enemy) override;
private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UJTSHealthComponent> EntranceHealth;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> EntranceTarget;
	UPROPERTY(Transient) TObjectPtr<AJTSPlanetSurfaceAttachment> EntranceAttachment;
	UPROPERTY(ReplicatedUsing=ApplyCollapsed) bool bCollapsed = false;
	UFUNCTION() void ApplyCollapsed();
	UFUNCTION() void OnEntranceDestroyed(AController* InstigatorController, AActor* Causer);
	float NormalizedDistance(FVector Point, const AJTSPlanetAnchor* Planet) const;
	TWeakObjectPtr<AJTSPlanetAnchor> EcologyPlanet;
};
