// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSSpacecraftSurfaceEnvelopeComponent.generated.h"

class AJTSPlanetAnchor;

/** All distances are centimetres. The shell is a navigation surface, measured above the entire hull. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSSurfaceEnvelopeSettings
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight", meta = (ClampMin = "10"))
	float FlatClearance = 100;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight", meta = (ClampMin = "0"))
	float RoughnessClearanceScale = 0.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight", meta = (ClampMin = "0"))
	float MaximumRoughnessClearance = 400;
	/** Rounded mountain shoulders start before the peak; this limits the canopy's terrain grade. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight", meta = (ClampMin = "3", ClampMax = "30"))
	float MaximumTerrainGradeDegrees = 14;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight", meta = (ClampMin = "50"))
	float ShoulderRoundingDistance = 600;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight", meta = (ClampMin = "1"))
	float SmoothMaximumWidth = 5;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight", meta = (ClampMin = "500"))
	float MinimumSamplingRadius = 3600;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight", meta = (ClampMin = "1000"))
	float MaximumSamplingRadius = 12000;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight", meta = (ClampMin = "0.5"))
	float PredictionTime = 1.8f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight", meta = (ClampMin = "0.02"))
	float SamplingInterval = 0.1f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight", meta = (ClampMin = "500"))
	float ActivationAltitude = 8000;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight", meta = (ClampMin = "1"))
	float HoverOffset = 20;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight", meta = (ClampMin = "20"))
	float FollowingEnterHeight = 300;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight", meta = (ClampMin = "50"))
	float FollowingExitHeight = 700;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight", meta = (ClampMin = "1"))
	float HeightResponse = 5;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight", meta = (ClampMin = "1"))
	float NormalResponse = 8;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Surface Flight")
	bool bDrawDebug = false;
};

USTRUCT(BlueprintType)
struct SPACE_API FJTSSurfaceEnvelopeFrame
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Surface Flight")
	bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "Surface Flight")
	FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Surface Flight")
	FVector Normal = FVector::UpVector;
	UPROPERTY(BlueprintReadOnly, Category = "Surface Flight")
	FVector RadialUp = FVector::UpVector;
	UPROPERTY(BlueprintReadOnly, Category = "Surface Flight")
	FVector GroundLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Surface Flight")
	float Roughness = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Surface Flight")
	float HullClearance = 0;
};

/** Samples real spherical terrain and constructs a smooth upper envelope. No tick, input or movement. */
UCLASS(ClassGroup = (Ship), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSSpacecraftSurfaceEnvelopeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSSpacecraftSurfaceEnvelopeComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void Refresh(AJTSPlanetAnchor* Planet, const FVector& Position, const FVector& Velocity, float GroundAltitude, float DeltaTime, bool bAllowFollowingExit = true, bool bTakeoffSampling = false);
	/** Queries the server's cached terrain neighbourhood at a position; clients use replicated GetFrame. */
	UFUNCTION(BlueprintPure, Category = "Ship|Surface Flight")
	bool Evaluate(AJTSPlanetAnchor* Planet, const FVector& Position, FJTSSurfaceEnvelopeFrame& OutFrame) const;
	void Reset();
	/** Highest sampled terrain plus full hull clearance, used before leaving a crater vertically. */
	bool GetTakeoffClearanceRadius(double& OutRadius) const;
	UFUNCTION(BlueprintPure, Category = "Ship|Surface Flight")
	FJTSSurfaceEnvelopeFrame GetFrame() const { return Frame; }
	UFUNCTION(BlueprintPure, Category = "Ship|Surface Flight")
	bool IsFollowing() const { return bFollowing; }
	const FJTSSurfaceEnvelopeSettings& GetSettings() const { return Settings; }
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Surface Flight")
	FJTSSurfaceEnvelopeSettings Settings;
private:
	struct FSample { FVector Location; double Radius; };
	bool BuildSamples(AJTSPlanetAnchor* Planet, const FVector& Position, float Radius, const FVector& Velocity, bool bTakeoffSampling);
	UPROPERTY(Replicated)
	FJTSSurfaceEnvelopeFrame Frame;
	UPROPERTY(Replicated)
	bool bFollowing = false;
	TWeakObjectPtr<AJTSPlanetAnchor> SamplePlanet;
	TArray<FSample> Samples;
	FVector SampleOrigin = FVector::ZeroVector;
	FVector SampleAxis = FVector::ForwardVector;
	FTransform SurfaceTransform = FTransform::Identity;
	float SampleRadius = 0;
	float SampleElapsed = BIG_NUMBER;
	float FootprintRadius = 0;
	float Roughness = 0;
};
