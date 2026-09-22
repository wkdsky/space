// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "space/World/JTSPlanetLandingTypes.h"

#include "JTSSpacecraftFlightMovementComponent.generated.h"

class AJTSPlanetAnchor;

/** Tuning values shared by the third-person flight model and future spacecraft upgrades. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSSpacecraftFlightStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxMoveSpeed = 3500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LiftSpeed = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Acceleration = 4500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Deceleration = 5500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BrakeStrength = 9000.0f;

	/** Maximum automatic facing change in degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FacingTurnRate = 180.0f;

	/** Angular acceleration used to remove fixed-step, staircase-looking spacecraft turns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FacingTurnAcceleration = 1080.0f;

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
DECLARE_MULTICAST_DELEGATE_OneParam(FOnJTSSpacecraftAssistedLandingPhaseChanged, EJTSSpacecraftLandingAssistPhase);

/** Camera-relative target-velocity movement for the persistent spacecraft Pawn. */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSSpacecraftFlightMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	UJTSSpacecraftFlightMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetMoveInput(const FVector2D& Value);
	void SetVerticalInput(float Value);
	void SetViewForward(const FVector& Value);
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

	/** True while movement uses the active planet's radial-up and surface-tangent frame. */
	UFUNCTION(BlueprintPure, Category = "Flight")
	bool IsUsingPlanetSurfaceFlightFrame() const;

	/** Continuous strength of real-surface stabilization: one near terrain, zero in free flight. */
	UFUNCTION(BlueprintPure, Category = "Flight")
	float GetSurfaceFlightAssistAlpha() const;

	/** Returns the latest real-mesh altitude sample when it belongs to Planet. */
	bool GetResolvedSurfaceAltitude(const AJTSPlanetAnchor* Planet, float& OutAltitude) const;

	/** Stable inertial/surface-blended Up shared by movement, camera and HUD consumers. */
	FVector GetReferenceUp() const;

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

	/** Captures the fixed free-flight horizon at a semantic vehicle state transition such as takeoff. */
	void CaptureInertialReferenceUp(const FVector& NewReferenceUp);

	UPROPERTY(BlueprintAssignable, Category = "Flight")
	FOnJTSSpacecraftBoostStateChanged OnBoostStateChanged;

	FOnJTSSpacecraftAssistedLandingCompleted OnAssistedLandingCompleted;
	FOnJTSSpacecraftAssistedLandingFailed OnAssistedLandingFailed;
	/** Kept separate from the completion event so the pawn can replicate concise landing status to every client. */
	FOnJTSSpacecraftAssistedLandingPhaseChanged OnAssistedLandingPhaseChanged;

