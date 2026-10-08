#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "UObject/UnrealType.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/World/JTSMoonResourceActor.h"
#include "space/World/JTSSurfacePlacementBounds.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMoonMixedResourceTest, "JTS.Moon.Resources.MixedMiningYield",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSMoonMixedResourceTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->InitializeActorsForPlay(FURL());
	AActor* Ground = World->SpawnActor<AActor>();
	UBoxComponent* Floor = NewObject<UBoxComponent>(Ground);
	Ground->SetRootComponent(Floor);
	Floor->SetBoxExtent(FVector(5000, 5000, 25));
	Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Floor->SetCollisionResponseToAllChannels(ECR_Block);
	Floor->RegisterComponent();
	APawn* Miner = World->SpawnActor<APawn>(FVector(0, 0, 150), FRotator::ZeroRotator);
	UJTSInventoryComponent* Inventory = NewObject<UJTSInventoryComponent>(Miner);
	Miner->AddInstanceComponent(Inventory);
	Inventory->RegisterComponent();
	TestTrue(TEXT("Server can equip a mining item"), Inventory->TryAddItemById(EJTSItemId::PowerHammer));
	Inventory->SelectQuickbarSlot(0);

	const EJTSMoonResourceNodeSize Sizes[] = {EJTSMoonResourceNodeSize::MediumRock, EJTSMoonResourceNodeSize::LargeRock,
		EJTSMoonResourceNodeSize::MediumMetalRock, EJTSMoonResourceNodeSize::LargeMetalRock};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Sizes); ++Index)
	{
		const int32 ExpectedRock = Index % 2 == 0 ? 2 : 6;
		const int32 ExpectedMetal = Index < 2 ? 0 : ExpectedRock;
		AJTSMoonResourceActor* Node = World->SpawnActor<AJTSMoonResourceActor>(FVector(300, 0, 150), FRotator::ZeroRotator);
		Node->InitializeMiningNode(EJTSResourceType::Rock, ExpectedRock, Sizes[Index], ExpectedMetal, 1);
		TestEqual(TEXT("Node counts both payload types"), Node->GetTotalYieldUnits(), ExpectedRock + ExpectedMetal);
		const float WorkBefore = Node->GetRemainingMiningWork();
		TestFalse(TEXT("Wrong held item cannot mutate mining state"), Node->ApplyMiningWork(Miner, EJTSItemId::Knife, 1000));
		TestEqual(TEXT("Rejected work leaves the node unchanged"), Node->GetRemainingMiningWork(), WorkBefore);
		for (int32 Hit = 0; Hit < 100 && !Node->IsActorBeingDestroyed(); ++Hit)
		{
			if (!TestTrue(TEXT("Verified mining tool work succeeds"), Node->ApplyMiningWork(Miner, EJTSItemId::PowerHammer, 1000))) break;
		}
		TestTrue(TEXT("Completing mining consumes the node"), Node->IsActorBeingDestroyed());
		int32 Rock = 0, Metal = 0;
		for (TActorIterator<AJTSWorldPickupActor> It(World); It; ++It)
		{
			if (It->IsActorBeingDestroyed()) continue;
			const FJTSItemInstance Item = It->GetItemInstance();
			if (Item.ItemId == EJTSItemId::Rock) Rock += Item.StackCount;
			if (Item.ItemId == EJTSItemId::Ore) Metal += Item.StackCount;
			It->Destroy();
		}
		TestEqual(TEXT("Mining conserves stone yield"), Rock, ExpectedRock);
		TestEqual(TEXT("Mixed mining also produces metal in the same operation"), Metal, ExpectedMetal);
		TestFalse(TEXT("Consumed nodes cannot produce duplicate drops"), Node->ApplyMiningWork(Miner, EJTSItemId::PowerHammer, 1000));
	}
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMoonAuthoredResourceTest, "JTS.Moon.Resources.AuthoredMeshAndCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSMoonAuthoredResourceTest::RunTest(const FString& Parameters)
{
	UClass* ResourceClass = LoadClass<AJTSMoonResourceActor>(nullptr,
		TEXT("/Game/Space/Blueprints/Modes/BP_MoonResourceActor.BP_MoonResourceActor_C"));
	if (!TestNotNull(TEXT("Configured Moon resource Blueprint loads"), ResourceClass)) return false;
	const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->InitializeActorsForPlay(FURL());
	const FVector Up = FVector(1, 2, 3).GetSafeNormal();
	const FRotator Rotation = FRotationMatrix::MakeFromZ(Up).Rotator();
	const EJTSMoonResourceNodeSize Sizes[] = {EJTSMoonResourceNodeSize::MediumRock, EJTSMoonResourceNodeSize::LargeRock,
		EJTSMoonResourceNodeSize::MediumMetalRock, EJTSMoonResourceNodeSize::LargeMetalRock};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Sizes); ++Index)
	{
		for (int32 Variant = 0; Variant < 2; ++Variant)
		{
			AJTSMoonResourceActor* Node = World->SpawnActor<AJTSMoonResourceActor>(ResourceClass, FVector(Index * 1000, Variant * 1000, 500), Rotation);
			Node->InitializeMiningNode(EJTSResourceType::Rock, 2, Sizes[Index], Index >= 2 ? 2 : 0, Variant);
			UStaticMeshComponent* Mesh = Node->FindComponentByClass<UStaticMeshComponent>();
			TestTrue(TEXT("Every node variant uses an authored mesh"), Mesh && Mesh->GetStaticMesh()
				&& Mesh->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Game/Space/Meshes/Resources/Moon/")));
			if (Mesh && Mesh->GetStaticMesh())
			{
				TestTrue(TEXT("Imported pivot is centred at the interaction origin"), Mesh->Bounds.Origin.Equals(Node->GetActorLocation(), 0.1));
				TestTrue(TEXT("Authored variants have consistent unscaled size"), FMath::IsNearlyEqual(
					Mesh->GetStaticMesh()->GetBoundingBox().GetSize().GetMax() * Mesh->GetRelativeScale3D().X, 100.0, 0.1));
				TestTrue(TEXT("All authored material slots are retained"), Mesh->GetNumMaterials() == Mesh->GetStaticMesh()->GetStaticMaterials().Num());
				FHitResult Hit;
				World->LineTraceSingleByChannel(Hit, Node->GetActorLocation() + Up * 300, Node->GetActorLocation() - Up * 300, ECC_Visibility);
				TestTrue(TEXT("Mining trace hits the real rock collision on arbitrary radial up"), Hit.GetActor() == Node);
				// Exercise the presentation direction independently of spawning/ground placement.
				*FindFProperty<FStructProperty>(Node->GetClass(), TEXT("SurfaceUp"))->ContainerPtrToValuePtr<FVector>(Node) = Up;
				FindFProperty<FBoolProperty>(Node->GetClass(), TEXT("bUsesRealPlanetSurface"))->SetPropertyValue_InContainer(Node, Variant != 0);
				Mesh->SetRelativeLocation(Mesh->GetRelativeLocation() + FVector(35, -20, 10));
				Mesh->BoundsScale = 2.5f;
				Mesh->UpdateBounds();
				FJTSSurfaceVisualProjectionBounds Bounds;
				JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(Mesh, Mesh->Bounds.Origin, Up, Bounds);
				const FVector MarkerOffset = Node->GetInteractionAnchorWorldLocation() - Mesh->Bounds.Origin;
				TestTrue(TEXT("Marker remains above visual centre rather than a bounds corner"),
					(MarkerOffset - Up * FVector::DotProduct(MarkerOffset, Up)).IsNearlyZero(0.1));
				TestTrue(TEXT("Marker height uses physical radial bounds and clearance"), FMath::IsNearlyEqual(
					FVector::DotProduct(MarkerOffset, Up), Bounds.HighestProjectionFromRoot + 28.0f, 0.1));
			}
			Node->Destroy();
		}
	}
	AJTSWorldPickupActor* Pickup = World->SpawnActor<AJTSWorldPickupActor>(FVector(0, -1000, 500), Rotation);
	Pickup->InitializeItem(EJTSWorldPickupItemType::Rock);
	*FindFProperty<FStructProperty>(Pickup->GetClass(), TEXT("SurfaceUp"))->ContainerPtrToValuePtr<FVector>(Pickup) = Up;
	UStaticMeshComponent* PickupMesh = Pickup->FindComponentByClass<UStaticMeshComponent>();
	if (TestNotNull(TEXT("Small rock visual loads"), PickupMesh))
	{
		PickupMesh->SetRelativeLocation(PickupMesh->GetRelativeLocation() + FVector(-20, 30, 5));
		FJTSSurfaceVisualProjectionBounds Bounds;
		JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(PickupMesh, PickupMesh->Bounds.Origin, Up, Bounds);
		const FVector MarkerOffset = Pickup->GetInteractionAnchorWorldLocation() - PickupMesh->Bounds.Origin;
		TestTrue(TEXT("Small rock marker follows the offset visual's top centre"), MarkerOffset.Equals(
			Up * (Bounds.HighestProjectionFromRoot + 22.0f), 0.1));
	}
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
