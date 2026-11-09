#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSColonyDefenseComponent.generated.h"

class AJTSPlanetAnchor;
class AJTSPlanetEnemySettlement;
class AJTSPlanetSurfaceCreature;

/** Schedules an indestructible colony's finite residents; settlement recipes own spawning and recovery. */
UCLASS(ClassGroup=(Planet), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSColonyDefenseComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSColonyDefenseComponent();
	void Initialize(AJTSPlanetAnchor* Planet, AJTSPlanetEnemySettlement* Workers, AJTSPlanetEnemySettlement* Soldiers);
	void RegisterResident(AJTSPlanetSurfaceCreature* Creature, const AJTSPlanetEnemySettlement* Settlement);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Colony") void RaiseAlarm(APawn* Attacker);
	UFUNCTION(BlueprintPure, Category="Colony") int32 GetLivingWorkers() const;
	UFUNCTION(BlueprintPure, Category="Colony") int32 GetLivingSoldiers() const;
	UFUNCTION(BlueprintPure, Category="Colony") int32 GetOutsideWorkers() const;
	UFUNCTION(BlueprintPure, Category="Colony") int32 GetOutsideSoldiers() const;
	UPROPERTY(EditAnywhere, Category="Colony|Population") FIntPoint RoamingWorkers = FIntPoint(8,10);
	UPROPERTY(EditAnywhere, Category="Colony|Defense") FIntPoint FightingWorkers = FIntPoint(2,3);
	UPROPERTY(EditAnywhere, Category="Colony|Defense") FIntPoint FightingSoldiers = FIntPoint(4,6);
	UPROPERTY(EditAnywhere, Category="Colony|Defense", meta=(ClampMin="100")) float DefenseLeash = 1800;
	UPROPERTY(EditAnywhere, Category="Colony|Defense", meta=(ClampMin="0.1")) float EmergenceInterval = 0.4f;
	/** Asset-local ground entrances and outward clearance, authored with the selected tower mesh. */
	UPROPERTY(EditAnywhere, Category="Colony|Entrances") TArray<FVector> EntranceOffsets;
	UPROPERTY(EditAnywhere, Category="Colony|Entrances", meta=(ClampMin="10")) float ExitDistance = 80;
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void ServiceColony();
	bool Release(AJTSPlanetSurfaceCreature* Creature);
	FVector EntranceFor(const AJTSPlanetSurfaceCreature* Creature) const;
	TWeakObjectPtr<AJTSPlanetAnchor> ActivePlanet;
	TWeakObjectPtr<AJTSPlanetEnemySettlement> WorkerSettlement;
	TWeakObjectPtr<AJTSPlanetEnemySettlement> SoldierSettlement;
	TArray<TWeakObjectPtr<AJTSPlanetSurfaceCreature>> Workers;
	TArray<TWeakObjectPtr<AJTSPlanetSurfaceCreature>> Soldiers;
	TArray<TWeakObjectPtr<AJTSPlanetSurfaceCreature>> WorkerDefenders;
	TArray<TWeakObjectPtr<APawn>> Aggressors;
	FTimerHandle ServiceTimer;
	int32 RoamingTarget = 0;
	int32 SoldierTarget = 0;
	int32 EntranceSequence = 0;
	double NextEmergence = 0;
	bool bAlarmed = false;
};
