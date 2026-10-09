// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "space/World/JTSPlanetLandingTypes.h"

#include "JTSSpacecraftFlightMovementComponent.generated.h"

class AJTSPlanetAnchor;
struct FJTSSurfaceEnvelopeFrame;

/** Tuning values shared by the third-person flight model and future spacecraft upgrades. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSSpacecraftFlightStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxMoveSpeed = 4200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LiftSpeed = 2800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Acceleration = 5200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Deceleration = 6200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BrakeStrength = 11000.0f;

	/** Maximum hull turn in degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FacingTurnRate = 110.0f;

	/** Angular acceleration of the hull turn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FacingTurnAcceleration = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BoostMultiplier = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BoostAccelerationMultiplier = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BoostTurnMultiplier = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0"))
	float StrafeSpeed = 2100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight", meta = (ClampMin = "0.0"))
	float ReverseSpeed = 1800.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJTSSpacecraftBoostStateChanged, bool, bIsBoosting);
DECLARE_MULTICAST_DELEGATE(FOnJTSSpacecraftAssistedLandingCompleted);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnJTSSpacecraftAssistedLandingFailed, EJTSLandingValidationFailure);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnJTSSpacecraftAssistedLandingPhaseChanged, EJTSSpacecraftLandingAssistPhase);

/** Server-authoritative six-axis flight, with real-surface assistance close to a planet. */
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
	void SetSteeringInput(const FVector2D& Value);
	void SetRollInput(float Value);
	void SetFlightAssistEnabled(bool bEnabled);
	void SetSpeedLimit(float NewLimit);
	UFUNCTION(BlueprintPure, Category = "Flight")
	float GetSpeedLimit() const { return SpeedLimit; }
	UFUNCTION(BlueprintPure, Category = "Flight")
	bool IsFlightAssistEnabled() const { return bFlightAssistEnabled; }
	/** T latches a 180-degree yaw in the current deck plane. Other thrust remains independent. */
	void SetTurnAround(bool bNewTurnAround);
	void SetBoosting(bool bNewBoosting);
	void SetBraking(bool bNewBraking);
	void ClearInput();

	/**
	 * Follows a terrain/gear-fitted pose with concurrent swept correction, alignment and descent.
	 * The support component rechecks that pose throughout the approach and at touchdown.
	 */
	bool BeginAssistedLanding(AJTSPlanetAnchor* Planet, const FTransform& LandingPose, float DurationSeconds);

	/** Ends an unfinished controlled landing without reporting a successful touchdown. */
	void CancelAssistedLanding();
	/** Takeoff/abort may leave the hull below the shell; lift out without landing recapture. */
	void BeginEnvelopeEscape(bool bRequireTakeoffClearance = false);
	bool IsAtEnvelopeBoundary() const;

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

	/** Stable inertial/radial Up for vertical thrust, camera and HUD; never levels the hull. */
	FVector GetReferenceUp() const;

	UFUNCTION(BlueprintPure, Category = "Flight")
	float GetCurrentSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Flight")
	float GetSpeedNormalized() const;

	UFUNCTION(BlueprintPure, Category = "Flight")
	float GetThrottleNormalized() const;

	/** Positive while the pilot is commanding lift. Space is +1, descend is negative. */
	UFUNCTION(BlueprintPure, Category = "Flight")
	float GetVerticalInput() const;

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

	/** Small input values are discarded before building a hull-relative movement direction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Handling", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.25"))
	float MovementDeadZone = 0.05f;

	/** Enables planet-relative vertical thrust; real-mesh clearance protection stays active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Handling")
	bool bUsePlanetSurfaceFlightFrame = true;

	/** Fraction of TakeoffTransitionAltitude below which radial lift is fully engaged. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Surface Assist", meta = (ClampMin = "0.0", ClampMax = "0.95", UIMin = "0.0", UIMax = "0.75"))
	float SurfaceAssistFullStrengthAltitudeRatio = 0.60f;

	/** Real-mesh radial trace cadence. One shared craft makes this much cheaper than a trace every frame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Surface Assist", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "0.25"))
	float SurfaceProximityProbeInterval = 0.05f;

	/** Forces a fresh sample after a large move, including teleport and server correction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Surface Assist", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float SurfaceProximityProbeDistance = 300.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Configure SurfaceEnvelopeComponent.Settings instead."))
	float SurfaceClearanceSafetyMargin = 120.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Configure SurfaceEnvelopeComponent.Settings instead."))
	float SurfaceClearanceLookAheadTime = 0.75f;

	/** Maximum automatic upward recovery speed if terrain or replication places the hull inside the envelope. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Surface Assist", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SurfaceClearanceRecoverySpeed = 450.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Configure SurfaceEnvelopeComponent.Settings instead."))
	float SurfaceNosePitchGuardDistance = 450.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Configure SurfaceEnvelopeComponent.Settings instead."))
	float SurfaceNosePitchTerrainSampleDistance = 300.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Configure SurfaceEnvelopeComponent.Settings instead."))
	float SurfaceTerrainAvoidanceLookAheadTime = 1.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Configure SurfaceEnvelopeComponent.Settings instead."))
	float SurfaceTerrainAvoidancePitchMarginDegrees = 3.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Configure SurfaceEnvelopeComponent.Settings instead."))
	float SurfaceNosePitchReleaseDistance = 120.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Configure SurfaceEnvelopeComponent.Settings instead."))
	float SurfaceNosePitchReleaseDelay = 0.18f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Configure SurfaceEnvelopeComponent.Settings instead."))
	float SurfaceTerrainPitchAttackDegreesPerSecond = 90.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Configure SurfaceEnvelopeComponent.Settings instead."))
	float SurfaceTerrainPitchRelaxDegreesPerSecond = 12.0f;

	/** Fallback desired descent duration used to derive a controlled initial vertical speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float DefaultLandingDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AssistedLandingMinimumDescentSpeed = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AssistedLandingMaximumDescentSpeed = 700.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float AssistedLandingRotationInterpolationSpeed = 5.0f;

	/** Slow final-area translation; the support component independently bounds the total correction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AssistedLandingCorrectionSpeed = 350.0f;

	/** Alignment tolerance for marking the final descent phase; movement begins immediately. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "0.1", ClampMax = "45.0", UIMin = "0.1", UIMax = "15.0"))
	float AssistedLandingAlignmentToleranceDegrees = 4.0f;

	/** Temporary clearance used while rotating the complete hull into its landing attitude. */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Concurrent descent no longer uses a staging altitude."))
	float AssistedLandingAlignmentClearance = 75.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Landing now uses a bounded position approach."))
	float AssistedLandingVelocityResponse = 1800.0f;

	/** The final metres slow smoothly to this cap instead of creeping for the whole descent. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AssistedLandingTouchdownSpeed = 85.0f;

	/** Height at which the controlled descent starts braking for a soft surface contact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AssistedLandingBrakingDistance = 220.0f;

	/** Final root position tolerance, capped at 2 cm to preserve measured sole contact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Landing", meta = (ClampMin = "0.25", ClampMax = "2.0", UIMin = "0.25", UIMax = "2.0"))
	float LandingContactTolerance = 1.0f;

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
	bool ApplySurfaceEnvelope(float DeltaTime);
	FQuat UpdateRotation(float DeltaTime);
	FQuat BuildFreeFlightDesiredRotation(const FQuat& CurrentRotation) const;
	FQuat InterpolateTowardRotation(float DeltaTime, const FQuat& CurrentRotation, const FQuat& DesiredRotation);
	void SubmitExteriorAltitude(float DeltaTime);
	void CompleteAssistedLanding();
	void FailAssistedLanding(EJTSLandingValidationFailure Failure);
	void SetAssistedLandingPhase(EJTSSpacecraftLandingAssistPhase NewPhase);
	bool MoveWithCollisionSweep(const FVector& Delta, const FQuat& NewRotation, FHitResult& OutHit);

	FVector BuildTargetVelocity(const FVector& ReferenceUp) const;
	float GetAccelerationRate() const;
	void SetBoostState(bool bNewBoosting);

	FVector2D MoveInput = FVector2D::ZeroVector;
	float VerticalInput = 0.0f;
	/** X is yaw, Y is pitch. Both are rates in [-1, 1] from the steering keys. */
	FVector2D SteeringInput = FVector2D::ZeroVector;
	float RollInput = 0.0f;
	bool bFlightAssistEnabled = true;
	float SpeedLimit = 1.0f;
	bool bTurnAround = false;
	/** Opposite of the hull forward, lying in the deck plane captured when T was pressed. */
	FVector TurnAroundTargetForward = FVector::ZeroVector;
	/** Deck normal captured when T was pressed. The turnaround yaws about this axis. */
	FVector TurnAroundDeckUp = FVector::ZeroVector;
	bool bTurnAroundLatched = false;
	FVector CurrentReferenceUp = FVector::UpVector;
	FVector InertialReferenceUp = FVector::UpVector;
	FVector CachedSurfaceProbeLocation = FVector::ZeroVector;
	float CurrentFacingTurnSpeedRadians = 0.0f;
	float SurfaceFlightAssistAlpha = 0.0f;
	float CachedSurfaceAltitude = 0.0f;
	float SurfaceProximityProbeElapsed = 0.0f;
	float EnvelopeLandingRetryElapsed = 0.0f;

	bool bBoosting = false;
	bool bBraking = false;
	bool bAssistedLanding = false;
	bool bInertialReferenceUpInitialized = false;
	bool bHasSurfaceProximity = false;
	bool bEnvelopeEscape = false;
	bool bTakeoffClearance = false;
	double TakeoffClearanceRadius = 0;
	FTransform AssistedLandingPose = FTransform::Identity;

	float LandingSupportCheckElapsed = 0.0f;

	float AssistedLandingDescentSpeed = 0.0f;
	float AssistedLandingElapsed = 0.0f;
	EJTSSpacecraftLandingAssistPhase AssistedLandingPhase = EJTSSpacecraftLandingAssistPhase::None;
	TWeakObjectPtr<AJTSPlanetAnchor> TargetPlanet;
};
