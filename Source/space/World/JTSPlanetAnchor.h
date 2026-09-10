// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"

#include "JTSPlanetAnchor.generated.h"

class AActor;
class UPrimitiveComponent;
class USceneComponent;
class UWorld;

/** Result from one radial query against a planet's real gameplay mesh collision. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSPlanetSurfaceHit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Planet|Surface")
	bool bBlockingHit = false;

	UPROPERTY(BlueprintReadOnly, Category = "Planet|Surface")
	FVector ImpactPoint = FVector::ZeroVector;

	/** The actual collision normal from the gameplay mesh, oriented outward when necessary. */
	UPROPERTY(BlueprintReadOnly, Category = "Planet|Surface")
	FVector ImpactNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "Planet|Surface")
	FVector TraceStart = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Planet|Surface")
	FVector TraceEnd = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Planet|Surface")
	float Distance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Planet|Surface")
	TObjectPtr<AActor> HitActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Planet|Surface")
	TObjectPtr<UPrimitiveComponent> HitComponent = nullptr;
};

/** A small authored/runtime surface frame for POIs, spawn anchors, and future region generation. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSPlanetSurfaceFrame
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Planet|Surface")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Planet|Surface")
	FVector Up = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "Planet|Surface")
	FVector Forward = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Planet|Surface")
	FVector Right = FVector::RightVector;

	UPROPERTY(BlueprintReadOnly, Category = "Planet|Surface")
	FTransform Transform = FTransform::Identity;
};

/**
 * Gameplay definition for one real spherical planet in the persistent SpaceWorld.
 *
 * The manually placed GameplaySurfaceActor / GameplaySurfaceComponent owns the visible mesh and
 * collision. This actor only supplies centre-relative queries, future non-surface gravity bounds, travel thresholds,
 * and authored surface-anchor helpers. It never loads a local mesh asset and never owns a fake
 * tangent-plane gameplay surface.
 */
UCLASS(BlueprintType)
class SPACE_API AJTSPlanetAnchor : public AActor
{
	GENERATED_BODY()

public:
	AJTSPlanetAnchor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Planet")
	FName GetPlanetId() const;

	/** Centre of this gameplay planet. By default it is this actor's location. */
	UFUNCTION(BlueprintPure, Category = "Planet")
	FVector GetPlanetCenter() const;

	/** Coarse radius for UI, altitude approximation, approach, arc distance, and candidate radial traces; never authoritative ground or gravity direction. */
	UFUNCTION(BlueprintPure, Category = "Planet")
	float GetApproximateRadius() const;

	/** Compatibility name retained for existing flight/Blueprint references. */
	UFUNCTION(BlueprintPure, Category = "Planet", meta = (DeprecatedFunction, DeprecationMessage = "Use GetApproximateRadius."))
	float GetPlanetRadius() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Surface")
	AActor* GetGameplaySurfaceActor() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Surface")
	UPrimitiveComponent* GetGameplaySurfaceComponent() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Surface")
	bool HasGameplaySurface() const;

	/** True for the configured mesh actor/component and actors attached to it. */
	bool OwnsGameplaySurfaceActor(const AActor* Candidate) const;

	UFUNCTION(BlueprintPure, Category = "Planet|Gravity")
	bool IsGravityEnabled() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Gravity")
	float GetGravityStrength() const;

	/** Future non-Surface altitude gate, measured above ApproximateRadius. Surface characters ignore it. */
	UFUNCTION(BlueprintPure, Category = "Planet|Gravity")
	float GetGravityInfluenceRange() const;

	/** Future Takeoff/SpaceFlight state-transition altitude, independent of Surface character gravity. */
	UFUNCTION(BlueprintPure, Category = "Planet|Travel")
	float GetSpaceExitRange() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Travel")
	bool IsWithinSpaceExitRange(const FVector& WorldPosition) const;

	UFUNCTION(BlueprintPure, Category = "Planet|Gravity")
	bool IsWithinGravityInfluence(const FVector& WorldPosition) const;

	UFUNCTION(BlueprintPure, Category = "Planet|Surface")
	FVector GetRadialUpVector(const FVector& WorldPosition) const;

