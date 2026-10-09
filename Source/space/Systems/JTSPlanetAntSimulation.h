#pragma once

#include "CoreMinimal.h"

struct FJTSPlanetEnemyActorFragment;
struct FJTSPlanetEnemyMovementFragment;
struct FJTSPlanetEnemyNavigationFragment;
struct FJTSPlanetAntConfigFragment;
struct FJTSPlanetAntActivityFragment;

/** Ant activity processing within the existing planet enemy Mass host. No independent Tick. */
struct FJTSPlanetAntSimulation
{
	static void Advance(const FJTSPlanetEnemyActorFragment& Binding,
		FJTSPlanetEnemyMovementFragment& Movement, FJTSPlanetEnemyNavigationFragment& Navigation,
		const FJTSPlanetAntConfigFragment& Config, FJTSPlanetAntActivityFragment& Activity,
		float Now, float DeltaSeconds);
	static void CommitPresentation(const FJTSPlanetEnemyActorFragment& Binding,
		const FJTSPlanetEnemyMovementFragment& Movement, const FJTSPlanetAntActivityFragment& Activity);
};
