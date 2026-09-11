// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "JTSPlanetLandingTypes.generated.h"

class AJTSPlanetLandingSite;

/** Runtime state of one spacecraft. Ground contact alone never changes this state to Landed. */
UENUM(BlueprintType)
enum class EJTSSpacecraftFlightState : uint8
{
	Flying UMETA(DisplayName = "Flying"),
	LandingRequest UMETA(DisplayName = "Landing Request"),
	LandingAssist UMETA(DisplayName = "Landing Assist"),
	Landed UMETA(DisplayName = "Landed")
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
	InvalidFlightState UMETA(DisplayName = "Invalid Flight State")
};

/** Which fallback produced a respawn transform near a landed spacecraft. */
UENUM(BlueprintType)
enum class EJTSRespawnTransformSource : uint8
{
	LandingAreaRandom UMETA(DisplayName = "Landing Area Random"),
	LandingAreaNearest UMETA(DisplayName = "Landing Area Nearest"),
	SpacecraftTop UMETA(DisplayName = "Spacecraft Top"),
	SpacecraftExit UMETA(DisplayName = "Spacecraft Exit")
};

/** Per-site rules. The site owns data only; it never moves a spacecraft. */
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

	/** Maximum local gravity-direction query length used to resolve the authored target onto terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float TargetSurfaceProbeDistance = 3000.0f;
};

/** One complete landing decision returned by the LandingManager to a spacecraft. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSPlanetLandingValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	EJTSLandingValidationFailure Failure = EJTSLandingValidationFailure::NoLandingSite;

	UPROPERTY(BlueprintReadOnly, Category = "Landing")
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
