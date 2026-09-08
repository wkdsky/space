#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "JTSHealthComponent.generated.h"

class AActor;
class AController;
class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FJTSOnHealthChanged, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FJTSOnDamaged, float, CurrentHealth, float, MaxHealth, float, Damage, AActor*, DamageCauser);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FJTSOnDeath, AController*, InstigatorController, AActor*, DamageCauser);

/**
 * Reusable authoritative health state for gameplay actors. It also listens to UE's standard
 * TakeAnyDamage path, so weapons and environmental damage can share the same entry point.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSHealthComponent();

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthNormalized() const;

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const;

	/** Applies authoritative damage and returns the health amount actually removed. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float ApplyDamage(float Damage, AController* InstigatorController, AActor* DamageCauser);

	/** Heals a living owner and returns the health amount actually restored. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float Heal(float Amount);

	/** Configures a spawn/default health pool. Dead actors are intentionally not revived by this function. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetMaxHealth(float NewMaxHealth, bool bFillHealth = true);

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FJTSOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FJTSOnDamaged OnDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FJTSOnDeath OnDeath;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void HandleOwnerTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void OnRep_MaxHealth();

	UFUNCTION()
	void OnRep_CurrentHealth();

	UFUNCTION()
	void OnRep_IsDead();

	bool CanModifyHealth() const;
	void BroadcastHealthChanged();
	void BroadcastDeathOnce(AController* InstigatorController, AActor* DamageCauser);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Health", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentHealth, Category = "Health", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_IsDead, Category = "Health", meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false;

	bool bDeathBroadcasted = false;
};
