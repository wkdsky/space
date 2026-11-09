#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/LocalPlayer.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Math/RotationMatrix.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/UnrealType.h"
#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "space/Player/JTSCharacter.h"
#include "space/Components/JTSHealthComponent.h"
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
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// A local viewer need not look at the source. The server's active threat area
	// defines who receives glare, including viewers beside the selected attacker.
	auto* PC=World->SpawnActor<APlayerController>();
	PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
	auto* Player=World->SpawnActor<AJTSCharacter>();
	if (!Player->HasActorBegunPlay()) Player->DispatchBeginPlay();
	Player->GetCharacterMovement()->DisableMovement(); Player->SetActorTickEnabled(false); PC->Possess(Player);
	Player->SetActorLocation(Origin+Forward*350+Up*100);
	auto* Camera=World->SpawnActor<ACameraActor>();
	PC->SetViewTarget(Camera);
	auto UpdateView=[&](float Angle)
	{
		Camera->SetActorLocationAndRotation(Player->GetActorLocation()+Up*100,
			FRotationMatrix::MakeFromXZ(Forward.RotateAngleAxis(Angle,Up),Up).Rotator());
		PC->PlayerCameraManager->UpdateCamera(.04f);
	};
	Mirrors[0]->AimAt(Player);
	const uint64 OriginalFrame=GFrameCounter;
	for(int32 I=0;I<10;++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,.04f); }
	for(float Angle : {0.f,90.f,180.f,270.f})
	{
		UpdateView(Angle);
		TestTrue(TEXT("Active glare remains visible at every camera heading"),Mirrors[0]->CanGlareAt(PC));
	}
	Player->SetActorLocation(Origin+Forward*2400+Up*100); UpdateView(0);
	TestFalse(TEXT("Leaving individual aggro range stops glare"),Mirrors[0]->CanGlareAt(PC));
	// Alarm coverage is centred on the mouth, not the defender's current station.
	const FVector AlarmOrigin=Origin+Forward*700;
	Player->SetActorLocation(AlarmOrigin+Forward*1600+Up*100);
	Bugs[0]->DefendEntrance(Player,AlarmOrigin,1800,0,1); Mirrors[0]->AimAt(Player);
	UpdateView(180);
	TestTrue(TEXT("The larger nest alarm still covers a viewer beyond the creature leash"),Mirrors[0]->CanGlareAt(PC));
	Player->SetActorLocation(AlarmOrigin+Forward*1900+Up*100); UpdateView(180);
	TestFalse(TEXT("Leaving the mouth's alarm radius stops glare"),Mirrors[0]->CanGlareAt(PC));
	Player->SetActorLocation(Origin+Forward*350+Up*100); UpdateView(0);
	Mirrors[0]->StopReflection();
	TestFalse(TEXT("A retracted mirror cannot apply lingering new glare"),Mirrors[0]->CanGlareAt(PC));
	GFrameCounter=OriginalFrame;
	PC->Player=nullptr;
	World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
	return true;
}
#endif
