#include "space/Systems/JTSPlanetEnemySubsystem.h"
#include "space/Systems/JTSPlanetEnemyPursuit.h"
#include "space/Components/JTSStellarTargetComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ShapeComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Math/RotationMatrix.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetSettlementEnemy.h"

namespace
{
	constexpr int32 MaxScansPerFrame = 24;
	constexpr int32 MaxSightQueriesPerFrame = 64;
	constexpr int32 MaxSteeringUpdatesPerFrame = 80;
	constexpr float SpatialCellSize = 256;
	TAutoConsoleVariable<int32> CVarDebugBodyCollision(TEXT("jts.EnemyBody.Debug"), 0,
		TEXT("Draw authoritative ground footprints, heights and residual overlap statistics (0/1)."), ECVF_Cheat);
	FIntVector BucketFor(const FVector& P)
	{
		return FIntVector(FMath::FloorToInt(P.X / SpatialCellSize),
			FMath::FloorToInt(P.Y / SpatialCellSize), FMath::FloorToInt(P.Z / SpatialCellSize));
	}
}

bool UJTSPlanetEnemySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && World != nullptr && World->IsGameWorld();
}

void UJTSPlanetEnemySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UMassEntitySubsystem>();
	Super::Initialize(Collection);
}

void UJTSPlanetEnemySubsystem::Deinitialize()
{
	if (UMassEntitySubsystem* Mass = GetWorld()->GetSubsystem<UMassEntitySubsystem>())
	{
		FMassEntityManager& Manager = Mass->GetMutableEntityManager();
		for (const FMassEntityHandle Entity : ActiveEntities)
		{
			if (Manager.IsEntityActive(Entity)) Manager.DestroyEntity(Entity);
		}
	}
	ActiveEntities.Reset();
	for (const auto& Entry : RegisteredActors)
		if (Entry.Key.IsValid()) Entry.Key->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleRegisteredEnemyDestroyed);
	RegisteredActors.Reset();
	SpatialBuckets.Reset();
	SettlementAlerts.Reset();
	EnemyArchetype.Reset();
	Super::Deinitialize();
}

TStatId UJTSPlanetEnemySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UJTSPlanetEnemySubsystem, STATGROUP_Tickables);
}

bool UJTSPlanetEnemySubsystem::IsTickable() const
{
	return ActiveEntities.Num() > 0;
}

FMassEntityHandle UJTSPlanetEnemySubsystem::RegisterEnemy(AActor* Actor, AJTSPlanetAnchor* Planet,
	const FVector& HomeLocation, const FVector& GroundLocation, const FJTSPlanetEnemyBehavior& Behavior)
{
	if (!IsValid(Actor) || !Actor->HasAuthority() || !IsValid(Planet) || Actor->GetWorld() != GetWorld())
	{
		return FMassEntityHandle();
	}
	UMassEntitySubsystem* Mass = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (Mass == nullptr) return FMassEntityHandle();
	FMassEntityManager& Manager = Mass->GetMutableEntityManager();
	if (const FMassEntityHandle* Existing = RegisteredActors.Find(Actor))
	{
		if (Manager.IsEntityActive(*Existing)) return *Existing;
		RegisteredActors.Remove(Actor);
	}
	if (!EnemyArchetype.IsValid())
	{
		const TArray<const UScriptStruct*> FragmentTypes = {
			FJTSPlanetEnemyActorFragment::StaticStruct(),
			FJTSPlanetEnemyMovementFragment::StaticStruct(),
			FJTSPlanetEnemyNavigationFragment::StaticStruct(),
			FJTSPlanetEnemyPerceptionFragment::StaticStruct(),
			FJTSPlanetEnemyCombatFragment::StaticStruct(),
			FJTSPlanetEnemyBehaviorFragment::StaticStruct(),
			FJTSPlanetEnemyBodyFragment::StaticStruct()
		};
		EnemyArchetype = Manager.CreateArchetype(FragmentTypes);
	}
	const FMassEntityHandle Entity = Manager.CreateEntity(EnemyArchetype);
	FJTSPlanetEnemyActorFragment& Binding = Manager.GetFragmentDataChecked<FJTSPlanetEnemyActorFragment>(Entity);
	Binding.Actor = Actor;
	Binding.Planet = Planet;
	FJTSPlanetEnemyMovementFragment& Movement = Manager.GetFragmentDataChecked<FJTSPlanetEnemyMovementFragment>(Entity);
	Movement.GroundLocation = GroundLocation;
	FJTSPlanetSurfaceHit SurfaceHit;
	if (Planet->ProjectPointToSurface(GroundLocation, SurfaceHit))
	{
		Movement.GroundLocation = SurfaceHit.ImpactPoint;
		Movement.SurfaceUp = Planet->GetRadialUpVector(SurfaceHit.ImpactPoint);
	}
	else Movement.SurfaceUp = Planet->GetRadialUpVector(GroundLocation);
	FJTSPlanetEnemyNavigationFragment& Navigation = Manager.GetFragmentDataChecked<FJTSPlanetEnemyNavigationFragment>(Entity);
	Navigation.HomeLocation = HomeLocation;
	Navigation.RoamTarget = HomeLocation;
	Manager.GetFragmentDataChecked<FJTSPlanetEnemyBehaviorFragment>(Entity).Values = Behavior;
	FJTSPlanetEnemyPerceptionFragment& Perception = Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entity);
	const float Now = GetWorld()->GetTimeSeconds();
	Perception.NextScanTime = Now + (Behavior.bAcquireOnSpawn ? 0 : FMath::FRandRange(0.0f, FMath::Max(0.05f, Behavior.ScanInterval)));
	Movement.LastUpdateTime = Now;
	Movement.NextUpdateTime = Now;
	Movement.GoalLocation = Movement.GroundLocation;
	Movement.ProposedLocation = Actor->GetActorLocation();
	Movement.ProposedRotation = Actor->GetActorQuat();
	auto& Body = Manager.GetFragmentDataChecked<FJTSPlanetEnemyBodyFragment>(Entity);
	Body.StableId = NextBodyId++;
	Body.Position = Actor->GetActorLocation();
	Body.Radius = FMath::Max(1.0f, Behavior.CollisionRadius);
	Body.HalfHeight = FMath::Max(0.0f, Behavior.CollisionHalfHeight);
	Body.InverseMass = FMath::Max(0.0f, Behavior.CollisionInverseMass);
	Body.Layer = Behavior.CollisionLayer;
	Body.bEnabled = Behavior.bBodyCollisionEnabled;
	RegisteredActors.Add(Actor, Entity);
	Actor->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleRegisteredEnemyDestroyed);
	ActiveEntities.Add(Entity);
	return Entity;
}

