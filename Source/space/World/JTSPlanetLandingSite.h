// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/World/JTSPlanetLandingTypes.h"

#include "JTSPlanetLandingSite.generated.h"

class AJTSPlanetAnchor;
class UBoxComponent;
class USceneComponent;
class UShapeComponent;

/**
 * A developer-authored legal landing area on one planet.
 *
 * This actor is neither a base nor a spawn point. Its volumes contribute to the union of legal
 * landing area managed by AJTSPlanetLandingManager. Additional Box, Sphere, or Capsule components
 * may be added in Blueprint; every shape component on this actor contributes to its local area.
 */
UCLASS(BlueprintType)
class SPACE_API AJTSPlanetLandingSite : public AActor
{
	GENERATED_BODY()

public:
	AJTSPlanetLandingSite();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Direct planet binding is preferred. PlanetId is a Blueprint/level convenience fallback. */
	UFUNCTION(BlueprintPure, Category = "Planet|Landing Site")
	AJTSPlanetAnchor* GetPlanetAnchor() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Landing Site")
	FName GetPlanetId() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Landing Site")
	bool IsLandingEnabled() const;

	/** True if Location belongs to any of this site's configured Box/Sphere/Capsule volumes. */
	UFUNCTION(BlueprintPure, Category = "Planet|Landing Site")
	bool IsLocationInsideLandingArea(const FVector& Location) const;

	/** Returns the closest point in this site's area. It does not perform collision or terrain checks. */
	UFUNCTION(BlueprintCallable, Category = "Planet|Landing Site")
	bool FindClosestPointInLandingArea(const FVector& Location, FVector& OutLocation) const;

	/**
	 * Finds an area candidate inside this site and within SearchRadius of NearLocation. The result is
	 * intentionally only an area point; the LandingManager owns terrain and capsule-clearance checks.
	 */
	UFUNCTION(BlueprintCallable, Category = "Planet|Landing Site")
	bool FindRandomValidRespawnPoint(
		const FVector& NearLocation,
		float SearchRadius,
		FVector& OutLocation,
		int32 MaxAttempts = 16) const;

	/**
	 * Optional authored orientation marker retained for Blueprint composition. Player-requested landing
	 * uses the current valid surface point instead, so this marker never pulls a craft across a site.
	 */
	UFUNCTION(BlueprintPure, Category = "Planet|Landing Site")
	FTransform GetLandingTargetTransform() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Landing Site")
	FJTSPlanetLandingValidationData GetLandingValidationData() const;

	/** Draws all configured volumes and the landing target once. Safe to call from Blueprint or debug tools. */
	UFUNCTION(BlueprintCallable, Category = "Planet|Landing Site|Debug")
	void DrawDebugLandingSite(float Duration = 5.0f) const;

private:
	void GetLandingVolumes(TArray<UShapeComponent*>& OutVolumes) const;
	bool IsPointInsideVolume(const UShapeComponent* Volume, const FVector& Location) const;
	bool FindClosestPointInVolume(const UShapeComponent* Volume, const FVector& Location, FVector& OutLocation) const;
	FVector MakeRandomCandidateInVolumeBounds(const UShapeComponent* Volume) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet|Landing Site", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Default legal region. Blueprint children can add more Box, Sphere, or Capsule components. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet|Landing Site", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> LandingVolume;

	/** Optional designer orientation marker for future scripted landing presentation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet|Landing Site", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> LandingTarget;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|Landing Site", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetAnchor> PlanetAnchor;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|Landing Site", meta = (AllowPrivateAccess = "true"))
	FName PlanetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Landing Site", meta = (AllowPrivateAccess = "true"))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Landing Site", meta = (AllowPrivateAccess = "true", ShowOnlyInnerProperties))
	FJTSPlanetLandingValidationData LandingValidationData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Landing Site|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDebugDrawLandingSite = false;
};
