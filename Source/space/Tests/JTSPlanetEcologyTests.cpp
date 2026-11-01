#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Math/RotationMatrix.h"
#include "UObject/UnrealType.h"
#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSPlanetSurfaceScatterComponent.h"
#include "space/Components/JTSPlanetSurfaceSteeringComponent.h"
#include "space/World/JTSPlanetSurfaceAttachment.h"
#include "EngineUtils.h"
#include "ProceduralMeshComponent.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetEcologyHabitat.h"
#include "space/World/JTSPlanetColonyNest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSPlanetEcologyTest, "JTS.Moon.Ecology.SphericalHabitatsAndColonyEscalation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSPlanetEcologyTest::RunTest(const FString&)
{
	struct FScope
	{
		UWorld* World;
		FScope()
		{
			auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
		~FScope() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	} Scope;
	auto* Planet = Scope.World->SpawnActor<AJTSPlanetAnchor>();
	const FVector Center(2700000, 370000, 200000);
	Planet->SetActorLocation(Center);
	auto* Surface = Scope.World->SpawnActor<AStaticMeshActor>();
	Surface->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Surface->SetActorLocation(Center);
	Surface->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Surface->SetActorScale3D(FVector(200));
	Surface->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	FindFProperty<FObjectProperty>(Planet->GetClass(), TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet, Surface);
	FindFProperty<FFloatProperty>(Planet->GetClass(), TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet, 10000);
	const FVector Origin = Center + FVector(10000, 0, 0);
	const FRotator Frame = FRotationMatrix::MakeFromXZ(FVector::YAxisVector, FVector::XAxisVector).Rotator();
	auto Configure = [&](AJTSPlanetEnemySettlement* Settlement, int32 Count)
	{
		FindFProperty<FClassProperty>(Settlement->GetClass(), TEXT("EnemyClass"))->SetPropertyValue_InContainer(Settlement, AJTSPlanetSurfaceCreature::StaticClass());
		FindFProperty<FIntProperty>(Settlement->GetClass(), TEXT("EnemyCount"))->SetPropertyValue_InContainer(Settlement, Count);
		FindFProperty<FFloatProperty>(Settlement->GetClass(), TEXT("SpawnRadius"))->SetPropertyValue_InContainer(Settlement, 1000);
	};
	auto* Habitat = Scope.World->SpawnActor<AJTSPlanetEcologyHabitat>(Origin, Frame);
	Configure(Habitat, 4); Habitat->CoreRadius = 500; Habitat->MinorAxisRatio = 0.8f;
	auto* Sun = Scope.World->SpawnActor<AStaticMeshActor>();
	Sun->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Sun->SetActorRotation(FRotator(0, 180, 0));
	Habitat->SunLight = Sun; Habitat->LightPreference = EJTSHabitatLight::Sunlit;
	const FVector InFringe = Center + FVector(FMath::Cos(0.07)*10000, FMath::Sin(0.07)*10000, 0);
	TestTrue(TEXT("Translated equatorial habitat accepts its core without World-Z assumptions"), Habitat->ContainsPoint(Origin, Planet, true));
	TestTrue(TEXT("Activity includes a point beyond the core"), Habitat->ContainsPoint(InFringe, Planet));
	TestFalse(TEXT("The same fringe point is outside physical core influence"), Habitat->ContainsPoint(InFringe, Planet, true));
	Sun->SetActorRotation(FRotator::ZeroRotator);
	TestFalse(TEXT("Mirror habitat rejects a point after illumination changes"), Habitat->ContainsPoint(Origin, Planet));
	Sun->SetActorRotation(FRotator(0, 180, 0));
	Scope.World->BeginPlay();
	Habitat->ActivateEcology(Planet, nullptr);
	TestEqual(TEXT("Core reuses the shared population template"), Habitat->GetSpawnedEnemyCount(), 4);
	Habitat->ActivateEcology(Planet, nullptr);
	TestEqual(TEXT("Repeated initialization cannot duplicate animals"), Habitat->GetSpawnedEnemyCount(), 4);
	Habitat->Deactivate(); Habitat->SetRole(ROLE_SimulatedProxy);
	Habitat->ActivateEcology(Planet, nullptr);
	TestEqual(TEXT("A client cannot spawn ecology"), Habitat->GetSpawnedEnemyCount(), 0);
	Habitat->SetRole(ROLE_Authority);
	FJTSPlanetSurfaceScatterEntry Prop;
	Prop.Meshes.Add(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Prop.Count = 3; Prop.MinimumSpacing = 100; Prop.bRefuge = true;
	Habitat->SurfaceScatter->Entries.Add(Prop);
	Habitat->SurfaceScatter->OwnerClearance = 150;
	const FTransform SurfaceBefore = Surface->GetActorTransform();
	UStaticMesh* SourceMesh = Surface->GetStaticMeshComponent()->GetStaticMesh();
	Habitat->SurfaceScatter->Generate(Planet, nullptr);
	TArray<FVector> FirstLayout;
	for (TActorIterator<AJTSPlanetSurfaceAttachment> It(Scope.World); It; ++It)
	{
		if (It->IsActorBeingDestroyed()) continue;
		FirstLayout.Add(It->GetActorLocation());
		TestTrue(TEXT("Generated props are independent replicated actors"), It->GetIsReplicated() && It->GetOwner() == Planet);
		TestTrue(TEXT("Random attachments remain inside their configured core"), Habitat->ContainsPoint(It->GetActorLocation(), Planet, true));
		TestTrue(TEXT("Attachment placement keeps the configured space around its marker clear"), Planet->ApproximateSurfaceArcDistance(Origin, It->GetActorLocation()) >= 150);
	}
	TestEqual(TEXT("Range generation creates configured independent props"), FirstLayout.Num(), 3);
	Habitat->SurfaceScatter->Clear();
	TestEqual(TEXT("Clearing attachments removes generated refuge references"), Habitat->Refuges.Num(), 0);
	Habitat->SurfaceScatter->Generate(Planet, nullptr);
	bool bDifferentLayout = false;
	for (TActorIterator<AJTSPlanetSurfaceAttachment> It(Scope.World); It; ++It)
		if (!It->IsActorBeingDestroyed() && !FirstLayout.ContainsByPredicate([&](const FVector& P) { return P.Equals(It->GetActorLocation(), 1); })) bDifferentLayout = true;
	TestTrue(TEXT("A new generation chooses a different layout"), bDifferentLayout);
	TestTrue(TEXT("Generating and clearing props preserves the original planet mesh and transform"),
		Surface->GetStaticMeshComponent()->GetStaticMesh() == SourceMesh && Surface->GetActorTransform().Equals(SurfaceBefore));
	auto* Patch = Scope.World->SpawnActor<AJTSPlanetSurfaceAttachment>();
	auto* PatchMesh = DuplicateObject<UStaticMesh>(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")), GetTransientPackage());
	PatchMesh->bAllowCPUAccess = true;
	Patch->Initialize(PatchMesh, Planet, FTransform(Frame, Origin), true, false, 0.7f);
	auto* Display = Patch->FindComponentByClass<UProceduralMeshComponent>();
	TestTrue(TEXT("A flat attachment builds its own independent projected mesh"), Display && Display->GetNumSections() > 0);
	if (Display && Display->GetProcMeshSection(0)) for (const auto& Vertex : Display->GetProcMeshSection(0)->ProcVertexBuffer)
	{
		const FVector Point = Display->GetComponentTransform().TransformPosition(Vertex.Position);
		FJTSPlanetSurfaceHit Projected;
		TestTrue(TEXT("Private attachment vertices follow the existing real surface"), Planet->ProjectPointToSurface(Point, Projected) && FVector::Distance(Point, Projected.ImpactPoint) < 1.1);
	}
	// A cavity must keep its thickness when its footprint is projected onto a curved surface.
	auto* RaisedPatch = Scope.World->SpawnActor<AJTSPlanetSurfaceAttachment>();
	auto* CavityMesh = DuplicateObject<UStaticMesh>(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")), GetTransientPackage());
	CavityMesh->bAllowCPUAccess = true;
	RaisedPatch->Initialize(CavityMesh, Planet, FTransform(Frame, Origin), true, true, 0.7f, true);
	if (!RaisedPatch->HasActorBegunPlay()) RaisedPatch->DispatchBeginPlay();
	auto* CavityDisplay = RaisedPatch->FindComponentByClass<UProceduralMeshComponent>();
	float Lowest = TNumericLimits<float>::Max(), Highest = -TNumericLimits<float>::Max();
	if (CavityDisplay && CavityDisplay->GetProcMeshSection(0)) for (const auto& Vertex : CavityDisplay->GetProcMeshSection(0)->ProcVertexBuffer)
	{
		const FVector Point = CavityDisplay->GetComponentTransform().TransformPosition(Vertex.Position);
		FJTSPlanetSurfaceHit Ground;
		if (Planet->ProjectPointToSurface(Point, Ground))
		{
			const float Height = FVector::DotProduct(Point - Ground.ImpactPoint, Ground.ImpactNormal);
			Lowest = FMath::Min(Lowest, Height); Highest = FMath::Max(Highest, Height);
		}
	}
	TestTrue(TEXT("Conforming a cavity preserves its real wall height instead of flattening it"), Highest - Lowest > 90);
	TestTrue(TEXT("Conforming geometry retains configured cast shadows"), CavityDisplay && CavityDisplay->CastShadow);
	TestTrue(TEXT("An explicitly destructible attachment accepts melee hits"), RaisedPatch->CanReceiveMeleeHit_Implementation(nullptr));
	RaisedPatch->SetRole(ROLE_SimulatedProxy);
	RaisedPatch->ReceiveMeleeHit_Implementation(nullptr, EJTSMeleeAttackType::Axe);
	TestEqual(TEXT("A client cannot damage the independent surface piece"), RaisedPatch->FindComponentByClass<UJTSHealthComponent>()->GetHealth(), 100.0f);
	RaisedPatch->SetRole(ROLE_Authority);
	for (int32 Hit = 0; Hit < 5; ++Hit) RaisedPatch->ReceiveMeleeHit_Implementation(nullptr, EJTSMeleeAttackType::Axe);
	TestTrue(TEXT("Destroying the attachment removes the entire cavity"), RaisedPatch->IsActorBeingDestroyed());
	TestTrue(TEXT("Removing a cavity preserves the underlying planet mesh and transform"),
		Surface->GetStaticMeshComponent()->GetStaticMesh() == SourceMesh && Surface->GetActorTransform().Equals(SurfaceBefore));
	auto* Provider = Scope.World->SpawnActor<AJTSPlanetEcologyHabitat>(Origin, Frame);
	Provider->Refuges.Add(Patch); Provider->RefugeProviders.Add(Habitat);
	Habitat->RefugeProviders.Add(Provider);
	TestTrue(TEXT("A different species' provider cannot supply this creature's refuge"), Habitat->FindRefugeActor(Origin) != Patch);
	Configure(Provider, 0);
	TestTrue(TEXT("A matching species can use a resident provider without looping on provider cycles"), Habitat->FindRefugeActor(Origin) == Patch);
	Habitat->RefugeProviders.Reset(); Provider->RefugeProviders.Reset();
	auto* NeedsRefuge = Scope.World->SpawnActor<AJTSPlanetEcologyHabitat>(Origin, Frame);
	Configure(NeedsRefuge, 1); NeedsRefuge->bRequireRefuge = true;
	NeedsRefuge->ActivateEcology(Planet, nullptr);
	TestEqual(TEXT("A resident requiring a refuge cannot spawn over bare ground"), NeedsRefuge->GetSpawnedEnemyCount(), 0);
	NeedsRefuge->Refuges.Add(Patch); NeedsRefuge->ActivateEcology(Planet, nullptr);
	TestEqual(TEXT("The same recipe can spawn after a real refuge is available"), NeedsRefuge->GetSpawnedEnemyCount(), 1);
	NeedsRefuge->Deactivate();
	auto* Emerging = Scope.World->SpawnActor<AJTSPlanetSurfaceCreature>(Origin, Frame);
	if (!Emerging->HasActorBegunPlay()) Emerging->DispatchBeginPlay();
	auto* Traversal = Emerging->FindComponentByClass<UJTSPlanetSurfaceSteeringComponent>();
	Traversal->MaxSlopeDegrees = 0;
	TestFalse(TEXT("The shared settlement retries a spawn beyond its species slope limit"), Emerging->InitializeForSettlement_Implementation(Planet, Origin, Origin));
	Traversal->MaxSlopeDegrees = 35;
	TestTrue(TEXT("A walkable spawn initializes normally"), Emerging->InitializeForSettlement_Implementation(Planet, Origin, Origin));
	const FVector EmergingRefuge = Habitat->FindRefuge(Origin);
	Emerging->SetHabitat(Habitat, Origin);
	TestTrue(TEXT("A resident emerges at its actual refuge rather than a random surface spawn"),
		Planet->ApproximateSurfaceArcDistance(EmergingRefuge, Emerging->GetActorLocation()) < 1);
	for (int32 I = 0; I < 50; ++I)
	{
		// TimerManager processes once per engine frame, including in an isolated test world.
		++GFrameCounter;
		Scope.World->Tick(LEVELTICK_All, 0.05f);
	}
	TestTrue(TEXT("An emerged creature walks clear of its entrance"), Planet->ApproximateSurfaceArcDistance(EmergingRefuge, Emerging->GetActorLocation()) > 50);
	Emerging->Destroy();
	auto* Nest = Scope.World->SpawnActor<AJTSPlanetColonyNest>(Origin, Frame);
	auto* Workers = Scope.World->SpawnActor<AJTSPlanetEnemySettlement>(Origin, Frame);
	auto* Guards = Scope.World->SpawnActor<AJTSPlanetEnemySettlement>(Origin, Frame);
	Configure(Workers, 3); Configure(Guards, 1);
	auto Bind = [&](const TCHAR* Name, UObject* Value) { FindFProperty<FObjectProperty>(Nest->GetClass(), Name)->SetObjectPropertyValue_InContainer(Nest, Value); };
	Bind(TEXT("Planet"), Planet); Bind(TEXT("Workers"), Workers); Bind(TEXT("Guards"), Guards);
	// This isolated test world has no GameMode to dispatch deferred BeginPlay.
	if (!Nest->HasActorBegunPlay()) Nest->DispatchBeginPlay();
	auto* Health = Nest->FindComponentByClass<UJTSHealthComponent>();
	Health->SetMaxHealth(160, true);
	TestEqual(TEXT("Closed towers have no visible workers"), Workers->GetSpawnedEnemyCount(), 0);
	Health->ApplyDamage(15, nullptr, nullptr);
	TestEqual(TEXT("First strike releases workers"), Workers->GetSpawnedEnemyCount(), 3);
	TestEqual(TEXT("First strike does not release soldiers"), Guards->GetSpawnedEnemyCount(), 0);
	Health->ApplyDamage(95, nullptr, nullptr);
	TestEqual(TEXT("Severe shell damage releases one guard"), Guards->GetSpawnedEnemyCount(), 1);
	Health->ApplyDamage(10, nullptr, nullptr);
	TestEqual(TEXT("Further damage does not duplicate either cohort"), Workers->GetSpawnedEnemyCount()+Guards->GetSpawnedEnemyCount(), 4);
	auto* Symbolic = Scope.World->SpawnActor<AJTSPlanetEcologyHabitat>(Origin, Frame);
	auto* Destructible = Scope.World->SpawnActor<AJTSPlanetEcologyHabitat>(Origin, Frame);
	Destructible->bDestructibleEntrance = true;
	if (!Symbolic->HasActorBegunPlay()) Symbolic->DispatchBeginPlay();
	if (!Destructible->HasActorBegunPlay()) Destructible->DispatchBeginPlay();
	TestFalse(TEXT("Ordinary symbolic entrances cannot receive melee damage"), Symbolic->CanReceiveMeleeHit_Implementation(nullptr));
	TestTrue(TEXT("Only explicitly configured entrances can be destroyed"), Destructible->CanReceiveMeleeHit_Implementation(nullptr));
	TestEqual(TEXT("Even destructible mouths do not block pawns"), Destructible->RefugeMesh->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
	for (int32 Hit = 0; Hit < 5; ++Hit) Destructible->ReceiveMeleeHit_Implementation(nullptr, EJTSMeleeAttackType::Axe);
	TestFalse(TEXT("Destroyed entrance is no longer targetable"), Destructible->CanReceiveMeleeHit_Implementation(nullptr));
	TestFalse(TEXT("Destroyed entrance disappears on the replicated collapse state"), Destructible->RefugeMesh->IsVisible());
	return true;
}
#endif
