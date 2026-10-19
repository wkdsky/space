#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Components/JTSSpacecraftInterferenceComponent.h"
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
	/** Bodies in one local system retain their authored size/separation ratios through one shared transform.
	 * None keeps the independent presentation used for isolated bodies. Does not merge gameplay bubbles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName PresentationGroup;
	/** Opt visual-only bodies into flight proximity without creating gravity, landing or collision. Gameplay anchors always participate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnableNearFlight = true;
	/** Optional server-driven narrative intercept before this body's near-flight boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FJTSFlightInterferenceSettings Interference;
	/** Local handling for a visual-only body; gameplay anchors use their own size adaptation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1.0")) double NearFlightSpeedMultiplier = 1.0;
	/** Visual-only bodies receive the same full-size handling adaptation as gameplay anchors. Zero disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", Units="cm")) double NearFlightReferenceRadius = 21000.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) double NearFlightSizeExponent = 0.5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) double NearFlightMaxSpeedMultiplier = 3.0;
	/** Width outside the near shell, proportional to the full authored radius; independent of navigation markers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01")) double FlightTransitionDistanceRadii = 80.0;
	/** Level-owned retained width: resizing or retuning cannot shrink an established slowdown region. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", Units="cm")) double FlightTransitionMinimumWidth = 0.0;
	/** Smaller positive values make attenuation gentler without moving the outer boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01", ClampMax="12")) double FlightApproachExponent = 1.0;
	/** Soft geometric change budget, combined with the relative budget below. Zero for both disables adaptation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", Units="deg/s")) double FlightAngularChangeDegreesPerSecond = 8.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) double FlightRelativeAngularChangePerSecond = 0.65;
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
	/** Padding beyond the entire group's enclosed near-flight shells, in group radii. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space|Presentation", meta=(ClampMin="0")) double GroupLocalPaddingRadii = 0.25;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space|Presentation", meta=(ClampMin="1")) double GroupTransitionRadii = 4.0;
	bool IsConfigured() const { return !Bodies.IsEmpty(); }
	UFUNCTION(BlueprintCallable, Category="Space") void CaptureAuthoredBodies(bool bRestorePrevious = true);
	UFUNCTION(BlueprintPure, Category="Space") FVector NavigationPosition(const FVector& WorldPosition) const;
	UFUNCTION(BlueprintPure, Category="Space") FVector NavigationVelocity(const FVector& Position, const FVector& Velocity) const;
	UFUNCTION(BlueprintPure, Category="Space") double RangeCentimeters(const FVector& Position, const AActor* Body) const;
	UFUNCTION(BlueprintPure, Category="Space") double RouteKilometers(const AActor* First, const AActor* Second) const;
	UFUNCTION(BlueprintPure, Category="Space") AJTSPlanetAnchor* ContainingPlanet(const FVector& Position) const;
	/** Includes opted-in non-collidable bodies; never makes them a gameplay/landing planet. */
	UFUNCTION(BlueprintPure, Category="Space") AActor* ContainingNearFlightBody(const FVector& Position) const;
	UFUNCTION(BlueprintPure, Category="Space") double AuthoredRadius(const AActor* Body) const;
	UFUNCTION(BlueprintPure, Category="Space") FVector AuthoredCenter(const AActor* Body) const;
	/** Rendered centre, which follows the group's shared transform; never a gravity/landing reference. */
	UFUNCTION(BlueprintPure, Category="Space") FVector PresentationCenter(const FVector& Position, const AActor* Body) const;
	UFUNCTION(BlueprintPure, Category="Space") double FlightTransitionWidth(const AActor* Body) const;
	UFUNCTION(BlueprintPure, Category="Space") double VisualScale(const FVector& Position, const AActor* Body) const;
	UFUNCTION(BlueprintPure, Category="Space") double DisplayRadiusKilometers(const AActor* Body) const;
	double ApparentDiameterRadians(const FVector& Position, const AActor* Body) const;
	UFUNCTION(BlueprintPure, Category="Space") double BubbleRadius(const AActor* Body) const;
	/** Continuous physical speed field: all overlapping body contributions are normalized and blended in log-speed space.
	 * The unboosted FlightDirection command supplies geometric growth/shrink rate; reversing it gives the same field.
	 * Ordinary/Shift retain their input ratio. No camera, visibility or marker query participates.
	 * Marker distances and anisotropic NAV derivatives cannot cause a physical crawl. Exponent <= 0 disables outer fields.
	 * NearBoundaryMultiplier is retained for existing Blueprint callers; the field joins local handling directly.
	 */
	UFUNCTION(BlueprintPure, Category="Space") double CruiseApproachMultiplier(const FVector& Position,
		const FVector& FlightDirection, double CruiseMultiplier, double NearBoundaryMultiplier, double Exponent) const;
	/** Bounds a cruise simulation step by remaining near-bubble clearance, including before marker entry. */
	double CruiseSimulationStep(const FVector& Position, const FVector& WorldVelocity, double RemainingSeconds) const;
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
		FName GroupName;
		int32 GroupIndex = INDEX_NONE;
		double Radius = 1.0, PhysicalRadius = 1.0, Multiplier = 1.0;
		bool bCollisionEnabled = false;
		bool bNearFlight = false;
		double NearSpeed = 1.0, TransitionWidth = 1.0, ApproachExponent = 1.0;
		double AngularChange = 0.0, RelativeAngularChange = 0.0;
	};
	struct FCapturedGroup
	{
		FName Name;
		int32 PrimaryIndex = INDEX_NONE;
		double Inner = 1.0, Outer = 1.0;
	};
	struct FCoordinateKnot { double World, Navigation; };
	TArray<FCapturedBody> Captured;
	TArray<FCapturedGroup> Groups;
	TArray<FCoordinateKnot> Axes[3];
	const FCapturedBody* Find(const AActor* Body) const;
	double MapAxis(int32 Axis, double Value) const;
	FVector BasePosition(const FVector& Position) const;
	double GroupVisualScale(const FVector& Position, const FCapturedGroup& Group) const;
};
