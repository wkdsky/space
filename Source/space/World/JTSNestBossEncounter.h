#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JTSNestBossEncounter.generated.h"
class AJTSPlanetAnchor;
class AJTSPlanetEcologyHabitat;
class AJTSPlanetBoss;
class UProceduralMeshComponent;
class UMaterialInterface;
class AJTSExpeditionKeyPickup;

/** Level-authored encounter: independent surface ridges and one authoritative nest-clear trigger. */
UCLASS(Blueprintable)
class SPACE_API AJTSNestBossEncounter : public AActor
{
	GENERATED_BODY()
public:
	AJTSNestBossEncounter();
	UPROPERTY(EditInstanceOnly, Category="Encounter") TObjectPtr<AJTSPlanetAnchor> Planet;
	UPROPERTY(EditInstanceOnly, Category="Encounter") TArray<TObjectPtr<AJTSPlanetEcologyHabitat>> Nests;
	UPROPERTY(EditAnywhere, Category="Encounter") TSubclassOf<AJTSPlanetBoss> BossClass;
	UPROPERTY(EditAnywhere, Category="Encounter") float WakeDelay = 1.6f;
	UPROPERTY(EditAnywhere, Category="Encounter|Reward") FName EncounterId;
	UPROPERTY(EditAnywhere, Category="Encounter|Reward") TSubclassOf<AJTSExpeditionKeyPickup> RewardClass;
	UPROPERTY(EditAnywhere, Category="Encounter|Reward") float RewardForwardOffset = 450;
	/** Only an endpoint reference. No ring geometry or collision is generated. */
	UPROPERTY(EditAnywhere, Category="Encounter|Ridges", meta=(ClampMin="1")) float EndRingRadius = 650;
	UPROPERTY(EditAnywhere, Category="Encounter|Ridges") float RidgeHalfWidth = 65;
	UPROPERTY(EditAnywhere, Category="Encounter|Ridges") float RidgeHeight = 3;
	UPROPERTY(EditAnywhere, Category="Encounter|Ridges") TObjectPtr<UMaterialInterface> RidgeMaterial;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UProceduralMeshComponent> Ridges;
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly) TObjectPtr<AJTSPlanetBoss> ActiveBoss;
	UFUNCTION(CallInEditor, BlueprintCallable, Category="Encounter") void RebuildRidges();
	UFUNCTION(BlueprintPure, Category="Encounter") bool AreAllNestsDestroyed() const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
	void CheckNests();
	void WakeBoss();
	UFUNCTION() void OnBossDefeated(AController* InstigatorController, AActor* Causer);
	void SpawnReward();
	UPROPERTY() TObjectPtr<AJTSExpeditionKeyPickup> ActiveReward;
	FVector RewardOrigin = FVector::ZeroVector;
	bool bTriggered = false;
	FTimerHandle CheckTimer;
	FTimerHandle WakeTimer;
	FTimerHandle RewardTimer;
};
