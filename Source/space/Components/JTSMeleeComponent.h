#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Interaction/JTSMeleeTarget.h"

#include "JTSMeleeComponent.generated.h"

class AActor;
class APawn;

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
	void SetCurrentMeleeTarget(AActor* NewTarget);

	/** Mirrors interaction refresh cadence without adding Character Tick work. */
	UPROPERTY(EditDefaultsOnly, Category = "Melee", meta = (ClampMin = "0.03", UIMin = "0.03"))
	float TargetRefreshInterval = 0.075f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Melee", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> CurrentMeleeTarget;

	FTimerHandle TargetRefreshTimerHandle;
	double NextAttackTime = 0.0;
};
