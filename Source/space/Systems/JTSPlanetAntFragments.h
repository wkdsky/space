#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "space/Components/JTSPlanetSurfaceSteeringComponent.h"
#include "space/World/JTSMoonAntActor.h"
#include "JTSPlanetAntFragments.generated.h"

/** Snapshot of Blueprint/data-asset rules; no level, mesh or asset paths belong here. */
USTRUCT()
struct FJTSPlanetAntConfigFragment : public FMassFragment
{
	GENERATED_BODY()
	float EmergingDuration = 0.6f, ReactionDuration = 0.25f, BurrowDuration = 0.6f;
	float SurfaceDurationMin = 90, SurfaceDurationMax = 150;
	float RoamSpeed = 85, RoamRadius = 6000, HomeRadius = 7000, NearRadius = 900;
	float RetargetMin = 2.5f, RetargetMax = 6;
	float FleeSpeed = 180, FleeDurationMin = 1, FleeDurationMax = 2, FleeDistance = 500;
	float TurnDegrees = 240, SupportHeight = 4, BurrowDepth = 8;
};

/** All authoritative ant state and steering memory lives in the entity, not Actor Tick. */
USTRUCT()
struct FJTSPlanetAntActivityFragment : public FMassFragment
{
	GENERATED_BODY()
	EJTSMoonAntState Phase = EJTSMoonAntState::Emerging;
	float PhaseElapsed = 0, SurfaceElapsed = 0, SurfaceDuration = 0;
	float ActivityRadius = 0, BlockedElapsed = 0, FleeElapsedDistance = 0, FleeDuration = 0;
	float BurrowOffset = 0;
	FVector FleeSource = FVector::ZeroVector;
	FVector FleeDirection = FVector::ZeroVector;
	FJTSPlanetSurfaceSteeringState Traversal;
	bool bDespawnRequested = false;
};
