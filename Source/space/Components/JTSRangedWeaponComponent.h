// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

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

	UFUNCTION(BlueprintCallable, Category = "Ranged")
	void StartFire();

	UFUNCTION(BlueprintCallable, Category = "Ranged")
	void StopFire();

	UFUNCTION(Server, Reliable)
	void ServerStartFire();

	UFUNCTION(Server, Reliable)
	void ServerStopFire();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShotTrace(FVector_NetQuantize TraceStart, FVector_NetQuantize TraceEnd);

private:
	const UJTSItemDefinition* GetActiveRangedDefinition() const;
	bool FireOnce();
	bool GetAim(FVector& OutOrigin, FVector& OutDirection) const;
	void ScheduleAutomaticFire(const UJTSItemDefinition* Definition);
	void ClearFireTimer();

	FTimerHandle AutomaticFireTimerHandle;
	bool bFireHeld = false;
	bool bDebugShotTraces = false;
};
