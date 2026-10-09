#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSPlanetSurfaceSteeringComponent.generated.h"

class AJTSPlanetAnchor;
struct FJTSPlanetSurfaceHit;

/** Traversal memory can live in a Mass fragment instead of the presentation component. */
struct FJTSPlanetSurfaceSteeringState
{
	FVector PreviousHeading = FVector::ZeroVector;
	FVector AvoidanceHeading = FVector::ZeroVector;
	float AvoidanceRemaining = 0.0f;
	float PreferredSide = 0.0f;
};

/** Local surface traversal for small creatures. Called by their existing update, never ticks independently. */
UCLASS(ClassGroup = (Planet), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSPlanetSurfaceSteeringComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSPlanetSurfaceSteeringComponent();
	bool IsWalkable(const AJTSPlanetAnchor* Planet, const FJTSPlanetSurfaceHit& Hit) const;
	bool Advance(AJTSPlanetAnchor* Planet, const FVector& Ground, const FVector& DesiredDirection,
		float Speed, float DeltaSeconds, FVector& OutGround, FVector& OutHeading);
	bool AdvanceWithState(AJTSPlanetAnchor* Planet, const FVector& Ground, const FVector& DesiredDirection,
		float Speed, float DeltaSeconds, FJTSPlanetSurfaceSteeringState& State,
		FVector& OutGround, FVector& OutHeading) const;
	/** External forces keep their direction; constrain the whole segment without voluntary steering. */
	bool ConstrainDisplacement(AJTSPlanetAnchor* Planet, const FVector& Ground, const FVector& Displacement,
		FVector& OutGround, FVector& OutHeading) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Traversal", meta = (ClampMin = "0", ClampMax = "85"))
	float MaxSlopeDegrees = 35.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Traversal", meta = (ClampMin = "1"))
	float ProbeDistance = 65.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Traversal", meta = (ClampMin = "1"))
	float BodyRadius = 12.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Traversal", meta = (ClampMin = "0.1"))
	float AvoidanceCommitSeconds = 1.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Traversal", meta = (ClampMin = "1"))
	float HeadingResponse = 5.0f;

private:
	bool Probe(AJTSPlanetAnchor* Planet, const FVector& Ground, const FVector& Heading,
		float Distance, FJTSPlanetSurfaceHit& OutHit) const;
	FJTSPlanetSurfaceSteeringState StandaloneState;
};
