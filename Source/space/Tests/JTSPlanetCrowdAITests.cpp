#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "UObject/UnrealType.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "space/Core/JTSGameState.h"
#include "space/Components/JTSStellarTargetComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/World/JTSMoonCubeEnemy.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/Systems/JTSPlanetEnemySubsystem.h"
#include "space/Systems/JTSPlanetCrowdNavigation.h"
#include "space/Systems/JTSPlanetEnemyPursuit.h"
#include "space/Systems/JTSReplicatedMotionBuffer.h"

namespace
{
	// Development-only fixture for observing the real rendered level. No assets or AI defaults change.
	FAutoConsoleCommandWithWorldAndArgs GBodyTestSpawn(TEXT("jts.EnemyBody.TestSpawn"),
		TEXT("Development QA: TestSpawn <planet actor name> <count> <surface X> <Y> <Z>. Spawns stationary coincident bodies."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client || Args.Num() != 5) return;
			AJTSPlanetAnchor* Planet = nullptr;
			for (TActorIterator<AJTSPlanetAnchor> It(World); It; ++It) if (It->GetName() == Args[0]) Planet = *It;
			if (!Planet) return;
			FJTSPlanetSurfaceHit Hit;
			if (!Planet->ProjectPointToSurface(FVector(FCString::Atod(*Args[2]), FCString::Atod(*Args[3]), FCString::Atod(*Args[4])), Hit)) return;
			const FVector Ground = Hit.ImpactPoint, Up = Planet->GetRadialUpVector(Ground);
			FJTSPlanetEnemyBehavior Behavior; Behavior.RoamSpeed = 0; Behavior.ChaseSpeed = 0;
			Behavior.SeparationWeight = 0; Behavior.bAcquireOnSpawn = false;
			auto* AI = World->GetSubsystem<UJTSPlanetEnemySubsystem>();
			FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			for (int32 I = 0; I < FMath::Clamp(FCString::Atoi(*Args[1]), 1, 1000); ++I)
			{
				auto* Cube = World->SpawnActor<AJTSMoonCubeEnemy>(Ground + Up * Behavior.HoverHeight,
					FRotationMatrix::MakeFromZ(Up).Rotator(), Spawn);
				Cast<UBoxComponent>(Cube->GetRootComponent())->IgnoreActorWhenMoving(Planet->GetGameplaySurfaceActor(), true);
				AI->RegisterEnemy(Cube, Planet, Ground, Ground, Behavior);
			}
		}));
	struct FCrowdWorld
	{
		UWorld* World;
		AJTSPlanetAnchor* Planet;
		AStaticMeshActor* Surface;
		AJTSGameState* State;
		uint64 OriginalFrameCounter = GFrameCounter;
		FCrowdWorld()
		{
			const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->InitializeActorsForPlay(FURL());
			State = World->SpawnActor<AJTSGameState>(); World->SetGameState(State);
			Planet = World->SpawnActor<AJTSPlanetAnchor>();
			Surface = World->SpawnActor<AStaticMeshActor>();
			Surface->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
			Surface->SetActorScale3D(FVector(200));
			Surface->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
			FindFProperty<FObjectProperty>(Planet->GetClass(), TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet, Surface);
			FindFProperty<FFloatProperty>(Planet->GetClass(), TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet, 10000.0f);
		}
		~FCrowdWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = OriginalFrameCounter; }
		FVector Ground(const FVector& Candidate) const
		{
			FJTSPlanetSurfaceHit Hit;
			return Planet->ProjectPointToSurface(Candidate, Hit) ? Hit.ImpactPoint : Candidate;
		}
		void Begin() { World->BeginPlay(); State->HandleBeginPlay(); }
		void Step() { ++GFrameCounter; World->Tick(LEVELTICK_All, 1.0f / 60); }
		AJTSCharacter* Player(const FVector& GroundPosition)
		{
			FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* P = World->SpawnActor<AJTSCharacter>(GroundPosition + Planet->GetRadialUpVector(GroundPosition) * 100, FRotator::ZeroRotator, Spawn);
			P->SetGameplayPlanet(Planet); P->SetActorTickEnabled(false);
			P->GetCharacterMovement()->DisableMovement(); P->GetCharacterMovement()->SetComponentTickEnabled(false);
			return P;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSCrowdForceTest, "JTS.Moon.CrowdPhysicalFieldAndBudgets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSCrowdForceTest::RunTest(const FString&)
{
	FCrowdWorld W; W.Begin();
	const FVector Home=W.Ground(FVector(0,0,10000)), Up=W.Planet->GetRadialUpVector(Home);
	auto* Source=W.Player(Home); auto* Field=W.World->SpawnActor<AActor>();
	auto* AI=W.World->GetSubsystem<UJTSPlanetEnemySubsystem>();
	TArray<AJTSMoonCubeEnemy*> Cubes;
	FJTSPlanetEnemyBehavior Behavior; Behavior.LeashRadius=3000; Behavior.RoamRadius=1000;
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for(int32 I=0;I<200;++I)
	{
		const float Angle=(I%20)*2*PI/20, Radius=I==0?0:350+(I/20)*65;
		const FVector Ground=W.Ground(Home+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0));
		auto* Cube=W.World->SpawnActor<AJTSMoonCubeEnemy>(Ground+W.Planet->GetRadialUpVector(Ground)*45,FRotator::ZeroRotator,Spawn);
		Cast<UBoxComponent>(Cube->GetRootComponent())->IgnoreActorWhenMoving(W.Surface,true);
		AI->RegisterEnemy(Cube,W.Planet,Home,Ground,Behavior); Cubes.Add(Cube);
	}
	float TotalMs=0;
	for(int32 Frame=0;Frame<240;++Frame)
	{
		for(auto* Cube:Cubes)
		{
			auto* Status=Cube->FindComponentByClass<UJTSStellarTargetComponent>();
			Status->ApplyAttraction(Source,Field,Home+Up*73,1650,900,65,.3f);
			Status->ApplyRepulsion(Source,180,650,.2f);
		}
		W.Step(); TotalMs+=AI->GetLastTickMilliseconds();
		if(AI->GetLastSteeringCount()>80 || AI->GetLastMovementCount()!=200 || AI->GetLastSightQueryCount()>64 || AI->GetLastNavigationQueryCount()>64)
			AddError(TEXT("Forces must retain the shared crowd work budgets"));
	}
	float Minimum=MAX_flt, Mean=0;
	for(auto* Cube:Cubes)
	{
		const float Distance=W.Planet->ApproximateSurfaceArcDistance(Home,Cube->GetActorLocation());
		Minimum=FMath::Min(Minimum,Distance); Mean+=Distance;
		TestFalse(TEXT("Crowd force integration remains finite"),Cube->GetActorLocation().ContainsNaN());
	}
	AddInfo(FString::Printf(TEXT("200 force-controlled Mass cubes: mean tick %.3f ms; minimum player distance %.1f cm; mean distance %.1f cm (headless CPU smoke test)"),TotalMs/240,Minimum,Mean/200));
	TestTrue(TEXT("Even crowded enemies cannot enter the player's attack reach"),Minimum>400);
	TestTrue(TEXT("An enemy born overlapping the caster's capsule is expelled instead of sweep-locked"),
		W.Planet->ApproximateSurfaceArcDistance(Home,Cubes[0]->GetActorLocation())>600);
	TestFalse(TEXT("Temporary repulsion egress never leaves normal pawn collision ignored"),
		Cast<UBoxComponent>(Cubes[0]->GetRootComponent())->GetMoveIgnoreActors().Contains(Source));
	TestTrue(TEXT("Crowd remains around the force boundary instead of flying away indefinitely"),Mean/200>600 && Mean/200<1200);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSCrowdAcquisitionTest, "JTS.Moon.CrowdAcquisitionAndBudgets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSCrowdAcquisitionTest::RunTest(const FString&)
{
	FCrowdWorld Scope;
	Scope.Begin();
	const FVector Home = Scope.Ground(FVector(0,0,10000));
	AJTSCharacter* Player = Scope.Player(Scope.Ground(FVector(1600,0,10000)));
	auto* AI = Scope.World->GetSubsystem<UJTSPlanetEnemySubsystem>();
	auto& Manager = Scope.World->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
	FJTSPlanetEnemyBehavior Behavior;
	Behavior.SightRadius = 6500; Behavior.AggroRadius = 3500; Behavior.LeashRadius = 4500;
	Behavior.RetaliationLeashRadius = 9000; Behavior.RoamRadius = 2800; Behavior.HomeReturnRadius = 600;
	Behavior.ChaseSpeed = 380; Behavior.TargetMemorySeconds = 6; Behavior.RetaliationMemorySeconds = 15;
	TArray<FMassEntityHandle> Entities;
	TArray<AJTSMoonCubeEnemy*> Actors;
	float InitialDistance = 0;
	for (int32 I = 0; I < 200; ++I)
	{
		const FVector Ground = Scope.Ground(FVector((I % 20 - 10) * 130, (I / 20 - 5) * 130, 10000));
		FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Cube = Scope.World->SpawnActor<AJTSMoonCubeEnemy>(Ground + Scope.Planet->GetRadialUpVector(Ground) * 45, FRotator::ZeroRotator, Spawn);
		Cast<UBoxComponent>(Cube->GetRootComponent())->IgnoreActorWhenMoving(Scope.Surface, true);
		Entities.Add(AI->RegisterEnemy(Cube, Scope.Planet, Home, Ground, Behavior)); Actors.Add(Cube);
		InitialDistance += Scope.Planet->ApproximateSurfaceArcDistance(Ground, Player->GetActorLocation());
	}
	for (int32 Frame = 0; Frame < 12; ++Frame)
	{
		Scope.Step();
		if (AI->GetLastScanCount() > 24 || AI->GetLastSightQueryCount() > 64 || AI->GetLastSteeringCount() > 80 || AI->GetLastMovementCount() != 200
			|| AI->GetLastNavigationQueryCount() > 64) AddError(TEXT("A crowd frame exceeded its work budget"));
	}
	TestEqual(TEXT("All 200 acquire a nearby player within twelve budgeted frames"), AI->GetTrackedTargetCount(), 200);
	TArray<float> Times;
	FJTSPlanetEnemyWorkStats Phases;
	for (int32 Frame = 0; Frame < 180; ++Frame)
	{
		Scope.Step(); Times.Add(AI->GetLastTickMilliseconds());
		const auto& Stats = AI->GetLastWorkStats();
		Phases.NavigationMilliseconds += Stats.NavigationMilliseconds;
		Phases.GatherMilliseconds += Stats.GatherMilliseconds;
		Phases.SenseMilliseconds += Stats.SenseMilliseconds;
		Phases.MovementMilliseconds += Stats.MovementMilliseconds;
		Phases.CombatMilliseconds += Stats.CombatMilliseconds;
		Phases.SeparationMilliseconds += Stats.SeparationMilliseconds;
		Phases.SurfaceMilliseconds += Stats.SurfaceMilliseconds;
		Phases.TransformMilliseconds += Stats.TransformMilliseconds;
	}
	float FinalDistance = 0, TotalMs = 0;
	for (auto* Cube : Actors) FinalDistance += Scope.Planet->ApproximateSurfaceArcDistance(Cube->GetActorLocation(), Player->GetActorLocation());
	for (float Ms : Times) TotalMs += Ms;
	Times.Sort();
	AddInfo(FString::Printf(TEXT("200-cube server AI: mean %.3f ms, p95 %.3f ms (headless automation, not rendered/multiplayer FPS)"),
		TotalMs / Times.Num(), Times[FMath::FloorToInt(Times.Num() * 0.95f)]));
	AddInfo(FString::Printf(TEXT("AI phase averages (ms): nav %.3f, gather %.3f, sense %.3f, movement %.3f, combat %.3f"),
		Phases.NavigationMilliseconds / 180, Phases.GatherMilliseconds / 180, Phases.SenseMilliseconds / 180,
		Phases.MovementMilliseconds / 180, Phases.CombatMilliseconds / 180));
	AddInfo(FString::Printf(TEXT("Movement phase averages (ms): separation %.3f, surface %.3f, transform %.3f"),
		Phases.SeparationMilliseconds / 180, Phases.SurfaceMilliseconds / 180, Phases.TransformMilliseconds / 180));
	TestTrue(TEXT("The crowd closes distance instead of remaining at its settlement"), FinalDistance < InitialDistance * 0.75f);

	// A distant shot wakes the settlement even when outside normal visibility / behind the horizon.
	const FVector Far = Scope.Ground(FVector(7000,0,10000));
	Player->SetActorLocation(Far + Scope.Planet->GetRadialUpVector(Far) * 100);
	AI->NotifyDamaged(Entities[0], Player);
	const auto& Retaliation = Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entities[0]);
	TestTrue(TEXT("A player firing from well beyond the old 11m leash is pursued"), Retaliation.Target.Get() == Player);
	TestTrue(TEXT("Retaliation retains the Blueprint-configurable fifteen-second memory"),
		Retaliation.RetaliationUntilTime >= Scope.World->GetTimeSeconds() + 14.9f);
	for (int32 Frame = 0; Frame < 30; ++Frame) Scope.Step();
	TestEqual(TEXT("An attacked settlement shares the alert with the entire crowd"), AI->GetTrackedTargetCount(), 200);
	const float ColonyDeadline = Retaliation.RetaliationUntilTime;
	AI->NotifyDamaged(Entities[1], Player);
	TestEqual(TEXT("A later hit on another colony member inherits the original deadline"),
		Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entities[1]).RetaliationUntilTime, ColonyDeadline);
	// Deliberately place the player outside the spherical leash, rather than use a flat distance.
	Player->SetActorLocation(FVector(10000,0,0));
	for (int32 Frame = 0; Frame < 35; ++Frame) Scope.Step();
	TestEqual(TEXT("Targets outside the 90m retaliation leash are released"), AI->GetTrackedTargetCount(), 0);
	AJTSPlanetAnchor* OtherPlanet = Scope.World->SpawnActor<AJTSPlanetAnchor>();
	Player->SetActorLocation(Home); Player->SetGameplayPlanet(OtherPlanet);
	AI->NotifyDamaged(Entities[0], Player);
	TestFalse(TEXT("A character on another planet cannot trigger retaliation"),
		Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entities[0]).Target.IsValid());
	Scope.Player(Scope.Ground(FVector(-1300,0,10000)));
	Scope.Player(Scope.Ground(FVector(0,1400,10000)));
	Scope.Player(Scope.Ground(FVector(0,-1500,10000)));
	Player->SetGameplayPlanet(Scope.Planet);
	Player->SetActorLocation(Home + FVector(0,0,100));
	// Recover before the old colony alert expires: it must not re-enrol returned enemies.
	for (int32 Frame = 0; Frame < 600; ++Frame) Scope.Step();
	TMap<int32, float> EligibleSince;
	for (int32 Frame = 0; Frame < 35; ++Frame)
	{
		Scope.Step();
		for (int32 I = 0; I < Entities.Num(); ++I)
		{
			const auto E = Entities[I];
			const bool bEligible = !Manager.GetFragmentDataChecked<FJTSPlanetEnemyNavigationFragment>(E).bReturningHome
				&& Scope.World->GetTimeSeconds() >= Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(E).NextAcquireAllowedTime;
			if (bEligible) EligibleSince.FindOrAdd(I, Scope.World->GetTimeSeconds());
			else EligibleSince.Remove(I);
		}
		if (AI->GetLastScanCount() > 24 || AI->GetLastSightQueryCount() > 64 || AI->GetLastSteeringCount() > 80 || AI->GetLastMovementCount() != 200)
			AddError(TEXT("Four-player acquisition exceeded its work budget"));
	}
	int32 EligibleCount = 0;
	for (const auto E : Entities)
		if (!Manager.GetFragmentDataChecked<FJTSPlanetEnemyNavigationFragment>(E).bReturningHome
			&& Scope.World->GetTimeSeconds() >= Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(E).NextAcquireAllowedTime)
			++EligibleCount;
	// Independent footprints can delay entry into the crowded return area. Returning/cooldown bodies
	// remain deliberately ineligible; this check covers acquisition starvation, not crowd formation.
	TestTrue(TEXT("Some returned bodies are eligible to acquire again"), EligibleCount > 0);
	TestTrue(TEXT("Returning bodies cannot be selected as eligible targets"), AI->GetTrackedTargetCount() <= EligibleCount);
	for (const auto& Entry : EligibleSince)
		if (Scope.World->GetTimeSeconds() - Entry.Value > Behavior.ScanInterval * 1.15f + .2f)
			TestTrue(TEXT("Every body eligible for a full scan budget eventually reacquires"),
				Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entities[Entry.Key]).Target.IsValid());
	for (const FMassEntityHandle E : Entities)
	{
		const auto& P = Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(E);
		TestEqual(TEXT("Returned colony members retain the completed episode deadline"), P.CompletedRetaliationUntilTime, ColonyDeadline);
		TestFalse(TEXT("A stale colony alert cannot restart retaliation after returning"), P.bProvokedPursuit);
	}
	TestTrue(TEXT("The old colony episode is still live during this regression check"), Scope.World->GetTimeSeconds() < ColonyDeadline);
	Player->SetActorLocation(Far + Scope.Planet->GetRadialUpVector(Far) * 100);
	AI->NotifyDamaged(Entities[0], Player);
	TestFalse(TEXT("Another hit cannot rejoin the completed but still-live colony episode"),
		Manager.GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Entities[0]).bProvokedPursuit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSCrowdContinuousMotionTest, "JTS.Moon.CrowdEveryFrameMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSCrowdContinuousMotionTest::RunTest(const FString&)
{
	FCrowdWorld W; W.Begin();
	const FVector Home = W.Ground(FVector(0,0,10000));
	auto* AI = W.World->GetSubsystem<UJTSPlanetEnemySubsystem>();
	auto& Manager = W.World->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
	FJTSPlanetEnemyBehavior Behavior;
	Behavior.RoamRadius = 8000; Behavior.LeashRadius = 9000; Behavior.HomeReturnRadius = 50;
	Behavior.RoamSpeed = 200; Behavior.ChaseSpeed = 380; Behavior.Acceleration = 900;
	Behavior.IdleMovementInterval = 0.3f; Behavior.SeparationWeight = 0;
	TArray<AJTSMoonCubeEnemy*> Cubes;
	TArray<FMassEntityHandle> Entities;
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 I = 0; I < 200; ++I)
	{
		const FVector Ground = W.Ground(FVector(1200 + (I%20)*130, (I/20-5)*130, 10000));
		auto* Cube = W.World->SpawnActor<AJTSMoonCubeEnemy>(Ground + W.Planet->GetRadialUpVector(Ground)*45, FRotator::ZeroRotator, Spawn);
		Cast<UBoxComponent>(Cube->GetRootComponent())->IgnoreActorWhenMoving(W.Surface, true);
		const FMassEntityHandle E = AI->RegisterEnemy(Cube, W.Planet, Home, Ground, Behavior);
		auto& N = Manager.GetFragmentDataChecked<FJTSPlanetEnemyNavigationFragment>(E);
		N.RoamTarget = W.Ground(Ground + FVector(0,1600,0)); N.NextRoamTime = 1000;
		Cubes.Add(Cube); Entities.Add(E);
	}
	for (int32 Frame = 0; Frame < 45; ++Frame) W.Step();
	int32 Pauses = 0, LargeSteps = 0;
	float TotalMs = 0;
	for (int32 Mode = 0; Mode < 2; ++Mode)
	{
		if (Mode == 1)
		{
			for (const FMassEntityHandle E : Entities)
				Manager.GetFragmentDataChecked<FJTSPlanetEnemyNavigationFragment>(E).bReturningHome = true;
			for (int32 Frame = 0; Frame < 45; ++Frame) W.Step();
		}
		for (int32 Frame = 0; Frame < 60; ++Frame)
		{
			TArray<FVector> Before; for (auto* Cube : Cubes) Before.Add(Cube->GetActorLocation());
			W.Step(); TotalMs += AI->GetLastTickMilliseconds();
			TestEqual(TEXT("All 200 collision bodies advance every frame"), AI->GetLastMovementCount(), 200);
			for (int32 I = 0; I < Cubes.Num(); ++I)
			{
				const double Step = FVector::Distance(Before[I], Cubes[I]->GetActorLocation());
				if (Step < 0.1) ++Pauses;
				if (Step > (Behavior.ChaseSpeed + AI->BodySeparationSettings.MaxCorrectionSpeed) / 60.0 + 1) ++LargeSteps;
				TestTrue(TEXT("The swept collision body publishes movement velocity"), Cubes[I]->GetVelocity().Size() > 1);
			}
		}
	}
	TestEqual(TEXT("Patrol and return have no artificial frozen frames at 300ms steering intervals"), Pauses, 0);
	TestEqual(TEXT("No interval-sized position jumps"), LargeSteps, 0);
	AddInfo(FString::Printf(TEXT("200 continuous collision bodies: mean AI %.3f ms over patrol/return (headless CPU only)"), TotalMs/120));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSCrowdSnapshotTest, "JTS.Moon.CrowdSnapshotInterpolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSCrowdSnapshotTest::RunTest(const FString&)
{
	FJTSReplicatedMotionBuffer Buffer;
	FVector Position = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	Buffer.Push(0, FVector::ZeroVector, FQuat::Identity, FVector(380,0,0));
	int32 Packet = 1, Pauses = 0;
	double PreviousX = 0, MaxStep = 0;
	for (int32 Frame = 0; Frame < 360; ++Frame)
	{
		const double Time = Frame/120.0;
		while (Packet/30.0 + (Packet%3 == 0 ? 0.012 : 0.0) <= Time)
		{
			const double SendTime = Packet/30.0;
			const double ReceiveTime = SendTime + (Packet%3 == 0 ? 0.012 : 0.0);
			// One dropped snapshot, plus variable arrival spacing, at an unchanged 30Hz network rate.
			if (Packet != 35)
				Buffer.Push(ReceiveTime, FVector(SendTime*380,0,0), FQuat(FVector::UpVector, SendTime*.5), FVector(380,0,0));
			++Packet;
		}
		TestTrue(TEXT("A motion sample is available"), Buffer.Sample(Time, .1f, Position, Rotation));
		if (Frame > 25)
		{
			const double Step = Position.X - PreviousX;
			if (Step < .01) ++Pauses;
			MaxStep = FMath::Max(MaxStep, Step);
			TestTrue(TEXT("Interpolation remains finite and forward moving"), Step >= 0 && !Position.ContainsNaN() && Rotation.IsNormalized());
		}
		PreviousX = Position.X;
	}
	TestEqual(TEXT("120fps presentation never freezes between jittered 30Hz snapshots"), Pauses, 0);
	TestTrue(TEXT("Jitter does not produce packet-sized position jumps"), MaxStep < 8);
	Buffer.Push(3.0, FVector(1140,0,0), FQuat::Identity, FVector::ZeroVector);
	Buffer.Push(3.1, FVector(1140,0,0), FQuat::Identity, FVector::ZeroVector);
	Buffer.Sample(4.0, .1f, Position, Rotation);
	TestTrue(TEXT("A stopped collision never extrapolates through an obstacle"), Position.Equals(FVector(1140,0,0)));
	Buffer.Push(4.0, FVector(10000,0,0), FQuat::Identity, FVector::ZeroVector);
	Buffer.Sample(4.0, .1f, Position, Rotation);
	TestTrue(TEXT("Teleport resets interpolation immediately"), Position.Equals(FVector(10000,0,0)));
	Buffer.Push(3.9, FVector(0,0,0), FQuat::Identity, FVector::ZeroVector);
	Buffer.Sample(4.1, .1f, Position, Rotation);
	TestTrue(TEXT("Stale samples cannot rewind a replicated enemy"), Position.Equals(FVector(10000,0,0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSPursuitLimitsTest, "JTS.Moon.PursuitDistanceAndFixedDeadline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSPursuitLimitsTest::RunTest(const FString&)
{
	FCrowdWorld Scope; Scope.Begin();
	const FVector Home = Scope.Ground(FVector(0,0,10000));
	AJTSCharacter* Player = Scope.Player(Scope.Ground(FVector(5500,0,10000)));
	FJTSPlanetEnemyBehavior Behavior;
	Behavior.AggroRadius = 3500; Behavior.LeashRadius = 4500;
	Behavior.RetaliationLeashRadius = 9000; Behavior.RetaliationMemorySeconds = 15;
	Behavior.HomeReturnRadius = 600; Behavior.ReacquireCooldown = 3;
	FJTSPlanetEnemyNavigationFragment Navigation; Navigation.HomeLocation = Home;
	FJTSPlanetEnemyPerceptionFragment Perception;
	TestFalse(TEXT("A passive enemy cannot acquire a player outside its home-centred guard radius"),
		FJTSPlanetEnemyPursuit::CanAcquire(Scope.Planet, Home, Player, Navigation, Perception, Behavior, 10));
	TestTrue(TEXT("A damaging shot outside the guard radius starts temporary retaliation"),
		FJTSPlanetEnemyPursuit::Retaliate(Scope.Planet, Navigation, Perception, Behavior, Player, 10));
	const float Deadline = Perception.RetaliationUntilTime;
	TestTrue(TEXT("Retaliation uses the extended distance boundary"), Perception.bProvokedPursuit);
	for (float Now = 11; Now < 25; Now += 1)
	{
		TestTrue(TEXT("Repeated damage retains the active episode"),
			FJTSPlanetEnemyPursuit::Retaliate(Scope.Planet, Navigation, Perception, Behavior, Player, Now));
		TestEqual(TEXT("Continuous shooting cannot refresh the fifteen-second deadline"), Perception.RetaliationUntilTime, Deadline);
		Perception.bCurrentlyVisible = true; Perception.LastSensedTime = Now;
		FJTSPlanetEnemyPursuit::Update(Scope.Planet, Scope.Ground(FVector(4500,0,10000)), Navigation, Perception, Behavior, Now);
	}
	FJTSPlanetEnemyPursuit::Update(Scope.Planet, Scope.Ground(FVector(4500,0,10000)), Navigation, Perception, Behavior, 25);
	TestTrue(TEXT("Time expires even while the attacker remains visible and inside the distance boundary"), Navigation.bReturningHome);
	TestFalse(TEXT("Expiration clears the pursuit target"), Perception.Target.IsValid());
	TestEqual(TEXT("Returning records which retaliation episode has ended"), Perception.CompletedRetaliationUntilTime, Deadline);
	TestFalse(TEXT("Shooting a returning enemy cannot restart the chase"),
		FJTSPlanetEnemyPursuit::Retaliate(Scope.Planet, Navigation, Perception, Behavior, Player, 26));
	FJTSPlanetEnemyPursuit::Update(Scope.Planet, Home, Navigation, Perception, Behavior, 30);
	TestFalse(TEXT("Reaching the home radius ends the return state"), Navigation.bReturningHome);
	TestFalse(TEXT("A short home cooldown prevents instant repeated retaliation"),
		FJTSPlanetEnemyPursuit::Retaliate(Scope.Planet, Navigation, Perception, Behavior, Player, 31));
	TestTrue(TEXT("A later attack can start a fresh bounded episode after returning"),
		FJTSPlanetEnemyPursuit::Retaliate(Scope.Planet, Navigation, Perception, Behavior, Player, 34));
	Player->SetActorLocation(FVector(10000,0,0));
	FJTSPlanetEnemyPursuit::Update(Scope.Planet, Scope.Ground(FVector(4500,0,10000)), Navigation, Perception, Behavior, 35);
	TestTrue(TEXT("Crossing the distance boundary cancels before the time deadline"), Navigation.bReturningHome);
	Navigation = FJTSPlanetEnemyNavigationFragment(); Navigation.HomeLocation = Home;
	Perception = FJTSPlanetEnemyPerceptionFragment();
	TestFalse(TEXT("Damage from beyond the outer retaliation boundary cannot start an unlimited chase"),
		FJTSPlanetEnemyPursuit::Retaliate(Scope.Planet, Navigation, Perception, Behavior, Player, 40));
	Player->SetActorLocation(Scope.Ground(FVector(2500,0,10000)) + FVector(0,0,100));
	TestTrue(TEXT("A nearby player can be acquired in normal patrol mode"),
		FJTSPlanetEnemyPursuit::CanAcquire(Scope.Planet, Home, Player, Navigation, Perception, Behavior, 41));
	Perception.Target = Player; Perception.LastSensedTime = 41;
	Player->SetActorLocation(Scope.Ground(FVector(5500,0,10000)) + FVector(0,0,100));
	FJTSPlanetEnemyPursuit::Update(Scope.Planet, Home, Navigation, Perception, Behavior, 42);
	TestTrue(TEXT("Normal sight acquisition never grants the extended retaliation leash"), Navigation.bReturningHome);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSCrowdSphericalRouteTest, "JTS.Moon.SharedSphericalObstacleRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSCrowdSphericalRouteTest::RunTest(const FString&)
{
	FCrowdWorld Scope;
	auto* Wall = Scope.World->SpawnActor<AStaticMeshActor>(FVector(0,0,10040), FRotator::ZeroRotator);
	Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Wall->SetActorScale3D(FVector(1.6f,13,6));
	Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Scope.Begin();
	FJTSPlanetCrowdNavigation Navigation;
	const FVector Home = Scope.Ground(FVector(0,0,10000));
	const FVector Goal = Scope.Ground(FVector(1500,0,10000));
	FVector Position = Scope.Ground(FVector(-1500,0,10000));
	float MaxSideways = 0;
	int32 FramesWithoutRoute = 0;
	for (int32 Frame = 0; Frame < 600; ++Frame)
	{
		const float Time = Frame / 60.0f;
		FVector Direction = Navigation.GetDirection(Scope.Planet, Home, Position, Goal, nullptr, 2000, 6000, Time);
		// Repeated consumers share one field, with zero additional immediate physics work.
		for (int32 I = 0; I < 199; ++I)
			Navigation.GetDirection(Scope.Planet, Home, Position, Goal, nullptr, 2000, 6000, Time);
		Navigation.Tick(Scope.World, Time, 64, 1024);
		if (Navigation.GetLastPhysicsQueryCount() > 64) AddError(TEXT("Shared navigation exceeded its physics budget"));
		if (Direction.IsNearlyZero()) { ++FramesWithoutRoute; continue; }
		const FVector Next = Scope.Ground(Position + Direction * 60);
		FCollisionQueryParams Params; Params.AddIgnoredActor(Scope.Surface);
		FHitResult Hit;
		const bool Blocked = Scope.World->SweepSingleByChannel(Hit, Position + Scope.Planet->GetRadialUpVector(Position) * 65,
			Next + Scope.Planet->GetRadialUpVector(Next) * 65, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(40), Params);
		if (Blocked) { AddError(TEXT("Shared flow direction attempted to cross the wall")); break; }
		Position = Next; MaxSideways = FMath::Max(MaxSideways, FMath::Abs(Position.Y));
		if (Scope.Planet->ApproximateSurfaceArcDistance(Position, Goal) < 180) break;
	}
	TestTrue(TEXT("The route goes around the 13m-wide wall"), MaxSideways > 680);
	TestTrue(TEXT("Shared route reaches the target on the real mesh"), Scope.Planet->ApproximateSurfaceArcDistance(Position, Goal) < 180);
	TestTrue(TEXT("Time slicing eventually supplies a route"), FramesWithoutRoute < 500);
	TestEqual(TEXT("Two hundred consumers use one completed shared field"), Navigation.GetReadyFieldCount(), 1);
	// A different side of the planet must use radial up, not global Z.
	FJTSPlanetCrowdNavigation SideNavigation;
	const FVector SideHome = Scope.Ground(FVector(10000,0,0));
	const AJTSCharacter* SidePlayer = Scope.Player(SideHome);
	const FVector EyeOffset = SidePlayer->GetPawnViewLocation() - SidePlayer->GetActorLocation();
	TestTrue(TEXT("Combat eye height follows radial gravity on the side of the planet"),
		EyeOffset.Size() > 30 && FVector::DotProduct(EyeOffset.GetSafeNormal(), Scope.Planet->GetRadialUpVector(SideHome)) > 0.99f);
	const FVector SideGoal = Scope.Ground(FVector(10000,1200,0));
	FVector SideDirection;
	for (int32 Frame = 0; Frame < 200; ++Frame)
	{
		SideDirection = SideNavigation.GetDirection(Scope.Planet, SideHome, SideHome, SideGoal, nullptr, 1000, 5000, Frame / 60.f);
		SideNavigation.Tick(Scope.World, Frame / 60.f);
	}
	TestFalse(TEXT("Side-of-planet route is available"), SideDirection.IsNearlyZero());
	TestTrue(TEXT("Side route is tangent to radial gravity"),
		FMath::Abs(FVector::DotProduct(SideDirection, Scope.Planet->GetRadialUpVector(SideHome))) < 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSBodyLifecycleTest, "JTS.Moon.BodyCollision.RuntimeLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSBodyLifecycleTest::RunTest(const FString&)
{
	FCrowdWorld W; W.Begin();
	const FVector Home = W.Ground(FVector(0,0,10000));
	auto* AI = W.World->GetSubsystem<UJTSPlanetEnemySubsystem>();
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Cube = W.World->SpawnActor<AJTSMoonCubeEnemy>(Home+W.Planet->GetRadialUpVector(Home)*45, FRotator::ZeroRotator, Spawn);
	TestTrue(TEXT("Settlement initialization succeeds"), IJTSPlanetSettlementEnemy::Execute_InitializeForSettlement(Cube, W.Planet, Home, Home));
	FJTSPlanetEnemyBehavior Behavior;
	const auto First = AI->RegisterEnemy(Cube, W.Planet, Home, Home, Behavior);
	TestTrue(TEXT("Repeated registration returns the same entity"), First == AI->RegisterEnemy(Cube,W.Planet,Home,Home,Behavior));
	W.Step();
	FJTSPlanetEnemyBodyFragment Data;
	TestTrue(TEXT("Body snapshot exists"), AI->GetBodyCollisionData(First, Data));
	const uint64 Id = Data.StableId;
	TestEqual(TEXT("Only one footprint registered"), AI->GetBodySeparationStats().Participants, 1);
	Cube->SetActorEnableCollision(false); W.Step();
	TestEqual(TEXT("Actor collision off removes footprint"), AI->GetBodySeparationStats().Participants, 0);
	Cube->SetActorEnableCollision(true); W.Step();
	TestEqual(TEXT("Actor collision on restores footprint"), AI->GetBodySeparationStats().Participants, 1);
	AI->SetBodyCollisionEnabled(First, false); W.Step();
	TestEqual(TEXT("Explicit collision state disables participation"), AI->GetBodySeparationStats().Participants, 0);
	AI->UnregisterEnemy(First); AI->UnregisterEnemy(First);
	TestFalse(TEXT("Pool release removes stale handle"), AI->GetBodyCollisionData(First, Data));
	const auto Second = AI->RegisterEnemy(Cube,W.Planet,Home,Home,Behavior);
	AI->GetBodyCollisionData(Second, Data);
	TestTrue(TEXT("Reused Actor obtains a fresh stable identity"), Data.StableId != Id);
	AI->UnregisterEnemy(Second);
	TestTrue(TEXT("Settlement initialization accepts an explicitly released live Actor"),
		IJTSPlanetSettlementEnemy::Execute_InitializeForSettlement(Cube,W.Planet,Home,Home));
	const auto Reused = AI->RegisterEnemy(Cube,W.Planet,Home,Home,Behavior);
	W.Step(); TestEqual(TEXT("Reuse does not retain duplicate footprint"), AI->GetBodySeparationStats().Participants, 1);
	Cube->Destroy();
	TestFalse(TEXT("Actor destruction immediately unregisters even direct registrations"), AI->GetBodyCollisionData(Reused, Data));
	W.Step(); TestEqual(TEXT("Destroyed Actor leaves no footprint"), AI->GetBodySeparationStats().Participants, 0);
	auto* Dead = W.World->SpawnActor<AJTSMoonCubeEnemy>(Home,FRotator::ZeroRotator,Spawn);
	IJTSPlanetSettlementEnemy::Execute_InitializeForSettlement(Dead,W.Planet,Home,Home);
	const auto DeadEntity=AI->RegisterEnemy(Dead,W.Planet,Home,Home,Behavior);
	W.Step(); Dead->GetHealthComponent()->ApplyDamage(2000,nullptr,nullptr);
	TestFalse(TEXT("Existing death rule immediately unregisters body"), AI->GetBodyCollisionData(DeadEntity,Data));
	TestEqual(TEXT("Death removes participation before delayed corpse destruction"), AI->GetBodySeparationStats().Participants, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSBodyEnvironmentTest, "JTS.Moon.BodyCollision.RuntimeMovementAndEnvironment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSBodyEnvironmentTest::RunTest(const FString&)
{
	FCrowdWorld W; W.Begin();
	auto* AI=W.World->GetSubsystem<UJTSPlanetEnemySubsystem>();
	auto& Manager=W.World->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
	const FVector Home=W.Ground(FVector(0,0,10000));
	FJTSPlanetEnemyBehavior Behavior; Behavior.RoamSpeed=0; Behavior.ChaseSpeed=0; Behavior.SeparationWeight=0;
	Behavior.bAcquireOnSpawn=false; Behavior.CollisionRadius=70;
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto SpawnBody=[&](const FVector& Ground)
	{
		auto* Cube=W.World->SpawnActor<AJTSMoonCubeEnemy>(Ground+W.Planet->GetRadialUpVector(Ground)*45,FRotator::ZeroRotator,Spawn);
		Cast<UBoxComponent>(Cube->GetRootComponent())->IgnoreActorWhenMoving(W.Surface,true);
		const auto Entity=AI->RegisterEnemy(Cube,W.Planet,Home,Ground,Behavior);
		return TPair<AJTSMoonCubeEnemy*,FMassEntityHandle>(Cube,Entity);
	};
	auto A=SpawnBody(Home), B=SpawnBody(Home);
	IJTSPlanetSettlementEnemy::Execute_OnSettlementAttackStarted(A.Key);
	Manager.GetFragmentDataChecked<FJTSPlanetEnemyCombatFragment>(A.Value).bImpactPending=true;
	Manager.GetFragmentDataChecked<FJTSPlanetEnemyCombatFragment>(A.Value).ImpactTime=MAX_flt;
	for(int32 I=0;I<120;++I) W.Step();
	TestEqual(TEXT("Stationary/attacking bodies are still participants"), AI->GetBodySeparationStats().Participants, 2);
	TestEqual(TEXT("Static coincident bodies separate on the real mesh"), AI->GetBodySeparationStats().ResidualPairs, 0);
	TestTrue(TEXT("Body positions remain finite"), !A.Key->GetActorLocation().ContainsNaN() && !B.Key->GetActorLocation().ContainsNaN());
	for(auto Item:{A,B})
	{
		const FVector Ground=W.Ground(Item.Key->GetActorLocation());
		TestTrue(TEXT("Correction retains real ground hover height"), FMath::Abs(FVector::Distance(Item.Key->GetActorLocation(),Ground)-45)<.2);
	}
	// Preserve active movement intent while only projecting overlapping bodies apart.
	A.Key->SetActorLocation(W.Ground(Home+FVector(-160,0,0))+FVector(0,0,45));
	B.Key->SetActorLocation(W.Ground(Home+FVector(160,0,0))+FVector(0,0,45));
	for(auto Item:{A,B})
	{
		auto& Move=Manager.GetFragmentDataChecked<FJTSPlanetEnemyMovementFragment>(Item.Value);
		Move.GroundLocation=W.Ground(Item.Key->GetActorLocation()); Move.NextUpdateTime=MAX_flt;
		Move.DesiredVelocity=FVector(Item.Value==A.Value?120:-120,0,0);
		Move.GoalLocation=Home+FVector(Item.Value==A.Value?1000:-1000,0,0); Move.StopDistance=0;
	}
	for(int32 I=0;I<180;++I)
	{
		W.Step();
		TestTrue(TEXT("Opposing native movement does not continually occupy the same space"), FVector::Distance(A.Key->GetActorLocation(),B.Key->GetActorLocation())>=140.5);
		TestTrue(TEXT("Position projection cannot create abnormal velocity"), A.Key->GetVelocity().Size()<425 && B.Key->GetVelocity().Size()<425);
	}
	A.Key->Destroy(); B.Key->Destroy();
	// A wall constrains accumulated correction. Residual overlap is permitted and diagnosed.
	auto* Wall=W.World->SpawnActor<AStaticMeshActor>(Home+FVector(150,0,45),FRotator::ZeroRotator);
	Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
	Wall->SetActorScale3D(FVector(1,12,6)); Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	const FVector NearWall=W.Ground(Home+FVector(54,0,0));
	A=SpawnBody(NearWall); B=SpawnBody(NearWall);
	for(int32 I=0;I<120;++I)
	{
		W.Step();
		TestTrue(TEXT("Environment-constrained separation never pushes native box through wall"), A.Key->GetActorLocation().X<=55.1 && B.Key->GetActorLocation().X<=55.1);
	}
	TestTrue(TEXT("Solver exposes bounded residuals instead of hiding bodies"), AI->GetBodySeparationStats().Iterations<=AI->BodySeparationSettings.Iterations);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSBodyDenseSurfaceTest, "JTS.Moon.BodyCollision.RuntimeDenseCurvedSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSBodyDenseSurfaceTest::RunTest(const FString&)
{
	FCrowdWorld W; W.Begin();
	auto* AI = W.World->GetSubsystem<UJTSPlanetEnemySubsystem>();
	const FVector Home = W.Ground(FVector(2000, 4500, 8500));
	const FVector Up = W.Planet->GetRadialUpVector(Home);
	FJTSPlanetEnemyBehavior Behavior; Behavior.RoamSpeed = 0; Behavior.ChaseSpeed = 0;
	Behavior.SeparationWeight = 0; Behavior.bAcquireOnSpawn = false;
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TArray<AJTSMoonCubeEnemy*> Cubes;
	for (int32 I = 0; I < 200; ++I)
	{
		auto* Cube = W.World->SpawnActor<AJTSMoonCubeEnemy>(Home + Up * 45, FRotationMatrix::MakeFromZ(Up).Rotator(), Spawn);
		Cast<UBoxComponent>(Cube->GetRootComponent())->IgnoreActorWhenMoving(W.Surface, true);
		AI->RegisterEnemy(Cube, W.Planet, Home, Home, Behavior); Cubes.Add(Cube);
	}
	float MaxStep = 0, MaxGroundError = 0, TailSpeed = 0;
	for (int32 Frame = 0; Frame < 1800; ++Frame)
	{
		W.Step(); MaxStep = FMath::Max(MaxStep, AI->GetBodySeparationStats().MaxStepCorrection);
		for (auto* Cube : Cubes)
		{
			if (Cube->GetActorLocation().ContainsNaN()) { AddError(TEXT("Dense curved-surface body produced NaN")); return false; }
			if (Frame >= 1740) TailSpeed = FMath::Max(TailSpeed, float(Cube->GetVelocity().Size()));
		}
	}
	for (auto* Cube : Cubes) MaxGroundError = FMath::Max(MaxGroundError,
		float(FMath::Abs(FVector::Distance(Cube->GetActorLocation(), W.Ground(Cube->GetActorLocation())) - 45)));
	AddInfo(FString::Printf(TEXT("200 coincident curved-mesh actors: residual %d, max depth %.3f cm, max step %.3f cm, ground error %.3f cm, tail speed %.3f cm/s"),
		AI->GetBodySeparationStats().ResidualPairs, AI->GetBodySeparationStats().MaxPenetration, MaxStep, MaxGroundError, TailSpeed));
	TestEqual(TEXT("Native mesh constraints allow 200 coincident stationary bodies to settle"), AI->GetBodySeparationStats().ResidualPairs, 0);
	TestEqual(TEXT("All 200 stationary bodies retain participation"), AI->GetBodySeparationStats().Participants, 200);
	TestTrue(TEXT("All solver iterations obey the total step budget"), MaxStep <= 5.001f);
	TestTrue(TEXT("Dense separation retains the real mesh surface"), MaxGroundError < .2f);
	TestTrue(TEXT("Dense settled bodies do not continue to jitter"), TailSpeed < 1);
	return true;
}
#endif
