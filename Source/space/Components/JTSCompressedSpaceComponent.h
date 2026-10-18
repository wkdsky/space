#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSCompressedSpaceComponent.generated.h"

class AJTSPlanetAnchor;

/** All concrete references, astronomical coordinates and art adjustments belong to the level. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSCompressedCelestialBody
{
	GENERATED_BODY()
	/** A gameplay PlanetAnchor, or a visual-only background actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AActor> Actor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector NavigationPositionKilometers = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.001")) double RadiusKilometers = 1.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1.0")) double VisualRadiusMultiplier = 1.0;
};

/** Continuous compressed coordinates shared by distance, velocity and celestial presentation.
 * No Tick, authority mutations, asset selection, or planet gameplay lives here.
 */
UCLASS(ClassGroup=(Space), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSCompressedSpaceComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSCompressedSpaceComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space") TArray<FJTSCompressedCelestialBody> Bodies;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space", meta=(ClampMin="1.0")) double NearAltitudeCentimeters = 17690.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space", meta=(ClampMin="0.001")) double FreeSpaceKilometersPerCentimeter = 60.0;
	bool IsConfigured() const { return !Bodies.IsEmpty(); }
	UFUNCTION(BlueprintCallable, Category="Space") void CaptureAuthoredBodies(bool bRestorePrevious = true);
	UFUNCTION(BlueprintPure, Category="Space") FVector NavigationPosition(const FVector& WorldPosition) const;
	UFUNCTION(BlueprintPure, Category="Space") FVector NavigationVelocity(const FVector& Position, const FVector& Velocity) const;
	UFUNCTION(BlueprintPure, Category="Space") double RangeCentimeters(const FVector& Position, const AActor* Body) const;
	UFUNCTION(BlueprintPure, Category="Space") double RouteKilometers(const AActor* First, const AActor* Second) const;
	UFUNCTION(BlueprintPure, Category="Space") AJTSPlanetAnchor* ContainingPlanet(const FVector& Position) const;
	UFUNCTION(BlueprintPure, Category="Space") double AuthoredRadius(const AActor* Body) const;
	UFUNCTION(BlueprintPure, Category="Space") double VisualScale(const FVector& Position, const AActor* Body) const;
	UFUNCTION(BlueprintPure, Category="Space") double DisplayRadiusKilometers(const AActor* Body) const;
	UFUNCTION(BlueprintPure, Category="Space") double BubbleRadius(const AActor* Body) const;
	/** Finds the earliest gameplay bubble crossed by a movement segment, including fast fly-throughs. */
	bool ClipAtFirstBubbleEntry(const FVector& Start, const FVector& End, FVector& OutPosition) const;
	double LocalKilometersPerCentimeter() const { return 14.0 / NearAltitudeCentimeters; }
	bool ContainsBody(const AActor* Body) const;
	void ApplyPresentation(const FVector& Position);
	void RestorePresentation();
private:
	struct FCapturedBody
	{
		TWeakObjectPtr<AActor> Body;
		TWeakObjectPtr<AActor> Visual;
		FVector Center, Location, Scale, Navigation;
		double Radius = 1.0, PhysicalRadius = 1.0, Multiplier = 1.0;
		bool bCollisionEnabled = false;
	};
	struct FCoordinateKnot { double World, Navigation; };
	TArray<FCapturedBody> Captured;
	TArray<FCoordinateKnot> Axes[3];
	const FCapturedBody* Find(const AActor* Body) const;
	double MapAxis(int32 Axis, double Value) const;
	FVector BasePosition(const FVector& Position) const;
};
