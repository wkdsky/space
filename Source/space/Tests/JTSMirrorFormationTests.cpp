#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Math/RotationMatrix.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/UnrealType.h"
#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "space/Components/JTSMirrorDefenseComponent.h"
#include "space/Systems/JTSMirrorGlareSubsystem.h"
#include "space/World/JTSPlanetAnchor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMirrorFormationTest,"JTS.Moon.MirrorDefense.FormationAndGlareOcclusion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSMirrorFormationTest::RunTest(const FString&)
{
	const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));World->InitializeActorsForPlay(FURL());World->BeginPlay();
	const FVector Center(123000,456000,789000),Up=FVector::ForwardVector,Forward=FVector::RightVector;
	auto* Planet=World->SpawnActor<AJTSPlanetAnchor>();Planet->SetActorLocation(Center);Planet->MaximumPlacementSlopeDegrees=50;
	auto* Surface=World->SpawnActor<AStaticMeshActor>();Surface->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Surface->SetActorLocation(Center);Surface->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Surface->SetActorScale3D(FVector(200));Surface->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	FindFProperty<FObjectProperty>(Planet->GetClass(),TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet,Surface);
	FindFProperty<FFloatProperty>(Planet->GetClass(),TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet,10000);
	const FVector Origin=Center+Up*10000;
	auto Class=LoadClass<AJTSPlanetSurfaceCreature>(nullptr,TEXT("/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_MirrorSkimmer.BP_MirrorSkimmer_C"));
	TArray<AJTSPlanetSurfaceCreature*> Bugs;TArray<UJTSMirrorDefenseComponent*> Mirrors;TArray<FVector> Stations;
	for (int32 I=0;I<3;++I)
	{
		auto* Bug=World->SpawnActor<AJTSPlanetSurfaceCreature>(Class,Origin,FRotationMatrix::MakeFromXZ(Forward,Up).Rotator());
		if (!Bug->HasActorBegunPlay()) Bug->DispatchBeginPlay();
		Bugs.Add(Bug);Mirrors.Add(Bug->FindComponentByClass<UJTSMirrorDefenseComponent>());
	}
	TestEqual(TEXT("Server-side source registry includes all three reflectors"),World->GetSubsystem<UJTSMirrorGlareSubsystem>()->GetSources().Num(),3);
	for (auto* Mirror : Mirrors)
	{
		FVector Station,Separate;
		TestTrue(TEXT("Coincident reflectors reserve distinct real-surface stations even with duplicate slots"),Mirror->FindGuardStation(Planet,Origin,Forward,1,Station));
		for (const FVector& Other : Stations)
			TestTrue(TEXT("Reserved stations leave shell clearance"),FVector::Distance(Station,Other)>=Mirror->GuardSpacing-3.f);
		Stations.Add(Station);
		TestTrue(TEXT("Exact overlap has a non-zero walkable separation direction"),Mirror->FindSeparationGoal(Planet,Separate));
		TestTrue(TEXT("Separation follows the translated spherical surface"),FVector::VectorPlaneProject(Separate-Origin,Up).Size()>20);
	}
	for (int32 I=0;I<3;++I)Bugs[I]->SetActorLocation(Stations[I]);
	const FVector Start=Mirrors[0]->GetMirrorLocation(),Eye=Start+Up*200;
	auto* Proxy=World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Proxy);Proxy->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(20,50,50));Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);Box->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
	Box->ComponentTags.Add(TEXT("InteractionProxy"));Box->RegisterComponent();Proxy->SetActorLocation(Start+Up*100);
	TestTrue(TEXT("An invisible mouth interaction proxy cannot extinguish glare"),Mirrors[0]->HasClearGlarePath(Eye,nullptr));
	Box->ComponentTags.Reset();
	TestFalse(TEXT("A physical occluder at the same location still blocks glare"),Mirrors[0]->HasClearGlarePath(Eye,nullptr));
	World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
	return true;
}
#endif
