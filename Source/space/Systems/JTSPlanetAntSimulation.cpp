#include "space/Systems/JTSPlanetAntSimulation.h"
#include "space/Systems/JTSPlanetAntFragments.h"
#include "space/Systems/JTSPlanetEnemyFragments.h"
#include "space/Components/JTSStellarTargetComponent.h"
#include "space/World/JTSPlanetAnchor.h"

namespace
{
	void Enter(FJTSPlanetAntActivityFragment& State, EJTSMoonAntState Phase)
	{
		State.Phase = Phase;
		State.PhaseElapsed = 0;
	}

	void ChooseTarget(AJTSPlanetAnchor* Planet, AJTSMoonAntActor* Actor,
		FJTSPlanetEnemyMovementFragment& Movement, FJTSPlanetEnemyNavigationFragment& Navigation,
		const FJTSPlanetAntConfigFragment& Config, const FJTSPlanetAntActivityFragment& Activity, float Now)
	{
		const bool bReturn = Planet->ApproximateSurfaceArcDistance(Movement.GroundLocation, Navigation.HomeLocation) > Config.HomeRadius;
		const float Radius = bReturn ? Config.NearRadius : FMath::Clamp(Activity.ActivityRadius, 60.f, Config.RoamRadius);
		const float Min = FMath::Max(35.f, Radius * .65f);
		const float Max = FMath::Max(Min, FMath::Min(Config.RoamRadius, Radius * 1.35f + 80));
		FJTSPlanetSurfaceFrame Frame;
		FJTSPlanetSurfaceHit Hit;
		bool bFound = false;
		if (Planet->GetSurfaceFrameAt(Navigation.HomeLocation, Actor->GetActorForwardVector(), Frame))
		{
			for (int32 Attempt = 0; Attempt < 12 && !bFound; ++Attempt)
			{
				const float Angle = FMath::FRandRange(0, UE_TWO_PI);
				bFound = Planet->ProjectPointToSurface(Navigation.HomeLocation
					+ (Frame.Forward * FMath::Cos(Angle) + Frame.Right * FMath::Sin(Angle)) * FMath::FRandRange(Min, Max), Hit)
					&& Actor->GetSurfaceSteering()->IsWalkable(Planet, Hit);
			}
		}
		Navigation.RoamTarget = bFound ? Hit.ImpactPoint : Movement.GroundLocation;
		Navigation.NextRoamTime = Now + FMath::FRandRange(Config.RetargetMin, Config.RetargetMax);
	}
}