void UJTSPlanetEnemySubsystem::UnregisterEnemy(FMassEntityHandle Entity)
{
	if (UMassEntitySubsystem* Mass = GetWorld()->GetSubsystem<UMassEntitySubsystem>())
	{
		FMassEntityManager& Manager = Mass->GetMutableEntityManager();
		if (Manager.IsEntityActive(Entity))
		{
			const auto& Binding = Manager.GetFragmentDataChecked<FJTSPlanetEnemyActorFragment>(Entity);
			if (AActor* Actor = Binding.Actor.Get())
			{
				Actor->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleRegisteredEnemyDestroyed);
				RegisteredActors.Remove(Actor);
			}
			Manager.DestroyEntity(Entity);
		}
	}
	ActiveEntities.RemoveSingleSwap(Entity);
	for (auto It = RegisteredActors.CreateIterator(); It; ++It)
		if (It.Value() == Entity) It.RemoveCurrent();
	if (ActiveEntities.IsEmpty()) BodySeparationStats = FJTSGroundBodySeparationStats();
}

void UJTSPlanetEnemySubsystem::HandleRegisteredEnemyDestroyed(AActor* Actor)
{
	if (const FMassEntityHandle* Entity = RegisteredActors.Find(Actor)) UnregisterEnemy(*Entity);
}

void UJTSPlanetEnemySubsystem::SetBodyCollisionEnabled(FMassEntityHandle Entity, bool bEnabled)
{
	auto* Mass = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (Mass && Mass->GetEntityManager().IsEntityActive(Entity))
		Mass->GetMutableEntityManager().GetFragmentDataChecked<FJTSPlanetEnemyBodyFragment>(Entity).bEnabled = bEnabled;
}

bool UJTSPlanetEnemySubsystem::GetBodyCollisionData(FMassEntityHandle Entity, FJTSPlanetEnemyBodyFragment& OutBody) const
{
	const auto* Mass = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!Mass || !Mass->GetEntityManager().IsEntityActive(Entity)) return false;
	OutBody = Mass->GetEntityManager().GetFragmentDataChecked<FJTSPlanetEnemyBodyFragment>(Entity);
	return true;
}

void UJTSPlanetEnemySubsystem::NotifyDamaged(FMassEntityHandle Entity, AJTSCharacter* Attacker)
{
    if (!IsValid(Attacker) || GetWorld()->GetNetMode() == NM_Client) return;
    UMassEntitySubsystem* Mass = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
    if (!Mass || !Mass->GetEntityManager().IsEntityActive(Entity)) return;
    FMassEntityManager& Manager = Mass->GetMutableEntityManager();
    const auto& Binding = Manager.GetFragmentDataChecked<FJTSPlanetEnemyActorFragment>(Entity);
    auto& Navigation = Manager.GetFragmentDataChecked<FJTSPlanetEnemyNavigationFragment>(Entity);
    auto& Perception = Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entity);
    const auto& Behavior = Manager.GetFragmentDataChecked<FJTSPlanetEnemyBehaviorFragment>(Entity).Values;
    const float Now = GetWorld()->GetTimeSeconds();
    if (Behavior.bShareSettlementAlert)
        for (const FSettlementAlert& Alert : SettlementAlerts)
            if (Alert.Planet == Binding.Planet && FVector::DistSquared(Alert.Home, Navigation.HomeLocation) < 100
                && Alert.bProvoked && Alert.Until > Now && Alert.Until <= Perception.CompletedRetaliationUntilTime)
                return;
    if (!FJTSPlanetEnemyPursuit::Retaliate(Binding.Planet.Get(), Navigation, Perception, Behavior, Attacker, Now)) return;
    Manager.GetFragmentDataChecked<FJTSPlanetEnemyMovementFragment>(Entity).NextUpdateTime = Now;
    if (Behavior.bShareSettlementAlert)
    {
        const float SharedUntil = ShareAlert(Binding.Planet.Get(), Navigation.HomeLocation, Attacker, Now,
            Perception.bProvokedPursuit ? Perception.RetaliationUntilTime : Now + Behavior.TargetMemorySeconds,
            Perception.bProvokedPursuit);
        if (Perception.bProvokedPursuit) Perception.RetaliationUntilTime = FMath::Min(Perception.RetaliationUntilTime, SharedUntil);
    }
}

float UJTSPlanetEnemySubsystem::ShareAlert(AJTSPlanetAnchor* Planet, const FVector& Home, AJTSCharacter* Target,
    float Now, float Until, bool bProvoked)
{
    for (FSettlementAlert& Alert : SettlementAlerts)
    {
        if (Alert.Planet != Planet || FVector::DistSquared(Alert.Home, Home) >= 100) continue;
        if (Alert.bProvoked && Alert.Until > Now)
        {
            // A fresh observer or another hit cannot extend the colony's active retaliation episode.
            if (bProvoked) Alert.Until = FMath::Min(Alert.Until, Until);
        }
        else { Alert.Until = Until; Alert.bProvoked = bProvoked; }
        Alert.Target = Target; Alert.Location = Target->GetActorLocation(); Alert.ReportedTime = Now;
        return Alert.Until;
    }
    SettlementAlerts.Add(FSettlementAlert{Planet, Target, Home, Target->GetActorLocation(), Until, Now, bProvoked});
    return Until;
}

bool UJTSPlanetEnemySubsystem::IsEligiblePlayer(const AJTSCharacter* Player, const AJTSPlanetAnchor* Planet)
{
	return IsValid(Player) && IsValid(Planet) && Player->GetGameplayPlanet() == Planet
		&& !Player->IsBoarded()
		&& (!IsValid(Player->GetHealthComponent()) || !Player->GetHealthComponent()->IsDead());
}

void UJTSPlanetEnemySubsystem::ReceiveAlert(AJTSPlanetAnchor* Planet, const FJTSPlanetEnemyNavigationFragment& Navigation,
	FJTSPlanetEnemyPerceptionFragment& Perception, const FJTSPlanetEnemyBehavior& Behavior, float Now)
{
	if (!Behavior.bShareSettlementAlert || Navigation.bReturningHome) return;
	for (const FSettlementAlert& Alert : SettlementAlerts)
	{
		if (Alert.Planet != Planet || FVector::DistSquared(Alert.Home, Navigation.HomeLocation) >= 100 || Alert.Until <= Now
			|| (Alert.bProvoked && Alert.Until <= Perception.CompletedRetaliationUntilTime)
			|| !FJTSPlanetEnemyPursuit::CanAcquire(Planet, Navigation.HomeLocation, Alert.Target.Get(), Navigation, Perception,
				Behavior, Now, Alert.bProvoked)) continue;
		if (Perception.bCurrentlyVisible && Perception.Target.IsValid() && Perception.Target != Alert.Target) continue;
		Perception.Target = Alert.Target; Perception.LastKnownLocation = Alert.Location;
		Perception.LastSensedTime = FMath::Max(Perception.LastSensedTime, Alert.ReportedTime);
		if (Alert.bProvoked)
		{
			Perception.RetaliationUntilTime = Perception.bProvokedPursuit
				? FMath::Min(Perception.RetaliationUntilTime, Alert.Until) : Alert.Until;
			Perception.bProvokedPursuit = true;
		}
		return;
	}
}

