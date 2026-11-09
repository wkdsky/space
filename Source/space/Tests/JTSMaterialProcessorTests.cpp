#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "space/Components/JTSMaterialProcessorComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSSpaceWorldManager.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/Systems/JTSExpeditionSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "space/Player/JTSCharacter.h"
#include "space/UI/SJTSMaterialGraph.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SWindow.h"
#include "InputCoreTypes.h"

namespace
{
	struct FProcessingWorld
	{
		UWorld* World;
		AJTSSpacecraftActor* Ship;
		UJTSMaterialProcessorComponent* Processor;
		FProcessingWorld()
		{
			const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			// Deliberately no GameInstance: these tests cannot create/read/write a production save subsystem.
			World->InitializeActorsForPlay(FURL());
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Ship = World->SpawnActor<AJTSSpacecraftActor>(FVector(0, 0, 2000), FRotator::ZeroRotator, P);
			Processor = Ship->GetMaterialProcessor();
			Processor->RecipeBook = LoadObject<UJTSMaterialProcessingRecipes>(nullptr,
				TEXT("/Game/Space/Data/Ships/DA_MaterialProcessingRecipes.DA_MaterialProcessingRecipes"));
		}
		~FProcessingWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		AJTSCharacter* Operator()
		{
			AJTSSpaceWorldManager* Manager = World->SpawnActor<AJTSSpaceWorldManager>();
			Manager->SetCurrentPlanet(World->SpawnActor<AJTSPlanetAnchor>());
			Manager->SetSurfaceGameplayReady(true);
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<AJTSCharacter>(Ship->GetBoardingInteractionCenter(), FRotator::ZeroRotator, P);
		}
	};
	FJTSMaterialGraph Chain(std::initializer_list<FName> Programs)
	{
		FJTSMaterialGraph G;
		FJTSMaterialNode Input; Input.Kind = EJTSMaterialNodeKind::Input; Input.Id = 0; G.Nodes.Add(Input);
		int32 Last = 0;
		for (const FName Program : Programs)
		{
			FJTSMaterialNode N; N.Id = Last + 1; N.RecipeId = Program; G.Nodes.Add(N);
			FJTSMaterialLink L; L.From = Last; L.To = N.Id; G.Links.Add(L); Last = N.Id;
		}
		FJTSMaterialNode Output; Output.Kind = EJTSMaterialNodeKind::Output; Output.Id = Last + 1; G.Nodes.Add(Output);
		FJTSMaterialLink L; L.From = Last; L.To = Output.Id; G.Links.Add(L);
		return G;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSProcessingRecipesTest, "JTS.Ship.MaterialProcessor.AuthoredRecipesAndFractionalStorage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSProcessingRecipesTest::RunTest(const FString&)
{
	FProcessingWorld W;
	if (!TestNotNull(TEXT("Blueprint-configurable recipe book exists"), W.Processor->RecipeBook.Get())) return false;
	TestEqual(TEXT("Four approved programs"), W.Processor->RecipeBook->Recipes.Num(), 4);
	UClass* ShipClass = LoadClass<AJTSSpacecraftActor>(nullptr, TEXT("/Game/Space/Blueprints/Ships/BP_Spacecraft.BP_Spacecraft_C"));
	if (!TestNotNull(TEXT("Authored spacecraft Blueprint"), ShipClass)) return false;
	TestTrue(TEXT("Project Blueprint selects the recipe asset"), ShipClass->GetDefaultObject<AJTSSpacecraftActor>()->GetMaterialProcessor()->RecipeBook == W.Processor->RecipeBook);
	W.Ship->RestoreStorageFromExpedition({ { EJTSResourceType::Water, 2 }, { EJTSResourceType::InorganicCarbon, 1 } });
	TMap<EJTSResourceType, int64> Delta; FString Error;
	const auto G = Chain({ TEXT("SynthesizeFuel") });
	if (!TestTrue(TEXT("Half-unit fuel recipe previews"), W.Processor->Preview(G, Delta, Error))) { AddError(Error); return false; }
	TestEqual(TEXT("Consumes one water"), Delta.FindRef(EJTSResourceType::Water), int64(-2));
	TestEqual(TEXT("Consumes half carbon"), Delta.FindRef(EJTSResourceType::InorganicCarbon), int64(-1));
	TestEqual(TEXT("Produces half fuel"), Delta.FindRef(EJTSResourceType::Fuel), int64(1));
	TestTrue(TEXT("First conversion commits"), W.Ship->ApplyMaterialDeltas(Delta));
	TestEqual(TEXT("Half fuel is visible"), W.Ship->GetResourceUnits(EJTSResourceType::Fuel), .5);
	TestEqual(TEXT("Half carbon remains"), W.Ship->GetResourceHalfUnits(EJTSResourceType::InorganicCarbon), int64(1));
	const auto Saved = W.Ship->ExportStorageSnapshot();
	UJTSExpeditionSaveGame* Envelope = NewObject<UJTSExpeditionSaveGame>();
	Envelope->Snapshot.SpacecraftStorage = Saved; Envelope->Snapshot.MaterialProcessingGraph = G;
	TArray<uint8> Bytes;
	TestTrue(TEXT("Snapshot serializes in memory without touching any save slot"), UGameplayStatics::SaveGameToMemory(Envelope, Bytes));
	UJTSExpeditionSaveGame* Loaded = Cast<UJTSExpeditionSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("Snapshot deserializes"), Loaded)) return false;
	W.Ship->RestoreStorageFromExpedition({}); W.Ship->RestoreStorageSnapshot(Loaded->Snapshot.SpacecraftStorage);
	W.Processor->RestoreGraph(Loaded->Snapshot.MaterialProcessingGraph);
	TestEqual(TEXT("Saved connections survive serialization"), W.Processor->GetSavedGraph().Links.Num(), 2);
	TestEqual(TEXT("Save/replication snapshot retains half fuel"), W.Ship->GetResourceHalfUnits(EJTSResourceType::Fuel), int64(1));
	TestTrue(TEXT("Remaining half carbon is usable"), W.Processor->Preview(G, Delta, Error));
	TestTrue(TEXT("Second half fuel commits"), W.Ship->ApplyMaterialDeltas(Delta));
	TestEqual(TEXT("Two halves become one usable whole fuel"), W.Ship->GetFuelCount(), 1);
	TestTrue(TEXT("Whole-unit flight cost can consume merged fuel"), W.Ship->TryConsumeResource(EJTSResourceType::Fuel, 1));
	W.Ship->RestoreStorageFromExpedition({ { EJTSResourceType::MoonAntCorpse, 1 } });
	TestTrue(TEXT("Corpse fuel program previews"), W.Processor->Preview(Chain({ TEXT("ProcessCorpseFuel") }), Delta, Error));
	TestEqual(TEXT("One corpse gives one fuel"), Delta.FindRef(EJTSResourceType::Fuel), int64(2));
	TestEqual(TEXT("One corpse gives three water"), Delta.FindRef(EJTSResourceType::Water), int64(6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSProcessingChainTest, "JTS.Ship.MaterialProcessor.ChainedRecipesAndAtomicFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSProcessingChainTest::RunTest(const FString&)
{
	FProcessingWorld W;
	if (!TestNotNull(TEXT("Authored recipe book"), W.Processor->RecipeBook.Get())) return false;
	W.Ship->RestoreStorageFromExpedition({ { EJTSResourceType::MoonAntCorpse, 1 }, { EJTSResourceType::InorganicCarbon, 1 } });
	const auto G = Chain({ TEXT("ProcessCorpseFood"), TEXT("SynthesizeFood") });
	TMap<EJTSResourceType, int64> Delta; FString Error;
	TestTrue(TEXT("Downstream synthesis uses water from upstream corpse processing"), W.Processor->Preview(G, Delta, Error));
	TestEqual(TEXT("One corpse consumed"), Delta.FindRef(EJTSResourceType::MoonAntCorpse), int64(-2));
	TestEqual(TEXT("Net output is two food"), Delta.FindRef(EJTSResourceType::Food), int64(4));
	TestEqual(TEXT("Net water is two, not duplicated three"), Delta.FindRef(EJTSResourceType::Water), int64(4));
	AJTSCharacter* Player = W.Operator();
	if (!TestTrue(TEXT("Fixture operator can use terminal"), W.Ship->CanUseShipTerminal(Player))) return false;
	TestTrue(TEXT("Server commits full chain"), W.Processor->Submit(Player, G, true, Error));
	TestEqual(TEXT("Two food in shared warehouse"), W.Ship->GetFoodCount(), 2);
	TestFalse(TEXT("Insufficient corpse rejects repeat"), W.Processor->Submit(Player, G, true, Error));
	TestEqual(TEXT("Failed repeat never partially consumes existing food"), W.Ship->GetFoodCount(), 2);
	TestEqual(TEXT("Failed repeat never partially changes water"), W.Ship->GetWaterCount(), 2);
	Player->SetActorLocation(FVector(500000, 0, 2000));
	TestFalse(TEXT("Remote operator cannot execute or save"), W.Processor->Submit(Player, G, false, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSProcessingGraphTest, "JTS.Ship.MaterialProcessor.InvalidGraphAndCanvasEditing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSProcessingGraphTest::RunTest(const FString&)
{
	FProcessingWorld W;
	if (!TestNotNull(TEXT("Authored recipe book"), W.Processor->RecipeBook.Get())) return false;
	TMap<EJTSResourceType, int64> Delta; FString Error;
	auto G = Chain({ TEXT("SynthesizeFood") });
	G.Nodes[1].RecipeId = TEXT("ClientInventedFreeFood");
	TestFalse(TEXT("Client cannot invent a program"), W.Processor->Preview(G, Delta, Error));
	G = Chain({ TEXT("SynthesizeFood") }); G.Links.RemoveAt(0);
	TestFalse(TEXT("Disconnected program cannot run"), W.Processor->Preview(G, Delta, Error));
	G = Chain({ TEXT("SynthesizeFood"), TEXT("SynthesizeFuel") }); G.Links[2].To = 1;
	TestFalse(TEXT("Cycle cannot run"), W.Processor->Preview(G, Delta, Error));
	G = Chain({ TEXT("SynthesizeFood") }); G.Nodes[1].Batches = MAX_int32;
	TestFalse(TEXT("Unbounded batch rejected"), W.Processor->Preview(G, Delta, Error));
	W.Ship->RestoreStorageFromExpedition({ { EJTSResourceType::Water, 1 }, { EJTSResourceType::InorganicCarbon, 1 } });
	TSharedRef<SJTSMaterialGraph> Canvas = SNew(SJTSMaterialGraph);
	Canvas->Initialize(W.Processor); Canvas->ResetGraph(); Canvas->AddProgram(TEXT("SynthesizeFood"));
	TestTrue(TEXT("Palette insertion creates a connected executable chain"), W.Processor->Preview(Canvas->GetGraph(), Delta, Error));
	Canvas->AddProgram(TEXT("SynthesizeFuel")); Canvas->DeleteSelected();
	TestEqual(TEXT("Delete removes selected program"), Canvas->GetGraph().Nodes.Num(), 3);
	TestFalse(TEXT("Deleting a program disconnects rather than silently executes a different chain"), W.Processor->Preview(Canvas->GetGraph(), Delta, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSProcessingCanvasInputTest, "JTS.Ship.MaterialProcessor.SlateDragDisconnectAndReconnect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSProcessingCanvasInputTest::RunTest(const FString&)
{
	FProcessingWorld W;
	if (!TestNotNull(TEXT("Recipe book"), W.Processor->RecipeBook.Get())) return false;
	FSlateApplication& App = FSlateApplication::Get();
	TSharedRef<SJTSMaterialGraph> Canvas = SNew(SJTSMaterialGraph);
	Canvas->Initialize(W.Processor);
	TSharedRef<SWindow> Window = SNew(SWindow).Title(FText::FromString(TEXT("Material processor input test")))
		.ClientSize(FVector2D(990, 350)).SupportsMaximize(false).SupportsMinimize(false) [ Canvas ];
	App.AddWindow(Window);
	App.Tick();
	Canvas->ResetGraph(); Canvas->AddProgram(TEXT("SynthesizeFood"));
	FWidgetPath Path;
	App.GeneratePathToWidgetUnchecked(Canvas, Path);
	const FGeometry Geometry = Canvas->GetCachedGeometry();
	const FModifierKeysState Modifiers;
	auto Pointer = [&](FVector2D Local, FKey Button, bool bDown)
	{
		TSet<FKey> Buttons; if (bDown) Buttons.Add(Button);
		const auto Absolute = Geometry.LocalToAbsolute(Local);
		return FPointerEvent(0, Absolute, Absolute, Buttons, Button, 0.f, Modifiers);
	};
	auto Down = [&](FVector2D P, FKey Button)
	{
		const auto Event = Pointer(P, Button, true);
		const FReply Reply = Canvas->OnMouseButtonDown(Geometry, Event);
		App.ProcessReply(Path, Reply, &Path, &Event);
	};
	auto Up = [&](FVector2D P, FKey Button)
	{
		const auto Event = Pointer(P, Button, false);
		const FReply Reply = Canvas->OnMouseButtonUp(Geometry, Event);
		App.ProcessReply(Path, Reply, &Path, &Event);
	};
	Down(FVector2D(356, 100), EKeys::LeftMouseButton);
	TestTrue(TEXT("Dragging captures the runtime canvas"), Canvas->HasMouseCapture());
	Canvas->OnMouseMove(Geometry, Pointer(FVector2D(406, 125), EKeys::LeftMouseButton, true));
	Up(FVector2D(406, 125), EKeys::LeftMouseButton);
	const auto* Program = Canvas->GetGraph().Nodes.FindByPredicate([](const auto& N) { return N.Kind == EJTSMaterialNodeKind::Program; });
	TestTrue(TEXT("Node follows a 50 by 25 drag in graph space"), Program && Program->Position.Equals(FVector2D(390, 110), .01));
	Down(FVector2D(254, 129), EKeys::RightMouseButton); Up(FVector2D(254, 129), EKeys::RightMouseButton);
	TestEqual(TEXT("Right click disconnects just that port"), Canvas->GetGraph().Links.Num(), 1);
	Down(FVector2D(254, 129), EKeys::LeftMouseButton);
	Canvas->OnMouseMove(Geometry, Pointer(FVector2D(390, 154), EKeys::LeftMouseButton, true));
	Up(FVector2D(390, 154), EKeys::LeftMouseButton);
	TestEqual(TEXT("Dragging output to moved input reconnects"), Canvas->GetGraph().Links.Num(), 2);
	W.Ship->RestoreStorageFromExpedition({ { EJTSResourceType::Water, 1 }, { EJTSResourceType::InorganicCarbon, 1 } });
	TMap<EJTSResourceType, int64> Deltas; FString Error;
	TestTrue(TEXT("Reconnected graph is executable"), W.Processor->Preview(Canvas->GetGraph(), Deltas, Error));
	Down(FVector2D(587, 233), EKeys::LeftMouseButton); Up(FVector2D(587, 233), EKeys::LeftMouseButton);
	Program = Canvas->GetGraph().Nodes.FindByPredicate([](const auto& N) { return N.Kind == EJTSMaterialNodeKind::Program; });
	TestEqual(TEXT("Plus control changes actual batch count"), Program ? Program->Batches : 0, 2);
	Canvas->OnKeyDown(Geometry, FKeyEvent(EKeys::Delete, Modifiers, 0, false, 0, 0));
	TestEqual(TEXT("Delete removes selected program and its connections"), Canvas->GetGraph().Nodes.Num(), 2);
	App.ReleaseAllPointerCapture();
	App.RequestDestroyWindow(Window);
	return true;
}
#endif
