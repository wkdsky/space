#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "JTSPlanetFragmentEnvelopeComponent.generated.h"

/** A convex wrap of one fragment, in full-size planet-local centimetres. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSFragmentHull
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FVector> Vertices;
	/** Outward-wound triangle indices. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<int32> Triangles;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector BoundsMin = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector BoundsMax = FVector::ZeroVector;
};

/** Authored geometry only; no mesh paths or level references in gameplay code. */
UCLASS(BlueprintType)
class SPACE_API UJTSFragmentEnvelopeData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FJTSFragmentHull> Fragments;
	/** Tight enclosing ellipsoid, centred on the explosion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector RegionRadii = FVector::OneVector;
};

/** Stateless geometry queries; the existing ship envelope owns flight and replicated frames. */
UCLASS(ClassGroup = (Planet), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSPlanetFragmentEnvelopeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSPlanetFragmentEnvelopeComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fragments") TObjectPtr<UJTSFragmentEnvelopeData> Data;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fragments") FRotator GeometryRotation = FRotator::ZeroRotator;
	bool HasGeometry() const { return Data && !Data->Fragments.IsEmpty(); }
	/** Actual shortest distance outside the ellipsoid. All interior gaps return zero. */
	UFUNCTION(BlueprintPure, Category = "Fragments") double GetRegionAltitude(const FVector& WorldPosition) const;
	/** Signed distance to the union of rounded wraps; normals are local contact normals, not gravity. */
	bool QuerySurface(const FVector& WorldPosition, double Padding, double SearchDistance, double SmoothWidth,
		double& OutHeight, FVector& OutNormal, FVector& OutGround) const;
};