bool UJTSPlanetEnemySubsystem::HasSightLine(const AActor* Observer, const AJTSCharacter* Player,
	AJTSPlanetAnchor* Planet, const FCollisionQueryParams& SightParams)
{
	if (!IsValid(Observer) || !IsValid(Player) || !IsValid(Planet)) return false;
	if (LastSightQueryCount >= MaxSightQueriesPerFrame) return false;
	++LastSightQueryCount;
	const FVector From = Observer->GetActorLocation() + Observer->GetActorUpVector() * 24.0f;
	const FVector To = Player->GetActorLocation() + Planet->GetRadialUpVector(Player->GetActorLocation()) * 55.0f;
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, SightParams)
		|| Hit.GetActor() == Player;
}

void UJTSPlanetEnemySubsystem::ScanForTargets(const FJTSPlanetEnemyActorFragment& Binding,
    const FJTSPlanetEnemyNavigationFragment& Navigation, FJTSPlanetEnemyPerceptionFragment& Perception,
    const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds,
    const TArray<AJTSCharacter*>& Players, const FCollisionQueryParams& SightParams)
{
    AJTSPlanetAnchor* Planet = Binding.Planet.Get();
    AActor* Actor = Binding.Actor.Get();
    if (!IsValid(Actor) || !IsValid(Planet)) return;
    Perception.NextScanTime = TimeSeconds + FMath::Max(0.05f, Behavior.ScanInterval) * FMath::FRandRange(0.85f, 1.15f);
    if (Navigation.bReturningHome || TimeSeconds < Perception.NextAcquireAllowedTime) return;
    AJTSCharacter* Best = nullptr;
    float BestScore = TNumericLimits<float>::Max();
    for (AJTSCharacter* Player : Players)
    {
        if (!FJTSPlanetEnemyPursuit::CanAcquire(Planet, Navigation.HomeLocation, Player, Navigation, Perception, Behavior, TimeSeconds)) continue;
        const float SenseRadius = Perception.bProvokedPursuit && Perception.Target == Player
            ? FMath::Max(Behavior.SightRadius, Behavior.RetaliationLeashRadius) : Behavior.SightRadius;
        const float Distance = Planet->ApproximateSurfaceArcDistance(Actor->GetActorLocation(), Player->GetActorLocation());
        const float Score = Distance * (Perception.Target.Get() == Player ? 0.8f : 1.0f);
        if (Distance > SenseRadius || Score >= BestScore || !HasSightLine(Actor, Player, Planet, SightParams)) continue;
        Best = Player; BestScore = Score;
    }
    Perception.bCurrentlyVisible = Best != nullptr;
    if (Best)
    {
        Perception.Target = Best; Perception.LastKnownLocation = Best->GetActorLocation(); Perception.LastSensedTime = TimeSeconds;
        if (Behavior.bShareSettlementAlert)
        {
            const float SharedUntil = ShareAlert(Planet, Navigation.HomeLocation, Best, TimeSeconds,
                Perception.bProvokedPursuit ? Perception.RetaliationUntilTime : TimeSeconds + Behavior.TargetMemorySeconds,
                Perception.bProvokedPursuit);
            if (Perception.bProvokedPursuit) Perception.RetaliationUntilTime = FMath::Min(Perception.RetaliationUntilTime, SharedUntil);
        }
    }
    else if (Behavior.bShareSettlementAlert)
    {
        ReceiveAlert(Planet, Navigation, Perception, Behavior, TimeSeconds);
    }
}

void UJTSPlanetEnemySubsystem::ChooseRoamTarget(FJTSPlanetEnemyNavigationFragment& Navigation,
	AJTSPlanetAnchor* Planet, const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds)
{
	Navigation.RoamTarget = Navigation.HomeLocation;
	Navigation.NextRoamTime = TimeSeconds + FMath::Max(0.1f, Behavior.RoamRetargetSeconds)
		* FMath::FRandRange(0.75f, 1.25f);
	if (!IsValid(Planet) || Behavior.RoamRadius <= 0.0f) return;
	const FVector Direction = Planet->GetRadialUpVector(Navigation.HomeLocation);
	for (int32 Attempt = 0; Attempt < 4; ++Attempt)
	{
		FJTSPlanetSurfaceHit Hit;
		if (Planet->RandomPointInSurfaceCap(Direction, Behavior.RoamRadius, Hit)
			&& Planet->ApproximateSurfaceArcDistance(Navigation.HomeLocation, Hit.ImpactPoint) <= Behavior.RoamRadius)
		{
			Navigation.RoamTarget = Hit.ImpactPoint;
			return;
		}
	}
}

