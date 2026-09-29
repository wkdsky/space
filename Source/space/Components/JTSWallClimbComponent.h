// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "JTSWallClimbComponent.generated.h"

class AJTSCharacter;

/**
 * Ice-axe traversal on surfaces steeper than the character can stand on.
 * The component owns the probe, the attach, and the stepped translation.
 * Inventory decides whether the axes are in hand. Animation only reads the phase.
 */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSWallClimbComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSWallClimbComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Climb")
	bool IsClimbing() const { return bClimbing; }

	UFUNCTION(BlueprintPure, Category = "Climb")
	float GetSwingAlpha() const { return SwingAlpha; }

	UFUNCTION(BlueprintPure, Category = "Climb")
	bool IsLeadHandLeft() const { return bLeadHandLeft; }

	UFUNCTION(BlueprintPure, Category = "Climb")
	FVector GetSurfaceNormal() const { return SurfaceNormal; }

	UFUNCTION(BlueprintPure, Category = "Climb")
	FVector GetStepDirection() const { return StepDirection; }

	/** Local owner intent. The server places the hands; a remote client only predicts the same step. */
	void SubmitClimbIntent(const FVector& WishDirection, bool bJump);

	/** Local owner presses into the facing. The server re-probes before the hands plant. */
	void TryAttachFromApproach();

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool HasIceAxesInHand() const;
	FVector GetGravityUp() const;
	bool ProbeClimbSurface(FHitResult& OutHit) const;
	bool IsSurfaceTooSteepToStand(const FVector& Normal) const;
	void BeginClimb(const FHitResult& Hit);
	void EndClimb();
	void StartStep(const FVector& WorldStep);
	void AdvanceStep(float DeltaTime);
	void FaceSurface();

	UFUNCTION(Server, Unreliable)
	void ServerSubmitClimbIntent(FVector_NetQuantizeNormal WishDirection, bool bJump);

	UFUNCTION(Server, Reliable)
	void ServerTryAttach();

	UFUNCTION()
	void OnRep_Climbing();

	UPROPERTY(ReplicatedUsing = OnRep_Climbing)
	bool bClimbing = false;

	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal SurfaceNormal = FVector::UpVector;

	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal StepDirection = FVector::UpVector;

	UPROPERTY(Replicated)
	bool bLeadHandLeft = true;

	UPROPERTY(Replicated)
	float StepStartWorldTime = -1.0f;

	float SwingAlpha = 0.0f;
	float StepElapsed = 0.0f;
	bool bStepActive = false;
	FVector StepStart = FVector::ZeroVector;
	FVector StepTarget = FVector::ZeroVector;
	float NextAttachRequestSeconds = -1.0f;
	float SavedGravityScale = 1.0f;
	bool bSavedGravityScale = false;

	UPROPERTY(EditDefaultsOnly, Category = "Climb", meta = (ClampMin = "20.0"))
	float StepDistance = 62.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Climb", meta = (ClampMin = "0.12"))
	float StepSeconds = 0.42f;

	UPROPERTY(EditDefaultsOnly, Category = "Climb", meta = (ClampMin = "20.0"))
	float ProbeDistance = 78.0f;
};
