// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Items/JTSItemTypes.h"

#include "JTSRangedWeaponComponent.generated.h"

class UJTSItemDefinition;

/**
 * Minimal server-authoritative hitscan path for item definitions with RangedWeapon capability.
 * Ammunition is intentionally infinite for the prototype; damage and mining work remain separate.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSRangedWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSRangedWeaponComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Ranged")
	bool HasActiveRangedWeapon() const;

	UFUNCTION(BlueprintPure, Category = "Ranged|Aim")
	bool IsAiming() const { return bIsAiming && HasActiveRangedWeapon() && CanUseWeapon(); }

	/** True while the owner is holding the fire button, including the local predicted hold. */
	UFUNCTION(BlueprintPure, Category = "Ranged")
	bool IsFireHeld() const { return bFireHeld; }

	UFUNCTION(BlueprintPure, Category = "Ranged|Aim")
	float GetActiveAimFOV() const;

	UFUNCTION(BlueprintCallable, Category = "Ranged|Aim")
	void StartAim();

	UFUNCTION(BlueprintCallable, Category = "Ranged|Aim")
	void StopAim();
	/** Clears held fire and ADS on each local copy when the character grips a wall. */
	void CancelForClimb();

	UFUNCTION(BlueprintCallable, Category = "Ranged")
	void StartFire();

	UFUNCTION(BlueprintCallable, Category = "Ranged")
	void StopFire();

	UFUNCTION(Server, Reliable)
	void ServerStartFire();

	UFUNCTION(Server, Reliable)
	void ServerStopFire();

	UFUNCTION(Server, Reliable)
	void ServerStartAim();

	UFUNCTION(Server, Reliable)
	void ServerStopAim();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShotTrace(FVector_NetQuantize MuzzleStart, FVector_NetQuantize TraceEnd,
		FVector_NetQuantizeNormal ImpactNormal, EJTSItemId ShotItem, bool bHitSomething, bool bDamageableHit,
		bool bCritical);

	UFUNCTION(Client, Unreliable)
	void ClientConfirmRangedHit(bool bCritical);

	UFUNCTION(BlueprintPure, Category = "Ranged|Feedback")
	float GetReticleKickAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Ranged|Feedback")
	bool HasRecentConfirmedHit() const;

	UFUNCTION(BlueprintPure, Category = "Ranged|Feedback")
	bool HasRecentConfirmedCriticalHit() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	const UJTSItemDefinition* GetActiveRangedDefinition() const;
	bool CanUseWeapon() const;
	bool FireOnce();
	void PlayLocalShotFeedback(const UJTSItemDefinition* Definition);
	/** Repeats a held trigger at the active item's configured fire interval. */
	void ScheduleHeldFire(const UJTSItemDefinition* Definition);
	void ClearFireTimer();

	FTimerHandle AutomaticFireTimerHandle;
	bool bFireHeld = false;
	bool bNextLeftServerShot = false;
	bool bNextLeftFeedbackShot = false;
	bool bDebugShotTraces = false;
	double NextFireTimeSeconds = 0.0;
	double NextLocalFeedbackTimeSeconds = 0.0;
	double LastLocalShotSeconds = -100.0;
	double LastConfirmedHitSeconds = -100.0;
	double LastConfirmedCriticalHitSeconds = -100.0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Ranged|Aim", meta = (AllowPrivateAccess = "true"))
	bool bIsAiming = false;
};