void UJTSPlanetEnemySubsystem::UpdateSteering(const FJTSPlanetEnemyActorFragment& Binding,
	FJTSPlanetEnemyMovementFragment& Movement,
	FJTSPlanetEnemyNavigationFragment& Navigation,
	const FJTSPlanetEnemyPerceptionFragment& Perception,
	const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds,
	const FCollisionQueryParams& ObstacleParams)
{
	AActor* Actor = Binding.Actor.Get();
	AJTSPlanetAnchor* Planet = Binding.Planet.Get();
	if (!IsValid(Actor) || !IsValid(Planet)) return;

	const float HomeDistance = Planet->ApproximateSurfaceArcDistance(Movement.GroundLocation, Navigation.HomeLocation);
	const float PursuitLimit = FJTSPlanetEnemyPursuit::Leash(Behavior, Perception);
	const bool bHasTarget = IsEligiblePlayer(Perception.Target.Get(), Planet)
		&& Planet->ApproximateSurfaceArcDistance(Navigation.HomeLocation, Perception.Target->GetActorLocation()) <= PursuitLimit;
	const bool bReturning = Navigation.bReturningHome
		|| (!bHasTarget && HomeDistance > Behavior.RoamRadius + 150);
	const bool bPursuing = !bReturning && bHasTarget;
	FVector Goal = Navigation.HomeLocation;
	float TopSpeed = Behavior.ChaseSpeed;
	float StopDistance = 70.0f;
	if (bPursuing)
	{
		Goal = Perception.LastKnownLocation;
		if (Perception.bCurrentlyVisible)
		{
			AJTSCharacter* Target = Perception.Target.Get();
			const FVector Lead = FVector::VectorPlaneProject(Target->GetVelocity(), Movement.SurfaceUp)
				* FMath::Clamp(Behavior.TargetLeadSeconds, 0.0f, 0.5f);
			const FVector Predicted = Target->GetActorLocation() + Lead;
			Goal = Planet->ApproximateSurfaceArcDistance(Navigation.HomeLocation, Predicted) <= PursuitLimit
				? Predicted : Target->GetActorLocation();
		}
		StopDistance = Behavior.AttackRange * 0.92f;
	}
	else if (!bReturning)
	{
		if (Navigation.NextRoamTime <= TimeSeconds
			|| Planet->ApproximateSurfaceArcDistance(Movement.GroundLocation, Navigation.RoamTarget) < 65.0f)
		{
			ChooseRoamTarget(Navigation, Planet, Behavior, TimeSeconds);
		}
		Goal = Navigation.RoamTarget;
		TopSpeed = Behavior.RoamSpeed;
		StopDistance = 45.0f;
	}

	const float Distance = Planet->ApproximateSurfaceArcDistance(Movement.GroundLocation, Goal);
	Movement.RequestedInterval = bPursuing ? (Distance > Behavior.DistantMovementDistance
		? FMath::Max(Behavior.MovementInterval, Behavior.DistantMovementInterval) : Behavior.MovementInterval)
		: Behavior.IdleMovementInterval;
	FVector Direction = FVector::VectorPlaneProject(Goal - Movement.GroundLocation,
		Movement.SurfaceUp).GetSafeNormal();
	if ((bPursuing || bReturning) && Navigation.RouteUntilTime > TimeSeconds && Distance > StopDistance + 250)
	{
		const FVector Route = CrowdNavigation.GetDirection(Planet, Navigation.HomeLocation, Movement.GroundLocation,
			Goal, bPursuing ? static_cast<AActor*>(Perception.Target.Get()) : Planet,
			Behavior.RoamRadius, bReturning ? FMath::Max(Behavior.LeashRadius, Behavior.RetaliationLeashRadius) : PursuitLimit, TimeSeconds);
		if (!Route.IsNearlyZero()) Direction = FVector::VectorPlaneProject(Route, Movement.SurfaceUp).GetSafeNormal();
	}
	if (TimeSeconds >= Navigation.NextObstacleProbeTime && ObstacleQueriesRemaining > 0 && Distance > StopDistance + 50)
	{
		--ObstacleQueriesRemaining;
		Navigation.NextObstacleProbeTime = TimeSeconds + 0.2f;
		FCollisionQueryParams Params = ObstacleParams;
		Params.AddIgnoredActor(Planet->GetGameplaySurfaceActor());
		FHitResult Obstacle;
		const FVector ProbeStart = Movement.GroundLocation + Movement.SurfaceUp * 65;
		FCollisionObjectQueryParams Obstacles;
		Obstacles.AddObjectTypesToQuery(ECC_WorldStatic);
		Obstacles.AddObjectTypesToQuery(ECC_PhysicsBody);
		if (GetWorld()->SweepSingleByObjectType(Obstacle, ProbeStart, ProbeStart + Direction * 180,
			FQuat::Identity, Obstacles, FCollisionShape::MakeSphere(48), Params))
		{
			Navigation.RouteUntilTime = TimeSeconds + 8;
			Navigation.AvoidanceDirection = FVector::VectorPlaneProject(Direction, Obstacle.ImpactNormal);
			Navigation.AvoidanceDirection = FVector::VectorPlaneProject(Navigation.AvoidanceDirection, Movement.SurfaceUp).GetSafeNormal();
			if (Navigation.AvoidanceDirection.IsNearlyZero())
				Navigation.AvoidanceDirection = FVector::CrossProduct(Movement.SurfaceUp, Obstacle.ImpactNormal).GetSafeNormal();
		}
		else Navigation.AvoidanceDirection = FVector::ZeroVector;
	}
	if (!Navigation.AvoidanceDirection.IsNearlyZero()) Direction = Navigation.AvoidanceDirection;
	const double SeparationStarted = FPlatformTime::Seconds();
	const FVector Separation = GetSeparation(Actor, Planet, Movement.SurfaceUp, Behavior.SeparationRadius);
	LastWorkStats.SeparationMilliseconds += float((FPlatformTime::Seconds() - SeparationStarted) * 1000);
	const float Congestion = FMath::Max(0.0f, -FVector::DotProduct(Separation, Direction));
	// A packed crowd must not keep steering into the player. The position solver still owns the
	// hard footprint, but desired velocity has to leave the overlap or the next step walks back in.
	Direction = (Direction * (1.0f - Congestion) + Separation * (Behavior.SeparationWeight + Congestion)).GetSafeNormal();
	const float Acceleration = FMath::Max(1.0f, Behavior.Acceleration);
	const float DesiredSpeed = Direction.IsNearlyZero() ? 0.0f : FMath::Min(FMath::Max(0.0f, TopSpeed),
		FMath::Sqrt(2.0f * Acceleration * FMath::Max(0.0f, Distance - StopDistance)))
		* FMath::Clamp(1.0f - Congestion * 0.9f, 0.15f, 1.0f);
	Movement.DesiredVelocity = Direction * DesiredSpeed;
	Movement.GoalLocation = Goal;
	Movement.StopDistance = StopDistance;
}

