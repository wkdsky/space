#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/LineBatchComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "space/World/JTSMoonResourceActor.h"
#include "space/World/JTSMoonResourceCoverage.h"
#include "space/World/JTSMoonResourceSpawner.h"
#include "space/World/JTSPlanetAnchor.h"

namespace
{
	struct FCoverageWorld
	{
		UWorld* World = nullptr;
		AJTSPlanetAnchor* Planet = nullptr;

		FCoverageWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->InitializeActorsForPlay(FURL());
			Planet = World->SpawnActor<AJTSPlanetAnchor>();
			FindFProperty<FFloatProperty>(Planet->GetClass(), TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet, 15000);
			AActor* Surface = World->SpawnActor<AActor>();
			UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Surface);
			Surface->SetRootComponent(Mesh);
			Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
			Mesh->SetWorldScale3D(FVector(300));
			Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Mesh->SetCollisionResponseToAllChannels(ECR_Block);
			Mesh->RegisterComponent();
			FindFProperty<FObjectPropertyBase>(Planet->GetClass(), TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet, Surface);
			World->BeginPlay();
		}

		~FCoverageWorld()
		{
			World->EndPlay(EEndPlayReason::Quit);
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		FVector Point(double Angle) const { return FVector(FMath::Sin(Angle), 0, FMath::Cos(Angle)) * Planet->GetApproximateRadius(); }

		AJTSMoonResourceSpawner* Add(double Angle, bool bCrystal = false)
		{
			AJTSMoonResourceSpawner* Spawner = World->SpawnActor<AJTSMoonResourceSpawner>(Point(Angle), FRotator::ZeroRotator);
			Spawner->bOverrideCoverageRadius = true;
			Spawner->Radius = 2000;
			Spawner->ResourceDensity = 0.8f;
			FJTSMoonResourceSpawnSettings Settings;
			Settings.MinimumResourceSpacing = 20;
			Spawner->ApplyMoonSpawnSettings(Settings);
			Spawner->SetOwningPlanet(Planet);
			SetKind(Spawner, bCrystal);
			return Spawner;
		}

		static void SetKind(AJTSMoonResourceSpawner* Spawner, bool bCrystal)
		{
			auto& Ratios = Spawner->ResourceRatios;
			Ratios.SmallRock = Ratios.LargeRock = Ratios.MediumMetalRock = Ratios.LargeMetalRock = Ratios.MediumCrystalRock = 0;
			Ratios.MediumRock = bCrystal ? 0 : 100;
			Ratios.LargeCrystalRock = bCrystal ? 100 : 0;
			Ratios.Normalize();
		}

		TArray<AJTSMoonResourceActor*> Nodes() const
		{
			TArray<AJTSMoonResourceActor*> Result;
			for (TActorIterator<AJTSMoonResourceActor> It(World); It; ++It) if (!It->IsActorBeingDestroyed()) Result.Add(*It);
			return Result;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMoonCoverageOwnershipTest, "JTS.Moon.Resources.CoverageOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSMoonCoverageOwnershipTest::RunTest(const FString& Parameters)
{
	FCoverageWorld Fixture;
	AJTSMoonResourceSpawner* A = Fixture.Add(0);
	AJTSMoonResourceSpawner* B = Fixture.Add(0, true);
	TArray<AJTSMoonResourceSpawner*> Group{A, B};
	A->OverlapPriority = 10;
	B->OverlapPriority = 0;
	FJTSMoonResourceCoverage Coverage(Fixture.Planet, Group, 42);
	TestTrue(TEXT("Different ratios choose the lower priority number"), Coverage.ResolveOwner(Fixture.Point(0)) == B);
	TestEqual(TEXT("Fully covered region contributes no area"), Coverage.GetOwnedAreaSquareMeters(A), 0.0f);
	TestTrue(TEXT("Point outside the union has no owner"), Coverage.ResolveOwner(Fixture.Point(1)) == nullptr);
	A->OverlapPriority = B->OverlapPriority = 0;
	bool bSawA = false, bSawB = false;
	for (int32 Seed = 0; Seed < 64; ++Seed)
	{
		FJTSMoonResourceCoverage Tie(Fixture.Planet, Group, Seed);
		auto* Owner = Tie.ResolveOwner(Fixture.Point(0));
		bSawA |= Owner == A;
		bSawB |= Owner == B;
		TestTrue(TEXT("Equal-priority overlap chooses one stable layer throughout its area"), Tie.ResolveOwner(Fixture.Point(0.05)) == Owner);
		TArray<AJTSMoonResourceSpawner*> Reversed{B, A};
		FJTSMoonResourceCoverage Repeat(Fixture.Planet, Reversed, Seed);
		TestTrue(TEXT("Fixed seed and actor identity reproduce ties independently of input order"), Repeat.ResolveOwner(Fixture.Point(0)) == Owner);
	}
	TestTrue(TEXT("Random ties can select either configuration"), bSawA && bSawB);
	FCoverageWorld::SetKind(B, false);
	B->ResourceDensity = 2;
	A->OverlapPriority = -100;
	B->OverlapPriority = 100;
	TestTrue(TEXT("Equal ratios with different densities respect priority even when the winner is less dense"), Coverage.ResolveOwner(Fixture.Point(0)) == A);
	A->OverlapPriority = 100;
	B->OverlapPriority = -100;
	TestTrue(TEXT("Priority swap selects the other density configuration"), Coverage.ResolveOwner(Fixture.Point(0)) == B);
	A->OverlapPriority = B->OverlapPriority = 0;
	bSawA = bSawB = false;
	for (int32 Seed = 0; Seed < 64; ++Seed)
	{
		FJTSMoonResourceCoverage Tie(Fixture.Planet, Group, Seed);
		auto* Owner = Tie.ResolveOwner(Fixture.Point(0));
		bSawA |= Owner == A;
		bSawB |= Owner == B;
		TestTrue(TEXT("Different densities with equal priorities use one stable random layer"), Tie.ResolveOwner(Fixture.Point(0.05)) == Owner);
	}
	TestTrue(TEXT("Density-only random ties can select either configuration"), bSawA && bSawB);
	B->ResourceDensity = A->ResourceDensity;
	auto* IdenticalOwner = Coverage.ResolveOwner(Fixture.Point(0));
	A->OverlapPriority = -100;
	B->OverlapPriority = 100;
	TestTrue(TEXT("Only identical ratios and density ignore priority"), Coverage.ResolveOwner(Fixture.Point(0)) == IdenticalOwner);
	A->OverlapPriority = 100;
	B->OverlapPriority = -100;
	TestTrue(TEXT("Swapping priorities preserves identical ratios and density ownership"), Coverage.ResolveOwner(Fixture.Point(0)) == IdenticalOwner);
	B->ResourceDensity = 0;
	TestTrue(TEXT("A zero-density winner masks the same-ratio losing region"), Coverage.ResolveOwner(Fixture.Point(0)) == B);
	TestEqual(TEXT("The losing density contributes no area"), Coverage.GetOwnedAreaSquareMeters(A), 0.0f);
	B->ResourceDensity = A->ResourceDensity;
	FCoverageWorld::SetKind(B, true);
	B->SetActorLocation(Fixture.Point(0.2));
	A->OverlapPriority = 10;
	B->OverlapPriority = 0;
	FJTSMoonResourceCoverage Partial(Fixture.Planet, Group, 42);
	TestTrue(TEXT("Uncovered part of A retains A"), Partial.ResolveOwner(Fixture.Point(0)) == A);
	TestTrue(TEXT("Uncovered part of B retains B"), Partial.ResolveOwner(Fixture.Point(0.2)) == B);
	TestTrue(TEXT("Only the overlapping part uses B"), Partial.ResolveOwner(Fixture.Point(0.1)) == B);
	const float AreaA = Partial.GetOwnedAreaSquareMeters(A);
	const float FullArea = A->GetCoverageAreaSquareMeters();
	TestTrue(TEXT("Partial clipping reduces area instead of moving suppressed resources outside the overlap"), AreaA > 0 && AreaA < FullArea);
	TestTrue(TEXT("Union area lies between one and two whole circles"), AreaA + Partial.GetOwnedAreaSquareMeters(B) > FullArea
		&& AreaA + Partial.GetOwnedAreaSquareMeters(B) < 2 * FullArea);
	Partial.RecordPlacement(Fixture.Point(0), 100);
	TestFalse(TEXT("Spacing is shared even when the new region requests zero spacing"), Partial.CanPlace(Fixture.Point(0.001), 0));
	TestTrue(TEXT("Separated placements remain permitted"), Partial.CanPlace(Fixture.Point(0.05), 100));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMoonCoverageGenerationTest, "JTS.Moon.Resources.UnionGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSMoonCoverageGenerationTest::RunTest(const FString& Parameters)
{
	FCoverageWorld Fixture;
	AJTSMoonResourceSpawner* A = Fixture.Add(0);
	AJTSMoonResourceSpawner* B = Fixture.Add(0, true);
	TArray<AJTSMoonResourceSpawner*> Group{A, B};
	A->OverlapPriority = 10;
	B->OverlapPriority = 0;
	const int32 Target = B->GetTargetResourceCount();
	TestEqual(TEXT("Coincident circles generate one density budget"), AJTSMoonResourceSpawner::GenerateResourceGroup(Group), Target);
	for (auto* Node : Fixture.Nodes()) TestTrue(TEXT("Covered rock content is absent; only crystal configuration remains"), Node->GetOwner() == B
		&& Node->GetNodeSize() == EJTSMoonResourceNodeSize::LargeCrystalRock);
	A->OverlapPriority = -1;
	TestEqual(TEXT("Regeneration replaces rather than retains old winners"), AJTSMoonResourceSpawner::GenerateResourceGroup(Group), Target);
	for (auto* Node : Fixture.Nodes()) TestTrue(TEXT("Priority swap removes all previous crystal content"), Node->GetOwner() == A
		&& Node->GetNodeSize() == EJTSMoonResourceNodeSize::MediumRock);
	FCoverageWorld::SetKind(B, false);
	TestEqual(TEXT("Identical ratios and density never double their resource count"), AJTSMoonResourceSpawner::GenerateResourceGroup(Group), Target);
	auto* IdenticalOwner = Fixture.Nodes()[0]->GetOwner();
	A->OverlapPriority = 100;
	B->OverlapPriority = -100;
	TestEqual(TEXT("Identical ratios and density keep one budget after priority changes"), AJTSMoonResourceSpawner::GenerateResourceGroup(Group), Target);
	for (auto* Node : Fixture.Nodes()) TestTrue(TEXT("Identical configuration ownership ignores priority changes"), Node->GetOwner() == IdenticalOwner);
	B->ResourceDensity = 1.6f;
	A->OverlapPriority = -100;
	B->OverlapPriority = 100;
	TestEqual(TEXT("Same ratios with different density generate only the lower-density priority winner"), AJTSMoonResourceSpawner::GenerateResourceGroup(Group), Target);
	for (auto* Node : Fixture.Nodes()) TestTrue(TEXT("Higher density cannot override priority"), Node->GetOwner() == A);
	A->OverlapPriority = 100;
	B->OverlapPriority = -100;
	TestEqual(TEXT("Swapped priority uses the other density instead of summing both"), AJTSMoonResourceSpawner::GenerateResourceGroup(Group), B->GetTargetResourceCount());
	for (auto* Node : Fixture.Nodes()) TestTrue(TEXT("All density-swapped content belongs to the new winner"), Node->GetOwner() == B);
	B->ResourceDensity = 0;
	TestEqual(TEXT("Same-ratio zero-density priority winner suppresses all covered content"), AJTSMoonResourceSpawner::GenerateResourceGroup(Group), 0);
	B->ResourceDensity = A->ResourceDensity;
	FCoverageWorld::SetKind(B, true);
	B->SetActorLocation(Fixture.Point(0.2));
	A->OverlapPriority = 10;
	B->OverlapPriority = 0;
	FJTSMoonResourceCoverage Partial(Fixture.Planet, Group, 42);
	const int32 PartialTarget = FMath::RoundToInt(Partial.GetOwnedAreaSquareMeters(A) * A->ResourceDensity / 100)
		+ FMath::RoundToInt(Partial.GetOwnedAreaSquareMeters(B) * B->ResourceDensity / 100);
	TestEqual(TEXT("Partial overlap uses clipped density budgets"), AJTSMoonResourceSpawner::GenerateResourceGroup(Group), PartialTarget);
	bool bHasA = false, bHasB = false;
	for (auto* Node : Fixture.Nodes())
	{
		bHasA |= Node->GetOwner() == A;
		bHasB |= Node->GetOwner() == B;
		TestTrue(TEXT("Every generated object belongs to the only visible layer at its position"), Partial.ResolveOwner(Node->GetActorLocation()) == Node->GetOwner());
	}
	TestTrue(TEXT("Both non-overlapping remainders retain their content"), bHasA && bHasB);
	B->SetActorLocation(A->GetActorLocation());
	B->ResourceDensity = 0;
	TestEqual(TEXT("Zero-density winning configuration hides the losing content too"), AJTSMoonResourceSpawner::GenerateResourceGroup(Group), 0);
#if WITH_EDITORONLY_DATA
	ULineBatchComponent* Preview = A->FindComponentByClass<ULineBatchComponent>();
	if (TestNotNull(TEXT("Editor build retains a preview component for development only"), Preview))
	{
		TestTrue(TEXT("Coverage preview is excluded from cooked game content"), Preview->IsEditorOnly());
		TestFalse(TEXT("Game world never draws the coverage outline"), !Preview->BatchedLines.IsEmpty() || Preview->IsVisible());
		TestTrue(TEXT("Preview stays hidden in game"), Preview->bHiddenInGame);
	}
#endif
	return true;
}

#endif
