#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;
class AJTSPlanetAnchor;
struct FJTSPlanetCrowdNavigationImpl;

/** Shared, time-sliced reverse-Dijkstra fields on the real spherical mesh.
 * Only navigation geometry and route costs live here; perception and combat remain in the Mass host.
 */
class FJTSPlanetCrowdNavigation
{
public:
	FJTSPlanetCrowdNavigation();
	~FJTSPlanetCrowdNavigation();
	void Tick(UWorld* World, float TimeSeconds, int32 PhysicsQueryBudget = 64, int32 IntegrationBudget = 1024);
	FVector GetDirection(AJTSPlanetAnchor* Planet, const FVector& Home, const FVector& Position,
		const FVector& Goal, const AActor* GoalActor, float RoamRadius, float LeashRadius, float TimeSeconds);
	int32 GetLastPhysicsQueryCount() const;
	int32 GetReadyFieldCount() const;
private:
	TUniquePtr<FJTSPlanetCrowdNavigationImpl> Impl;
};