void UJTSPlanetEnemySubsystem::UpdateMovement(const FJTSPlanetEnemyActorFragment& Binding,
	FJTSPlanetEnemyMovementFragment& Movement, const FJTSPlanetEnemyNavigationFragment& Navigation,
	const FJTSPlanetEnemyPerceptionFragment& Perception, const FJTSPlanetEnemyBehavior& Behavior,
	float TimeSeconds, float DeltaSeconds, const FComponentQueryParams& EnvironmentParams)
{
	AActor* Actor = Binding.Actor.Get();
	AJTSPlanetAnchor* Planet = Binding.Planet.Get();
	if (!IsValid(Actor) || !IsValid(Planet) || DeltaSeconds <= SMALL_NUMBER) return;
	Movement.PreviousLocation = Actor->GetActorLocation();
	const float HomeDistance = Planet->ApproximateSurfaceArcDistance(Movement.GroundLocation, Navigation.HomeLocation);
	const float PursuitLimit = FJTSPlanetEnemyPursuit::Leash(Behavior, Perception);
	const float Distance = Planet->ApproximateSurfaceArcDistance(Movement.GroundLocation, Movement.GoalLocation);
	const auto* Status = Actor->FindComponentByClass<UJTSStellarTargetComponent>();
	const FVector DesiredVelocity = FVector::VectorPlaneProject(Movement.DesiredVelocity, Movement.SurfaceUp)
		* (Status ? Status->GetMovementScale() : 1.f);
	const float Acceleration = FMath::Max(1.0f, Behavior.Acceleration);
	auto* StellarTarget = Actor->FindComponentByClass<UJTSStellarTargetComponent>();
	const bool bForced = StellarTarget && StellarTarget->HasActiveFieldForces();
	FVector SurfaceStep;
	float ImpactSpeed = 0;
	if (bForced)
	{
		Movement.RequestedInterval = FMath::Min(Movement.RequestedInterval, 0.05f);
		SurfaceStep = StellarTarget->IntegrateFieldMotion(Actor->GetActorLocation(), Movement.SurfaceUp,
			DesiredVelocity, Acceleration, FVector::ZeroVector, DeltaSeconds, Movement.Velocity, ImpactSpeed);
	}
	else
	{
		// Remaining physical momentum brakes through the same bounded steering acceleration after a field ends.
		Movement.Velocity = FMath::VInterpConstantTo(Movement.Velocity, DesiredVelocity, DeltaSeconds, Acceleration);
		Movement.Velocity = FVector::VectorPlaneProject(Movement.Velocity, Movement.SurfaceUp);
		const float Step = FMath::Min(Movement.Velocity.Size() * DeltaSeconds, FMath::Max(0.0f, Distance - Movement.StopDistance));
		SurfaceStep = Movement.Velocity.GetSafeNormal() * Step;
	}
	FJTSPlanetSurfaceHit Hit;
	const double SurfaceStarted = FPlatformTime::Seconds();
	const FVector Candidate = Movement.GroundLocation + SurfaceStep;
	if (!SurfaceStep.IsNearlyZero()
		&& Planet->ProbeSurfaceAlongGravity(Candidate + Planet->GetRadialUpVector(Candidate) * 150, 350, Hit))
	{
		const float NextHomeDistance = Planet->ApproximateSurfaceArcDistance(Navigation.HomeLocation, Hit.ImpactPoint);
		if (bForced || NextHomeDistance <= PursuitLimit || NextHomeDistance < HomeDistance)
		{
			Movement.GroundLocation = Hit.ImpactPoint;
			// Triangle face normals can jump at mesh seams; gravity/hover follow continuous radial up.
			Movement.SurfaceUp = Planet->GetRadialUpVector(Hit.ImpactPoint);
		}
		else Movement.Velocity = FVector::ZeroVector;
	}
	else if (!SurfaceStep.IsNearlyZero()) Movement.Velocity = FVector::ZeroVector;
	LastWorkStats.SurfaceMilliseconds += float((FPlatformTime::Seconds() - SurfaceStarted) * 1000);

	const FVector Facing = Movement.Velocity.SizeSquared() > 25.0f
		? Movement.Velocity.GetSafeNormal() : DesiredVelocity.GetSafeNormal();
	FQuat Rotation = Actor->GetActorQuat();
	if (!Facing.IsNearlyZero())
	{
		const FQuat Desired = FRotationMatrix::MakeFromXZ(Facing, Movement.SurfaceUp).ToQuat();
		const float Angle = Rotation.AngularDistance(Desired);
		const float Alpha = Angle > KINDA_SMALL_NUMBER
			? FMath::Clamp(FMath::DegreesToRadians(FMath::Max(0.0f, Behavior.TurnSpeedDegrees)) * DeltaSeconds / Angle, 0.0f, 1.0f)
			: 1.0f;
		Rotation = FQuat::Slerp(Rotation, Desired, Alpha);
	}
	const FVector DesiredLocation = Movement.GroundLocation + Movement.SurfaceUp * FMath::Max(0.0f, Behavior.HoverHeight);
	const double TransformStarted = FPlatformTime::Seconds();
	FHitResult MovementHit;
	FComponentQueryParams Params = EnvironmentParams;
	if (bForced)
	{
		TArray<AActor*, TInlineAllocator<4>> IgnoredCasters;
		StellarTarget->GetRepulsionCasters(IgnoredCasters);
		// Allow egress from an initial overlap with the caster; terrain and other blockers still sweep normally.
		for (AActor* Caster : IgnoredCasters) Params.AddIgnoredActor(Caster);
	}
	Movement.ProposedLocation = ConstrainBodyMove(Actor, Planet, Movement.PreviousLocation, DesiredLocation, Rotation, Params, &MovementHit);
	Movement.ProposedRotation = Rotation;
	LastWorkStats.TransformMilliseconds += float((FPlatformTime::Seconds() - TransformStarted) * 1000);
	if (FVector::DistSquared(Movement.ProposedLocation, DesiredLocation) > 4.0f)
	{
		const FVector Normal = FVector::VectorPlaneProject(MovementHit.ImpactNormal, Movement.SurfaceUp).GetSafeNormal();
		const float Incoming = FVector::DotProduct(Movement.Velocity, Normal);
		if (bForced && !Normal.IsNearlyZero() && Incoming < 0)
		{
			ImpactSpeed = FMath::Max(ImpactSpeed, -Incoming);
			Movement.Velocity -= Normal * (Incoming * 1.25f);
		}
		else Movement.Velocity = FVector::ZeroVector;
		FJTSPlanetSurfaceHit ActualHit;
		if (Planet->ProjectPointToSurface(Movement.ProposedLocation, ActualHit))
		{
			Movement.GroundLocation = ActualHit.ImpactPoint;
			Movement.SurfaceUp = Planet->GetRadialUpVector(ActualHit.ImpactPoint);
		}
	}
	if (bForced && ImpactSpeed > 180 && TimeSeconds >= Movement.NextForceImpactTime
		&& Actor->GetClass()->ImplementsInterface(UJTSPlanetSettlementEnemy::StaticClass()))
	{
		Movement.NextForceImpactTime = TimeSeconds + 0.2f;
		IJTSPlanetSettlementEnemy::Execute_OnSettlementForceImpact(Actor, Movement.Velocity.GetSafeNormal(), ImpactSpeed);
	}
}

FVector UJTSPlanetEnemySubsystem::ConstrainBodyMove(AActor* Actor, AJTSPlanetAnchor* Planet,
	const FVector& From, const FVector& Desired, const FQuat& Rotation,
	const FComponentQueryParams& EnvironmentParams, FHitResult* OutHit) const
{
	if (Desired.ContainsNaN() || !IsValid(Planet)) return From;
	auto* Root = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
	if (!Root || !Actor->GetActorEnableCollision() || !Root->IsQueryCollisionEnabled() || From.Equals(Desired, 0.00001)) return Desired;
	FComponentQueryParams Params = EnvironmentParams;
	Params.bFindInitialOverlaps = true;
	for (const auto* Ignored : Root->GetMoveIgnoreActors()) Params.AddIgnoredActor(Ignored);
	for (const auto* Ignored : Root->GetMoveIgnoreComponents()) Params.AddIgnoredComponent(Ignored);
	TArray<FHitResult> Hits;
	if (Cast<UShapeComponent>(Root))
	{
		FHitResult Hit;
		if (GetWorld()->SweepSingleByChannel(Hit, From, Desired, Rotation, Root->GetCollisionObjectType(),
			Root->GetCollisionShape(), Params, FCollisionResponseParams(Root->GetCollisionResponseToChannels()))) Hits.Add(Hit);
	}
	else GetWorld()->ComponentSweepMulti(Hits, Root, From, Desired, Rotation, Params);
	const FHitResult* First = nullptr;
	for (const auto& Hit : Hits)
		if (Hit.bBlockingHit && (!First || Hit.Time < First->Time)) First = &Hit;
	if (!First) return Desired;
	if (OutHit) *OutHit = *First;
	if (First->bStartPenetrating) return From;
	return FMath::Lerp(From, Desired, FMath::Max(0.0f, First->Time - 0.0001f));
}

