// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/World/JTSPlanetLandingTypes.h"
#include "JTSSpacecraftLandingSupportComponent.generated.h"

/** Blueprint-authored geometry in the ship root frame, measured with the gear fully deployed. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSLandingFootDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing")
	FName FootComponentName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing")
	FName StrutComponentName;

	/** Sole centre in the foot mesh's own frame, used to swivel around the contact face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing")
	FVector SoleCentreLocal = FVector::ZeroVector;

	/** Telescopic lower mesh length; presentation stretches it from its fixed upper attachment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "1.0"))
	float StrutRestLength = 34.0f;

	/** Origin is the sole centre; XY is the actual flat contact face, Z points out of the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing")
	FTransform DeployedContactFrame = FTransform::Identity;

	/** Safe inset rectangle inside the actual sole, in ship-root centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "1.0"))
	FVector2D PadHalfExtent = FVector2D(20.0f, 14.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0"))
	float MaxCompression = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0"))
	float MaxExtension = 32.0f;
};

/** Finite terrain tolerances and a bounded assist; all lengths are centimetres. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSLandingSupportSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0"))
	float MaxLandingHeight = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0"))
	float MaxLandingSpeed = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0"))
	float MaxCaptureDescentSpeed = 3200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float MaxHullSlopeDegrees = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0", ClampMax = "25.0"))
	float MaxFootTiltDegrees = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0"))
	float MaxPadRoughness = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float MaxPadNormalVariationDegrees = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0"))
	float MinimumStabilityMargin = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing")
	FVector CentreOfMassLocal = FVector::ZeroVector;

	/** Assist never chooses a ground point outside this tangent radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0", ClampMax = "500.0"))
	float MaxPositionCorrection = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0", ClampMax = "15.0"))
	float MaxYawCorrectionDegrees = 8.0f;

	/** Extra altitude above the final root pose for gear deployment, translation and rotation. */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Landing descends immediately; no staging altitude."))
	float ApproachClearance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing")
	bool bDrawDebug = false;
};

/** Terrain fit only: no input, spawning, site ownership or per-frame tick. Server owns contact state. */
UCLASS(ClassGroup = (Ship), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSSpacecraftLandingSupportComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSSpacecraftLandingSupportComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Ship|Landing")
	bool FindLanding(AJTSPlanetAnchor* Planet, bool bControlledDescent, FJTSPlanetLandingValidationResult& OutResult) const;
	/** Throttled HUD prediction. Landing requests always use a fresh FindLanding instead. */
	bool GetLandingPreview(AJTSPlanetAnchor* Planet, FJTSPlanetLandingValidationResult& OutResult) const;

	/** Fixed-pose check used repeatedly during descent and again at touchdown. Never searches or lifts. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Landing")
	bool ValidatePose(AJTSPlanetAnchor* Planet, FTransform Pose, FJTSPlanetLandingValidationResult& OutResult) const;
	/** Exact developer-authored initial ground point; fits height/attitude without choosing another location. */
	bool FitAtSurface(AJTSPlanetAnchor* Planet, const FVector& Ground, const FVector& Forward,
		FJTSPlanetLandingValidationResult& OutResult) const;

	UFUNCTION(BlueprintPure, Category = "Ship|Landing")
	TArray<FJTSLandingFootContact> GetFootContacts() const { return FootContacts; }

	const FJTSLandingSupportSettings& GetSettings() const { return Settings; }
	const TArray<FJTSLandingFootDefinition>& GetFeet() const { return Feet; }
	void SetContacts(const TArray<FJTSLandingFootContact>& Contacts);
	/** Applied only by presentation after its authored fold pose, also on replicated clients. */
	void ApplyContactPresentation(float DeployAlpha) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Landing")
	TArray<FJTSLandingFootDefinition> Feet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Landing")
	FJTSLandingSupportSettings Settings;

private:
	bool HasValidRig() const;
	bool FitCandidate(AJTSPlanetAnchor* Planet, const FVector& Ground, const FVector& Forward,
		FJTSPlanetLandingValidationResult& OutResult) const;
	bool ProbeFoot(AJTSPlanetAnchor* Planet, const FTransform& Pose, const FJTSLandingFootDefinition& Foot,
		FJTSLandingFootContact& OutContact, EJTSLandingValidationFailure& OutFailure) const;
	bool HasClearApproach(const FTransform& Pose) const;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Ship|Landing", meta = (AllowPrivateAccess = "true"))
	TArray<FJTSLandingFootContact> FootContacts;

	mutable FJTSPlanetLandingValidationResult CachedPreview;
	mutable TWeakObjectPtr<AJTSPlanetAnchor> CachedPreviewPlanet;
	mutable FTransform CachedPreviewPose = FTransform::Identity;
	mutable double PreviewTime = -1;
};