void FJTSPlanetAntSimulation::Advance(const FJTSPlanetEnemyActorFragment& Binding,
	FJTSPlanetEnemyMovementFragment& Movement, FJTSPlanetEnemyNavigationFragment& Navigation,
	const FJTSPlanetAntConfigFragment& Config, FJTSPlanetAntActivityFragment& State, float Now, float Dt)
{
	auto* Actor = Cast<AJTSMoonAntActor>(Binding.Actor.Get());
	AJTSPlanetAnchor* Planet = Binding.Planet.Get();
	if (!IsValid(Actor) || !IsValid(Planet) || Dt <= 0) return;
	Movement.PreviousLocation = Actor->GetActorLocation();
	Movement.ProposedLocation = Movement.PreviousLocation;
	Movement.ProposedRotation = Actor->GetActorQuat();
	State.PhaseElapsed += Dt;
	FVector Direction = FVector::ZeroVector;
	float Speed = 0;
	switch (State.Phase)
	{
	case EJTSMoonAntState::Emerging:
	{
		const float Alpha = Config.EmergingDuration > SMALL_NUMBER ? FMath::Clamp(State.PhaseElapsed / Config.EmergingDuration, 0.f, 1.f) : 1.f;
		State.BurrowOffset = FMath::Lerp(-Config.BurrowDepth, 0.f, FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f));
		if (Alpha >= 1)
		{
			Enter(State, EJTSMoonAntState::Roaming);
			State.SurfaceElapsed = 0;
			State.SurfaceDuration = FMath::FRandRange(Config.SurfaceDurationMin, Config.SurfaceDurationMax);
			Navigation.NextRoamTime = 0;
		}
		break;
	}
	case EJTSMoonAntState::Roaming:
		State.SurfaceElapsed += Dt;
		if (State.SurfaceElapsed >= State.SurfaceDuration)
		{
			Enter(State, EJTSMoonAntState::Burrowing);
			break;
		}
		if (Now >= Navigation.NextRoamTime || Planet->ApproximateSurfaceArcDistance(Movement.GroundLocation, Navigation.RoamTarget) <= 30
			|| Planet->ApproximateSurfaceArcDistance(Movement.GroundLocation, Navigation.HomeLocation) > Config.HomeRadius)
			ChooseTarget(Planet, Actor, Movement, Navigation, Config, State, Now);
		Direction = Planet->ProjectDirectionToSurfaceTangent(Navigation.RoamTarget - Movement.GroundLocation, Movement.GroundLocation);
		Speed = Config.RoamSpeed;
		break;
	case EJTSMoonAntState::ReactingToHit:
		if (State.PhaseElapsed >= Config.ReactionDuration)
		{
			FVector Away = Planet->ProjectDirectionToSurfaceTangent(Movement.GroundLocation - State.FleeSource, Movement.GroundLocation).GetSafeNormal();
			if (Away.IsNearlyZero()) Away = Planet->ProjectDirectionToSurfaceTangent(Actor->GetActorForwardVector(), Movement.GroundLocation).GetSafeNormal();
			State.FleeDirection = FQuat(Planet->GetRadialUpVector(Movement.GroundLocation), FMath::DegreesToRadians(
				(FMath::RandBool() ? 1.f : -1.f) * FMath::FRandRange(15.f, 30.f))).RotateVector(Away);
			State.FleeDuration = FMath::FRandRange(Config.FleeDurationMin, Config.FleeDurationMax);
			State.FleeElapsedDistance = 0;
			Enter(State, EJTSMoonAntState::Fleeing);
		}
		break;
	case EJTSMoonAntState::Fleeing:
		if (State.PhaseElapsed >= State.FleeDuration || State.FleeElapsedDistance >= Config.FleeDistance)
			Enter(State, EJTSMoonAntState::Burrowing);
		else { Direction = State.FleeDirection; Speed = Config.FleeSpeed; }
		break;
	case EJTSMoonAntState::Burrowing:
	{
		const float Alpha = Config.BurrowDuration > SMALL_NUMBER ? FMath::Clamp(State.PhaseElapsed / Config.BurrowDuration, 0.f, 1.f) : 1.f;
		State.BurrowOffset = FMath::Lerp(0.f, -Config.BurrowDepth, FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f));
		State.bDespawnRequested = Alpha >= 1;
		break;
	}
	}
	if (State.Phase != EJTSMoonAntState::Emerging && State.Phase != EJTSMoonAntState::Burrowing) State.BurrowOffset = 0;
	auto* Status = Actor->FindComponentByClass<UJTSStellarTargetComponent>();
	Movement.DesiredVelocity = Direction.GetSafeNormal() * Speed * (Status ? Status->GetMovementScale() : 1.f);
	const bool bForced = Status && Status->HasActiveFieldForces();
	FVector Step;
	if (bForced)
	{
		float ImpactSpeed = 0;
		Step = Status->IntegrateFieldMotion(Actor->GetActorLocation(), Planet->GetRadialUpVector(Movement.GroundLocation),
			Movement.DesiredVelocity, 520.f, FVector::ZeroVector, Dt, Movement.Velocity, ImpactSpeed);
	}
	else
	{
		Movement.Velocity = Movement.DesiredVelocity;
		Step = Movement.Velocity * Dt;
	}
	if (Step.IsNearlyZero()) return;
	FVector Ground, Heading;
	const bool bMoved = bForced
		? Actor->GetSurfaceSteering()->ConstrainDisplacement(Planet, Movement.GroundLocation, Step, Ground, Heading)
		: Actor->GetSurfaceSteering()->AdvanceWithState(Planet, Movement.GroundLocation, Step, Step.Size() / Dt,
			Dt, State.Traversal, Ground, Heading);
	if (!bMoved)
	{
		Movement.Velocity = FVector::ZeroVector;
		if (State.Phase == EJTSMoonAntState::Roaming && (State.BlockedElapsed += Dt) >= .6f)
		{
			ChooseTarget(Planet, Actor, Movement, Navigation, Config, State, Now);
			State.BlockedElapsed = 0;
		}
		return;
	}
	State.BlockedElapsed = 0;
	if (bForced)
	{
		State.Traversal.PreviousHeading = Heading;
		State.Traversal.AvoidanceRemaining = 0;
	}
	if (State.Phase == EJTSMoonAntState::Fleeing)
		State.FleeElapsedDistance += Planet->ApproximateSurfaceArcDistance(Movement.GroundLocation, Ground);
	Movement.Velocity = (Ground - Movement.GroundLocation) / Dt;
	Movement.GroundLocation = Ground;
	FJTSPlanetSurfaceHit Hit;
	if (Planet->ProjectPointToSurface(Ground, Hit)) Movement.SurfaceUp = Hit.ImpactNormal.GetSafeNormal();
	const FQuat Desired = FRotationMatrix::MakeFromXZ(Heading, Movement.SurfaceUp).ToQuat();
	const float Angle = Movement.ProposedRotation.AngularDistance(Desired);
	Movement.ProposedRotation = FQuat::Slerp(Movement.ProposedRotation, Desired, Angle > SMALL_NUMBER
		? FMath::Clamp(FMath::DegreesToRadians(Config.TurnDegrees) * Dt / Angle, 0.f, 1.f) : 1.f);
	Movement.ProposedLocation = Ground + Movement.SurfaceUp * Config.SupportHeight;
}

void FJTSPlanetAntSimulation::CommitPresentation(const FJTSPlanetEnemyActorFragment& Binding,
	const FJTSPlanetEnemyMovementFragment& Movement, const FJTSPlanetAntActivityFragment& State)
{
	if (auto* Actor = Cast<AJTSMoonAntActor>(Binding.Actor.Get()))
		Actor->ApplyMassPresentation(Movement.GroundLocation, Movement.SurfaceUp, State.Phase, State.BurrowOffset);
}