	UFUNCTION(BlueprintPure, Category = "Planet|Gravity")
	FVector GetGravityDirection(const FVector& WorldPosition) const;

	UFUNCTION(BlueprintPure, Category = "Planet|Surface")
	float GetApproximateAltitude(const FVector& WorldPosition) const;

	/**
	 * Traces from outside the mesh toward PlanetCenter and accepts only the configured gameplay surface.
	 * This intentionally uses mesh collision rather than an approximate-radius sphere.
	 */
	UFUNCTION(BlueprintCallable, Category = "Planet|Surface")
	bool TraceToSurface(const FVector& WorldPosition, FJTSPlanetSurfaceHit& OutSurfaceHit) const;

	UFUNCTION(BlueprintCallable, Category = "Planet|Surface")
	bool ProjectPointToSurface(const FVector& WorldPosition, FJTSPlanetSurfaceHit& OutSurfaceHit) const;

	UFUNCTION(BlueprintCallable, Category = "Planet|Surface")
	bool GetSurfaceNormalAt(const FVector& WorldPosition, FVector& OutSurfaceNormal) const;

	/** Builds an X-forward/Y-right/Z-up frame from actual mesh collision. */
	UFUNCTION(BlueprintCallable, Category = "Planet|Surface")
	bool GetSurfaceFrameAt(
		const FVector& WorldPosition,
		const FVector& PreferredForward,
		FJTSPlanetSurfaceFrame& OutSurfaceFrame) const;

	UFUNCTION(BlueprintPure, Category = "Planet|Surface")
	FVector ProjectDirectionToSurfaceTangent(const FVector& WorldDirection, const FVector& WorldPosition) const;

	UFUNCTION(BlueprintPure, Category = "Planet|Surface")
	FRotator MakeSurfaceAlignedRotation(const FVector& WorldPosition, const FVector& PreferredForward) const;

	/** Snaps an authored actor to the real mesh and aligns its local Z axis with the actual surface normal. */
	UFUNCTION(BlueprintCallable, Category = "Planet|Surface")
	bool SnapActorToPlanetSurface(AActor* ActorToSnap, float SurfaceOffset = 0.0f);

	/** Uniform random outward unit vector, for future spherical placement helpers. */
	UFUNCTION(BlueprintPure, Category = "Planet|Generation")
	FVector RandomDirectionOnSphere() const;

	/** Converts a coarse surface arc distance into its central angle for spherical-cap generation. */
	UFUNCTION(BlueprintPure, Category = "Planet|Generation")
	float ArcDistanceToAngleRadians(float ArcDistance) const;

	/** Picks a point within a spherical cap, then resolves it against real mesh collision. */
	UFUNCTION(BlueprintCallable, Category = "Planet|Generation")
	bool RandomPointInSurfaceCap(
		const FVector& CenterDirection,
		float ArcDistance,
		FJTSPlanetSurfaceHit& OutSurfaceHit) const;

	/** Approximate geodesic distance using the planet radius and radial directions. */
	UFUNCTION(BlueprintPure, Category = "Planet|Generation")
	float ApproximateSurfaceArcDistance(const FVector& WorldPositionA, const FVector& WorldPositionB) const;

	/** Generic landing/POI transform. An authored LandingAnchorActor wins; fallback comes from real surface collision. */
	UFUNCTION(BlueprintPure, Category = "Planet|Landing")
	FTransform GetLandingTransform() const;

	/**
	 * Resolves LandingAnchorActor against the real gameplay mesh. This deliberately has no legacy
	 * transform fallback so real-planet startup can require an authored landing anchor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Planet|Landing")
	bool GetLandingSurfaceTransform(FTransform& OutLandingTransform) const;

	/** Generic approach transform around the real planet centre. */
	UFUNCTION(BlueprintPure, Category = "Planet|Approach")
	FTransform GetApproachEntryTransform() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Content")
	bool HasPlanetContentLevel() const;

	const TSoftObjectPtr<UWorld>& GetPlanetContentLevel() const;
	FTransform GetPlanetContentTransform() const;

