#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Math/RotationMatrix.h"
#include "UObject/UnrealType.h"
#include "space/Components/JTSCompressedSpaceComponent.h"
#include "space/Components/JTSPlanetGravityComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/UI/JTSNavigationMapModel.h"
#include "space/UI/JTSCruiseNavigationWidget.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSpaceWorldManager.h"

namespace
{
	struct FNavigationWorld
	{
		UWorld* World;
		AJTSSpaceWorldManager* Manager;
		AJTSPlanetAnchor* Primary;
		AJTSSpacecraftActor* Ship;
		FNavigationWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Manager = World->SpawnActor<AJTSSpaceWorldManager>();
			Primary = AddPlanet(TEXT("Main"), FVector::ZeroVector, 10000);
			Ship = World->SpawnActor<AJTSSpacecraftActor>();
			Manager->SetCurrentPlanet(Primary);
			Manager->SetSurfaceGameplayReady(true);
		}
		~FNavigationWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		AJTSPlanetAnchor* AddPlanet(FName Id, FVector Center, float Radius)
		{
			auto* Planet = World->SpawnActor<AJTSPlanetAnchor>(Center, FRotator::ZeroRotator);
			FindFProperty<FNameProperty>(Planet->GetClass(), TEXT("PlanetId"))->SetPropertyValue_InContainer(Planet, Id);
			FindFProperty<FFloatProperty>(Planet->GetClass(), TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet, Radius);
			auto* Ground = World->SpawnActor<AStaticMeshActor>(Center, FRotator::ZeroRotator);
			Ground->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
			Ground->SetActorScale3D(FVector(Radius / 50));
			Ground->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
			FindFProperty<FObjectProperty>(Planet->GetClass(), TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet, Ground);
			Manager->RegisterPlanet(Planet);
			return Planet;
		}
		void AddBody(AActor* Actor, FVector Navigation, double Radius, FName Group)
		{
			FJTSCompressedCelestialBody Body;
			Body.Actor = Actor; Body.NavigationPositionKilometers = Navigation;
			Body.RadiusKilometers = Radius; Body.PresentationGroup = Group;
			Manager->CompressedSpace->Bodies.Add(Body);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSNavigationCoordinatesTest, "JTS.NavigationMap.SphericalCoordinates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSNavigationCoordinatesTest::RunTest(const FString&)
{
	const FVector Center(100000, -2000, 3000);
	const FQuat Rotation(FVector(1, 1, 1).GetSafeNormal(), 1.3);
	for (const FVector Direction : {FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FVector(0, 0, -1), FVector(-1, -0.001, 0)})
	{
		const auto A = FJTSNavigationMapModel::SurfaceCoordinates(Direction * 10000, FVector::ZeroVector, FQuat::Identity);
		const auto B = FJTSNavigationMapModel::SurfaceCoordinates(Center + Rotation.RotateVector(Direction * 10000), Center, Rotation);
		TestTrue(TEXT("Planet rotation and translation preserve surface coordinates"), A.Equals(B, 0.001));
		TestTrue(TEXT("Finite latitude including both poles"), FMath::IsFinite(B.X) && FMath::Abs(B.X) <= 90.0);
	}
	FJTSNavigationTerrainPatch Patch; Patch.Radius = 10000; Patch.HalfWidth = 1000;
	const FVector NorthPoint = FVector(-0.1, 0, 1).GetSafeNormal() * 10000;
	TestTrue(TEXT("North appears above the player"), Patch.Project(NorthPoint, FVector::ZeroVector).Y < 0);
	const FQuat CaptureRotation = FRotationMatrix::MakeFromXY(-Patch.Up, Patch.East).ToQuat();
	const FVector CameraPoint = CaptureRotation.UnrotateVector(NorthPoint - Patch.Origin);
	TestTrue(TEXT("Surface markers match the orthographic texture after its vertical display flip"),
		Patch.Project(NorthPoint, FVector::ZeroVector).Equals(FVector2D(CameraPoint.Y, CameraPoint.Z) / Patch.HalfWidth, 0.001));
	TestTrue(TEXT("Occluded far-side ship remains off-map"), Patch.Project(FVector(0, 0, -10000), FVector::ZeroVector).Size() > 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSNavigationAnimationTest, "JTS.NavigationMap.ReversibleAnimation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSNavigationAnimationTest::RunTest(const FString&)
{
	double Zoom30 = 0, Zoom120 = 0;
	for (int32 I = 0; I < 30; ++I) Zoom30 = FJTSNavigationMapModel::AdvanceZoom(Zoom30, 2, 1.0 / 30, 1.4);
	for (int32 I = 0; I < 120; ++I) Zoom120 = FJTSNavigationMapModel::AdvanceZoom(Zoom120, 2, 1.0 / 120, 1.4);
	TestTrue(TEXT("Zoom is independent of frame rate"), FMath::IsNearlyEqual(Zoom30, Zoom120, 1.e-6));
	const double Reversed = FJTSNavigationMapModel::AdvanceZoom(Zoom30, 0, 0.1, 1.4);
	TestTrue(TEXT("Reversal continues from the current geometry"), Reversed < Zoom30 && Reversed > 0);
	TestEqual(TEXT("Large delta clamps at the target"), FJTSNavigationMapModel::AdvanceZoom(0, 1, 20, 1.4), 1.0);
	TestEqual(TEXT("Negative delta never advances the animation"), FJTSNavigationMapModel::AdvanceZoom(0.5, 2, -1, 1.4), 0.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSNavigationGroupingTest, "JTS.NavigationMap.SystemGroupingAndStages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSNavigationGroupingTest::RunTest(const FString&)
{
	FNavigationWorld W;
	auto* MoonA = W.AddPlanet(TEXT("SatelliteA"), FVector(200000, 0, 0), 4000);
	auto* MoonB = W.AddPlanet(TEXT("SatelliteB"), FVector(320000, 0, 0), 2000);
	auto* Remote = W.AddPlanet(TEXT("Remote"), FVector(3000000, 0, 0), 8000);
	W.AddBody(W.Primary, FVector::ZeroVector, 3000, TEXT("Local"));
	W.AddBody(MoonA, FVector(10000, 0, 0), 30, TEXT("Local"));
	W.AddBody(MoonB, FVector(20000, 0, 0), 10, TEXT("Local"));
	W.AddBody(Remote, FVector(1.e8, 0, 0), 2500, TEXT("Other"));
	W.Manager->CompressedSpace->CaptureAuthoredBodies();
	FJTSNavigationMapModel Model;
	W.Ship->SetActorLocation(FVector(0, 0, 12000));
	auto Frame = Model.Build(W.Manager, W.Ship, nullptr);
	TestEqual(TEXT("Near-flight shell keeps the altimeter stage"), Frame.Stage, EJTSNavigationMapStage::Surface);
	TestEqual(TEXT("All three members belong to the current system"), Frame.Bodies.Num(), 3);
	TestEqual(TEXT("Interstellar catalog collapses satellites into their primary"), Frame.Systems.Num(), 2);
	W.Ship->SetActorLocation(FVector(0, 0, 30000));
	Frame = Model.Build(W.Manager, W.Ship, nullptr);
	TestEqual(TEXT("Free flight outside the near shell remains in the local system"), Frame.Stage, EJTSNavigationMapStage::System);
	TestTrue(TEXT("Local system keeps the ship at its origin"), Frame.ShipPosition.IsNearlyZero());
	FindFProperty<FBoolProperty>(MoonA->GetClass(), TEXT("bShowOnStarChart"))->SetPropertyValue_InContainer(MoonA, false);
	Frame = Model.Build(W.Manager, W.Ship, nullptr);
	TestTrue(TEXT("Uncharted bodies retain their gravity field without a circle or name"),
		Frame.Bodies.ContainsByPredicate([](const FJTSNavigationMapBody& Body) { return !Body.bCharted && Body.Gravity > 0; }));
	W.Ship->SetActorLocation(FVector(0, 0, 10000000));
	Frame = Model.Build(W.Manager, W.Ship, nullptr);
	TestEqual(TEXT("Leaving the whole system enters interstellar navigation"), Frame.Stage, EJTSNavigationMapStage::Interstellar);
	// Reorder authored membership: primary identity and catalog must not depend on insertion order.
	Algo::Reverse(W.Manager->CompressedSpace->Bodies);
	W.Manager->CompressedSpace->CaptureAuthoredBodies();
	const auto Reordered = Model.Build(W.Manager, W.Ship, nullptr);
	TestEqual(TEXT("Primary identity survives reordered level data"), Reordered.SystemName, Frame.SystemName);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSNavigationGroundTest, "JTS.NavigationMap.PlayerCenteredSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSNavigationGroundTest::RunTest(const FString&)
{
	FNavigationWorld W;
	W.Ship->SetActorLocation(FVector(0, 0, 10300));
	auto* A = W.World->SpawnActor<AJTSCharacter>(FVector(0, 0, 10100), FRotator::ZeroRotator);
	auto* B = W.World->SpawnActor<AJTSCharacter>(FVector(1000, 0, 10100), FRotator::ZeroRotator);
	A->FindComponentByClass<UJTSPlanetGravityComponent>()->SetPlanetAnchor(W.Primary);
	B->FindComponentByClass<UJTSPlanetGravityComponent>()->SetPlanetAnchor(W.Primary);
	FJTSNavigationMapModel MapA, MapB;
	const auto FrameA = MapA.Build(W.Manager, W.Ship, A);
	const auto FrameB = MapB.Build(W.Manager, W.Ship, B);
	TestTrue(TEXT("On-foot player gets the ground map"), FrameA.bGroundMap && FrameB.bGroundMap);
	TestNotEqual(TEXT("Each local player has independent live coordinates"), FrameA.Coordinates, FrameB.Coordinates);
	TestTrue(TEXT("Ground camera is centered exactly on the player"), FrameA.Terrain->Origin.Equals(A->GetActorLocation()));
	A->SetActorLocation(FVector(300, 0, 10100));
	const auto Moved = MapA.Build(W.Manager, W.Ship, A);
	TestNotEqual(TEXT("Moving player updates coordinates immediately"), Moved.Coordinates, FrameA.Coordinates);
	TestTrue(TEXT("Player stays at center while terrain follows continuously"), Moved.PlayerPosition.IsNearlyZero());
	TestTrue(TEXT("Camera follows immediately without cache-distance thresholds"), Moved.Terrain->Origin.Equals(A->GetActorLocation()));
	TestTrue(TEXT("Stationary shared ship shifts relative to the moving astronaut"), !Moved.ShipPosition.Equals(FrameA.ShipPosition));
	for (int32 I = 1; I <= 30; ++I)
	{
		A->SetActorLocation(FVector(300 + I, 0, 10100));
		const auto Step = MapA.Build(W.Manager, W.Ship, A);
		TestTrue(TEXT("Every small movement recenters the map"), Step.PlayerPosition.IsNearlyZero()
			&& Step.Terrain->Origin.Equals(A->GetActorLocation()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSNavigationScannerTest, "JTS.NavigationMap.ShipCentered3DScanner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSNavigationScannerTest::RunTest(const FString&)
{
	const FVector Contact(1000, 500, 200);
	const FVector Base = FJTSNavigationMapModel::ProjectSystemContact(Contact, FQuat::Identity, 5000);
	TestTrue(TEXT("Translation changes relative contact position"), !Base.Equals(FJTSNavigationMapModel::ProjectSystemContact(Contact - FVector(100, 0, 0), FQuat::Identity, 5000)));
	for (const FVector Axis : {FVector::ForwardVector, FVector::RightVector, FVector::UpVector})
	{
		const FQuat Rotation(Axis, 0.5);
		TestTrue(TEXT("Yaw, pitch and roll all affect the three-dimensional scanner"), !Base.Equals(FJTSNavigationMapModel::ProjectSystemContact(Contact, Rotation, 5000)));
		TestTrue(TEXT("Rotating both world and hull preserves relative geometry"), Base.Equals(FJTSNavigationMapModel::ProjectSystemContact(Rotation.RotateVector(Contact), Rotation, 5000), 0.00001));
	}
	TestTrue(TEXT("Above and below remain distinct when planar coordinates match"),
		FJTSNavigationMapModel::ProjectSystemContact(FVector(0, 0, 1000), FQuat::Identity, 5000).Z > 0
		&& FJTSNavigationMapModel::ProjectSystemContact(FVector(0, 0, -1000), FQuat::Identity, 5000).Z < 0);
	TestTrue(TEXT("Observer remains the exact origin"), FJTSNavigationMapModel::ProjectSystemContact(FVector::ZeroVector, FQuat::Identity, 5000).IsNearlyZero());
	TestTrue(TEXT("Distant contacts stay within the scanner sphere"), FJTSNavigationMapModel::ProjectSystemContact(FVector(1.e15, 0, 0), FQuat::Identity, 5000).Size() < 1);
	FNavigationWorld W;
	W.AddBody(W.Primary, FVector::ZeroVector, 3000, TEXT("Local"));
	W.Manager->CompressedSpace->CaptureAuthoredBodies();
	FJTSNavigationMapModel Model;
	W.Ship->SetActorLocation(FVector(0, 0, 30000));
	const auto First = Model.Build(W.Manager, W.Ship, nullptr);
	W.Ship->AddActorWorldOffset(FVector(20000, 0, 0));
	const auto Second = Model.Build(W.Manager, W.Ship, nullptr);
	TestTrue(TEXT("Ship stays centered throughout free flight"), First.ShipPosition.IsNearlyZero() && Second.ShipPosition.IsNearlyZero());
	TestTrue(TEXT("Planets move around the centered ship"), !First.Bodies[0].Position.Equals(Second.Bodies[0].Position));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSNavigationWidgetLifecycleTest, "JTS.NavigationMap.WidgetLayoutAndLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSNavigationWidgetLifecycleTest::RunTest(const FString&)
{
	TestEqual(TEXT("Generated UObject allocation matches the native widget size"),
		UJTSCruiseNavigationWidget::StaticClass()->GetStructureSize(), static_cast<int32>(sizeof(UJTSCruiseNavigationWidget)));
	FNavigationWorld W;
	for (int32 I = 0; I < 4; ++I)
	{
		auto* Widget = NewObject<UJTSCruiseNavigationWidget>();
		Widget->UpdateNavigation(W.Manager, W.Ship, nullptr);
		Widget->TakeWidget();
		Widget->ConditionalBeginDestroy();
	}
	CollectGarbage(RF_NoFlags);
	return true;
}

#endif
