#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Math/RotationMatrix.h"
#include "UObject/UnrealType.h"
#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "space/Components/JTSColonyDefenseComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetColonyNest.h"
#include "space/World/JTSPlanetEcologyHabitat.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSColonyDefenseTest, "JTS.Moon.Colony.FiniteDefensePursuitRecoveryAndReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSColonyDefenseTest::RunTest(const FString&)
{
	struct FScope
	{
		UWorld* World;
		uint64 OriginalFrame = GFrameCounter;
		FScope()
		{
			auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			const FURL URL(nullptr,TEXT("/Engine/Maps/Entry?game=/Script/Engine.GameModeBase"),TRAVEL_Absolute);
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
		}
		~FScope() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter=OriginalFrame; }
	} Scope;
	const FVector Center(2700000,370000,200000), Origin = Center + FVector(10000,0,0);
	const FRotator Frame = FRotationMatrix::MakeFromXZ(FVector::YAxisVector, FVector::XAxisVector).Rotator();
	auto* Planet = Scope.World->SpawnActor<AJTSPlanetAnchor>(); Planet->SetActorLocation(Center);
	auto* Surface = Scope.World->SpawnActor<AStaticMeshActor>();
	Surface->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Surface->SetActorLocation(Center); Surface->SetActorScale3D(FVector(200));
	Surface->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Surface->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	FindFProperty<FObjectProperty>(Planet->GetClass(),TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet,Surface);
	FindFProperty<FFloatProperty>(Planet->GetClass(),TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet,10000);
	auto* Nest = Scope.World->SpawnActor<AJTSPlanetColonyNest>(Origin,Frame);
	auto* Workers = Scope.World->SpawnActor<AJTSPlanetEcologyHabitat>(Origin,Frame);
	auto* Soldiers = Scope.World->SpawnActor<AJTSPlanetEcologyHabitat>(Origin,Frame);
	auto Configure = [&](AJTSPlanetEcologyHabitat* Habitat,int32 Capacity)
	{
		FindFProperty<FClassProperty>(Habitat->GetClass(),TEXT("EnemyClass"))->SetPropertyValue_InContainer(Habitat,AJTSPlanetSurfaceCreature::StaticClass());
		Habitat->SetInitialPopulation(Capacity); Habitat->CoreRadius=650; Habitat->Refuges.Add(Nest);
		FindFProperty<FFloatProperty>(Habitat->GetClass(),TEXT("SpawnRadius"))->SetPropertyValue_InContainer(Habitat,650);
		FindFProperty<FFloatProperty>(Habitat->GetClass(),TEXT("MinimumSpacing"))->SetPropertyValue_InContainer(Habitat,25);
		FindFProperty<FBoolProperty>(Habitat->GetClass(),TEXT("bMaintainPopulation"))->SetPropertyValue_InContainer(Habitat,true);
		FJTSPopulationRecoverySettings Recovery;
		Recovery.bEnabled=true; Recovery.Delay=2; Recovery.Duration=5; Recovery.CheckInterval=.25f;
		Recovery.BatchSize=1; Recovery.MinimumPlayerDistance=0;
		FindFProperty<FStructProperty>(Habitat->GetClass(),TEXT("Recovery"))->CopyCompleteValue(
			FindFProperty<FStructProperty>(Habitat->GetClass(),TEXT("Recovery"))->ContainerPtrToValuePtr<void>(Habitat),&Recovery);
	};
	Configure(Workers,20); Configure(Soldiers,12);
	auto Bind = [&](const TCHAR* Name,UObject* Value) { FindFProperty<FObjectProperty>(Nest->GetClass(),Name)->SetObjectPropertyValue_InContainer(Nest,Value); };
	Bind(TEXT("Planet"),Planet); Bind(TEXT("Workers"),Workers); Bind(TEXT("Guards"),Soldiers);
	auto* Defense=Nest->FindComponentByClass<UJTSColonyDefenseComponent>();
	Defense->EntranceOffsets={FVector(180,0,0),FVector(-180,0,0)};
	auto Tick = [&](float Seconds)
	{
		for (int32 I=0; I<FMath::CeilToInt(Seconds/.05f); ++I) { ++GFrameCounter; Scope.World->Tick(LEVELTICK_All,.05f); }
	};
	Scope.World->BeginPlay();
	if (!Nest->HasActorBegunPlay()) Nest->DispatchBeginPlay();
	for (TActorIterator<AJTSPlanetSurfaceCreature> It(Scope.World);It;++It)
		if (!It->HasActorBegunPlay()) It->DispatchBeginPlay();
	Tick(8);
	TestEqual(TEXT("A colony has 20 finite workers including its sheltered reserve"),Defense->GetLivingWorkers(),20);
	TestEqual(TEXT("Twelve soldiers are living inside rather than an unlimited spawn budget"),Defense->GetLivingSoldiers(),12);
	TestTrue(TEXT("Calm colony exposes 8 to 10 workers"),Defense->GetOutsideWorkers()>=8 && Defense->GetOutsideWorkers()<=10);
	TestEqual(TEXT("Soldiers stay sheltered before provocation"),Defense->GetOutsideSoldiers(),0);
	auto* Pawn=Scope.World->SpawnActor<APawn>();
	auto* Capsule=NewObject<UCapsuleComponent>(Pawn); Capsule->InitCapsuleSize(40,96); Pawn->SetRootComponent(Capsule); Capsule->RegisterComponent();
	auto* PawnHealth=NewObject<UJTSHealthComponent>(Pawn);Pawn->AddInstanceComponent(PawnHealth);PawnHealth->RegisterComponent();
	if (!Pawn->HasActorBegunPlay()) Pawn->DispatchBeginPlay();
	PawnHealth->SetMaxHealth(100000,true);
	FJTSPlanetSurfaceHit TargetSurface; Planet->ProjectPointToSurface(Origin+FVector(0,950,0),TargetSurface);
	Pawn->SetActorLocation(TargetSurface.ImpactPoint+Planet->GetRadialUpVector(TargetSurface.ImpactPoint)*96);
	Pawn->SetActorRotation(Frame);
	Nest->ReceiveMeleeHit_Implementation(Pawn,EJTSMeleeAttackType::Axe);
	Tick(10);
	TestTrue(TEXT("Only 2 to 3 existing workers defend the attacked colony"),Defense->GetOutsideWorkers()>=2 && Defense->GetOutsideWorkers()<=3);
	TestTrue(TEXT("Four to six soldiers emerge for defense"),Defense->GetOutsideSoldiers()>=4 && Defense->GetOutsideSoldiers()<=6);
	TestTrue(TEXT("Soldiers reach and damage a target beyond the calm habitat boundary"),PawnHealth->GetHealth()<100000);
	auto* NestHealth=Nest->FindComponentByClass<UJTSHealthComponent>();
	for (int32 I=0;I<20;++I) { Nest->ReceiveMeleeHit_Implementation(Pawn,EJTSMeleeAttackType::Axe); NestHealth->ApplyDamage(10000,nullptr,Pawn); }
	TestEqual(TEXT("Repeated strikes never damage the colony"),NestHealth->GetHealth(),NestHealth->GetMaxHealth());
	TestEqual(TEXT("Repeated alarms cannot duplicate the finite soldiers"),Defense->GetLivingSoldiers(),12);
	for (TActorIterator<AJTSPlanetSurfaceCreature> It(Scope.World);It;++It)
		if (FindFProperty<FWeakObjectProperty>(It->GetClass(),TEXT("Habitat"))->GetPropertyValue_InContainer(*It).Get()==Soldiers)
			It->FindComponentByClass<UJTSHealthComponent>()->ApplyDamage(100000,nullptr,nullptr);
	Tick(1.5f);
	TestEqual(TEXT("A depleted soldier population remains empty during its recovery delay"),Defense->GetLivingSoldiers(),0);
	Tick(9);
	TestEqual(TEXT("The shared recovery curve restores the finite soldier capacity"),Defense->GetLivingSoldiers(),12);
	Pawn->SetActorLocation(Origin+FVector(0,4000,0));
	Tick(15);
	TestEqual(TEXT("All surviving soldiers return into the permanent colony"),Defense->GetOutsideSoldiers(),0);
	TestTrue(TEXT("After danger leaves the normal worker patrol resumes"),Defense->GetOutsideWorkers()>=8 && Defense->GetOutsideWorkers()<=10);
	int32 ReturningResidents=0;
	for (TActorIterator<AJTSPlanetSurfaceCreature> It(Scope.World);It;++It)
		if (It->IsAlive() && It->IsShelteringInColony() && !It->IsInsideColony()) ++ReturningResidents;
	TestEqual(TEXT("Returning residents finish entering the actual colony mouths"),ReturningResidents,0);
	Nest->SetRole(ROLE_SimulatedProxy);
	Nest->ReceiveMeleeHit_Implementation(Pawn,EJTSMeleeAttackType::Axe);
	Tick(.5f);
	TestEqual(TEXT("A client cannot release a new soldier wave"),Defense->GetOutsideSoldiers(),0);
	Nest->SetRole(ROLE_Authority);
	return true;
}
#endif
