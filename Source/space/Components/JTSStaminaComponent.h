#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSStaminaComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FJTSOnStaminaChanged, float, CurrentStamina, float, MaxStamina);

/** Server-owned endurance shared by sprinting and free-hand wall climbing. */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSStaminaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr float CriticalFraction = 0.25f;

	UJTSStaminaComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Stamina")
	float GetCurrentStamina() const { return CurrentStamina; }
	UFUNCTION(BlueprintPure, Category = "Stamina")
	float GetMaxStamina() const { return MaxStamina; }
	UFUNCTION(BlueprintPure, Category = "Stamina")
	bool IsExhausted() const { return bExhausted; }
	UFUNCTION(BlueprintPure, Category = "Stamina")
	bool CanSprint() const { return !bExhausted && CurrentStamina > KINDA_SMALL_NUMBER; }
	UFUNCTION(BlueprintPure, Category = "Stamina")
	bool CanClimb() const
	{
		return !bExhausted && MaxStamina > KINDA_SMALL_NUMBER
			&& CurrentStamina > MaxStamina * CriticalFraction
			&& CurrentStamina >= MinimumClimbStartStamina;
	}

	/** Server-only: applies the ability rank and grants the newly added capacity on upgrade. */
	void RefreshCapacity();
	/** Server-only: continuous sprint intent supplied by the owning character RPC. */
	void SetSprintRequested(bool bRequested);
	/** Server-only: returns false when the requested action cannot be paid for. */
	bool Spend(float Amount);
	void Restore(float Amount);

	UPROPERTY(BlueprintAssignable, Category = "Stamina")
	FJTSOnStaminaChanged OnStaminaChanged;

private:
	UFUNCTION()
	void OnRep_Stamina();
	void PublishChange();

	UPROPERTY(ReplicatedUsing = OnRep_Stamina, VisibleInstanceOnly, Category = "Stamina")
	float CurrentStamina = 100.0f;
	UPROPERTY(ReplicatedUsing = OnRep_Stamina, VisibleInstanceOnly, Category = "Stamina")
	float MaxStamina = 100.0f;
	UPROPERTY(ReplicatedUsing = OnRep_Stamina)
	bool bExhausted = false;
	bool bSprintRequested = false;

	UPROPERTY(EditDefaultsOnly, Category = "Stamina", meta = (ClampMin = "0"))
	float SprintDrainPerSecond = 8.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Stamina", meta = (ClampMin = "0"))
	float RecoveryPerSecond = 15.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Stamina", meta = (ClampMin = "0"))
	float RecoveryDelaySeconds = 0.7f;
	UPROPERTY(EditDefaultsOnly, Category = "Stamina", meta = (ClampMin = "1"))
	float MinimumClimbStartStamina = 10.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Stamina", meta = (ClampMin = "0", ClampMax = "1"))
	float ExhaustionRecoveryFraction = 0.18f;
	float RecoveryDelayRemaining = 0.0f;
};
