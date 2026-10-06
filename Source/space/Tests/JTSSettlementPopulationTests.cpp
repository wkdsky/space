#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/UnrealType.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetEnemySettlement.h"
#include "space/World/JTSMoonCubeEnemy.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/UI/JTSFloatingDamageActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSSettlementPopulationTest, "JTS.Moon.SettlementMaintains200Cubes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSSettlementPopulationTest::RunTest(const FString&)
{
	struct FWorldScope
	{
		UWorld* World;
		AJTSGameState* State;
		uint64 OriginalFrameCounter = GFrameCounter;
		FWorldScope()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->InitializeActorsForPlay(FURL());
			State = World->SpawnActor<AJTSGameState>();
			World->SetGameState(State);
		}
		~FWorldScope() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = OriginalFrameCounter; }
	} Scope;
	auto* Planet = Scope.World->SpawnActor<AJTSPlanetAnchor>();
	auto* Surface = Scope.World->SpawnActor<AStaticMeshActor>();
	Surface->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Surface->SetActorScale3D(FVector(200));
	Surface->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	FindFProperty<FObjectProperty>(Planet->GetClass(), TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet, Surface);
	FindFProperty<FFloatProperty>(Planet->GetClass(), TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet, 10000.0f);
	auto* Settlement = Scope.World->SpawnActor<AJTSPlanetEnemySettlement>(FVector(0, 0, 10500), FRotator::ZeroRotator);
	FindFProperty<FClassProperty>(Settlement->GetClass(), TEXT("EnemyClass"))->SetPropertyValue_InContainer(Settlement, AJTSMoonCubeEnemy::StaticClass());
	FindFProperty<FIntProperty>(Settlement->GetClass(), TEXT("EnemyCount"))->SetPropertyValue_InContainer(Settlement, 200);
	FindFProperty<FFloatProperty>(Settlement->GetClass(), TEXT("SpawnRadius"))->SetPropertyValue_InContainer(Settlement, 2800.0f);
	FindFProperty<FBoolProperty>(Settlement->GetClass(), TEXT("bMaintainPopulation"))->SetPropertyValue_InContainer(Settlement, true);
	Scope.World->BeginPlay();
	Scope.State->HandleBeginPlay();
	auto Advance = [&](float Seconds)
	{
		for (int32 Step = 0; Step < FMath::CeilToInt(Seconds / 0.1f); ++Step)
		{
			++GFrameCounter;
			Scope.World->Tick(LEVELTICK_All, 0.1f);
		}
	};
	Settlement->Activate(Planet, nullptr);
	if (!TestEqual(TEXT("Initial spherical settlement populates 200 live cubes"), Settlement->GetSpawnedEnemyCount(), 200)) return false;
	Settlement->Activate(Planet, nullptr);
	TestEqual(TEXT("Repeated activation cannot duplicate the population"), Settlement->GetSpawnedEnemyCount(), 200);
	TActorIterator<AJTSMoonCubeEnemy> First(Scope.World);
	for (int32 Pulse = 0; Pulse < 10; ++Pulse) First->GetHealthComponent()->ApplyDamage(1, nullptr, nullptr);
	int32 PopupCount = 0;
	for (TActorIterator<AJTSFloatingDamageActor> It(Scope.World); It; ++It) ++PopupCount;
	TestEqual(TEXT("Rapid damage coalesces into one bounded-lifetime popup per enemy"), PopupCount, 1);
	int32 Killed = 0;
	for (TActorIterator<AJTSMoonCubeEnemy> It(Scope.World); It && Killed < 37; ++It, ++Killed)
		It->GetHealthComponent()->ApplyDamage(2000, nullptr, nullptr);
	TestEqual(TEXT("Dead cubes immediately stop counting even before corpse removal"), Settlement->GetSpawnedEnemyCount(), 163);
	Advance(1.1f);
	TestEqual(TEXT("One refill replenishes only its bounded batch of twenty"), Settlement->GetSpawnedEnemyCount(), 183);
	Advance(1.1f);
	TestEqual(TEXT("Next refill restores the target population"), Settlement->GetSpawnedEnemyCount(), 200);
	for (TActorIterator<AJTSMoonCubeEnemy> It(Scope.World); It; ++It)
		if (!It->GetHealthComponent()->IsDead()) It->GetHealthComponent()->ApplyDamage(2000, nullptr, nullptr);
	TestEqual(TEXT("A full clear leaves zero live enemies"), Settlement->GetSpawnedEnemyCount(), 0);
	Advance(11.0f);
	TestEqual(TEXT("Repeated refill restores 200 after a full clear"), Settlement->GetSpawnedEnemyCount(), 200);
	Advance(2.0f);
	TestEqual(TEXT("Steady state never exceeds 200"), Settlement->GetSpawnedEnemyCount(), 200);
	Settlement->Deactivate();
	Advance(2.0f);
	TestEqual(TEXT("Deactivation clears enemies and cancels replenishment"), Settlement->GetSpawnedEnemyCount(), 0);
	Settlement->SetRole(ROLE_SimulatedProxy);
	Settlement->Activate(Planet, nullptr);
	Advance(2.0f);
	TestEqual(TEXT("Client-role settlement cannot generate enemies"), Settlement->GetSpawnedEnemyCount(), 0);
	Settlement->SetRole(ROLE_Authority);
	return true;
}
#endif
