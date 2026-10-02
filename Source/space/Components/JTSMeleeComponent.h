#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Interaction/JTSMeleeTarget.h"

#include "JTSMeleeComponent.generated.h"

class AActor;
class APawn;
class USoundBase;

/** Broad attack category used by animation and presentation code. */
UENUM(BlueprintType)
enum class EJTSAttackType : uint8
{
	Punch UMETA(DisplayName = "Punch"),
	MeleeWeapon UMETA(DisplayName = "Melee Weapon"),
	Tool UMETA(DisplayName = "Tool")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttackStarted, EJTSAttackType, AttackType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttackFinished, EJTSAttackType, AttackType);

/**
 * Owns the one camera-driven melee acquisition and attack path shared by first- and third-person views.
 * Attack timing remains compatible with Moon GameMode; aim and damage tuning live on this component.
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

	/** True while the attack button is held, including the locally predicted hold on a client. */
	UFUNCTION(BlueprintPure, Category = "Melee|Attack")
	bool IsAttackInputHeld() const { return bAttackHeld; }

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

	/** Samples the server-side fist or held-weapon sweep at an animation hit frame. */
	UFUNCTION(BlueprintCallable, Category = "Melee|Attack")
	void PerformHitCheck();

	/** Input intent RPCs; hit targets are always acquired again on the authority. */
	UFUNCTION(Server, Reliable)
	void ServerStartAttack();

	UFUNCTION(Server, Reliable)
	void ServerPerformHitCheck();

	UFUNCTION(Server, Reliable)
	void ServerTryAttack();

	UFUNCTION(Server, Reliable)
	void ServerReleaseAttack();

	UFUNCTION(Client, Unreliable)
	void ClientConfirmPunchHit();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPunchImpact(FVector_NetQuantize Location);

	/** Presentation is multicasted after the server accepts each attack segment. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBeginAttackPresentation(EJTSAttackType AttackType, bool bUseLeftPunch, bool bIsComboContinuation);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastEndAttackPresentation(EJTSAttackType AttackType);

	/** Bind a montage, weapon animation, or attack effects here without coupling this component to an Anim Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "Melee|Attack")
	FOnAttackStarted OnAttackStarted;

	/** Broadcast after the server closes one attack segment so presentation can return to the base pose. */
	UPROPERTY(BlueprintAssignable, Category = "Melee|Attack")
	FOnAttackFinished OnAttackFinished;

	/** Starts one Punch, Knife, or Axe swing; contact is resolved by the server sweep. */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	bool TryAttack();

	UFUNCTION(BlueprintPure, Category = "Melee")
	AActor* GetCurrentMeleeTarget() const;

	UFUNCTION(BlueprintPure, Category = "Melee")
	EJTSMeleeAttackType GetCurrentAttackType() const;

	/** Damage is configured by the attacker/equipped weapon, never by the target receiving it. */
	UFUNCTION(BlueprintPure, Category = "Melee|Damage")
	float GetDamageForAttackType(EJTSMeleeAttackType AttackType) const;

	/** True while an empty-handed punch is in progress, including a held or buffered combo link. */
	UFUNCTION(BlueprintPure, Category = "Melee|Attack")
	bool IsUnarmedComboActive() const;

	/**
	 * 0 at rest, rising through the wind-up and peaking near the contact frame of a held-weapon swing.
	 * Presentation reads this so the arm chop stays on the same clock as the hit.
	 */
	UFUNCTION(BlueprintPure, Category = "Melee|Attack")
	float GetMeleeSwingPhase() const;

	/** Seconds since this punch's presentation began. Zero once the swing has been cleared. */
	UFUNCTION(BlueprintPure, Category = "Melee|Attack")
	float GetPunchVisualElapsed() const;
	float GetUnarmedPunchHitDelay() const { return FMath::Max(0.01f, UnarmedPunchHitDelay); }
	float GetUnarmedPunchChainDelay() const { return FMath::Max(GetUnarmedPunchHitDelay(), UnarmedPunchChainDelay); }
	float GetUnarmedPunchRecoveryDelay() const { return FMath::Max(GetUnarmedPunchChainDelay() + 0.01f, UnarmedPunchRecoveryDelay); }

	UFUNCTION(BlueprintPure, Category = "Melee|Feedback")
	float GetConfirmedPunchHitFeedbackAlpha() const;

	/** True while a punch presentation is still travelling or settling. */
	UFUNCTION(BlueprintPure, Category = "Melee|Attack")
	bool IsPunchVisualActive() const;

	/** Selects the alternating punch asset for the current unarmed swing. */
	UFUNCTION(BlueprintPure, Category = "Melee|Attack")
	bool IsCurrentPunchLeft() const;

	/** True only for the presentation callback of a punch that continues a preceding swing. */
	UFUNCTION(BlueprintPure, Category = "Melee|Attack")
	bool IsContinuingUnarmedCombo() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	AActor* FindBestMeleeTarget(APawn* AttackingPawn) const;
	bool FindBestAimCandidate(APawn* AttackingPawn, AActor*& OutTarget, FVector& OutTargetLocation, bool bRequireMeleeTargetInterface) const;
	bool FindBestPunchCandidate(APawn* AttackingPawn, AActor*& OutTarget, FVector& OutTargetLocation, bool bRequireMeleeTargetInterface, bool bRequireLineOfSight, float MaximumTargetRange) const;
	/** Analytic fist path for the current punch. Presentation and the server sweep share this so a hit does not wait on a rendered pose. */
	bool GetPunchFistPath(APawn* AttackingPawn, FVector& OutStart, FVector& OutEnd) const;
	void SweepHeldWeaponTip();
	FVector GetMeleeTargetAimPoint(AActor* Candidate) const;
	bool IsValidMeleeTarget(AActor* Candidate, APawn* AttackingPawn) const;
	bool IsValidDamageTarget(AActor* Candidate, APawn* AttackingPawn) const;
	bool GetPlayerAimView(APawn* AttackingPawn, FVector& OutCameraLocation, FVector& OutAimDirection) const;
	/** Held tools trace along the pawn's own facing. Punches keep the camera view. */
	bool GetHeldItemAimView(APawn* AttackingPawn, FVector& OutOrigin, FVector& OutAimDirection) const;
	bool IsWithinPunchRange(APawn* AttackingPawn, const FVector& TargetLocation) const;
	bool IsWithinMeleeRange(APawn* AttackingPawn, const FVector& TargetLocation, float MaximumRange) const;
	bool HasMeleeLineOfSight(APawn* AttackingPawn, AActor* Candidate, const FVector& TargetLocation) const;
	bool ApplyAttackToTarget(AActor* Target, APawn* AttackingPawn, EJTSMeleeAttackType AttackType);
	bool IsMoonMeleeAvailable() const;
	EJTSAttackType ResolveAttackType() const;
	void BeginAttack(EJTSAttackType AttackType);
	void ResetAttackFailSafeTimer();
	void ClearAttackFailSafeTimer();
	void HandleAttackFailSafeTimeout();
	void ScheduleUnarmedPunchEvents();
	void ClearUnarmedPunchTimers();
	void HandleUnarmedPunchHit();
	void SweepPunchFist();
	void HandleUnarmedPunchChainWindow();
	void HandleUnarmedPunchRecovery();
	void ScheduleHeldWeaponEvents();
	void ClearHeldWeaponTimers();
	void HandleHeldWeaponHit();
	void HandleHeldWeaponChainWindow();
	void HandleHeldWeaponRecovery();
	void EndAttackState();
	void SetCurrentMeleeTarget(AActor* NewTarget);

	/** Mirrors interaction refresh cadence without adding Character Tick work. */
	UPROPERTY(EditDefaultsOnly, Category = "Melee", meta = (ClampMin = "0.03", UIMin = "0.03"))
	float TargetRefreshInterval = 0.075f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Melee", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> CurrentMeleeTarget;

	/** One punch-only candidate acquired at swing start and revalidated at the attack-hit notify. */

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

	/** Server-authoritative hit frame for the native empty-hand punch loop. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Punch", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01"))
	float UnarmedPunchHitDelay = 0.11f;

	/** Held or buffered input starts the next alternating punch before the hands return to rest. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Punch", meta = (AllowPrivateAccess = "true", ClampMin = "0.02", UIMin = "0.02"))
	float UnarmedPunchChainDelay = 0.18f;

	/** Without more input, the final punch is allowed to finish and settle back to the lowered idle pose. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Punch", meta = (AllowPrivateAccess = "true", ClampMin = "0.03", UIMin = "0.03"))
	float UnarmedPunchRecoveryDelay = 0.35f;

	/** Selected by the player Blueprint; played for everyone at a confirmed punch impact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Punch|Feedback", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> PunchImpactSound;

	/** Light air movement cue for each accepted punch, including misses. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Punch|Feedback", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> PunchSwingSound;

	/** Brief local view response on confirmed contact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Punch|Feedback", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "4.0"))
	float PunchHitViewKickDegrees = 0.65f;

	/** Native timing fallback for held melee weapons and tools, so they do not depend on Blueprint animation notifies. */
	/** Lands with the visual strike: the raise into the first cut is about 0.22s and the cut itself is 0.15s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Weapon", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01"))
	float HeldWeaponHitDelay = 0.36f;

	/** One visual chop cycle later, so the next hit lines up with the next time the tool reaches the bottom. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Weapon", meta = (AllowPrivateAccess = "true", ClampMin = "0.02", UIMin = "0.02"))
	float HeldWeaponChainDelay = 0.52f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Weapon", meta = (AllowPrivateAccess = "true", ClampMin = "0.03", UIMin = "0.03"))
	float HeldWeaponRecoveryDelay = 0.68f;

	/** Broad category resolved from the currently selected equipment when an attack starts. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Melee|Attack", meta = (AllowPrivateAccess = "true"))
	EJTSAttackType CurrentAttackType = EJTSAttackType::Punch;

	/** Alternates left/right only while the empty-handed combo advances. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Melee|Attack", meta = (AllowPrivateAccess = "true"))
	bool bCurrentPunchUsesLeft = false;

	/** Set during the attack-start broadcast so presentation can blend an existing combo instead of restarting from rest. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Melee|Attack", meta = (AllowPrivateAccess = "true"))
	bool bCurrentPunchIsComboContinuation = false;

	/** Local presentation clock for a held-weapon chop. Starts when the multicast presentation begins. */
	bool bMeleeSwingClockActive = false;
	float MeleeSwingClockElapsed = 0.0f;

	/**
	 * Local presentation clock for one unarmed punch. One BeginAttack is one legal hit;
	 * a left-right visual cycle is two of those swings.
	 */
	bool bPunchVisualClockActive = false;
	float PunchVisualElapsed = 0.0f;
	double LastConfirmedPunchHitSeconds = -100.0;

	/** UI target preview distance; damage uses the fist or held-item contact sweep. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MeleeAimTraceDistance = 1200.0f;

	/** Radius of the held item's contact sweep around its authored tip. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MeleeAimAssistRadius = 15.0f;

	/** Actual contact reach measured from the player capsule, never from the third-person camera. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Punch", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PunchRange = 180.0f;

	/** Nearby target search radius used only when an unarmed punch begins or needs one reacquire. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Aim Assist", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PunchTargetAcquireRadius = 200.0f;

	/** Normal aim-cone tolerance for punchable targets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Aim Assist", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "89.0", UIMin = "0.0", UIMax = "45.0"))
	float GenericPunchAimAssistAngle = 8.0f;

	/** Small MoonAnts get a wider but still forward-facing punch aim cone. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Aim Assist", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "89.0", UIMin = "0.0", UIMax = "45.0"))
	float MoonAntPunchAimAssistAngle = 14.0f;

	/** Serialized compatibility for old punch target grace; contact now follows the fist sweep. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Punch damage now uses the fist sweep."))
	float CachedTargetGraceRange = 205.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Punch", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float PunchDamage = 1.0f;

	/** Retains the prototype's fast-kill Knife behavior while keeping damage owned by the attacker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Damage", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float KnifeDamage = 3.0f;

	/** Retains the prototype's fast-kill Axe behavior while keeping damage owned by the attacker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Damage", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AxeDamage = 3.0f;

	/** Punches are intentionally single-target; this remains one for the current prototype. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Punch", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "1", UIMin = "1", UIMax = "1"))
	int32 PunchMaxTargets = 1;

	/** Draws punch acquisition and final target checks only while a swing begins or resolves. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDebugMeleeAim = false;

	/** Serialized compatibility only. Replaced by MeleeAimTraceDistance and PunchRange. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use MeleeAimTraceDistance and PunchRange."))
	float AttackRange = 100.0f;

	/** Serialized compatibility only. Replaced by MeleeAimAssistRadius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use MeleeAimAssistRadius."))
	float AttackRadius = 35.0f;

	/** Serialized compatibility only. Replaced by PunchDamage, KnifeDamage, and AxeDamage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Legacy", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use the per-attack damage properties."))
	float AttackDamage = 10.0f;

	FTimerHandle TargetRefreshTimerHandle;
	FTimerHandle AttackFailSafeTimerHandle;
	FTimerHandle UnarmedPunchHitTimerHandle;
	FTimerHandle UnarmedPunchChainTimerHandle;
	FTimerHandle UnarmedPunchRecoveryTimerHandle;
	FTimerHandle HeldWeaponHitTimerHandle;
	FTimerHandle HeldWeaponChainTimerHandle;
	FTimerHandle HeldWeaponRecoveryTimerHandle;
	TSet<TWeakObjectPtr<AActor>> HitActorsThisSwing;
	FVector PreviousPunchSample = FVector::ZeroVector;
	FVector PreviousWeaponTip = FVector::ZeroVector;
	bool bHasPreviousPunchSample = false;
	bool bHasPreviousWeaponTip = false;
	double NextAttackTime = 0.0;
};