	/** Compatibility alias. Exterior is no longer separate from gameplay; returns PlanetCenter. */
	UFUNCTION(BlueprintPure, Category = "Planet|LegacyFlatSurface", meta = (DeprecatedFunction, DeprecationMessage = "Use GetPlanetCenter."))
	FVector GetExteriorCenter() const;

	UFUNCTION(BlueprintPure, Category = "Planet|LegacyFlatSurface", meta = (DeprecatedFunction, DeprecationMessage = "Use GetApproximateAltitude."))
	float GetExteriorAltitude(const FVector& WorldPosition) const;

	UFUNCTION(BlueprintPure, Category = "Planet|LegacyFlatSurface", meta = (DeprecatedFunction, DeprecationMessage = "Use GetGravityDirection."))
	FVector GetDirectionToPlanet(const FVector& WorldPosition) const;

	UFUNCTION(BlueprintPure, Category = "Planet|LegacyFlatSurface", meta = (DeprecatedFunction, DeprecationMessage = "A real gameplay mesh is configured through GameplaySurfaceActor."))
	AActor* GetExteriorVisualActor() const;

	/** Legacy flat-streamed-surface frame retained only for serialized Blueprint compatibility. */
	UFUNCTION(BlueprintPure, Category = "Planet|LegacyFlatSurface", meta = (DeprecatedFunction, DeprecationMessage = "Real planets have no authoritative flat surface frame."))
	FTransform GetSurfaceFrameTransform() const;

	UFUNCTION(BlueprintPure, Category = "Planet|LegacyFlatSurface", meta = (DeprecatedFunction, DeprecationMessage = "Use GetRadialUpVector or GetSurfaceFrameAt."))
	FVector SurfaceLocalToWorld(const FVector& SurfaceLocalPosition) const;

	UFUNCTION(BlueprintPure, Category = "Planet|LegacyFlatSurface", meta = (DeprecatedFunction, DeprecationMessage = "Real planets have no authoritative flat surface frame."))
	FVector WorldToSurfaceLocal(const FVector& WorldPosition) const;

	UFUNCTION(BlueprintPure, Category = "Planet|LegacyFlatSurface", meta = (DeprecatedFunction, DeprecationMessage = "Use GetRadialUpVector or GetSurfaceNormalAt."))
	FVector GetSurfaceUpVector() const;

	UFUNCTION(BlueprintPure, Category = "Planet|LegacyFlatSurface", meta = (DeprecatedFunction, DeprecationMessage = "Use GetApproximateAltitude or TraceToSurface."))
	float GetSurfaceAltitude(const FVector& WorldPosition) const;

	UFUNCTION(BlueprintPure, Category = "Planet|LegacyFlatSurface", meta = (DeprecatedFunction, DeprecationMessage = "A real planet centre is its actor location or CenterActor."))
	FVector GetRecommendedExteriorCenter() const;

	UFUNCTION(BlueprintCallable, Category = "Planet|LegacyFlatSurface", meta = (DeprecatedFunction, DeprecationMessage = "A real planet centre is configured by the anchor transform or CenterActor."))
	void SetExteriorCenterToRecommended();

	bool HasSurfaceLevel() const;
	const TSoftObjectPtr<UWorld>& GetSurfaceLevel() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Travel")
	float GetTakeoffTransitionAltitude() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Travel")
	float GetSpaceFlightAltitude() const;

	UFUNCTION(BlueprintPure, Category = "Planet|LegacyFlatSurface", meta = (DeprecatedFunction, DeprecationMessage = "Use planet content streaming instead of a flat surface level."))
	float GetSurfaceUnloadAltitude() const;

	UFUNCTION(BlueprintPure, Category = "Planet|LegacyFlatSurface", meta = (DeprecatedFunction, DeprecationMessage = "Use planet content streaming instead of a flat surface level."))
	float GetSurfaceLoadAltitude() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Travel")
	float GetApproachTransitionAltitude() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Landing")
	float GetLandingApproachRange() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Landing")
	float GetLandingAssistAltitude() const;

	UFUNCTION(BlueprintPure, Category = "Planet|State")
	bool IsActivePlanet() const;

