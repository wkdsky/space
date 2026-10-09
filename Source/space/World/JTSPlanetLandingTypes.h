// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "JTSPlanetLandingTypes.generated.h"

class AJTSPlanetLandingSite;
class AJTSPlanetAnchor;

/** Runtime state of one spacecraft. Ground contact alone never changes this state to Landed. */
UENUM(BlueprintType)
enum class EJTSSpacecraftFlightState : uint8
{
	Flying UMETA(DisplayName = "Flying"),
	LandingRequest UMETA(DisplayName = "Landing Request"),
	LandingAssist UMETA(DisplayName = "Landing Assist"),
	Landed UMETA(DisplayName = "Landed")
};

/** The visible/server-authoritative sub-state of a controlled touchdown. */
UENUM(BlueprintType)
enum class EJTSSpacecraftLandingAssistPhase : uint8
{
	None UMETA(DisplayName = "None"),
	Aligning UMETA(DisplayName = "Aligning"),
	Descending UMETA(DisplayName = "Descending"),
	Touchdown UMETA(DisplayName = "Touchdown")
};

/** Why a landing request could not become a controlled landing. */
UENUM(BlueprintType)
enum class EJTSLandingValidationFailure : uint8
{
	None UMETA(DisplayName = "None"),
	NoPlanet UMETA(DisplayName = "No Planet"),
	NoLandingSite UMETA(DisplayName = "No Landing Site"),
	OutsideLandingArea UMETA(DisplayName = "Outside Landing Area"),
	TooHigh UMETA(DisplayName = "Too High"),
	TooFast UMETA(DisplayName = "Too Fast"),
	InvalidAttitude UMETA(DisplayName = "Invalid Attitude"),
	TooSteep UMETA(DisplayName = "Too Steep"),
	NoSurface UMETA(DisplayName = "No Surface"),
	CollisionBlocked UMETA(DisplayName = "Collision Blocked"),
	InvalidLandingTarget UMETA(DisplayName = "Invalid Landing Target"),
	InvalidFlightState UMETA(DisplayName = "Invalid Flight State"),
	MissingLandingGear UMETA(DisplayName = "Landing Gear Not Configured"),
	UnsupportedFoot UMETA(DisplayName = "Landing Foot Has No Support"),
	UnevenFootSurface UMETA(DisplayName = "Landing Foot Surface Too Rough"),
	GearTravelExceeded UMETA(DisplayName = "Landing Gear Travel Exceeded"),
	UnstableSupport UMETA(DisplayName = "Centre Of Mass Outside Safe Support"),
	ApproachBlocked UMETA(DisplayName = "Landing Approach Blocked")
};

/** Which fallback produced a respawn transform near a landed spacecraft. */
UENUM(BlueprintType)
enum class EJTSRespawnTransformSource : uint8
{
	SpacecraftExitSafe UMETA(DisplayName = "Safe Spacecraft Exit"),
	LandingAreaRandom UMETA(DisplayName = "Landing Area Random"),
	LandingAreaNearest UMETA(DisplayName = "Landing Area Nearest"),
	SpacecraftTop UMETA(DisplayName = "Spacecraft Top"),
	SpacecraftExit UMETA(DisplayName = "Spacecraft Exit")
};

/** Legacy serialized site data. Runtime landing rules now belong to the ship's support component. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSPlanetLandingValidationData
{
	GENERATED_BODY()

	/** Distance above the real surface from which a craft may request a landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxLandingHeight = 1200.0f;

	/** Maximum total velocity permitted when the request is made. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxLandingSpeed = 450.0f;

	/**
	 * Maximum radial speed that hold-to-descend automatic landing may capture. Tangential speed still
	 * uses MaxLandingSpeed, so the assist cannot grab a craft sweeping sideways through the site.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxAutomaticLandingRadialSpeed = 3200.0f;

	/** Maximum angle between the resolved terrain normal and the radial local up vector. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0", ClampMax = "89.0", UIMin = "0.0", UIMax = "60.0"))
	float MaxSlopeDegrees = 28.0f;

	/**
	 * Retained for existing level data. Surface Alignment now resolves this angle during LandingAssist
	 * instead of rejecting a legal landing request for its current flight attitude.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0", ClampMax = "180.0", UIMin = "0.0", UIMax = "90.0"))
	float MaxLandingAttitudeDegrees = 35.0f;

	/** Extra clearance added above the spacecraft hull after the target surface is resolved. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SurfaceOffset = 12.0f;

	/** Maximum extra lift used to fit the complete hull above uneven terrain around the probe point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxSurfaceClearanceAdjustment = 500.0f;

	/** Maximum local gravity-direction query length used to resolve the authored target onto terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float TargetSurfaceProbeDistance = 3000.0f;
};

/** A measured contact, also replicated for the visible telescopic strut and swivelling foot. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSLandingFootContact
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	FName FootComponentName;

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	FName StrutComponentName;

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	FVector Normal = FVector::UpVector;

	/** Positive extends downwards; negative compresses, in world centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	float Extension = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	float Roughness = 0.0f;
};

/** One complete terrain/gear decision returned by the LandingManager to a spacecraft. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSPlanetLandingValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	EJTSLandingValidationFailure Failure = EJTSLandingValidationFailure::InvalidLandingTarget;

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	TObjectPtr<AJTSPlanetAnchor> Planet = nullptr;

	/** Compatibility field; runtime terrain landing always leaves this null. */
	UPROPERTY(BlueprintReadOnly, Category = "Landing", meta = (DeprecatedProperty, DeprecationMessage = "Use Planet, LandingTransform and FootContacts."))
	TObjectPtr<AJTSPlanetLandingSite> LandingSite = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	FTransform LandingTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	FVector GroundLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	FVector GroundNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	float GroundDistance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	float GroundSlopeDegrees = 0.0f;

	/** Root-to-surface clearance along GroundNormal that the spacecraft must preserve while landing. */
	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	float LandingClearance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	TArray<FJTSLandingFootContact> FootContacts;

	/** Tangent displacement from the pilot's original ground point, in centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	float PositionCorrection = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	float StabilityMargin = 0.0f;
};

/** Result returned when a landed spacecraft resolves a player respawn transform. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSPlayerRespawnTransformResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Respawn")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Respawn")
	EJTSRespawnTransformSource Source = EJTSRespawnTransformSource::SpacecraftExit;

	UPROPERTY(BlueprintReadOnly, Category = "Respawn")
	FTransform Transform = FTransform::Identity;
};
