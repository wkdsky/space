#pragma once

#include "CoreMinimal.h"
#include "JTSCelestialNavigationTypes.generated.h"

class AJTSPlanetAnchor;

/** Map presentation tuning. Concrete system membership uses CompressedSpace.Bodies.PresentationGroup. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSNavigationMapSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.2", Units = "s")) float TransitionSeconds = 1.4f;
	/** Radius of a system map relative to its outermost member's orbit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.2")) float SystemExtentMultiplier = 1.8f;
	/** Isolated bodies still have a local region outside the near-flight bubble. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "2")) float MinimumSystemRadii = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "0.4")) float StageHysteresis = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "500", Units = "cm")) float GroundMapHalfWidth = 6000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "128", ClampMax = "1024")) int32 GroundMapResolution = 512;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1000", Units = "cm")) float GroundCaptureHeight = 20000.0f;
};

/** Level-authored navigation metadata for an existing visual-only local-sky member. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSCelestialBackgroundNavigation
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AActor> Actor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "2")) int32 PaletteIndex = 2;
	/** Local mesh-space radius. Zero derives a mean radius from mesh bounds. Follows visual scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0")) float LocalVisualRadius = 0.0f;
	/** Backgrounds are scenery unless the level explicitly enables navigation UI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bShowOnStarChart = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bShowScreenMarker = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", Units = "cm")) float MarkerDistanceCentimeters = 0.0f;
};

/** Read-only presentation snapshot. A background never becomes a gameplay planet. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSCelestialNavigationBody
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FText DisplayName;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> VisualActor;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AJTSPlanetAnchor> Planet;
	UPROPERTY(BlueprintReadOnly) FVector Center = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) float VisualRadius = 0.0f;
	/** Full-size fragment geometry follows the same presentation ratio as its visible mesh. */
	UPROPERTY(BlueprintReadOnly) double FragmentPresentationScale = 1.0;
	UPROPERTY(BlueprintReadOnly) float RangeCentimeters = 0.0f;
	/** Shared ship-to-centre trigger range: simulated cm when configured, otherwise legacy world cm. Never camera distance. */
	UPROPERTY(BlueprintReadOnly) float ShipDistanceCentimeters = 0.0f;
	UPROPERTY(BlueprintReadOnly) float MarkerDistanceCentimeters = 0.0f;
	UPROPERTY(BlueprintReadOnly) int32 PaletteIndex = 2;
	UPROPERTY(BlueprintReadOnly) bool bVisible = false;
	UPROPERTY(BlueprintReadOnly) bool bShowOnStarChart = true;
	UPROPERTY(BlueprintReadOnly) bool bShowScreenMarker = true;
};
