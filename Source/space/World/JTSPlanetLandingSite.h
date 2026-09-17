// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
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

	/**
	 * True if Location belongs to any configured Box/Sphere/Capsule surface footprint.
	 * A landing site's local Z depth is authoring/projection slack, never a smaller legal band on
	 * an uneven spherical surface.
	 */
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
	void BuildRuntimeLandingMarker();
	bool ProjectMarkerPointToSurface(const FVector& SourcePoint, FVector& OutPoint, FVector& OutNormal) const;
	void AppendMarkerSegment(
		const FVector& Start,
		const FVector& End,
		const FVector& StartNormal,
		const FVector& EndNormal,
		TArray<FVector>& OutVertices,
		TArray<int32>& OutTriangles,
		TArray<FVector>& OutNormals,
		TArray<FVector2D>& OutUVs,
		TArray<FLinearColor>& OutColors,
		TArray<FProcMeshTangent>& OutTangents) const;
	void GetLandingVolumes(TArray<UShapeComponent*>& OutVolumes) const;
	/** Maps a world point to the volume's authored local XY footprint along this planet's gravity line. */
	bool GetSurfaceFootprintLocalLocation(const UShapeComponent* Volume, const FVector& Location, FVector& OutLocalLocation) const;
	bool IsPointInsideVolume(const UShapeComponent* Volume, const FVector& Location) const;
	bool FindClosestPointInVolume(const UShapeComponent* Volume, const FVector& Location, FVector& OutLocation) const;
	FVector MakeRandomCandidateInVolumeBounds(const UShapeComponent* Volume) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet|Landing Site", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Default legal region. Blueprint children can add more Box, Sphere, or Capsule components. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet|Landing Site", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> LandingVolume;

	/** Runtime-only ground ribbon generated from the same authored landing volumes used by validation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet|Landing Site", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProceduralMeshComponent> RuntimeLandingMarker;

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

	/** Shows a bright, terrain-projected outline for this scene-configured legal landing area in game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Landing Site|Marker", meta = (AllowPrivateAccess = "true"))
	bool bShowRuntimeLandingMarker = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Landing Site|Marker", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float RuntimeMarkerLineWidth = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Landing Site|Marker", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float RuntimeMarkerSurfaceOffset = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Landing Site|Marker", meta = (AllowPrivateAccess = "true", ClampMin = "100.0", UIMin = "100.0"))
	float RuntimeMarkerSurfaceProbeLift = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Landing Site|Marker", meta = (AllowPrivateAccess = "true", ClampMin = "100.0", UIMin = "100.0"))
	float RuntimeMarkerSurfaceProbeDistance = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Landing Site|Marker", meta = (AllowPrivateAccess = "true"))
	FLinearColor RuntimeMarkerColor = FLinearColor(0.04f, 0.85f, 1.0f, 1.0f);

	int32 RuntimeMarkerBuildAttempts = 0;
};
