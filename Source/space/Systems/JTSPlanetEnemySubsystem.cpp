#include "space/Systems/JTSPlanetEnemySubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Math/RotationMatrix.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetSettlementEnemy.h"

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
	if (!EnemyArchetype.IsValid())
	{
		const TArray<const UScriptStruct*> FragmentTypes = {
			FJTSPlanetEnemyActorFragment::StaticStruct(),
			FJTSPlanetEnemyMovementFragment::StaticStruct(),
			FJTSPlanetEnemyNavigationFragment::StaticStruct(),
			FJTSPlanetEnemyPerceptionFragment::StaticStruct(),
			FJTSPlanetEnemyCombatFragment::StaticStruct(),
			FJTSPlanetEnemyBehaviorFragment::StaticStruct()
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
		Movement.SurfaceUp = SurfaceHit.ImpactNormal.GetSafeNormal();
	}
	else Movement.SurfaceUp = Planet->GetRadialUpVector(GroundLocation);
	FJTSPlanetEnemyNavigationFragment& Navigation = Manager.GetFragmentDataChecked<FJTSPlanetEnemyNavigationFragment>(Entity);
	Navigation.HomeLocation = HomeLocation;
	Navigation.RoamTarget = HomeLocation;
	Manager.GetFragmentDataChecked<FJTSPlanetEnemyBehaviorFragment>(Entity).Values = Behavior;
	FJTSPlanetEnemyPerceptionFragment& Perception = Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entity);
	Perception.NextScanTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.0f, FMath::Max(0.05f, Behavior.ScanInterval));
	ActiveEntities.Add(Entity);
	return Entity;
}

void UJTSPlanetEnemySubsystem::UnregisterEnemy(FMassEntityHandle Entity)
{
	if (UMassEntitySubsystem* Mass = GetWorld()->GetSubsystem<UMassEntitySubsystem>())
	{
		FMassEntityManager& Manager = Mass->GetMutableEntityManager();
		if (Manager.IsEntityActive(Entity)) Manager.DestroyEntity(Entity);
	}
	ActiveEntities.RemoveSingleSwap(Entity);
}

void UJTSPlanetEnemySubsystem::NotifyDamaged(FMassEntityHandle Entity, AJTSCharacter* Attacker)
{
	if (!IsValid(Attacker) || GetWorld()->GetNetMode() == NM_Client) return;
	UMassEntitySubsystem* Mass = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (Mass == nullptr || !Mass->GetEntityManager().IsEntityActive(Entity)) return;
	FMassEntityManager& Manager = Mass->GetMutableEntityManager();
	const FJTSPlanetEnemyActorFragment& Binding = Manager.GetFragmentDataChecked<FJTSPlanetEnemyActorFragment>(Entity);
	const FJTSPlanetEnemyNavigationFragment& Navigation = Manager.GetFragmentDataChecked<FJTSPlanetEnemyNavigationFragment>(Entity);
	AJTSPlanetAnchor* Planet = Binding.Planet.Get();
	if (!IsEligiblePlayer(Attacker, Planet)
		|| Planet->ApproximateSurfaceArcDistance(Navigation.HomeLocation, Attacker->GetActorLocation())
			> Manager.GetFragmentDataChecked<FJTSPlanetEnemyBehaviorFragment>(Entity).Values.LeashRadius)
	{
		return;
	}
	FJTSPlanetEnemyPerceptionFragment& Perception = Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entity);
	const float Now = GetWorld()->GetTimeSeconds();
	Perception.Target = Attacker;
	Perception.LastKnownLocation = Attacker->GetActorLocation();
	Perception.LastSensedTime = Now;
	Perception.RetaliationUntilTime = Now + 5.0f;
	Perception.bCurrentlyVisible = false;
	Perception.NextScanTime = Now;
}

bool UJTSPlanetEnemySubsystem::IsEligiblePlayer(const AJTSCharacter* Player, const AJTSPlanetAnchor* Planet)
{
	return IsValid(Player) && IsValid(Planet) && Player->GetGameplayPlanet() == Planet
		&& !Player->IsBoarded()
		&& (!IsValid(Player->GetHealthComponent()) || !Player->GetHealthComponent()->IsDead());
}

bool UJTSPlanetEnemySubsystem::HasSightLine(const AActor* Observer, const AJTSCharacter* Player,
	AJTSPlanetAnchor* Planet, const TArray<AActor*>& EnemyActors) const
{
	if (!IsValid(Observer) || !IsValid(Player) || !IsValid(Planet)) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PlanetEnemySight), false, Observer);
	for (AActor* Enemy : EnemyActors) Params.AddIgnoredActor(Enemy);
	const FVector From = Observer->GetActorLocation() + Observer->GetActorUpVector() * 24.0f;
	const FVector To = Player->GetActorLocation() + Planet->GetRadialUpVector(Player->GetActorLocation()) * 55.0f;
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params)
		|| Hit.GetActor() == Player;
}

