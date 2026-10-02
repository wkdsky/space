#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mass/EntityHandle.h"
#include "space/Interaction/JTSCriticalHitTarget.h"
#include "space/Systems/JTSPlanetEnemyFragments.h"
#include "space/World/JTSPlanetSettlementEnemy.h"
#include "JTSMoonCubeEnemy.generated.h"

class AController;
class AJTSCharacter;
class AJTSPlanetAnchor;
class UBoxComponent;
class UJTSHealthComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UWidgetComponent;

/** Replicated hit target and presentation for one server-owned Mass enemy entity. */
UCLASS(Blueprintable)
class SPACE_API AJTSMoonCubeEnemy : public AActor, public IJTSCriticalHitTarget, public IJTSPlanetSettlementEnemy
{
	GENERATED_BODY()

public:
	AJTSMoonCubeEnemy();
	virtual bool InitializeForSettlement_Implementation(AJTSPlanetAnchor* Planet,
		FVector HomeLocation, FVector GroundLocation) override;
	virtual void OnSettlementAttackStarted_Implementation() override;
	void ConfigurePresentation(UStaticMesh* InBodyMesh, UStaticMesh* InWeakPointMesh,
		UMaterialInterface* InBodyMaterial, UMaterialInterface* InWeakPointMaterial);
	virtual float GetCriticalHitMultiplier(const FHitResult& Hit) const override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Moon|Cube")
	UJTSHealthComponent* GetHealthComponent() const { return HealthComponent; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void ShowHealthBar(float CurrentHealth, float MaxHealth);
	void HideHealthBar();
	void ResetVisualPulse();
	UFUNCTION()
	void OnRep_PresentationAssets();
	UFUNCTION()
	void HandleDamaged(float CurrentHealth, float MaxHealth, float Damage, AActor* DamageCauser);
	UFUNCTION()
	void HandleDeath(AController* InstigatorController, AActor* DamageCauser);
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDamageFeedback(float CurrentHealth, float InMaxHealth, float Damage,
		FVector_NetQuantize HitLocation, bool bCritical);
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastAttackPulse();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Cube", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> BodyCollider;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Cube", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> VisualRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Cube", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BodyMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Cube|Weak Point", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> WeakPointCollider;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Cube|Weak Point", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> WeakPointMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Cube|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSHealthComponent> HealthComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Cube|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> HealthBar;
	UPROPERTY(ReplicatedUsing = OnRep_PresentationAssets)
	TObjectPtr<UStaticMesh> BodyMeshAsset;
	UPROPERTY(ReplicatedUsing = OnRep_PresentationAssets)
	TObjectPtr<UStaticMesh> WeakPointMeshAsset;
	UPROPERTY(ReplicatedUsing = OnRep_PresentationAssets)
	TObjectPtr<UMaterialInterface> BodyMaterial;
	UPROPERTY(ReplicatedUsing = OnRep_PresentationAssets)
	TObjectPtr<UMaterialInterface> WeakPointMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Cube|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	float MaxHealth = 1000.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Cube|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	float CriticalMultiplier = 2.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Cube|Behavior", meta = (AllowPrivateAccess = "true"))
	FJTSPlanetEnemyBehavior Behavior;

	FMassEntityHandle EnemyEntity;
	TWeakObjectPtr<AJTSCharacter> DamageAttacker;
	FVector CurrentDamageHitLocation = FVector::ZeroVector;
	FVector SmoothedVisualLocation = FVector::ZeroVector;
	FQuat SmoothedVisualRotation = FQuat::Identity;
	bool bCurrentDamageCritical = false;
	FTimerHandle HealthBarTimer;
	FTimerHandle VisualPulseTimer;
};
