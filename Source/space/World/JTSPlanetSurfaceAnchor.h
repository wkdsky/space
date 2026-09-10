// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "JTSPlanetSurfaceAnchor.generated.h"

class AJTSPlanetAnchor;
class USceneComponent;

/**
 * A lightweight authored point on a real gameplay planet mesh.
 *
 * Its editor location selects a radial direction from the planet centre and its editor forward
 * selects the tangent forward direction. It only resolves one surface transform; gameplay actors
 * remain responsible for their own spawning and behavior.
 */
UCLASS(BlueprintType)
class SPACE_API AJTSPlanetSurfaceAnchor : public AActor
{
	GENERATED_BODY()

public:
	AJTSPlanetSurfaceAnchor();

	virtual void BeginPlay() override;

	/** Resolves the explicitly assigned planet, or the matching registered PlanetId when unassigned. */
	UFUNCTION(BlueprintPure, Category = "Planet|Surface Anchor")
	AJTSPlanetAnchor* GetPlanetAnchor() const;

	/** Returns the real-mesh surface transform without moving this anchor. */
	UFUNCTION(BlueprintCallable, Category = "Planet|Surface Anchor")
	bool GetSurfaceTransform(FTransform& OutSurfaceTransform) const;

	/** Requeries the real mesh, moves this authored anchor, and optionally aligns its rotation. */
	UFUNCTION(BlueprintCallable, Category = "Planet|Surface Anchor")
	bool SnapToPlanetSurface();

private:
	bool BuildSurfaceTransform(FTransform& OutSurfaceTransform) const;
	void DrawPlacementDebug(const FTransform& SurfaceTransform) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet|Surface Anchor", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Preferred direct binding for this authored location. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|Surface Anchor", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetAnchor> PlanetAnchor;

	/** Optional manager lookup when a direct PlanetAnchor has not been assigned. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|Surface Anchor", meta = (AllowPrivateAccess = "true"))
	FName PlanetId = TEXT("Moon");

	/** Distance outward from the actual collision impact point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Surface Anchor", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float SurfaceClearance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Surface Anchor", meta = (AllowPrivateAccess = "true"))
	bool bSnapOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Surface Anchor", meta = (AllowPrivateAccess = "true"))
	bool bAlignRotationToSurface = true;

	/** Draws the one-shot radial trace and resolved frame only when placement is requested. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDebugPlanetSurfacePlacement = false;
};