void UJTSPlanetEnemySubsystem::ScanForTargets(const FJTSPlanetEnemyActorFragment& Binding,
	const FJTSPlanetEnemyNavigationFragment& Navigation,
	FJTSPlanetEnemyPerceptionFragment& Perception,
	const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds,
	const TArray<AJTSCharacter*>& Players, const TArray<AActor*>& EnemyActors)
{
	AJTSPlanetAnchor* Planet = Binding.Planet.Get();
	AActor* Actor = Binding.Actor.Get();
	if (!IsValid(Actor) || !IsValid(Planet)) return;
	Perception.NextScanTime = TimeSeconds + FMath::Max(0.05f, Behavior.ScanInterval);
	AJTSCharacter* Best = nullptr;
	float BestScore = FMath::Max(0.0f, Behavior.SightRadius);
	for (AJTSCharacter* Player : Players)
	{
		if (!IsEligiblePlayer(Player, Planet)
			|| Planet->ApproximateSurfaceArcDistance(Navigation.HomeLocation, Player->GetActorLocation()) > Behavior.LeashRadius)
		{
			continue;
		}
		const float Distance = Planet->ApproximateSurfaceArcDistance(Actor->GetActorLocation(), Player->GetActorLocation());
		const float Score = Distance * (Perception.Target.Get() == Player ? 0.8f : 1.0f);
		if (Distance > Behavior.SightRadius || Score >= BestScore
			|| !HasSightLine(Actor, Player, Planet, EnemyActors)) continue;
		Best = Player;
		BestScore = Score;
	}
	if (Best != nullptr)
	{
		Perception.Target = Best;
		Perception.LastKnownLocation = Best->GetActorLocation();
		Perception.LastSensedTime = TimeSeconds;
		Perception.bCurrentlyVisible = true;
	}
	else
	{
		Perception.bCurrentlyVisible = false;
		if (!IsEligiblePlayer(Perception.Target.Get(), Planet)
			|| TimeSeconds > FMath::Max(Perception.LastSensedTime + Behavior.TargetMemorySeconds,
				Perception.RetaliationUntilTime))
		{
			Perception.Target.Reset();
		}
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

void UJTSPlanetEnemySubsystem::UpdateMovement(const FJTSPlanetEnemyActorFragment& Binding,
	FJTSPlanetEnemyMovementFragment& Movement,
	FJTSPlanetEnemyNavigationFragment& Navigation,
	const FJTSPlanetEnemyPerceptionFragment& Perception,
	const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds, float DeltaSeconds)
{
	AActor* Actor = Binding.Actor.Get();
	AJTSPlanetAnchor* Planet = Binding.Planet.Get();
	if (!IsValid(Actor) || !IsValid(Planet)) return;

	const float HomeDistance = Planet->ApproximateSurfaceArcDistance(Movement.GroundLocation, Navigation.HomeLocation);
	const bool bReturning = HomeDistance > FMath::Max(0.0f, Behavior.LeashRadius) * 0.9f;
	const bool bPursuing = !bReturning && IsEligiblePlayer(Perception.Target.Get(), Planet)
		&& TimeSeconds <= FMath::Max(Perception.LastSensedTime + Behavior.TargetMemorySeconds,
			Perception.RetaliationUntilTime);
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
			Goal = Planet->ApproximateSurfaceArcDistance(Navigation.HomeLocation, Predicted) <= Behavior.LeashRadius
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
	const FVector Direction = FVector::VectorPlaneProject(Goal - Movement.GroundLocation,
		Movement.SurfaceUp).GetSafeNormal();
	const float Acceleration = FMath::Max(1.0f, Behavior.Acceleration);
	const float DesiredSpeed = Direction.IsNearlyZero() ? 0.0f : FMath::Min(FMath::Max(0.0f, TopSpeed),
		FMath::Sqrt(2.0f * Acceleration * FMath::Max(0.0f, Distance - StopDistance)));
	Movement.Velocity = FMath::VInterpConstantTo(Movement.Velocity,
		Direction * DesiredSpeed, DeltaSeconds, Acceleration);
	Movement.Velocity = FVector::VectorPlaneProject(Movement.Velocity, Movement.SurfaceUp);
	const float Step = FMath::Min(Movement.Velocity.Size() * DeltaSeconds,
		FMath::Max(0.0f, Distance - StopDistance));
	FJTSPlanetSurfaceHit Hit;
	if (Step > KINDA_SMALL_NUMBER
		&& Planet->ProjectPointToSurface(Movement.GroundLocation + Movement.Velocity.GetSafeNormal() * Step, Hit))
	{
		const float NextHomeDistance = Planet->ApproximateSurfaceArcDistance(Navigation.HomeLocation, Hit.ImpactPoint);
		if (NextHomeDistance <= Behavior.LeashRadius || NextHomeDistance < HomeDistance)
		{
			Movement.GroundLocation = Hit.ImpactPoint;
			Movement.SurfaceUp = Hit.ImpactNormal.GetSafeNormal();
		}
		else Movement.Velocity = FVector::ZeroVector;
	}
	else if (Step > KINDA_SMALL_NUMBER) Movement.Velocity = FVector::ZeroVector;

	const FVector Facing = Movement.Velocity.SizeSquared() > 25.0f
		? Movement.Velocity.GetSafeNormal() : Direction;
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
	Actor->SetActorLocationAndRotation(DesiredLocation, Rotation, true);
	if (FVector::DistSquared(Actor->GetActorLocation(), DesiredLocation) > 4.0f)
	{
		Movement.Velocity = FVector::ZeroVector;
		FJTSPlanetSurfaceHit ActualHit;
		if (Planet->ProjectPointToSurface(Actor->GetActorLocation(), ActualHit))
		{
			Movement.GroundLocation = ActualHit.ImpactPoint;
			Movement.SurfaceUp = ActualHit.ImpactNormal.GetSafeNormal();
		}
	}
}

void UJTSPlanetEnemySubsystem::UpdateCombat(const FJTSPlanetEnemyActorFragment& Binding,
	const FJTSPlanetEnemyPerceptionFragment& Perception,
	FJTSPlanetEnemyCombatFragment& Combat,
	const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds, const TArray<AActor*>& EnemyActors)
{
	AActor* Actor = Binding.Actor.Get();
	AJTSPlanetAnchor* Planet = Binding.Planet.Get();
	if (!IsValid(Actor) || !IsValid(Planet)) return;
	if (Combat.bImpactPending && TimeSeconds >= Combat.ImpactTime)
	{
		Combat.bImpactPending = false;
		AJTSCharacter* Victim = Combat.PendingVictim.Get();
		Combat.PendingVictim.Reset();
		if (Behavior.AttackDamage > 0.0f && IsEligiblePlayer(Victim, Planet)
			&& Planet->ApproximateSurfaceArcDistance(Actor->GetActorLocation(), Victim->GetActorLocation()) <= Behavior.AttackReach
			&& HasSightLine(Actor, Victim, Planet, EnemyActors))
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

void UJTSPlanetEnemySubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_Client || ActiveEntities.IsEmpty()) return;
	UMassEntitySubsystem* Mass = World->GetSubsystem<UMassEntitySubsystem>();
	if (Mass == nullptr) return;
	FMassEntityManager& Manager = Mass->GetMutableEntityManager();
	const float Now = World->GetTimeSeconds();
	const float Step = FMath::Clamp(DeltaTime, 0.0f, 0.05f);
	TArray<AActor*> EnemyActors;
	EnemyActors.Reserve(ActiveEntities.Num());
	for (const FMassEntityHandle Entity : ActiveEntities)
	{
		if (Manager.IsEntityActive(Entity))
		{
			if (AActor* Actor = Manager.GetFragmentDataChecked<FJTSPlanetEnemyActorFragment>(Entity).Actor.Get())
			{
				EnemyActors.Add(Actor);
			}
		}
	}
	TArray<AJTSCharacter*> Players;
	bool bPlayersGathered = false;
	for (int32 Index = ActiveEntities.Num() - 1; Index >= 0; --Index)
	{
		if (!ActiveEntities.IsValidIndex(Index)) continue;
		const FMassEntityHandle Entity = ActiveEntities[Index];
		if (!Manager.IsEntityActive(Entity)) continue;
		const FJTSPlanetEnemyActorFragment& Binding = Manager.GetFragmentDataChecked<FJTSPlanetEnemyActorFragment>(Entity);
		const AActor* Actor = Binding.Actor.Get();
		const AJTSPlanetAnchor* Planet = Binding.Planet.Get();
		if (!IsValid(Actor) || !IsValid(Planet))
		{
			UnregisterEnemy(Entity);
			continue;
		}
		if (const UJTSHealthComponent* Health = Actor->FindComponentByClass<UJTSHealthComponent>();
			IsValid(Health) && Health->IsDead()) continue;
		FJTSPlanetEnemyMovementFragment& Movement = Manager.GetFragmentDataChecked<FJTSPlanetEnemyMovementFragment>(Entity);
		FJTSPlanetEnemyNavigationFragment& Navigation = Manager.GetFragmentDataChecked<FJTSPlanetEnemyNavigationFragment>(Entity);
		FJTSPlanetEnemyPerceptionFragment& Perception = Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entity);
		FJTSPlanetEnemyCombatFragment& Combat = Manager.GetFragmentDataChecked<FJTSPlanetEnemyCombatFragment>(Entity);
		const FJTSPlanetEnemyBehavior& Behavior = Manager.GetFragmentDataChecked<FJTSPlanetEnemyBehaviorFragment>(Entity).Values;
		if (Perception.NextScanTime <= Now)
		{
			if (!bPlayersGathered)
			{
				for (TActorIterator<AJTSCharacter> It(World); It; ++It) Players.Add(*It);
				bPlayersGathered = true;
			}
			ScanForTargets(Binding, Navigation, Perception, Behavior, Now, Players, EnemyActors);
		}
		UpdateMovement(Binding, Movement, Navigation, Perception, Behavior, Now, Step);
		UpdateCombat(Binding, Perception, Combat, Behavior, Now, EnemyActors);
	}
}