void UJTSPlanetEnemySubsystem::ResolveBodyOverlap(FMassEntityManager& Manager,
	const TArray<FMassEntityHandle>& Entities, float DeltaSeconds, const FComponentQueryParams& EnvironmentParams)
{
	TArray<FJTSGroundBody> Bodies;
	Bodies.Reserve(Entities.Num());
	for (const auto Entity : Entities)
	{
		const auto& Binding = Manager.GetFragmentDataChecked<FJTSPlanetEnemyActorFragment>(Entity);
		const auto& Movement = Manager.GetFragmentDataChecked<FJTSPlanetEnemyMovementFragment>(Entity);
		auto& Body = Manager.GetFragmentDataChecked<FJTSPlanetEnemyBodyFragment>(Entity);
		AActor* Actor = Binding.Actor.Get();
		auto* Root = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
		Body.bParticipating = Body.bEnabled && Actor->GetActorEnableCollision() && Root && Root->IsQueryCollisionEnabled();
		// The occupancy disk must contain the native root's horizontal envelope, including cube corners.
		const FVector Extent = Root ? Root->CalcBounds(FTransform(FQuat::Identity, FVector::ZeroVector, Actor->GetActorScale3D())).BoxExtent : FVector::ZeroVector;
		const FVector Up = Binding.Planet->GetRadialUpVector(Movement.ProposedLocation);
		float EnvelopeRadius = 0, EnvelopeHeight = 0;
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector Offset = Movement.ProposedRotation.RotateVector(Extent * FVector(
				Corner & 1 ? 1 : -1, Corner & 2 ? 1 : -1, Corner & 4 ? 1 : -1));
			EnvelopeRadius = FMath::Max(EnvelopeRadius, float(FVector::VectorPlaneProject(Offset, Up).Size()));
			EnvelopeHeight = FMath::Max(EnvelopeHeight, float(FMath::Abs(FVector::DotProduct(Offset, Up))));
		}
		const auto& Behavior = Manager.GetFragmentDataChecked<FJTSPlanetEnemyBehaviorFragment>(Entity).Values;
		Body.Radius = FMath::Max(Behavior.CollisionRadius, EnvelopeRadius);
		Body.HalfHeight = FMath::Max(Behavior.CollisionHalfHeight, EnvelopeHeight);
		Body.Position = Movement.ProposedLocation;
		FJTSGroundBody& Snapshot = Bodies.AddDefaulted_GetRef();
		Snapshot.Id = Body.StableId;
		Snapshot.Group = Binding.Planet->GetUniqueID();
		Snapshot.Center = Binding.Planet->GetPlanetCenter();
		Snapshot.Layer = Body.Layer;
		Snapshot.Position = Body.Position;
		Snapshot.Radius = Body.Radius;
		Snapshot.HalfHeight = Body.HalfHeight;
		Snapshot.InverseMass = Body.InverseMass;
		Snapshot.bParticipating = Body.bParticipating;
	}
	FJTSGroundBodySeparation::Solve(Bodies, BodySeparationSettings, DeltaSeconds,
		[&](int32 I, const FVector& From, const FVector& Requested)
		{
			const auto Entity = Entities[I];
			const auto& Binding = Manager.GetFragmentDataChecked<FJTSPlanetEnemyActorFragment>(Entity);
			const auto& Movement = Manager.GetFragmentDataChecked<FJTSPlanetEnemyMovementFragment>(Entity);
			const auto& Behavior = Manager.GetFragmentDataChecked<FJTSPlanetEnemyBehaviorFragment>(Entity).Values;
			AJTSPlanetAnchor* Planet = Binding.Planet.Get();
			const FVector GroundCandidate = Requested - Planet->GetRadialUpVector(Requested) * Behavior.HoverHeight;
			FJTSPlanetSurfaceHit Hit;
			if (!Planet->ProbeSurfaceAlongGravity(GroundCandidate + Planet->GetRadialUpVector(GroundCandidate) * 150, 350, Hit)) return From;
			const FVector Desired = Hit.ImpactPoint + Planet->GetRadialUpVector(Hit.ImpactPoint) * Behavior.HoverHeight;
			return ConstrainBodyMove(Binding.Actor.Get(), Planet, From, Desired, Movement.ProposedRotation, EnvironmentParams);
		}, BodySeparationStats);
	// Only final environment-constrained positions are committed to the Actors / replicated roots.
	for (int32 I = 0; I < Entities.Num(); ++I)
	{
		const auto Entity = Entities[I];
		const auto& Binding = Manager.GetFragmentDataChecked<FJTSPlanetEnemyActorFragment>(Entity);
		auto& Movement = Manager.GetFragmentDataChecked<FJTSPlanetEnemyMovementFragment>(Entity);
		auto& Body = Manager.GetFragmentDataChecked<FJTSPlanetEnemyBodyFragment>(Entity);
		AActor* Actor = Binding.Actor.Get();
		Actor->SetActorLocationAndRotation(Bodies[I].Position, Movement.ProposedRotation, false);
		Body.Position = Actor->GetActorLocation();
		Movement.SurfaceUp = Binding.Planet->GetRadialUpVector(Body.Position);
		Movement.GroundLocation = Body.Position - Movement.SurfaceUp
			* Manager.GetFragmentDataChecked<FJTSPlanetEnemyBehaviorFragment>(Entity).Values.HoverHeight;
		if (auto* Root = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
			Root->ComponentVelocity = DeltaSeconds > SMALL_NUMBER
				? (Body.Position - Movement.PreviousLocation) / DeltaSeconds : FVector::ZeroVector;
		// Position corrections never feed back into the next frame's steering/force acceleration.
		if (CVarDebugBodyCollision.GetValueOnGameThread() != 0 && Body.bParticipating)
		{
			FVector X, Y; Movement.SurfaceUp.FindBestAxisVectors(X, Y);
			const FColor Color = Body.InverseMass == 0 ? FColor::Yellow : FColor::Cyan;
			DrawDebugCircle(GetWorld(), Body.Position, Body.Radius, 24, Color, false, -1, 0, 1, X, Y, false);
			DrawDebugLine(GetWorld(), Body.Position - Movement.SurfaceUp * Body.HalfHeight,
				Body.Position + Movement.SurfaceUp * Body.HalfHeight, Color, false, -1, 0, 1);
		}
	}
	if (CVarDebugBodyCollision.GetValueOnGameThread() != 0 && GEngine)
		GEngine->AddOnScreenDebugMessage(0x4a545342, 0, FColor::Cyan, FString::Printf(
			TEXT("Enemy bodies %d | iterations %d | residual pairs %d | max depth %.2f cm | %.2f ms"),
			BodySeparationStats.Participants, BodySeparationStats.Iterations, BodySeparationStats.ResidualPairs,
			BodySeparationStats.MaxPenetration, BodySeparationStats.Milliseconds));
}

