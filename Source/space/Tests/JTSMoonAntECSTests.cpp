#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "space/Core/JTSGameState.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSStellarTargetComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Player/JTSCharacter.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Systems/JTSPlanetEnemySubsystem.h"
#include "space/Systems/JTSPlanetAntFragments.h"
#include "space/World/JTSMoonAntCorpsePickupActor.h"
#include "space/World/JTSSpaceWorldManager.h"
#include "space/Tests/JTSMoonAntTestHabitat.h"

namespace
{
	struct FAntWorld
	{
		UWorld* World;
		uint64 OriginalFrame = GFrameCounter;
		FAntWorld()
		{
			const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->InitializeActorsForPlay(FURL());
			World->SetGameState(World->SpawnActor<AJTSGameState>());
			World->BeginPlay();
			World->GetGameState()->HandleBeginPlay();
		}
		~FAntWorld()
		{
			World->EndPlay(EEndPlayReason::Quit);
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
			GFrameCounter = OriginalFrame;
		}
		void Step(float Seconds)
		{
			for (int32 I = 0; I < FMath::CeilToInt(Seconds * 60); ++I)
			{
				++GFrameCounter;
				World->Tick(LEVELTICK_All, 1.f / 60);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSAntCorpseItemTest, "JTS.Moon.Ants.CorpsePickupAndOrganicDeposit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSAntCorpseItemTest::RunTest(const FString&)
{
	FAntWorld W;
	// Existing authored definitions must keep overriding native defaults.
	const auto* Rock = UJTSItemDefinitionLibrary::GetItemDefinition(W.World, EJTSItemId::Rock);
	if (!TestNotNull(TEXT("Authored rock definition available"), Rock)) return false;
	TestTrue(TEXT("Rock lookup retains its authored Data Asset"), Rock == LoadObject<UJTSItemDefinition>(nullptr,
		TEXT("/Game/Space/Data/Items/DA_Item_Rock.DA_Item_Rock")));
	const auto* Definition = UJTSItemDefinitionLibrary::GetItemDefinition(W.World, EJTSItemId::MoonAntCorpse);
	if (!TestNotNull(TEXT("Corpse remains available without an optional Data Asset"), Definition)) return false;
	TestTrue(TEXT("Corpse can still be held"), Definition->IsHoldable());
	TestEqual(TEXT("Corpse identity is retained"), Definition->ItemId, EJTSItemId::MoonAntCorpse);

	FJTSMoonAntTestHabitat Habitat(W.World);
	auto* Ant = Habitat.SpawnAnt(FVector(500, 0, 0));
	if (!TestNotNull(TEXT("Live ant spawns"), Ant)) return false;
	Ant->GetHealthComponent()->ApplyDamage(10000, nullptr, nullptr);
	TActorIterator<AJTSMoonAntCorpsePickupActor> CorpseIt(W.World);
	if (!TestTrue(TEXT("Death produces a corpse pickup"), bool(CorpseIt))) return false;
	auto* Corpse = *CorpseIt;
	TestEqual(TEXT("Drop contains the corpse item"), Corpse->GetItemInstance().ItemId, EJTSItemId::MoonAntCorpse);
	W.Step(.6f);

	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Player = W.World->SpawnActor<AJTSCharacter>(Corpse->GetActorLocation() + FVector(0, 0, 100), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Collecting character spawns"), Player)) return false;
	auto* Inventory = Player->GetInventoryComponent();
	if (!TestNotNull(TEXT("Character has inventory"), Inventory)) return false;
	TestTrue(TEXT("Settled corpse can be collected"), Corpse->CanInteract_Implementation(Player));
	Corpse->Interact_Implementation(Player);
	TestEqual(TEXT("Pickup adds exactly one corpse to inventory"), Inventory->GetItemCount(EJTSItemId::MoonAntCorpse), 1);
	TestTrue(TEXT("Collected corpse actor is consumed"), Corpse->IsActorBeingDestroyed());

	W.World->SpawnActor<AJTSSpaceWorldManager>();
	auto* Ship = W.World->SpawnActor<AJTSSpacecraftActor>(FVector(5000, 0, 1000), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Shared spacecraft spawns"), Ship)) return false;
	const int32 OrganicBefore = Ship->GetResourceAmount(EJTSResourceType::Organic);
	Player->SetActorLocation(Ship->GetBoardingInteractionCenter());
	Ship->TryDepositResourcesFromPawn(Player); // Overlap may already have submitted the item.
	TestEqual(TEXT("Depositing consumes the carried corpse"), Inventory->GetItemCount(EJTSItemId::MoonAntCorpse), 0);
	TestEqual(TEXT("A corpse becomes one shared Organic resource"), Ship->GetResourceAmount(EJTSResourceType::Organic), OrganicBefore + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSAntMassLifecycleTest, "JTS.Moon.Ants.MassRegistrationMovementAndCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSAntMassLifecycleTest::RunTest(const FString&)
{
	FAntWorld W;
	FJTSMoonAntTestHabitat Habitat(W.World);
	if (!TestNotNull(TEXT("Authored ant class available"), Habitat.AntClass)) return false;
	auto* AI = W.World->GetSubsystem<UJTSPlanetEnemySubsystem>();
	auto& Manager = W.World->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
	TArray<AJTSMoonAntActor*> Ants;
	TArray<FMassEntityHandle> Handles;
	TArray<FVector> Starts;
	for (int32 I = 0; I < 270; ++I)
	{
		auto* Ant = Habitat.SpawnAnt(FVector((I % 27 - 13) * 75, (I / 27 - 5) * 75, 0));
		if (!TestTrue(TEXT("Native spawn registers a real Mass entity"), IsValid(Ant) && Ant->HasMassEntity())) return false;
		TestFalse(TEXT("Server Actor Tick disabled"), Ant->IsActorTickEnabled());
		Ants.Add(Ant); Handles.Add(Ant->GetMassEntityHandle()); Starts.Add(Ant->GetActorLocation());
	}
	TestEqual(TEXT("One entity per ant"), AI->GetRegisteredAntCount(), 270);
	W.Step(3);
	int32 Moving = 0;
	for (int32 I = 0; I < Ants.Num(); ++I)
	{
		if (FVector::Distance(Starts[I], Ants[I]->GetActorLocation()) > 10) ++Moving;
		TestFalse(TEXT("Finite spherical movement"), Ants[I]->GetActorLocation().ContainsNaN());
	}
	TestEqual(TEXT("Every ant advances in the shared host"), AI->GetLastMovementCount(), 270);
	TestTrue(TEXT("Mass movement continues without Actor Tick"), Moving > 250);
	Ants[0]->GetHealthComponent()->ApplyDamage(10000, nullptr, nullptr);
	TestFalse(TEXT("Death releases entity immediately"), Manager.IsEntityActive(Handles[0]));
	TestEqual(TEXT("Kill preserves corpse drop"), TActorIterator<AJTSMoonAntCorpsePickupActor>(W.World) ? 1 : 0, 1);
	Ants[1]->Destroy();
	TestEqual(TEXT("Explicit actor destruction also releases identity"), AI->GetRegisteredAntCount(), 268);
	for (int32 I = 2; I < Ants.Num(); ++I)
		Manager.GetFragmentDataChecked<FJTSPlanetAntActivityFragment>(Handles[I]).SurfaceDuration = .01f;
	W.Step(2);
	TestEqual(TEXT("Burrowing safely releases the whole batch"), AI->GetRegisteredAntCount(), 0);
	for (const auto Handle : Handles) TestFalse(TEXT("No leaked Mass handles"), Manager.IsEntityActive(Handle));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSAntMassControlTest, "JTS.Moon.Ants.MassStatusMovementControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSAntMassControlTest::RunTest(const FString&)
{
	FAntWorld W;
	FJTSMoonAntTestHabitat Habitat(W.World);
	auto* Ant = Habitat.SpawnAnt(FVector(500, 0, 0));
	if (!TestTrue(TEXT("Ant has ECS identity"), IsValid(Ant) && Ant->HasMassEntity())) return false;
	W.Step(.5f);
	auto* Status = Ant->FindComponentByClass<UJTSStellarTargetComponent>();
	auto& Manager = W.World->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
	const auto Handle = Ant->GetMassEntityHandle();
	Status->ApplySlow(.5f, 5);
	W.Step(.1f);
	TestTrue(TEXT("Slow scales entity desired speed"), FMath::IsNearlyEqual(float(
		Manager.GetFragmentDataChecked<FJTSPlanetEnemyMovementFragment>(Handle).DesiredVelocity.Size()), 42.5f, .1f));
	auto* Source = W.World->SpawnActor<APawn>();
	Status->ApplyCold(Source, 100, 0, 0);
	TestTrue(TEXT("Freeze applied to live ant"), Status->IsFrozen());
	const FVector FrozenPosition = Ant->GetActorLocation();
	W.Step(.5f);
	TestTrue(TEXT("Frozen ant stops under Mass movement"), FrozenPosition.Equals(Ant->GetActorLocation(), .01f));
	// A departed caster leaves control to expire, without the normal-tier shatter execution on thaw.
	Source->Destroy();
	W.Step(2.2f);
	TestFalse(TEXT("Freeze expires normally"), Status->IsFrozen());
	TestTrue(TEXT("Motion resumes after control expiration"), FVector::Distance(FrozenPosition, Ant->GetActorLocation()) > 2);
	W.Step(2);
	Status->ApplyRoot(1.f);
	TestTrue(TEXT("Root suppresses voluntary movement"), Status->IsHardControlled());
	const FVector RootedPosition = Ant->GetActorLocation();
	W.Step(.1f);
	TestTrue(TEXT("Root alone leaves the ant stationary"), RootedPosition.Equals(Ant->GetActorLocation(), .01f));
	const FVector Before = Ant->GetActorLocation();
	// A previous corner escape must not redirect an external force back against its source.
	auto& Traversal = Manager.GetFragmentDataChecked<FJTSPlanetAntActivityFragment>(Handle).Traversal;
	Traversal.PreviousHeading = Traversal.AvoidanceHeading = FVector(0, -1, 0);
	Traversal.AvoidanceRemaining = 10;
	Source = W.World->SpawnActor<APawn>();
	auto* Field = W.World->SpawnActor<AActor>();
	Status->ApplyAttraction(Source, Field, Before + FVector(0, 500, 0), 1500, 1000, 30, .5f);
	W.Step(.4f);
	TestTrue(TEXT("Field displaces an ant even during stopped locomotion"), FVector::Distance(Before, Ant->GetActorLocation()) > 10);
	TestTrue(TEXT("External force follows its source instead of old roaming memory"), Ant->GetActorLocation().Y > Before.Y + 10);
	FJTSPlanetSurfaceHit Hit;
	TestTrue(TEXT("Field motion retains real planet surface"), Habitat.Planet->ProjectPointToSurface(Ant->GetActorLocation(), Hit)
		&& FVector::Distance(Hit.ImpactPoint, Ant->GetActorLocation()) < 20);
	return true;
}
#endif
