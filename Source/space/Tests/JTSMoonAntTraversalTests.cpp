#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "UObject/UnrealType.h"
#include "space/Components/JTSPlanetSurfaceSteeringComponent.h"
#include "space/World/JTSPlanetAnchor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSAntSlopeTest, "JTS.Moon.Ants.RadialSlopeLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSAntSlopeTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World);
	auto* Planet = World->SpawnActor<AJTSPlanetAnchor>();
	Planet->SetActorLocation(FVector(31000, -27000, 19000));
	auto* Creature = World->SpawnActor<AActor>();
	auto* Steering = NewObject<UJTSPlanetSurfaceSteeringComponent>(Creature);
	for (const FVector& Up : {FVector::UpVector, FVector::ForwardVector, FVector(1, 2, -3).GetSafeNormal()})
	{
		FJTSPlanetSurfaceHit Hit;
		Hit.bBlockingHit = true;
		Hit.ImpactPoint = Planet->GetActorLocation() + Up * 10000;
		const FVector Axis = FVector::CrossProduct(Up, FVector(0.4, 0.7, 0.2)).GetSafeNormal();
		Hit.ImpactNormal = FQuat(Axis, FMath::DegreesToRadians(30.0f)).RotateVector(Up);
		TestTrue(TEXT("Gentle slope is walkable under every gravity direction"), Steering->IsWalkable(Planet, Hit));
		Hit.ImpactNormal = FQuat(Axis, FMath::DegreesToRadians(42.0f)).RotateVector(Up);
		TestFalse(TEXT("Crater-wall slope is blocked under every gravity direction"), Steering->IsWalkable(Planet, Hit));
	}
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSAntCornerTest, "JTS.Moon.Ants.EscapesBlockedCorner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSAntCornerTest::RunTest(const FString&)
{
	const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Planet = World->SpawnActor<AJTSPlanetAnchor>();
	auto* Surface = World->SpawnActor<AStaticMeshActor>();
	Surface->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Surface->SetActorScale3D(FVector(200));
	Surface->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	FindFProperty<FObjectProperty>(Planet->GetClass(), TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet, Surface);
	FindFProperty<FFloatProperty>(Planet->GetClass(), TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet, 10000);
	for (int32 I = 0; I < 2; ++I)
	{
		auto* Wall = World->SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Wall);
		Wall->SetRootComponent(Box); Box->RegisterComponent();
		Box->SetBoxExtent(I == 0 ? FVector(10, 500, 100) : FVector(500, 10, 100));
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Wall->SetActorLocation(I == 0 ? FVector(90, 0, 10060) : FVector(0, 90, 10060));
	}
	auto* Creature = World->SpawnActor<AActor>();
	auto* Steering = NewObject<UJTSPlanetSurfaceSteeringComponent>(Creature); Steering->RegisterComponent();
	FJTSPlanetSurfaceHit GroundHit; Planet->ProjectPointToSurface(FVector(0, 0, 10000), GroundHit);
	const FVector Start = GroundHit.ImpactPoint;
	FVector Ground = Start, Next, Heading, PreviousHeading;
	int32 Moved = 0, Reversals = 0;
	for (int32 I = 0; I < 60; ++I)
	{
		if (!Steering->Advance(Planet, Ground, FVector(1, 1, 0), 85, 0.05f, Next, Heading)) continue;
		TestTrue(TEXT("Traversal remains behind both solid walls"), Next.X < 68 && Next.Y < 68);
		if (!PreviousHeading.IsNearlyZero() && FVector::DotProduct(PreviousHeading, Heading) < -0.7f) ++Reversals;
		PreviousHeading = Heading; Ground = Next; ++Moved;
	}
	TestTrue(TEXT("A blocked wander goal makes progress instead of sticking at the corner"), Moved > 45);
	TestTrue(TEXT("Avoidance carries the creature out of the corner"), FVector::Distance(Start, Ground) > 70);
	TestTrue(TEXT("Commitment prevents repeated opposite-direction oscillation"), Reversals <= 2);
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}
#endif
