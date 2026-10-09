// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "JTSSpacecraftPresentationComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USpotLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Cosmetic landing gear and exhaust for one spacecraft.
 *
 * Gear deployment follows the replicated flight state, so every client plays the same
 * retraction. Exhaust strength follows the locally predicted flight input: Space lights
 * the belly nozzles, W lights the rear nozzle. Neither path changes movement.
 */
UCLASS(ClassGroup = (Ship), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSSpacecraftPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSSpacecraftPresentationComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 0 is fully retracted into the belly, 1 is the deployed stance. */
	UFUNCTION(BlueprintPure, Category = "Ship|Presentation")
	float GetGearDeployAlpha() const;

protected:
	/** Retraction duration after takeoff. */
	UPROPERTY(EditDefaultsOnly, Category = "Ship|Presentation|Gear", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float GearTransitionDuration = 2.4f;

	/** Deployment begins with landing; descent and deployment run concurrently. */
	UPROPERTY(EditDefaultsOnly, Category = "Ship|Presentation|Gear", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float GearDeploymentDuration = 0.65f;

	/** How quickly a plume catches the commanded throttle. */
	UPROPERTY(EditDefaultsOnly, Category = "Ship|Presentation|Exhaust", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float ExhaustResponse = 11.0f;

	/** Visible length of one rear plume at full forward throttle, in centimetres. */
	UPROPERTY(EditDefaultsOnly, Category = "Ship|Presentation|Exhaust", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MainPlumeLength = 780.0f;

	/** Visible length of one belly plume at full lift, in centimetres. */
	UPROPERTY(EditDefaultsOnly, Category = "Ship|Presentation|Exhaust", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float LiftPlumeLength = 210.0f;

	/** Width of one rear plume at full throttle. */
	UPROPERTY(EditDefaultsOnly, Category = "Ship|Presentation|Exhaust", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MainPlumeRadius = 52.0f;

	/** Width of one belly plume at full lift. */
	UPROPERTY(EditDefaultsOnly, Category = "Ship|Presentation|Exhaust", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float LiftPlumeRadius = 26.0f;

	/** Smallest scale that still reads as an idle glow before the plume is hidden. */
	UPROPERTY(EditDefaultsOnly, Category = "Ship|Presentation|Exhaust", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "0.5"))
	float ExhaustVisibleThreshold = 0.04f;

	/** Project art for the spherical cruise envelope, assigned on the ship Blueprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Presentation|Antigravity", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> AntigravityFieldMesh;

	/** Translucent field material with a scalar FieldAlpha parameter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Presentation|Antigravity", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> AntigravityFieldMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Presentation|Antigravity", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AntigravityFieldPadding = 110.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Presentation|Antigravity", meta = (AllowPrivateAccess = "true", ClampMin = "0.05", UIMin = "0.05"))
	float AntigravityFieldDeployDuration = 0.28f;

	/** How far one nose beam still lights terrain, in centimetres. */
	UPROPERTY(EditDefaultsOnly, Category = "Ship|Presentation|Headlight", meta = (ClampMin = "100.0", UIMin = "100.0"))
	float HeadlightRange = 12000.0f;

	/** Intensity of each of the two overlapping nose beams. Keep the combined wash near the character lamp. */
	UPROPERTY(EditDefaultsOnly, Category = "Ship|Presentation|Headlight", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "100.0"))
	float HeadlightIntensity = 16.0f;

private:
	struct FGearBinding
	{
		/** Weak so a blueprint recompile during play cannot leave a dangling component in the tick. */
		TWeakObjectPtr<USceneComponent> Component;
		/** Gear_FL and the rest. The upper, shin and foot of one leg share this name. */
		FName LegName = NAME_None;
		bool bIsShin = false;
		bool bIsFoot = false;
		FVector DeployedLocation = FVector::ZeroVector;
		FRotator DeployedRotation = FRotator::ZeroRotator;
		FVector DeployedScale = FVector::OneVector;
	};

	struct FPlumeBinding
	{
		TWeakObjectPtr<UStaticMeshComponent> Component;
		TWeakObjectPtr<UStaticMeshComponent> Core;
		TWeakObjectPtr<UStaticMeshComponent> Halo;
		TObjectPtr<UMaterialInstanceDynamic> Material;
		TObjectPtr<UMaterialInstanceDynamic> CoreMaterial;
		TObjectPtr<UMaterialInstanceDynamic> HaloMaterial;
		bool bIsMainNozzle = false;
		/** Centre bell of the rear row. It runs longer than the two outboard bells. */
		bool bIsCentreBell = false;
		float Length = 1.0f;
		float Radius = 1.0f;
	};

	void DiscoverRig();
	void ReleaseExhaustCores();
	void EnsureExhaustCore(FPlumeBinding& Plume);
	void ApplyGearPose(const FGearBinding& Leg, float Eased) const;
	void UpdateGear(float DeltaTime);
	void UpdateExhaust(float DeltaTime);
	void EnsureAntigravityField();
	void UpdateCruiseMode(float DeltaTime);
	float GetDesiredGearAlpha() const;
	float GetCommandedMainThrottle() const;
	float GetCommandedLiftThrottle() const;
	void ApplyPlume(FPlumeBinding& Plume, float Strength);
	void EnsureHeadlights();
	void UpdateHeadlights();

	struct FHeadlightBinding
	{
		TWeakObjectPtr<UStaticMeshComponent> Housing;
		TWeakObjectPtr<UStaticMeshComponent> Lens;
		TWeakObjectPtr<USpotLightComponent> Beam;
		TObjectPtr<UMaterialInstanceDynamic> LensMaterial;
	};

	TArray<FGearBinding> Gear;
	TArray<FHeadlightBinding> Headlights;
	TArray<FPlumeBinding> Plumes;
	TWeakObjectPtr<UStaticMeshComponent> AntigravityField;
	TObjectPtr<UMaterialInstanceDynamic> AntigravityFieldDynamicMaterial;
	FVector AntigravityFieldFullScale = FVector::OneVector;
	/** Authored deployed poses, keyed by component name. Rediscovery must not recapture an animated pose. */
	TMap<FName, FVector> CapturedGearLocation;
	TMap<FName, FRotator> CapturedGearRotation;
	TMap<FName, FVector> CapturedGearScale;
	float GearAlpha = 1.0f;
	float SmoothedMainThrottle = 0.0f;
	float SmoothedLiftThrottle = 0.0f;
	float AntigravityFieldAlpha = 0.0f;
	bool bCruiseMode = false;
	bool bRigReady = false;
};
