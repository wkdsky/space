// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "JTSPlanetAnchor.generated.h"

class AActor;
class USceneComponent;
class UWorld;

/**
 * Lightweight logical definition for a navigable planet.
 *
 * This actor deliberately owns no exterior mesh. The persistent world supplies visuals,
 * while this actor supplies the reference frame, surface level, and travel thresholds
 * shared by surface and space systems.
 */
UCLASS(BlueprintType)
class SPACE_API AJTSPlanetAnchor : public AActor
{
	GENERATED_BODY()

public:
	AJTSPlanetAnchor();

	UFUNCTION(BlueprintPure, Category = "Planet")
	FName GetPlanetId() const;

	UFUNCTION(BlueprintPure, Category = "Planet")
	float GetPlanetRadius() const;

	UFUNCTION(BlueprintPure, Category = "Planet")
	FVector GetExteriorCenter() const;

	/** Signed distance from the spherical exterior. Positive values are outside the planet. */
	UFUNCTION(BlueprintPure, Category = "Planet|Exterior")
	float GetExteriorAltitude(const FVector& WorldPosition) const;

	/** Unit vector from WorldPosition toward the exterior center. */
	UFUNCTION(BlueprintPure, Category = "Planet|Exterior")
	FVector GetDirectionToPlanet(const FVector& WorldPosition) const;

	/** Returns the persistent-world actor manually assigned to render this planet, if any. */
	UFUNCTION(BlueprintPure, Category = "Planet|Exterior")
	AActor* GetExteriorVisualActor() const;

	/** The local flat-surface frame used by gameplay and future reference-frame conversions. */
	UFUNCTION(BlueprintPure, Category = "Planet|Surface")
	FTransform GetSurfaceFrameTransform() const;

	/** The primary assisted-landing target. A later system may add multiple landing sites. */
	UFUNCTION(BlueprintPure, Category = "Planet|Landing")
	FTransform GetLandingTransform() const;

	/** The flight spawn entry for this planet; falls back to a position above its exterior sphere. */
	UFUNCTION(BlueprintPure, Category = "Planet|Approach")
	FTransform GetApproachEntryTransform() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Surface")
	FVector SurfaceLocalToWorld(const FVector& SurfaceLocalPosition) const;

	UFUNCTION(BlueprintPure, Category = "Planet|Surface")
	FVector WorldToSurfaceLocal(const FVector& WorldPosition) const;

	UFUNCTION(BlueprintPure, Category = "Planet|Surface")
	FVector GetSurfaceUpVector() const;

	/** Signed distance above the local surface plane, measured along the surface frame's Z axis. */
	UFUNCTION(BlueprintPure, Category = "Planet|Surface")
	float GetSurfaceAltitude(const FVector& WorldPosition) const;

	/** Returns the exterior-center location that places the surface frame origin at the planet top. */
	UFUNCTION(BlueprintPure, Category = "Planet|Exterior")
	FVector GetRecommendedExteriorCenter() const;

	/** Convenience editor action; it never changes an exterior visual actor or mesh asset. */
	UFUNCTION(BlueprintCallable, Category = "Planet|Exterior")
	void SetExteriorCenterToRecommended();

	bool HasSurfaceLevel() const;
	const TSoftObjectPtr<UWorld>& GetSurfaceLevel() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Travel")
	float GetTakeoffTransitionAltitude() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Travel")
	float GetSpaceFlightAltitude() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Travel")
	float GetSurfaceUnloadAltitude() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Travel")
	float GetSurfaceLoadAltitude() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Approach")
	float GetApproachTransitionAltitude() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Landing")
	float GetLandingApproachRange() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Landing")
	float GetLandingAssistAltitude() const;

	UFUNCTION(BlueprintPure, Category = "Planet|State")
	bool IsActivePlanet() const;

	void SetActivePlanet(bool bInIsActivePlanet);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Stable logical identifier. It intentionally does not depend on a LocalAssets mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet", meta = (AllowPrivateAccess = "true"))
	FName PlanetId = TEXT("Moon");

	/** Visual radius in Unreal centimeters. Moon defaults to its saved Fake Moon Bend curve radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Exterior", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PlanetRadius = 20000.0f;

	/** Center of the manually placed exterior visual in persistent-world coordinates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Exterior", meta = (AllowPrivateAccess = "true"))
	FVector ExteriorCenter = FVector(0.0f, 0.0f, -20000.0f);

	/** Optional manually assigned persistent-world visual actor (for example MoonExterior). */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|Exterior", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> ExteriorVisualActor;

	/** Optional scene actor that overrides SurfaceAnchorTransform at runtime. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|Surface", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> SurfaceAnchorActor;

	/** Fallback local-surface frame. Moon uses identity so its current XY/Z gameplay remains unchanged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Surface", meta = (AllowPrivateAccess = "true"))
	FTransform SurfaceAnchorTransform = FTransform::Identity;

	/** Optional scene actor that overrides LandingAnchorTransform at runtime. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|Landing", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> LandingAnchorActor;

	/** Fallback primary landing transform. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Landing", meta = (AllowPrivateAccess = "true"))
	FTransform LandingAnchorTransform = FTransform::Identity;

	/** Optional scene actor defining the direction and starting distance for a flight approach. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|Approach", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> ApproachEntryAnchorActor;

	/** Optional authored fallback. When disabled, a sphere-relative transform is generated automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Approach", meta = (AllowPrivateAccess = "true"))
	bool bUseApproachEntryTransform = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Approach", meta = (AllowPrivateAccess = "true", EditCondition = "bUseApproachEntryTransform"))
	FTransform ApproachEntryTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Approach", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float DefaultApproachEntryDistance = 45000.0f;

	/** Soft surface-world reference. No map path is hard-coded by native code. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Surface", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UWorld> SurfaceLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Travel", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float TakeoffTransitionAltitude = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Travel", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float SpaceFlightAltitude = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Streaming", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float SurfaceUnloadAltitude = 15000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Streaming", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float SurfaceLoadAltitude = 12000.0f;

	/** Enter Approach and request the surface level at this spherical exterior altitude. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Approach", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float ApproachTransitionAltitude = 20000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Landing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float LandingApproachRange = 12000.0f;

	/** At this exterior altitude, a visible loaded surface may take over with assisted landing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Landing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float LandingAssistAltitude = 2500.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Planet|State", meta = (AllowPrivateAccess = "true"))
	bool bIsActivePlanet = false;
};
