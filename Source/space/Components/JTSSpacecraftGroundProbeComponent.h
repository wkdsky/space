// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "JTSSpacecraftGroundProbeComponent.generated.h"

class AJTSPlanetAnchor;

/**
 * Current result of a spacecraft-to-real-surface probe. Distance is measured on the planet's
 * gravity probe; DockingHeight is the equivalent separation along the resolved terrain normal.
 */
USTRUCT(BlueprintType)
struct SPACE_API FJTSSpacecraftGroundInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Spacecraft|Ground Probe")
	bool bHasGround = false;

	/** Distance along the local Planet gravity probe from the spacecraft root to the mesh surface. */
	UPROPERTY(BlueprintReadOnly, Category = "Spacecraft|Ground Probe")
	float Distance = 0.0f;

	/** Root-to-surface separation along SurfaceNormal, used for collision-clear landing control. */
	UPROPERTY(BlueprintReadOnly, Category = "Spacecraft|Ground Probe")
	float DockingHeight = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Spacecraft|Ground Probe")
	FVector GroundLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Spacecraft|Ground Probe")
	FVector SurfaceNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "Spacecraft|Ground Probe")
	float SlopeDegrees = 0.0f;

	/** Surface frame with local Z aligned to SurfaceNormal and local X projected from flight heading. */
	UPROPERTY(BlueprintReadOnly, Category = "Spacecraft|Ground Probe")
	FTransform SurfaceTransform = FTransform::Identity;
};

/**
 * Dedicated real-surface probe for a spacecraft. It performs no movement and does not decide
 * landing legality; AJTSPlanetLandingManager owns rules and the flight movement component owns
 * the resulting descent.
 */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSSpacecraftGroundProbeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSSpacecraftGroundProbeComponent();

	/** Refreshes the current ground result through PlanetAnchor's real mesh surface query. */
	bool ProbeGround(AJTSPlanetAnchor* Planet, const FVector& PreferredForward, float MaxDistance = 0.0f);

	UFUNCTION(BlueprintPure, Category = "Spacecraft|Ground Probe")
	FJTSSpacecraftGroundInfo GetGroundInfo() const;

	UFUNCTION(BlueprintPure, Category = "Spacecraft|Ground Probe")
	bool HasGround() const;

	UFUNCTION(BlueprintCallable, Category = "Spacecraft|Ground Probe")
	void ClearGroundInfo();

protected:
	/** Default local gravity-probe length. LandingManager can supply a larger rule-specific distance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spacecraft|Ground Probe", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float GroundProbeDistance = 6000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spacecraft|Ground Probe|Debug")
	bool bDebugDrawGroundProbe = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spacecraft|Ground Probe|Debug", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DebugDrawDuration = 0.0f;

private:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Spacecraft|Ground Probe", meta = (AllowPrivateAccess = "true"))
	FJTSSpacecraftGroundInfo CurrentGroundInfo;
};
