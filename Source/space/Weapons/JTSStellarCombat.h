#pragma once
#include "CoreMinimal.h"
class APawn;
class UWorld;
namespace JTSStellarCombat
{
	FVector SurfaceUp(APawn* Pawn, FVector Position);
	FVector CastOrigin(APawn* Pawn);
	FVector AimPoint(APawn* Pawn, FVector Direction, float Range, FHitResult* OutHit = nullptr);
	void DamageArea(APawn* Pawn, FVector Center, float Radius, float Damage, int32 Limit, TSet<AActor*>* Excluded = nullptr);
}
