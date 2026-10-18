#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "space/World/JTSCelestialNavigationTypes.h"
#include "JTSCelestialNavigationLibrary.generated.h"

class AJTSSpaceWorldManager;
class AJTSSpacecraftActor;

UCLASS()
class SPACE_API UJTSCelestialNavigationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "Space|Navigation")
	static TArray<FJTSCelestialNavigationBody> GatherBodies(const AJTSSpaceWorldManager* Manager, const AJTSSpacecraftActor* Ship);
	/** Shared range gate, independent of player view, FOV, screen size, and previous visibility. */
	UFUNCTION(BlueprintPure, Category = "Space|Navigation")
	static bool IsWithinMarkerDistance(const FJTSCelestialNavigationBody& Body);

	/** Sphere silhouette in this player's actual projection, including constrained/split viewports. */
	static bool ProjectBody(const FJTSCelestialNavigationBody& Body, const FVector& ViewOrigin,
		const FMatrix& ViewProjection, const FIntRect& ViewRect, FBox2D& OutBounds);
	static bool IsOccluded(const FJTSCelestialNavigationBody& Body, const FVector& ViewOrigin,
		const TArray<FJTSCelestialNavigationBody>& Bodies);
};