protected:
	/** Base tuning is stable; EffectiveStats is the runtime upgrade-adjusted copy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Stats")
	FJTSSpacecraftFlightStats BaseStats;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Flight|Stats")
	FJTSSpacecraftFlightStats EffectiveStats;

	/** Small input values are discarded before building a camera-relative movement direction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Handling", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.25"))
	float MovementDeadZone = 0.05f;

	/** Enables the continuously blended real-surface flight assist. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Handling")
	bool bUsePlanetSurfaceFlightFrame = true;

	/** Fraction of TakeoffTransitionAltitude below which surface stabilization is fully engaged. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Surface Assist", meta = (ClampMin = "0.0", ClampMax = "0.95", UIMin = "0.0", UIMax = "0.75"))
	float SurfaceAssistFullStrengthAltitudeRatio = 0.60f;

	/** Maximum nose-down attitude at full low-altitude assist; nose-up attitude remains unrestricted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Surface Assist", meta = (ClampMin = "0.0", ClampMax = "89.0", UIMin = "0.0", UIMax = "30.0"))
	float MaximumSurfaceDiveAngleDegrees = 8.0f;

	/** Real-mesh radial trace cadence. One shared craft makes this much cheaper than a trace every frame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Surface Assist", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "0.25"))
	float SurfaceProximityProbeInterval = 0.05f;

	/** Forces a fresh sample after a large move, including teleport and server correction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Surface Assist", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float SurfaceProximityProbeDistance = 300.0f;

	/** Extra free-flight clearance kept between the physical hull and real terrain. LandingAssist is exempt. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Surface Assist", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SurfaceClearanceSafetyMargin = 120.0f;

	/** Time horizon used to cap descent before the hull reaches its terrain clearance envelope. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Surface Assist", meta = (ClampMin = "0.1", UIMin = "0.1", UIMax = "3.0"))
	float SurfaceClearanceLookAheadTime = 0.75f;

	/** Maximum automatic upward recovery speed if terrain or replication places the hull inside the envelope. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Surface Assist", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SurfaceClearanceRecoverySpeed = 450.0f;

	/** Fallback desired descent duration used to derive a controlled initial vertical speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float DefaultLandingDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AssistedLandingMinimumDescentSpeed = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AssistedLandingMaximumDescentSpeed = 700.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float AssistedLandingRotationInterpolationSpeed = 5.0f;

	/** Landing holds altitude until the local Z axis is this close to the real surface normal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "0.1", ClampMax = "45.0", UIMin = "0.1", UIMax = "15.0"))
	float AssistedLandingAlignmentToleranceDegrees = 4.0f;

	/** Temporary clearance used while rotating the complete hull into its landing attitude. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AssistedLandingAlignmentClearance = 75.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AssistedLandingVelocityResponse = 1800.0f;

	/** The final metres slow smoothly to this cap instead of creeping for the whole descent. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AssistedLandingTouchdownSpeed = 85.0f;

	/** Height at which the controlled descent starts braking for a soft surface contact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AssistedLandingBrakingDistance = 220.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LandingContactTolerance = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "0.0", ClampMax = "90.0", UIMin = "0.0", UIMax = "45.0"))
	float LandingCompletionAlignmentDegrees = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AssistedLandingTimeout = 20.0f;

	/** Optional radial gravity scale. Zero models the craft's automatic hover compensation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Gravity", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float PlanetGravityScale = 0.0f;

private:
	void TickAssistedLanding(float DeltaTime);
	void TickFlight(float DeltaTime);
	void RefreshSurfaceProximity(float DeltaTime);
	void UpdateReferenceFrame();
	void ApplyPlanetGravity(float DeltaTime);
	void ApplySurfaceClearanceProtection();
	FQuat UpdateRotation(float DeltaTime, const FVector& ReferenceUp);
	FQuat BuildSurfaceFlightDesiredRotation(
		const FQuat& CurrentRotation,
		const FVector& SurfaceUp,
		float AssistAlpha) const;
	FQuat BuildFreeFlightDesiredRotation(const FQuat& CurrentRotation) const;
	FQuat InterpolateTowardRotation(float DeltaTime, const FQuat& CurrentRotation, const FQuat& DesiredRotation);
	FVector ConstrainForwardToSurfaceEnvelope(
		const FVector& DesiredForward,
		const FVector& SurfaceUp,
		float AssistAlpha) const;
	bool HasForwardFlightIntent() const;
	void SubmitExteriorAltitude();
	void CompleteAssistedLanding();
	void FailAssistedLanding(EJTSLandingValidationFailure Failure);
	void SetAssistedLandingPhase(EJTSSpacecraftLandingAssistPhase NewPhase);
	bool MoveWithCollisionSweep(const FVector& Delta, const FQuat& NewRotation, FHitResult& OutHit);

	void GetViewBasis(const FVector& ReferenceUp, FVector& OutForward, FVector& OutRight) const;
	FVector BuildTargetVelocity(const FVector& ReferenceUp) const;
	float GetAccelerationRate() const;
	void SetBoostState(bool bNewBoosting);

	FVector2D MoveInput = FVector2D::ZeroVector;
	float VerticalInput = 0.0f;
	FVector ViewForward = FVector::ForwardVector;
	FVector CurrentReferenceUp = FVector::UpVector;
	FVector InertialReferenceUp = FVector::UpVector;
	FVector CachedSurfaceNormal = FVector::UpVector;
	FVector CachedSurfaceProbeLocation = FVector::ZeroVector;
	float CurrentFacingTurnSpeedRadians = 0.0f;
	float SurfaceFlightAssistAlpha = 0.0f;
	float CachedSurfaceAltitude = 0.0f;
	float CachedSurfaceDockingHeight = 0.0f;
	float SurfaceProximityProbeElapsed = 0.0f;
	bool bBoosting = false;
	bool bBraking = false;
	bool bAssistedLanding = false;
	bool bInertialReferenceUpInitialized = false;
	bool bHasSurfaceProximity = false;
	float AssistedLandingClearance = 0.0f;
	float AssistedLandingDescentSpeed = 0.0f;
	float AssistedLandingElapsed = 0.0f;
	EJTSSpacecraftLandingAssistPhase AssistedLandingPhase = EJTSSpacecraftLandingAssistPhase::None;
	TWeakObjectPtr<AJTSPlanetAnchor> TargetPlanet;
};
