#include "space/Systems/JTSPlanetEnemyPursuit.h"

#include "space/Player/JTSCharacter.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/World/JTSPlanetAnchor.h"

namespace
{
	bool Eligible(const AJTSCharacter* Player, const AJTSPlanetAnchor* Planet)
	{
		return IsValid(Player) && Player->GetGameplayPlanet() == Planet && !Player->IsBoarded()
			&& (!Player->GetHealthComponent() || !Player->GetHealthComponent()->IsDead());
	}
	void ReturnHome(FJTSPlanetEnemyNavigationFragment& Navigation, FJTSPlanetEnemyPerceptionFragment& Perception, float Now)
	{
		Navigation.bReturningHome = true;
		Navigation.RouteUntilTime = Now + 8;
		Navigation.AvoidanceDirection = FVector::ZeroVector;
		if (Perception.bProvokedPursuit)
			Perception.CompletedRetaliationUntilTime = FMath::Max(Perception.CompletedRetaliationUntilTime, Perception.RetaliationUntilTime);
		Perception.Target.Reset(); Perception.bCurrentlyVisible = false;
		Perception.bProvokedPursuit = false; Perception.RetaliationUntilTime = -1000;
		Perception.LastSensedTime = -1000;
	}
}

float FJTSPlanetEnemyPursuit::Leash(const FJTSPlanetEnemyBehavior& Behavior,
	const FJTSPlanetEnemyPerceptionFragment& Perception)
{
	return Perception.bProvokedPursuit ? FMath::Max(Behavior.LeashRadius, Behavior.RetaliationLeashRadius) : Behavior.LeashRadius;
}

bool FJTSPlanetEnemyPursuit::CanAcquire(const AJTSPlanetAnchor* Planet, const FVector& Home,
	const AJTSCharacter* Player, const FJTSPlanetEnemyNavigationFragment& Navigation,
	const FJTSPlanetEnemyPerceptionFragment& Perception, const FJTSPlanetEnemyBehavior& Behavior, float Now, bool bProvokedReport)
{
	if (!IsValid(Planet) || !Eligible(Player, Planet) || Navigation.bReturningHome || Now < Perception.NextAcquireAllowedTime) return false;
	const float Radius = bProvokedReport ? FMath::Max(Behavior.LeashRadius, Behavior.RetaliationLeashRadius)
		: (Perception.Target.Get() == Player ? Leash(Behavior, Perception) : FMath::Min(Behavior.AggroRadius, Behavior.LeashRadius));
	return Planet->ApproximateSurfaceArcDistance(Home, Player->GetActorLocation()) <= FMath::Max(0.0f, Radius);
}

bool FJTSPlanetEnemyPursuit::Retaliate(AJTSPlanetAnchor* Planet, FJTSPlanetEnemyNavigationFragment& Navigation,
	FJTSPlanetEnemyPerceptionFragment& Perception, const FJTSPlanetEnemyBehavior& Behavior, AJTSCharacter* Attacker, float Now)
{
	if (!CanAcquire(Planet, Navigation.HomeLocation, Attacker, Navigation, Perception, Behavior, Now, true)) return false;
	const bool bOutsideGuard = Planet->ApproximateSurfaceArcDistance(Navigation.HomeLocation, Attacker->GetActorLocation())
		> FMath::Min(Behavior.AggroRadius, Behavior.LeashRadius);
	if (Perception.bProvokedPursuit && Now >= Perception.RetaliationUntilTime)
	{ ReturnHome(Navigation, Perception, Now); return false; }
	if (bOutsideGuard && !Perception.bProvokedPursuit)
	{
		if (Behavior.RetaliationMemorySeconds <= 0) return false;
		Perception.bProvokedPursuit = true;
		Perception.RetaliationUntilTime = Now + Behavior.RetaliationMemorySeconds;
	}
	else if (!Perception.bProvokedPursuit)
		Perception.RetaliationUntilTime = Now + FMath::Max(0.0f, Behavior.RetaliationMemorySeconds);
	// Repeated damage may update the last reported position, but never extend this episode's deadline.
	Perception.Target = Attacker; Perception.LastKnownLocation = Attacker->GetActorLocation();
	Perception.LastSensedTime = Now; Perception.bCurrentlyVisible = false; Perception.NextScanTime = Now;
	return true;
}

void FJTSPlanetEnemyPursuit::Update(AJTSPlanetAnchor* Planet, const FVector& Ground,
	FJTSPlanetEnemyNavigationFragment& Navigation, FJTSPlanetEnemyPerceptionFragment& Perception,
	const FJTSPlanetEnemyBehavior& Behavior, float Now)
{
	if (!IsValid(Planet)) return;
	const float HomeDistance = Planet->ApproximateSurfaceArcDistance(Navigation.HomeLocation, Ground);
	if (Navigation.bReturningHome)
	{
		if (HomeDistance <= FMath::Max(70.0f, Behavior.HomeReturnRadius))
		{
			Navigation.bReturningHome = false; Navigation.NextRoamTime = Now;
			Perception.NextAcquireAllowedTime = Now + FMath::Max(0.0f, Behavior.ReacquireCooldown);
		}
		return;
	}
	if (Perception.Target.IsExplicitlyNull()) return;
	const AJTSCharacter* Target = Perception.Target.Get();
	const float Limit = Leash(Behavior, Perception);
	const bool bExpired = Perception.bProvokedPursuit ? Now >= Perception.RetaliationUntilTime
		: Now > FMath::Max(Perception.LastSensedTime + Behavior.TargetMemorySeconds, Perception.RetaliationUntilTime);
	if (!Eligible(Target, Planet) || bExpired || HomeDistance > Limit
		|| Planet->ApproximateSurfaceArcDistance(Navigation.HomeLocation, Target->GetActorLocation()) > Limit)
		ReturnHome(Navigation, Perception, Now);
}
