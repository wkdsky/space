#pragma once

#include "CoreMinimal.h"

class AJTSCharacter;
class AJTSPlanetAnchor;
class AJTSSpacecraftActor;
class AJTSSpaceWorldManager;

/** Presentation only. Flight/landing authority remains with the existing gameplay systems. */
enum class EJTSNavigationMapStage : uint8 { Surface, System, Interstellar };

struct FJTSNavigationMapBody
{
	FName Id;
	FString Name;
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D PlanePosition = FVector2D::ZeroVector;
	float Height = 0.0f;
	float Radius = 0.05f;
	float Gravity = 1.0f;
	int32 Palette = 2;
	bool bCharted = true;
	bool bFocus = false;
	bool bPrimary = false;
};

struct FJTSNavigationMapSystem
{
	FName Id;
	FString Name;
	FVector2D Position = FVector2D::ZeroVector;
	float Elevation = 0.0f;
	float RangeCentimeters = 0.0f;
	int32 Palette = 2;
	bool bActive = false;
};

/** Current orthographic surface view. Updated continuously around the local player. */
struct FJTSNavigationTerrainPatch
{
	FVector Origin = FVector::ZeroVector;
	FVector Up = FVector::UpVector;
	FVector East = FVector::RightVector;
	FVector North = -FVector::ForwardVector;
	double Radius = 1.0;
	float HalfWidth = 6000.0f;
	FVector2D Project(const FVector& WorldPosition, const FVector& PlanetCenter) const;
};

struct FJTSNavigationMapFrame
{
	EJTSNavigationMapStage Stage = EJTSNavigationMapStage::Interstellar;
	FName SystemId;
	FString SystemName;
	FString PlanetName;
	FString Coordinates;
	TArray<FJTSNavigationMapBody> Bodies;
	TArray<FJTSNavigationMapSystem> Systems;
	TSharedPtr<const FJTSNavigationTerrainPatch> Terrain;
	FVector2D ShipPosition = FVector2D::ZeroVector;
	FVector2D PlayerPosition = FVector2D::ZeroVector;
	FVector2D PlayerHeading = FVector2D(0, -1);
	FVector2D ShipHeading = FVector2D(0, -1);
	float Altitude = 0.0f;
	float ShipSurfaceDistance = 0.0f;
	float GroundHalfWidth = 6000.0f;
	float TransitionSeconds = 1.4f;
	float ShipBearing = 0.0f;
	bool bGroundMap = false;
	bool bPlayerOnFoot = false;
	bool bHasShip = false;
};

/** Collects local UI data. No Actor Tick, RPC, spawning, or gameplay mutations. */
class SPACE_API FJTSNavigationMapModel
{
public:
	FJTSNavigationMapFrame Build(const AJTSSpaceWorldManager* Manager,
		const AJTSSpacecraftActor* Ship, const AJTSCharacter* Character);
	static FVector2D SurfaceCoordinates(const FVector& Position, const FVector& Center, const FQuat& PlanetRotation);
	static double AdvanceZoom(double Current, double Target, double DeltaSeconds, double TransitionSeconds);
	static FVector2D SystemDialPosition(float BearingDegrees, float RangeCentimeters);
	/** Ship-local spherical scanner: X forward, Y right, Z up. Same mapping for every contact. */
	static FVector ProjectSystemContact(const FVector& Delta, const FQuat& ShipRotation, double Range);

private:
	FName ActiveSystem;
	EJTSNavigationMapStage PreviousStage = EJTSNavigationMapStage::Interstellar;
	TWeakObjectPtr<const AActor> FocusBody;
	TSharedPtr<FJTSNavigationTerrainPatch> Terrain;
	void UpdateTerrain(const AJTSPlanetAnchor* Planet, const FVector& Position, float HalfWidth);
};
