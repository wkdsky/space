#pragma once

#include "CoreMinimal.h"
#include "JTSCelestialNavigationTypes.generated.h"

class AJTSPlanetAnchor;

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
	UPROPERTY(BlueprintReadOnly) float RangeCentimeters = 0.0f;
	/** Shared ship-to-centre trigger range: simulated cm when configured, otherwise legacy world cm. Never camera distance. */
	UPROPERTY(BlueprintReadOnly) float ShipDistanceCentimeters = 0.0f;
	UPROPERTY(BlueprintReadOnly) float MarkerDistanceCentimeters = 0.0f;
	UPROPERTY(BlueprintReadOnly) int32 PaletteIndex = 2;
	UPROPERTY(BlueprintReadOnly) bool bVisible = false;
	UPROPERTY(BlueprintReadOnly) bool bShowOnStarChart = true;
	UPROPERTY(BlueprintReadOnly) bool bShowScreenMarker = true;
};
