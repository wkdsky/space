// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "space/World/JTSPlanetLandingTypes.h"

#include "JTSSpacecraftFlightMovementComponent.generated.h"

class AJTSPlanetAnchor;

/** Tuning values shared by the arcade flight model and future spacecraft upgrades. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSSpacecraftFlightStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxForwardSpeed = 3500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxReverseSpeed = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StrafeSpeed = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VerticalSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Acceleration = 4500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Deceleration = 5500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BrakeStrength = 9000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float PitchRate = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float YawRate = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RollRate = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BoostMultiplier = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BoostAccelerationMultiplier = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BoostTurnMultiplier = 0.8f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJTSSpacecraftBoostStateChanged, bool, bIsBoosting);
DECLARE_MULTICAST_DELEGATE(FOnJTSSpacecraftAssistedLandingCompleted);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnJTSSpacecraftAssistedLandingFailed, EJTSLandingValidationFailure);

/** Lightweight target-velocity flight movement for the persistent spacecraft Pawn. */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSSpacecraftFlightMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	UJTSSpacecraftFlightMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetForwardInput(float Value);
	void SetStrafeInput(float Value);
	void SetVerticalInput(float Value);
	void SetRollInput(float Value);
	void AddYawInput(float Value);
	void AddPitchInput(float Value);
	void SetBoosting(bool bNewBoosting);
	void SetBraking(bool bNewBraking);
	void ClearInput();

	/**
	 * Starts a ground-probe-driven landing. The movement component continuously resolves real surface
	 * data rather than interpolating to a fixed transform authored in level space.
	 */
	bool BeginAssistedLanding(AJTSPlanetAnchor* Planet, float LandingClearance, float DurationSeconds);

	/** Ends an unfinished controlled landing without reporting a successful touchdown. */
	void CancelAssistedLanding();

	UFUNCTION(BlueprintPure, Category = "Flight")
	bool IsBoosting() const;

	UFUNCTION(BlueprintPure, Category = "Flight")
	bool IsAssistedLanding() const;

	UFUNCTION(BlueprintPure, Category = "Flight")
	float GetCurrentSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Flight")
	float GetSpeedNormalized() const;

	UFUNCTION(BlueprintPure, Category = "Flight")
	float GetThrottleNormalized() const;

	UFUNCTION(BlueprintPure, Category = "Flight")
	FJTSSpacecraftFlightStats GetBaseStats() const;

	UFUNCTION(BlueprintPure, Category = "Flight")
	FJTSSpacecraftFlightStats GetEffectiveStats() const;

	/** Copies a future upgrade result into the runtime stats without changing movement algorithms. */
	UFUNCTION(BlueprintCallable, Category = "Flight|Upgrades")
	void SetEffectiveStats(const FJTSSpacecraftFlightStats& NewEffectiveStats);

	UFUNCTION(BlueprintCallable, Category = "Flight|Upgrades")
	void ResetEffectiveStats();

	/** The active planet is intentionally exposed as a reference, not a Moon-specific dependency. */
	void SetTargetPlanet(AJTSPlanetAnchor* NewTargetPlanet);

	UPROPERTY(BlueprintAssignable, Category = "Flight")
	FOnJTSSpacecraftBoostStateChanged OnBoostStateChanged;

	FOnJTSSpacecraftAssistedLandingCompleted OnAssistedLandingCompleted;
	FOnJTSSpacecraftAssistedLandingFailed OnAssistedLandingFailed;

protected:
	/** Base tuning is stable; EffectiveStats is the runtime upgrade-adjusted copy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Stats")
	FJTSSpacecraftFlightStats BaseStats;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Flight|Stats")
	FJTSSpacecraftFlightStats EffectiveStats;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Handling", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float InertialDampeningRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Input", meta = (ClampMin = "0.001", UIMin = "0.001"))
	float MouseLookSensitivity = 0.08f;

	/** Fallback desired descent duration used to derive a controlled initial vertical speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float DefaultLandingDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AssistedLandingMinimumDescentSpeed = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AssistedLandingMaximumDescentSpeed = 700.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float AssistedLandingRotationInterpolationSpeed = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AssistedLandingVelocityResponse = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LandingContactTolerance = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "0.0", ClampMax = "90.0", UIMin = "0.0", UIMax = "45.0"))
	float LandingCompletionAlignmentDegrees = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AssistedLandingTimeout = 20.0f;

	/** While a planet is bound, normal flight receives the planet's radial acceleration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Gravity", meta = (AllowPrivateAccess = "true"))
	bool bApplyPlanetaryGravity = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Gravity", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float MaximumPlanetGravitySpeed = 12000.0f;

private:
	void TickAssistedLanding(float DeltaTime);
	void TickFlight(float DeltaTime);
	void ApplyPlanetGravity(float DeltaTime);
	void UpdateRotation(float DeltaTime);
	void SubmitExteriorAltitude();
	void CompleteAssistedLanding();
	void FailAssistedLanding(EJTSLandingValidationFailure Failure);
	bool MoveWithCollisionSweep(const FVector& Delta, const FQuat& NewRotation, FHitResult& OutHit);

	FVector BuildTargetVelocity() const;
	float GetAccelerationRate() const;
	void SetBoostState(bool bNewBoosting);

	FVector InputVector = FVector::ZeroVector;
	float RollInput = 0.0f;
	float PendingYawInput = 0.0f;
	float PendingPitchInput = 0.0f;
	bool bBoosting = false;
	bool bBraking = false;
	bool bAssistedLanding = false;
	float AssistedLandingClearance = 0.0f;
	float AssistedLandingDescentSpeed = 0.0f;
	float AssistedLandingElapsed = 0.0f;
	TWeakObjectPtr<AJTSPlanetAnchor> TargetPlanet;
};
