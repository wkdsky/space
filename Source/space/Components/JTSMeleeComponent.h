#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Interaction/JTSMeleeTarget.h"

#include "JTSMeleeComponent.generated.h"

class AActor;
class APawn;

/** Broad attack category used by animation and presentation code. */
UENUM(BlueprintType)
enum class EJTSAttackType : uint8
{
	Punch UMETA(DisplayName = "Punch"),
	MeleeWeapon UMETA(DisplayName = "Melee Weapon"),
	Tool UMETA(DisplayName = "Tool")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttackStarted, EJTSAttackType, AttackType);

/**
 * Owns the one camera-driven melee acquisition and attack path shared by first- and third-person views.
 * Moon GameMode remains the authority for range, aim forgiveness, and cooldown values.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSMeleeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSMeleeComponent();

	/** Refreshes the current target under the player's current camera aim. */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	void RefreshMeleeTarget();

	/** Records that the attack input was pressed and starts or buffers one punch as appropriate. */
	UFUNCTION(BlueprintCallable, Category = "Melee|Attack")
	void AttackPressed();

	/** Records that the attack input was released without interrupting the active animation. */
	UFUNCTION(BlueprintCallable, Category = "Melee|Attack")
	void AttackReleased();

	/** Starts one attack directly. Retained for Blueprint callers that do not use the player input path. */
	UFUNCTION(BlueprintCallable, Category = "Melee|Attack")
	void StartAttack();

	/** Immediately clears the attack state. Use this for interruption or explicit cancellation. */
	UFUNCTION(BlueprintCallable, Category = "Melee|Attack")
	void StopAttack();

	/** Consumes one held or buffered attack input at an attack montage's chain notify. */
	UFUNCTION(BlueprintCallable, Category = "Melee|Attack")
	void TryChainAttack();

	/** Finishes the current attack, or safely chains if input arrived after the chain notify. */
	UFUNCTION(BlueprintCallable, Category = "Melee|Attack")
	void FinishCurrentAttack();

	/** Performs the melee sphere trace for an animation hit frame. */
	UFUNCTION(BlueprintCallable, Category = "Melee|Attack")
	void PerformHitCheck();

	/** Bind a montage, weapon animation, or attack effects here without coupling this component to an Anim Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "Melee|Attack")
	FOnAttackStarted OnAttackStarted;

	/** Performs one Punch, Knife, or Axe action against the currently aimed valid target. */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	bool TryAttack();

	UFUNCTION(BlueprintPure, Category = "Melee")
	AActor* GetCurrentMeleeTarget() const;

	UFUNCTION(BlueprintPure, Category = "Melee")
	EJTSMeleeAttackType GetCurrentAttackType() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	AActor* FindBestMeleeTarget(APawn* AttackingPawn) const;
	bool IsValidMeleeTarget(AActor* Candidate, APawn* AttackingPawn) const;
	bool IsMoonMeleeAvailable() const;
	EJTSAttackType ResolveAttackType() const;
	void BeginAttack(EJTSAttackType AttackType);
	void ResetAttackFailSafeTimer();
	void ClearAttackFailSafeTimer();
	void HandleAttackFailSafeTimeout();
	void EndAttackState();
	void SetCurrentMeleeTarget(AActor* NewTarget);

	/** Mirrors interaction refresh cadence without adding Character Tick work. */
	UPROPERTY(EditDefaultsOnly, Category = "Melee", meta = (ClampMin = "0.03", UIMin = "0.03"))
	float TargetRefreshInterval = 0.075f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Melee", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> CurrentMeleeTarget;

	/** True while the attack input is held; this enables automatic chaining at attack-chain notifies. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Melee|Attack", meta = (AllowPrivateAccess = "true"))
	bool bAttackHeld = false;

	/** Stores at most one press received while an attack is already playing. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Melee|Attack", meta = (AllowPrivateAccess = "true"))
	bool bAttackBuffered = false;

	/** True while a punch montage is active and awaiting an attack-chain or attack-end notify. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Melee|Attack", meta = (AllowPrivateAccess = "true"))
	bool bIsAttacking = false;

	/** Last-resort timeout for missing or interrupted animation notifies; it never controls normal combo cadence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Attack", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float AttackFailSafeTime = 3.0f;

	/** Broad category resolved from the currently selected equipment when an attack starts. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Melee|Attack", meta = (AllowPrivateAccess = "true"))
	EJTSAttackType CurrentAttackType = EJTSAttackType::Punch;

	/** Forward distance, in centimeters, covered by the hit-frame sphere trace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AttackRange = 100.0f;

	/** Radius, in centimeters, of the hit-frame sphere trace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AttackRadius = 35.0f;

	/** Damage applied to the first blocking actor hit by the sphere trace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AttackDamage = 10.0f;

	FTimerHandle TargetRefreshTimerHandle;
	FTimerHandle AttackFailSafeTimerHandle;
	double NextAttackTime = 0.0;
};
