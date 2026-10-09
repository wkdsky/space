#pragma once
#include "CoreMinimal.h"
class APawn;
class UWorld;
struct FCollisionQueryParams;
namespace JTSStellarCombat
{
	FVector SurfaceUp(APawn* Pawn, FVector Position);
	FVector CastOrigin(APawn* Pawn);
	FVector AimPoint(APawn* Pawn, FVector Direction, float Range, FHitResult* OutHit = nullptr);
	/** A wide beam brushing terrain must still hit a directly aimed, unobstructed small target. */
	bool TraceBeam(UWorld* World, const FVector& Start, const FVector& End, float Radius,
		const FCollisionQueryParams& Params, FHitResult& OutHit);
	void DamageArea(APawn* Pawn, FVector Center, float Radius, float Damage, int32 Limit, TSet<AActor*>* Excluded = nullptr);
}
