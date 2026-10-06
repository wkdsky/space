#pragma once

#include "space/Systems/JTSPlanetEnemyFragments.h"

/** Pure pursuit state rules; no scans, navigation queries, timers or actor movement here. */
struct FJTSPlanetEnemyPursuit
{
	static float Leash(const FJTSPlanetEnemyBehavior& Behavior, const FJTSPlanetEnemyPerceptionFragment& Perception);
	static bool CanAcquire(const AJTSPlanetAnchor* Planet, const FVector& Home, const AJTSCharacter* Player,
		const FJTSPlanetEnemyNavigationFragment& Navigation, const FJTSPlanetEnemyPerceptionFragment& Perception,
		const FJTSPlanetEnemyBehavior& Behavior, float Now, bool bProvokedReport = false);
	static bool Retaliate(AJTSPlanetAnchor* Planet, FJTSPlanetEnemyNavigationFragment& Navigation,
		FJTSPlanetEnemyPerceptionFragment& Perception, const FJTSPlanetEnemyBehavior& Behavior,
		AJTSCharacter* Attacker, float Now);
	static void Update(AJTSPlanetAnchor* Planet, const FVector& Ground,
		FJTSPlanetEnemyNavigationFragment& Navigation, FJTSPlanetEnemyPerceptionFragment& Perception,
		const FJTSPlanetEnemyBehavior& Behavior, float Now);
};