void UJTSPlanetEnemySubsystem::UpdateCombat(const FJTSPlanetEnemyActorFragment& Binding,
	const FJTSPlanetEnemyPerceptionFragment& Perception,
	FJTSPlanetEnemyCombatFragment& Combat,
	const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds, const FCollisionQueryParams& SightParams)
{
	AActor* Actor = Binding.Actor.Get();
	AJTSPlanetAnchor* Planet = Binding.Planet.Get();
	if (!IsValid(Actor) || !IsValid(Planet)) return;
	if (const auto* Status = Actor->FindComponentByClass<UJTSStellarTargetComponent>(); Status && Status->IsHardControlled())
	{
		Combat.bImpactPending = false; Combat.PendingVictim.Reset(); return;
	}
	if (!Perception.Target.IsValid()) { Combat.bImpactPending = false; Combat.PendingVictim.Reset(); return; }
	if (Combat.bImpactPending && TimeSeconds >= Combat.ImpactTime)
	{
		// Defer a damaging impact if the shared sight budget is exhausted; never damage through a wall.
		if (Behavior.AttackDamage > 0 && LastSightQueryCount >= MaxSightQueriesPerFrame) return;
		Combat.bImpactPending = false;
		AJTSCharacter* Victim = Combat.PendingVictim.Get();
		Combat.PendingVictim.Reset();
		if (Behavior.AttackDamage > 0.0f && IsEligiblePlayer(Victim, Planet)
			&& Planet->ApproximateSurfaceArcDistance(Actor->GetActorLocation(), Victim->GetActorLocation()) <= Behavior.AttackReach
			&& HasSightLine(Actor, Victim, Planet, SightParams))
		{
			UGameplayStatics::ApplyDamage(Victim, Behavior.AttackDamage, nullptr, Actor, UDamageType::StaticClass());
		}
	}
	AJTSCharacter* Target = Perception.Target.Get();
	if (TimeSeconds < Combat.NextAttackTime || !Perception.bCurrentlyVisible || !IsEligiblePlayer(Target, Planet)
		|| TimeSeconds - Perception.LastSensedTime > FMath::Max(0.4f, Behavior.ScanInterval * 2.0f)
		|| Planet->ApproximateSurfaceArcDistance(Actor->GetActorLocation(), Target->GetActorLocation()) > Behavior.AttackRange)
	{
		return;
	}
	Combat.NextAttackTime = TimeSeconds + FMath::Max(Behavior.AttackCooldown, Behavior.AttackWindup);
	Combat.ImpactTime = TimeSeconds + FMath::Max(0.0f, Behavior.AttackWindup);
	Combat.PendingVictim = Target;
	Combat.bImpactPending = true;
	if (Actor->GetClass()->ImplementsInterface(UJTSPlanetSettlementEnemy::StaticClass()))
	{
		IJTSPlanetSettlementEnemy::Execute_OnSettlementAttackStarted(Actor);
	}
}

FVector UJTSPlanetEnemySubsystem::GetSeparation(const AActor* Actor, const AJTSPlanetAnchor* Planet,
    const FVector& Up, float Radius) const
{
    const float Range = FMath::Clamp(Radius, 0.0f, SpatialCellSize - 1);
    if (Range <= 0) return FVector::ZeroVector;
    const FIntVector Center = BucketFor(Actor->GetActorLocation());
    FVector Force = FVector::ZeroVector;
    int32 Inspected = 0;
    for (int32 X = -1; X <= 1; ++X)
    for (int32 Y = -1; Y <= 1; ++Y)
    for (int32 Z = -1; Z <= 1; ++Z)
    {
        const auto* Bucket = SpatialBuckets.Find(Center + FIntVector(X,Y,Z));
        if (!Bucket) continue;
        for (const FCrowdSample& Other : *Bucket)
        {
            if (Other.Actor == Actor || Other.Planet != Planet) continue;
            if (++Inspected > 32) return Force.GetClampedToMaxSize(1);
            const FVector Offset = FVector::VectorPlaneProject(Actor->GetActorLocation() - Other.Position, Up);
            const float Distance = Offset.Size();
            if (Distance > 1 && Distance < Range)
                Force += Offset / Distance * (1 - Distance / Range);
        }
    }
    return Force.GetClampedToMaxSize(1);
}

int32 UJTSPlanetEnemySubsystem::GetTrackedTargetCount() const
{
    const UMassEntitySubsystem* Mass = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
    if (!Mass) return 0;
    const FMassEntityManager& Manager = Mass->GetEntityManager();
    int32 Count = 0;
    for (const FMassEntityHandle Entity : ActiveEntities)
        if (Manager.IsEntityActive(Entity)
            && Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entity).Target.IsValid()) ++Count;
    return Count;
}

void UJTSPlanetEnemySubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    const double Started = FPlatformTime::Seconds();
    double PhaseStarted = Started;
    LastWorkStats = FJTSPlanetEnemyWorkStats();
    LastTickMilliseconds = 0;
    LastScanCount = 0; LastMovementCount = 0; LastSteeringCount = 0; LastSightQueryCount = 0;
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client || ActiveEntities.IsEmpty()) return;
    UMassEntitySubsystem* Mass = World->GetSubsystem<UMassEntitySubsystem>();
    if (!Mass) return;
    FMassEntityManager& Manager = Mass->GetMutableEntityManager();
    const float Now = World->GetTimeSeconds();
    SettlementAlerts.RemoveAll([&](const FSettlementAlert& Alert)
        { return !Alert.Planet.IsValid() || !Alert.Target.IsValid() || Alert.Until < Now; });
    CrowdNavigation.Tick(World, Now);
    LastWorkStats.NavigationMilliseconds = float((FPlatformTime::Seconds() - PhaseStarted) * 1000);
    PhaseStarted = FPlatformTime::Seconds();
    ObstacleQueriesRemaining = 48;
    SpatialBuckets.Reset();
    FCollisionQueryParams SightParams(SCENE_QUERY_STAT(PlanetEnemySight), false);
	FComponentQueryParams EnvironmentParams(SCENE_QUERY_STAT(PlanetEnemyBodyEnvironment));
    TArray<FMassEntityHandle> LiveEntities;
    LiveEntities.Reserve(ActiveEntities.Num());
    for (int32 I = ActiveEntities.Num() - 1; I >= 0; --I)
    {
        const FMassEntityHandle Entity = ActiveEntities[I];
        if (!Manager.IsEntityActive(Entity)) { ActiveEntities.RemoveAtSwap(I); continue; }
        const auto& Binding = Manager.GetFragmentDataChecked<FJTSPlanetEnemyActorFragment>(Entity);
        AActor* Actor = Binding.Actor.Get();
        AJTSPlanetAnchor* Planet = Binding.Planet.Get();
        if (!IsValid(Actor) || !IsValid(Planet)) { UnregisterEnemy(Entity); continue; }
        if (const auto* Health = Actor->FindComponentByClass<UJTSHealthComponent>(); Health && Health->IsDead()) { UnregisterEnemy(Entity); continue; }
        auto& Navigation = Manager.GetFragmentDataChecked<FJTSPlanetEnemyNavigationFragment>(Entity);
        auto& Perception = Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entity);
        const bool bWasReturning = Navigation.bReturningHome;
        const TWeakObjectPtr<AJTSCharacter> PreviousTarget = Perception.Target;
        // A validated hit alert arrives before leash evaluation, so the colony can join this episode.
        ReceiveAlert(Planet, Manager.GetFragmentDataChecked<FJTSPlanetEnemyNavigationFragment>(Entity),
            Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entity),
            Manager.GetFragmentDataChecked<FJTSPlanetEnemyBehaviorFragment>(Entity).Values, Now);
        FJTSPlanetEnemyPursuit::Update(Planet,
            Manager.GetFragmentDataChecked<FJTSPlanetEnemyMovementFragment>(Entity).GroundLocation,
            Manager.GetFragmentDataChecked<FJTSPlanetEnemyNavigationFragment>(Entity),
            Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entity),
            Manager.GetFragmentDataChecked<FJTSPlanetEnemyBehaviorFragment>(Entity).Values, Now);
        if (bWasReturning != Navigation.bReturningHome || PreviousTarget != Perception.Target)
            Manager.GetFragmentDataChecked<FJTSPlanetEnemyMovementFragment>(Entity).NextUpdateTime = Now;
        SightParams.AddIgnoredActor(Actor); // Build the crowd ignore list once, rather than once per ray.
		// This solver is the sole owner of registered monster/monster contacts. Terrain/player responses remain native.
		EnvironmentParams.AddIgnoredActor(Actor);
		SpatialBuckets.FindOrAdd(BucketFor(Actor->GetActorLocation())).Add(FCrowdSample{Actor, Planet, Actor->GetActorLocation(),
			Manager.GetFragmentDataChecked<FJTSPlanetEnemyMovementFragment>(Entity).Velocity,
			Manager.GetFragmentDataChecked<FJTSPlanetEnemyBehaviorFragment>(Entity).Values.CollisionRadius});
        LiveEntities.Add(Entity);
    }
    if (LiveEntities.IsEmpty()) return;
    TArray<AJTSCharacter*> Players;
    for (TActorIterator<AJTSCharacter> It(World); It; ++It) Players.Add(*It);
    LastWorkStats.GatherMilliseconds = float((FPlatformTime::Seconds() - PhaseStarted) * 1000);
    PhaseStarted = FPlatformTime::Seconds();
    ScanCursor %= LiveEntities.Num();
    int32 Visited = 0;
    while (Visited < LiveEntities.Num() && LastScanCount < MaxScansPerFrame && LastSightQueryCount < MaxSightQueriesPerFrame - 4)
    {
        const FMassEntityHandle Entity = LiveEntities[ScanCursor];
        ScanCursor = (ScanCursor + 1) % LiveEntities.Num(); ++Visited;
        auto& Perception = Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entity);
        if (Perception.NextScanTime > Now) continue;
        const auto& Binding = Manager.GetFragmentDataChecked<FJTSPlanetEnemyActorFragment>(Entity);
        const auto& Navigation = Manager.GetFragmentDataChecked<FJTSPlanetEnemyNavigationFragment>(Entity);
        const auto& Behavior = Manager.GetFragmentDataChecked<FJTSPlanetEnemyBehaviorFragment>(Entity).Values;
        const bool bHadTarget = Perception.Target.IsValid();
        ScanForTargets(Binding, Navigation, Perception, Behavior, Now, Players, SightParams);
        if (!bHadTarget && Perception.Target.IsValid())
            Manager.GetFragmentDataChecked<FJTSPlanetEnemyMovementFragment>(Entity).NextUpdateTime = Now;
        ++LastScanCount;
    }
    LastWorkStats.SenseMilliseconds = float((FPlatformTime::Seconds() - PhaseStarted) * 1000);
    PhaseStarted = FPlatformTime::Seconds();
    MovementCursor %= LiveEntities.Num();
    Visited = 0;
    FCollisionQueryParams ObstacleParams(SCENE_QUERY_STAT(PlanetEnemyObstacle), false);
    while (Visited < LiveEntities.Num() && LastSteeringCount < MaxSteeringUpdatesPerFrame)
    {
        const FMassEntityHandle Entity = LiveEntities[MovementCursor];
        MovementCursor = (MovementCursor + 1) % LiveEntities.Num(); ++Visited;
        auto& Movement = Manager.GetFragmentDataChecked<FJTSPlanetEnemyMovementFragment>(Entity);
        if (Movement.NextUpdateTime > Now) continue;
        const auto& Binding = Manager.GetFragmentDataChecked<FJTSPlanetEnemyActorFragment>(Entity);
        auto& Navigation = Manager.GetFragmentDataChecked<FJTSPlanetEnemyNavigationFragment>(Entity);
        const auto& Perception = Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entity);
        const auto& Behavior = Manager.GetFragmentDataChecked<FJTSPlanetEnemyBehaviorFragment>(Entity).Values;
        UpdateSteering(Binding, Movement, Navigation, Perception, Behavior, Now, ObstacleParams);
        Movement.NextUpdateTime = Now + FMath::Max(0.025f, Movement.RequestedInterval);
        ++LastSteeringCount;
    }
    LastWorkStats.SteeringMilliseconds = float((FPlatformTime::Seconds() - PhaseStarted) * 1000);
    PhaseStarted = FPlatformTime::Seconds();
    // Every entity advances every frame. Budgeting perception/steering must never throttle motion.
    for (const FMassEntityHandle Entity : LiveEntities)
    {
        auto& Movement = Manager.GetFragmentDataChecked<FJTSPlanetEnemyMovementFragment>(Entity);
        UpdateMovement(Manager.GetFragmentDataChecked<FJTSPlanetEnemyActorFragment>(Entity), Movement,
            Manager.GetFragmentDataChecked<FJTSPlanetEnemyNavigationFragment>(Entity),
            Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entity),
            Manager.GetFragmentDataChecked<FJTSPlanetEnemyBehaviorFragment>(Entity).Values, Now,
            FMath::Clamp(DeltaTime, 0.0f, 0.25f), EnvironmentParams);
        Movement.LastUpdateTime = Now;
        ++LastMovementCount;
    }
	ResolveBodyOverlap(Manager, LiveEntities, FMath::Clamp(DeltaTime, 0.0f, 0.25f), EnvironmentParams);
    // Attack windups retain server-side LOS validation independently of steering refresh.
    LastWorkStats.MovementMilliseconds = float((FPlatformTime::Seconds() - PhaseStarted) * 1000);
    PhaseStarted = FPlatformTime::Seconds();
    for (const FMassEntityHandle Entity : LiveEntities)
        UpdateCombat(Manager.GetFragmentDataChecked<FJTSPlanetEnemyActorFragment>(Entity),
            Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entity),
            Manager.GetFragmentDataChecked<FJTSPlanetEnemyCombatFragment>(Entity),
            Manager.GetFragmentDataChecked<FJTSPlanetEnemyBehaviorFragment>(Entity).Values, Now, SightParams);
    LastWorkStats.CombatMilliseconds = float((FPlatformTime::Seconds() - PhaseStarted) * 1000);
    LastTickMilliseconds = float((FPlatformTime::Seconds() - Started) * 1000);
}