	void SetActivePlanet(bool bInIsActivePlanet);

private:
	bool TraceRadialDirectionToSurface(const FVector& RadialDirection, float OuterTraceRadius, FJTSPlanetSurfaceHit& OutSurfaceHit) const;
	bool IsGameplaySurfaceComponent(const UPrimitiveComponent* Candidate) const;
	FVector GetFallbackTangent(const FVector& UpVector) const;
	FTransform BuildSurfaceTransform(const FJTSPlanetSurfaceHit& SurfaceHit, const FVector& PreferredForward) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Stable logical identifier. It intentionally does not depend on a LocalAssets mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet", meta = (AllowPrivateAccess = "true"))
	FName PlanetId = TEXT("Moon");

	/** Optional centre override. If unset, GetActorLocation() is the planet centre. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> CenterActor;

	/** Coarse radius for altitude approximation, approach, spherical spawning, and arc distance; never gravity direction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float ApproximateRadius = 1800.0f;

	/** The manually placed real gameplay mesh actor, for example MoonPlanet in L_SpaceWorld. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|Surface", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> GameplaySurfaceActor;

	/** Optional precise component override when GameplaySurfaceActor has more than one collision primitive. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|Surface", meta = (AllowPrivateAccess = "true", UseComponentPicker, AllowAnyActor))
	TObjectPtr<UPrimitiveComponent> GameplaySurfaceComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Surface", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> SurfaceTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Surface", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float SurfaceTraceOuterPadding = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDebugPlanetSurface = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Gravity", meta = (AllowPrivateAccess = "true"))
	bool bEnableGravity = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Gravity", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float GravityStrength = 980.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Gravity", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float GravityInfluenceRange = 12000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Travel", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float SpaceExitRange = 15000.0f;

	/** Optional future per-planet content level. It is not a flat gameplay-surface authority. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Content", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UWorld> PlanetContentLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Content", meta = (AllowPrivateAccess = "true"))
	FTransform PlanetContentTransform = FTransform::Identity;

	/** An authored landing/POI actor remains valid on a real mesh. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|Landing", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> LandingAnchorActor;

	/** Retained solely for existing Blueprint defaults. New content should use LandingAnchorActor or surface queries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|LegacyFlatSurface", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use LandingAnchorActor or SnapActorToPlanetSurface."))
	FTransform LandingAnchorTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|LegacyFlatSurface", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use LandingAnchorActor or surface queries."))
	bool bUseLegacyLandingAnchorTransform = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|Approach", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> ApproachEntryAnchorActor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Approach", meta = (AllowPrivateAccess = "true"))
	bool bUseApproachEntryTransform = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Approach", meta = (AllowPrivateAccess = "true", EditCondition = "bUseApproachEntryTransform"))
	FTransform ApproachEntryTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Approach", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float DefaultApproachEntryDistance = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Travel", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float TakeoffTransitionAltitude = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Travel", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float SpaceFlightAltitude = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|LegacyFlatSurface", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use optional planet content streaming thresholds."))
	float SurfaceUnloadAltitude = 15000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|LegacyFlatSurface", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use optional planet content streaming thresholds."))
	float SurfaceLoadAltitude = 12000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Approach", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float ApproachTransitionAltitude = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Landing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float LandingApproachRange = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Landing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float LandingAssistAltitude = 1200.0f;

	/** ---- Retained flat-surface fields: serialized compatibility only; no real-planet runtime path reads them. ---- */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|LegacyFlatSurface", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use ApproximateRadius."))
	float PlanetRadius = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|LegacyFlatSurface", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "A real planet centre comes from the anchor transform or CenterActor."))
	FVector ExteriorCenter = FVector::ZeroVector;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|LegacyFlatSurface", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "The gameplay mesh itself is the planet visual."))
	TObjectPtr<AActor> ExteriorVisualActor;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|LegacyFlatSurface", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Real planets have no authoritative flat surface anchor."))
	TObjectPtr<AActor> SurfaceAnchorActor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|LegacyFlatSurface", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Real planets have no authoritative flat surface frame."))
	FTransform SurfaceAnchorTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|LegacyFlatSurface", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use PlanetContentLevel only for optional future content streaming."))
	TSoftObjectPtr<UWorld> SurfaceLevel;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Planet|State", meta = (AllowPrivateAccess = "true"))
	bool bIsActivePlanet = false;
};
