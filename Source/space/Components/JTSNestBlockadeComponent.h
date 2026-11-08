#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSNestBlockadeComponent.generated.h"

class APawn;
class UNiagaraSystem;
class AJTSPlanetSurfaceCreature;

/** Server-owned cooperative work, durable entrance identity, and its temporary defense cohort. */
UCLASS(ClassGroup=(Planet), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSNestBlockadeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSNestBlockadeComponent();
	UPROPERTY(EditAnywhere, Category="Blockade|Save") FName PersistentEntranceId;
	UPROPERTY(EditAnywhere, Category="Blockade|Work", meta=(ClampMin="0")) float DecayDelay = 0.8f;
	UPROPERTY(EditAnywhere, Category="Blockade|Work", meta=(ClampMin="0.01")) float DecayPerSecond = 0.18f;
	UPROPERTY(EditAnywhere, Category="Blockade|Work", meta=(ClampMin="0.05")) float TapWorkDuration = 0.2f;
	UPROPERTY(EditAnywhere, Category="Blockade|Defense", meta=(ClampMin="1", ClampMax="32")) int32 DefensePopulation = 10;
	UPROPERTY(EditAnywhere, Category="Blockade|Defense", meta=(ClampMin="410")) float DefenseLeashDistance = 1800;
	UPROPERTY(EditAnywhere, Category="Blockade|Defense", meta=(ClampMin="0.08")) float DefenderEmergenceInterval = 0.2f;
	/** All feeding areas sharing this refuge draw from the same finite population. */
	UPROPERTY(EditAnywhere, Category="Blockade|Population") bool bFinitePopulation = false;
	UPROPERTY(EditAnywhere, Category="Blockade|Population", meta=(ClampMin="0", ClampMax="512")) int32 PopulationCapacity = 14;
	UPROPERTY(EditAnywhere, Category="Blockade|Population", meta=(ClampMin="1")) float PopulationRecoveryDelay = 90;
	UPROPERTY(EditAnywhere, Category="Blockade|Population", meta=(ClampMin="1")) float PopulationRecoveryInterval = 12;
	UFUNCTION(BlueprintPure, Category="Blockade|Population") int32 GetRemainingPopulation() const { return bPopulationInitialized ? RemainingPopulation : PopulationCapacity; }
	UFUNCTION(BlueprintPure, Category="Blockade|Population") int32 GetReservePopulation() const { return FMath::Max(0, GetRemainingPopulation() - ResidentPopulation); }
	bool TryRegisterResident(AJTSPlanetSurfaceCreature* Creature);
	bool CanReleaseResident();
	void UnregisterResident(AJTSPlanetSurfaceCreature* Creature, bool bKilled);
	UPROPERTY(EditAnywhere, Category="Blockade|Effects") TObjectPtr<UNiagaraSystem> CollapseDust;
	UPROPERTY(EditAnywhere, Category="Blockade|Effects") FVector DustScale = FVector::OneVector;
	UPROPERTY(EditAnywhere, Category="Blockade|Effects", meta=(ClampMin="0")) float RubbleDelay = 0.45f;
	UFUNCTION(BlueprintPure, Category="Blockade") float GetProgress() const { return Progress; }
	UFUNCTION(BlueprintPure, Category="Blockade") bool HasRequiredTool(APawn* Pawn) const;
	bool CanWork(APawn* Pawn) const;
	void SetWorking(APawn* Pawn, bool bHeld);
	bool ApplyStrikeWork(APawn* Pawn);
	/** Keeps only actual attackers still inside this entrance's defensive radius. */
	APawn* FindDefenseAggressor();
	void RestorePersistentState();
	void ResetTransientState();
	UFUNCTION(NetMulticast, Reliable) void MulticastCollapseDust();
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
	void ServicePopulation();
	struct FWorker { bool bHeld = false; double PulseUntil = 0; double NextSwing = 0; float UnspentWork = 0; };
	void ServiceWork();
	void StartDefense();
	void UpdateDefense();
	UPROPERTY(Replicated) float Progress = 0;
	TMap<TWeakObjectPtr<APawn>, FWorker> Workers;
	TArray<TWeakObjectPtr<AJTSPlanetSurfaceCreature>> Defenders;
	TArray<TWeakObjectPtr<APawn>> DefenseAggressors;
	TArray<TWeakObjectPtr<AJTSPlanetSurfaceCreature>> Residents;
	UPROPERTY(Replicated) int32 RemainingPopulation = 0;
	UPROPERTY(Replicated) int32 ResidentPopulation = 0;
	UPROPERTY(Replicated) bool bPopulationInitialized = false;
	double NextPopulationRecovery = 0;
	bool bAlarmed = false;
	int32 PendingDefenders = 0;
	int32 DefenderSpawnSequence = 0;
	double NextDefenderEmergence = 0;
	double LastUpdate = 0;
	double LastWork = 0;
	FTimerHandle WorkTimer;
	FTimerHandle RubbleTimer;
};
