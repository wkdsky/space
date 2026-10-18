#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/UnrealType.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/World/JTSSpaceDebugTravelManager.h"

namespace
{
	struct FDebugTestWorld
	{
		UWorld* World;
		FDebugTestWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
		~FDebugTestWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSInventoryMouseTransactionTest, "JTS.Debug.InventoryMouseTransactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSInventoryMouseTransactionTest::RunTest(const FString& Parameters)
{
	FDebugTestWorld Fixture;
	auto* Character = Fixture.World->SpawnActor<AJTSCharacter>();
	auto* Inventory = Character->GetInventoryComponent();
	Inventory->TryAddItemById(EJTSItemId::WaistLamp);
	const auto Lamp = Inventory->GetItemAtSlot(0);
	TestFalse(TEXT("Fixture contains a real instance"), Lamp.IsEmpty());
	Inventory->ServerMoveInventorySlot(0, FGuid::NewGuid(), 1, FGuid());
	TestEqual(TEXT("A stale drag cannot move a different item"), Inventory->GetItemAtSlot(0).InstanceId, Lamp.InstanceId);
	Inventory->ServerMoveInventorySlot(0, Lamp.InstanceId, 1, FGuid());
	TestTrue(TEXT("Source becomes empty"), Inventory->GetItemAtSlot(0).IsEmpty());
	TestEqual(TEXT("Payload identity survives move"), Inventory->GetItemAtSlot(1).InstanceId, Lamp.InstanceId);
	Inventory->ServerEquipInventorySlot(0, Lamp.InstanceId);
	TestEqual(TEXT("Stale equip cannot select emptied source"), Inventory->GetSelectedQuickbarSlot(), 1);
	Inventory->ServerEquipInventorySlot(1, Lamp.InstanceId);
	TestTrue(TEXT("Right-click equip lights the belt lamp"), Inventory->IsWaistLampEquipped());
	Inventory->ServerDropInventorySlot(1, FGuid::NewGuid());
	TestEqual(TEXT("Stale drop preserves inventory"), Inventory->GetItemAtSlot(1).InstanceId, Lamp.InstanceId);
	Inventory->ServerMoveInventorySlot(1, Lamp.InstanceId, 99, FGuid());
	TestEqual(TEXT("Out of range destination rejected"), Inventory->GetUsedSlotCount(), 1);
	Inventory->ServerDropInventorySlot(1, Lamp.InstanceId);
	TestEqual(TEXT("A failed world drop preserves the carried payload"), Inventory->GetItemAtSlot(1).InstanceId, Lamp.InstanceId);
	auto* Ground = Fixture.World->SpawnActor<AStaticMeshActor>();
	Ground->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Ground->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Ground->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Ground->SetActorLocation(FVector(0, 0, -100));
	Ground->SetActorScale3D(FVector(100, 100, 1));
	Inventory->ServerDropInventorySlot(1, Lamp.InstanceId);
	TestTrue(TEXT("Valid drop removes the complete carried payload"), Inventory->GetItemAtSlot(1).IsEmpty());
	TestFalse(TEXT("Dropped lamp no longer remains equipped"), Inventory->IsWaistLampEquipped());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSDebugTravelAuthorityTest, "JTS.Debug.TravelAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSDebugTravelAuthorityTest::RunTest(const FString& Parameters)
{
	FDebugTestWorld Fixture;
	auto* Debug = Fixture.World->SpawnActor<AJTSSpaceDebugTravelManager>();
	auto* Controller = Fixture.World->SpawnActor<AJTSPlayerController>();
	auto* State = Fixture.World->SpawnActor<AJTSPlayerState>();
	FindFProperty<FObjectProperty>(Controller->GetClass(), TEXT("PlayerState"))->SetObjectPropertyValue_InContainer(Controller, State);
	TestFalse(TEXT("Non-host cannot mutate shared travel"), Debug->CanUse(Controller));
	State->SetExpeditionHost(true);
	TestTrue(TEXT("Host can use enabled development controls"), Debug->CanUse(Controller));
	TestFalse(TEXT("An arbitrary planet is never implicitly a debug destination"), Debug->ContainsDestination(TEXT("UnconfiguredPlanet")));
	TestFalse(TEXT("None is never a valid destination"), Debug->ContainsDestination(NAME_None));
	FindFProperty<FBoolProperty>(Debug->GetClass(), TEXT("bEnabled"))->SetPropertyValue_InContainer(Debug, false);
	TestFalse(TEXT("Disabled controls reject the host as well"), Debug->CanUse(Controller));
	return true;
}
#endif
