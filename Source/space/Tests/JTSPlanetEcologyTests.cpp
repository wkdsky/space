#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Math/RotationMatrix.h"
#include "UObject/UnrealType.h"
#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSMirrorDefenseComponent.h"
#include "space/UI/JTSMirrorGlareWidget.h"
#include "space/Components/JTSPlanetSurfaceScatterComponent.h"
#include "space/Components/JTSPlanetSurfaceSteeringComponent.h"
#include "space/World/JTSPlanetSurfaceAttachment.h"
#include "EngineUtils.h"
#include "ProceduralMeshComponent.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetEcologyHabitat.h"
#include "space/World/JTSPlanetColonyNest.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "GameFramework/Pawn.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "space/Interaction/InteractionComponent.h"
#include "space/Components/JTSNestBlockadeComponent.h"
#include "space/Components/JTSMeleeComponent.h"
#include "space/Components/JTSMiningStrikeComponent.h"
#include "space/World/JTSPlanetDistributionData.h"
#include "space/World/JTSMoonResourceSpawner.h"
#include "space/Systems/JTSExpeditionSaveGame.h"
#include "Kismet/GameplayStatics.h"

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
	Planet->MaximumPlacementSlopeDegrees = 50;
	for (const FVector Up : {FVector::XAxisVector, FVector::YAxisVector, FVector::ZAxisVector, -FVector::XAxisVector, -FVector::ZAxisVector})
	{
		FVector Tangent, Other;
		Up.FindBestAxisVectors(Tangent, Other);
		FJTSPlanetSurfaceHit Hit;
		Hit.bBlockingHit = true;
		Hit.ImpactPoint = Center + Up * 10000;
		Hit.ImpactNormal = FQuat(Tangent, FMath::DegreesToRadians(50.0)).RotateVector(Up);
		TestTrue(TEXT("Exactly 50 degrees is accepted on each side of a translated sphere"), Planet->IsSurfacePlacementAllowed(Hit));
		Hit.ImpactNormal = FQuat(Tangent, FMath::DegreesToRadians(50.1)).RotateVector(Up);
		TestFalse(TEXT("Over 50 degrees is rejected independently of World-Z"), Planet->IsSurfacePlacementAllowed(Hit));
		Hit.bBlockingHit = false;
		TestFalse(TEXT("A missing real surface is never a valid placement"), Planet->IsSurfacePlacementAllowed(Hit));
	}
	auto* Surface = Scope.World->SpawnActor<AStaticMeshActor>();
	Surface->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Surface->SetActorLocation(Center);
	Surface->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Surface->SetActorScale3D(FVector(200));
	Surface->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	FindFProperty<FObjectProperty>(Planet->GetClass(), TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet, Surface);
	FindFProperty<FFloatProperty>(Planet->GetClass(), TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet, 10000);
	const FVector Origin = Center + FVector(10000, 0, 0);
	TestTrue(TEXT("Short surface distances retain sub-centimetre angular precision"),
		FMath::IsNearlyEqual(Planet->ApproximateSurfaceArcDistance(Origin, Origin + FVector(0, 0.5, 0)), 0.5f, 0.001f));
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
	FJTSPlanetSurfaceHit FeedingGround;
	Planet->ProjectPointToSurface(Origin + FVector::YAxisVector * 350, FeedingGround);
	Emerging->SetActorLocation(FeedingGround.ImpactPoint);
	auto* FoodTarget = Scope.World->SpawnActor<AStaticMeshActor>(FeedingGround.ImpactPoint, Frame);
	Emerging->SetFeedingTarget(FoodTarget);
	FoodTarget->Destroy();
	for (int32 I = 0; I < 500; ++I)
	{
		++GFrameCounter;
		Scope.World->Tick(LEVELTICK_All, 0.05f);
	}
	TestTrue(TEXT("Destroying a feeding target sends its observer back to the real refuge"),
		Planet->ApproximateSurfaceArcDistance(EmergingRefuge, Emerging->GetActorLocation()) < 110);
	TestFalse(TEXT("After returning, a depleted-resource observer stays inside its refuge"), Emerging->FindComponentByClass<USkeletalMeshComponent>()->IsVisible());
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
	Habitat->SurfaceScatter->Clear();
	auto* Collector = Scope.World->SpawnActor<APawn>();
	auto* CollectorRoot = NewObject<USceneComponent>(Collector); Collector->SetRootComponent(CollectorRoot); CollectorRoot->RegisterComponent();
	auto* Inventory = NewObject<UJTSInventoryComponent>(Collector); Inventory->RegisterComponent();
	Collector->AddInstanceComponent(Inventory);
	auto* Deposit = Scope.World->SpawnActor<AJTSPlanetSurfaceAttachment>();
	const FVector DepositPosition = Origin + FVector(350, 0, 0);
	Deposit->Initialize(CavityMesh, Planet, FTransform(Frame, DepositPosition), false, true, .7f, false, nullptr,
		const_cast<UJTSItemDefinition*>(UJTSItemDefinitionLibrary::GetItemDefinition(Collector, EJTSItemId::InorganicCarbon)), 1);
	Collector->SetActorLocation(DepositPosition + FVector(0, 1000, 0));
	Deposit->Interact_Implementation(Collector);
	TestEqual(TEXT("Distant resource harvest cannot mutate inventory"), Inventory->GetItemCount(EJTSItemId::InorganicCarbon), 0);
	Collector->SetActorLocation(DepositPosition + FVector(0, 100, 0));
	Deposit->SetRole(ROLE_SimulatedProxy); Deposit->Interact_Implementation(Collector);
	TestEqual(TEXT("Client-local resource harvest cannot mutate inventory"), Inventory->GetItemCount(EJTSItemId::InorganicCarbon), 0);
	Deposit->SetRole(ROLE_Authority);
	const int32 FullRockCount=Inventory->GetInventoryCapacity()*Inventory->GetEffectiveStackLimit(EJTSItemId::Rock);
	TestTrue(TEXT("Fill the collector before testing a full inventory"), Inventory->TryAddItemById(EJTSItemId::Rock, FullRockCount));
	Deposit->Interact_Implementation(Collector);
	TestFalse(TEXT("Full inventory preserves the collectible deposit"), Deposit->IsActorBeingDestroyed());
	Inventory->TryRemoveItem(EJTSItemId::Rock, FullRockCount);
	Deposit->Interact_Implementation(Collector);
	TestEqual(TEXT("A nearby server harvest grants the distinct inorganic resource"), Inventory->GetItemCount(EJTSItemId::InorganicCarbon), 1);
	TestTrue(TEXT("A harvested deposit is removed for all peers"), Deposit->IsActorBeingDestroyed());
	Deposit->Interact_Implementation(Collector);
	TestEqual(TEXT("Repeated interaction cannot duplicate a deposit"), Inventory->GetItemCount(EJTSItemId::InorganicCarbon), 1);
	// The interaction component owns hold timing on the server, including cancellation and range checks.
	auto* Blockable = Scope.World->SpawnActor<AJTSPlanetEcologyHabitat>(Origin + FVector(0, 1800, 0), Frame);
	Blockable->RefugeMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Blockable->EntranceBlocker->SetStaticMesh(Blockable->RefugeMesh->GetStaticMesh());
	Blockable->bCanBlockEntrance = true; Blockable->BlockHoldDuration = 0.5f;
	Blockable->RefugeMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Blockable->RefugeMesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
	Blockable->EntranceInteractionOffset = FVector(0, 0, 100);
	const FVector WorkPosition=Blockable->GetActorLocation()-Blockable->GetActorForwardVector()*115+Blockable->GetActorUpVector()*25;
	Collector->SetActorLocation(WorkPosition); Collector->SetActorRotation(Blockable->GetActorRotation());
	auto* Melee=NewObject<UJTSMeleeComponent>(Collector); Collector->AddInstanceComponent(Melee); Melee->RegisterComponent();
	auto* Strike=NewObject<UJTSMiningStrikeComponent>(Collector); Collector->AddInstanceComponent(Strike); Strike->RegisterComponent();
	auto* Interaction = NewObject<UInteractionComponent>(Collector); Collector->AddInstanceComponent(Interaction); Interaction->RegisterComponent();
	auto TickFor = [&](int32 Frames) { for (int32 I=0; I<Frames; ++I) { ++GFrameCounter; Scope.World->Tick(LEVELTICK_All, 0.05f); } };
	TestFalse(TEXT("Ordinary habitat markers never offer blocking interaction"), Symbolic->CanInteract_Implementation(Collector));
	TestTrue(TEXT("Explicitly marked entrances offer the blocking action"), Blockable->CanInteract_Implementation(Collector));
	Blockable->SetRole(ROLE_SimulatedProxy); Blockable->Interact_Implementation(Collector);
	TestFalse(TEXT("Client-local blocking cannot change the nest"), Blockable->IsEntranceBlocked());
	Blockable->SetRole(ROLE_Authority);
	Interaction->ServerTryInteract_Implementation(Blockable);
	TestFalse(TEXT("An instant interaction RPC cannot bypass the hold"), Blockable->IsEntranceBlocked());
	Interaction->ServerBeginInteractionHold_Implementation(Blockable); TickFor(4);
	TestEqual(TEXT("No equipped pickaxe means no work"), Blockable->Blockade->GetProgress(), 0.0f);
	TestTrue(TEXT("Pickaxe is restored to the normal shop"), UJTSItemDefinitionLibrary::GetDefaultShopCatalog().Contains(EJTSItemId::Pickaxe));
	TestTrue(TEXT("Pickaxe can be equipped"), Inventory->TryAddItemById(EJTSItemId::Pickaxe));
	for (int32 Slot=0; Slot<Inventory->GetItemSlots().Num(); ++Slot)
		if (Inventory->GetItemAtSlot(Slot).ItemId == EJTSItemId::Pickaxe) Inventory->SelectQuickbarSlot(Slot);
	Interaction->ServerBeginInteractionHold_Implementation(Blockable); TickFor(4);
	TestFalse(TEXT("A partial hold does not block the entrance"), Blockable->IsEntranceBlocked());
	Interaction->ServerCancelInteractionHold_Implementation(); TickFor(2);
	const float RetainedProgress = Blockable->Blockade->GetProgress();
	TestTrue(TEXT("A tap retains progress during the grace period"), RetainedProgress > 0);
	TickFor(25);
	TestTrue(TEXT("Idle work decays gradually"), Blockable->Blockade->GetProgress() < RetainedProgress);
	TestFalse(TEXT("Releasing E cancels server hold completion"), Blockable->IsEntranceBlocked());
	Interaction->ServerBeginInteractionHold_Implementation(Blockable);
	Collector->SetActorLocation(Blockable->GetActorLocation() + FVector(0, -1000, 0)); TickFor(12);
	TestFalse(TEXT("Leaving interaction range cancels the hold"), Blockable->IsEntranceBlocked());
	Collector->SetActorLocation(WorkPosition);
	Interaction->ServerBeginInteractionHold_Implementation(Blockable); TickFor(25);
	TestTrue(TEXT("Confirmed server strikes complete the marked entrance"), Blockable->IsEntranceBlocked());
	TestTrue(TEXT("Completion requires multiple physical contacts"), Strike->GetConfirmedImpactCount() >= 2);
	// The rubble follows the confirmed final strike, whose timing comes from the equipped tool.
	TickFor(FMath::CeilToInt(Blockable->Blockade->RubbleDelay / .05f) + 1);
	TestTrue(TEXT("Blocking shows the configured plug mesh"), Blockable->EntranceBlocker->IsVisible());
	TestFalse(TEXT("A blocked entrance cannot be used again"), Blockable->CanInteract_Implementation(Collector));
	TestFalse(TEXT("A blocked entrance stops being a refuge"), Blockable->IsRefugeAvailable());
	// The AI can observe the player leaving before the mouth's service timer runs.
	auto* DefenseHome = Scope.World->SpawnActor<AJTSPlanetEcologyHabitat>(Origin, Frame);
	Configure(DefenseHome, 0); DefenseHome->bCanBlockEntrance = true;
	DefenseHome->RefugeMesh->SetStaticMesh(Blockable->RefugeMesh->GetStaticMesh());
	DefenseHome->Refuges.Add(DefenseHome);
	auto* BurstEmerging = Scope.World->SpawnActor<AJTSPlanetSurfaceCreature>(Origin, Frame);
	if (!BurstEmerging->HasActorBegunPlay()) BurstEmerging->DispatchBeginPlay();
	TestTrue(TEXT("Burst defender initializes on the actual surface"), BurstEmerging->InitializeForSettlement_Implementation(Planet, Origin, Origin));
	BurstEmerging->SetHabitat(DefenseHome, Origin);
	FJTSPlanetSurfaceFrame ExitFrame;
	TestTrue(TEXT("Burst exit follows the curved surface"), Planet->GetSurfaceFrameAt(Origin + FVector(0, 300, 0), FVector::YAxisVector, ExitFrame));
	BurstEmerging->BeginEntranceEmergence(FTransform(Frame, Origin), ExitFrame.Location);
	Collector->SetActorLocation(Origin + FVector(0, 800, 0));
	BurstEmerging->DefendEntrance(Collector, Origin, 1800);
	++GFrameCounter; Scope.World->Tick(LEVELTICK_All, 1.f / 120);
	const FVector FirstSmoothStep = BurstEmerging->GetActorLocation();
	++GFrameCounter; Scope.World->Tick(LEVELTICK_All, 1.f / 120);
	TestTrue(TEXT("Movement advances on consecutive render frames between AI decisions"),
		Planet->ApproximateSurfaceArcDistance(Origin, FirstSmoothStep) > 1
		&& Planet->ApproximateSurfaceArcDistance(FirstSmoothStep, BurstEmerging->GetActorLocation()) > 1);
	++GFrameCounter; Scope.World->Tick(LEVELTICK_All, 0.4f);
	TestTrue(TEXT("A slow server frame retains elapsed movement instead of capping it at 0.15 seconds"),
		Planet->ApproximateSurfaceArcDistance(Origin, BurstEmerging->GetActorLocation()) > 65);
	TestTrue(TEXT("Emergence moves clear of the opening while its animation plays"),
		Planet->ApproximateSurfaceArcDistance(Origin, BurstEmerging->GetActorLocation()) < 110);
	BurstEmerging->Destroy();
	// A raised player capsule and a crater radius different from the nominal sphere must still be reachable.
	// APawn rejects all damage in worlds without an authoritative GameMode.
	Scope.World->SetGameInstance(NewObject<UGameInstance>());
	TestTrue(TEXT("Combat fixture supplies an authoritative GameMode"), Scope.World->SetGameMode(FURL(nullptr, TEXT("/Temp/Combat?game=/Script/Engine.GameModeBase"), TRAVEL_Absolute)));
	FindFProperty<FFloatProperty>(Planet->GetClass(), TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet, 14000);
	auto* AttackTarget = Scope.World->SpawnActor<APawn>();
	auto* TargetCapsule = NewObject<UCapsuleComponent>(AttackTarget);
	AttackTarget->SetRootComponent(TargetCapsule); AttackTarget->AddInstanceComponent(TargetCapsule);
	TargetCapsule->InitCapsuleSize(34, 88); TargetCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TargetCapsule->SetCollisionResponseToAllChannels(ECR_Ignore); TargetCapsule->RegisterComponent();
	auto* TargetHealth = NewObject<UJTSHealthComponent>(AttackTarget);
	AttackTarget->AddInstanceComponent(TargetHealth); TargetHealth->RegisterComponent();
	if (!AttackTarget->HasActorBegunPlay()) AttackTarget->DispatchBeginPlay();
	TargetHealth->SetMaxHealth(1000, true);
	FJTSPlanetSurfaceFrame TargetFrame;
	TestTrue(TEXT("Combat target stands on real ground"), Planet->GetSurfaceFrameAt(Origin + FVector(0, 350, 0), FVector::YAxisVector, TargetFrame));
	AttackTarget->SetActorLocationAndRotation(TargetFrame.Location + TargetFrame.Up * 88, TargetFrame.Transform.GetRotation());
	auto* Attacker = Scope.World->SpawnActor<AJTSPlanetSurfaceCreature>(Origin, Frame);
	if (!Attacker->HasActorBegunPlay()) Attacker->DispatchBeginPlay();
	TestTrue(TEXT("Combat defender initializes on the actual surface"), Attacker->InitializeForSettlement_Implementation(Planet, Origin, Origin));
	Attacker->SetHabitat(DefenseHome, Origin);
	Attacker->BeginEntranceEmergence(FTransform(Frame, Origin), Origin);
	Attacker->DefendEntrance(AttackTarget, Origin, 1800, 3, 4);
	TickFor(100);
	TestTrue(TEXT("An individual approach converges and damages a raised capsule inside the nominal radius"), TargetHealth->GetHealth() < 1000);
	AttackTarget->SetActorLocation(TargetFrame.Location + TargetFrame.Up * 600);
	const float HealthOutOfReach = TargetHealth->GetHealth(); TickFor(60);
	TestEqual(TEXT("Surface projection cannot let a defender hit a player high above the ground"), TargetHealth->GetHealth(), HealthOutOfReach);
	Attacker->Destroy();
	// Reflectors stay at their mouth-relative station, including when the aggressor stands on them.
	TargetHealth->SetMaxHealth(1000, true);
	auto* Reflector = Scope.World->SpawnActor<AJTSPlanetSurfaceCreature>(Origin, Frame);
	if (!Reflector->HasActorBegunPlay()) Reflector->DispatchBeginPlay();
	auto* MirrorDefense = Reflector->FindComponentByClass<UJTSMirrorDefenseComponent>();
	MirrorDefense->bEnabled = true;
	TestTrue(TEXT("Mirror guard initializes on the real spherical ground"), Reflector->InitializeForSettlement_Implementation(Planet, Origin, Origin));
	Reflector->SetHabitat(DefenseHome, Origin);
	Reflector->BeginEntranceEmergence(FTransform(Frame, Origin), Origin);
	Reflector->DefendEntrance(AttackTarget, Origin, 1800, 1, 3);
	AttackTarget->SetActorLocation(TargetFrame.Location + TargetFrame.Up*88);
	TickFor(180);
	for (int32 I = 0; I < 120 && !MirrorDefense->IsReflecting(); ++I) TickFor(1);
	TestTrue(TEXT("Nest guard enters reflection rather than melee"), MirrorDefense->IsReflecting());
	bool bHeldReflection = true;
	for (int32 I = 0; I < 240; ++I) { TickFor(1); bHeldReflection &= MirrorDefense->IsReflecting(); }
	TestTrue(TEXT("A nest attacker inside the leash keeps reflection active without a timer cycle"), bHeldReflection);
	Reflector->FindComponentByClass<UJTSHealthComponent>()->ApplyDamage(1, nullptr, AttackTarget);
	TickFor(20);
	TestTrue(TEXT("Taking a hit cannot drop an active nest reflection"), MirrorDefense->IsReflecting());
	const FVector GuardStation = Reflector->GetActorLocation();
	TestTrue(TEXT("Reflector stays within the nest guard radius"), Planet->ApproximateSurfaceArcDistance(Origin, GuardStation) < 350);
	AttackTarget->SetActorLocation(GuardStation + Reflector->GetActorUpVector()*88); TickFor(80);
	TestEqual(TEXT("Standing on the mirror guard cannot cause contact or melee damage"),TargetHealth->GetHealth(),1000.f);
	AttackTarget->SetActorLocation(Origin + FVector(0,1100,88)); TickFor(80);
	TestTrue(TEXT("Moving the player cannot pull a reflector out of its nest station"), FVector::Distance(GuardStation, Reflector->GetActorLocation()) < 35);
	FindFProperty<FFloatProperty>(Reflector->GetClass(), TEXT("LeashRadius"))->SetPropertyValue_InContainer(Reflector, 1200);
	const FVector GuardDirection = (GuardStation - Origin).GetSafeNormal();
	AttackTarget->SetActorLocation(Origin + GuardDirection * 1750); TickFor(20);
	TestTrue(TEXT("Nest reflection uses the mouth radius even beyond the creature's smaller individual radius"),
		FVector::Distance(AttackTarget->GetActorLocation(), GuardStation) > 1200 && MirrorDefense->IsReflecting());
	AttackTarget->SetActorLocation(Origin + GuardDirection * 1850); TickFor(2);
	TestFalse(TEXT("Leaving the mouth-centred leash begins retraction"), MirrorDefense->IsReflecting());
	const FVector RecallPosition = Reflector->GetActorLocation();
	AttackTarget->SetActorLocation(GuardStation + Reflector->GetActorUpVector()*88);
	TickFor(60);
	TestFalse(TEXT("Returning to the nest radius without another attack cannot restore forgotten threat"), MirrorDefense->IsReflecting());
	TestTrue(TEXT("Losing threat leaves a visible, hittable pause before running home"), Reflector->CanReceiveMeleeHit_Implementation(AttackTarget)
		&& Reflector->GetActorLocation().Equals(RecallPosition, .1f));
	Reflector->Destroy();
	// Individual retaliation follows the animal's current position, never the nest or first-hit origin.
	FJTSPlanetSurfaceFrame PersonalFrame;
	TestTrue(TEXT("Individual combat fixture is away from its home"), Planet->GetSurfaceFrameAt(Origin + FVector(0, 2500, 0), FVector::YAxisVector, PersonalFrame));
	auto* PersonalMirror = Scope.World->SpawnActor<AJTSPlanetSurfaceCreature>(PersonalFrame.Location, PersonalFrame.Transform.Rotator());
	if (!PersonalMirror->HasActorBegunPlay()) PersonalMirror->DispatchBeginPlay();
	TestTrue(TEXT("Individual mirror initializes on real ground"), PersonalMirror->InitializeForSettlement_Implementation(Planet, Origin, PersonalFrame.Location));
	FindFProperty<FFloatProperty>(PersonalMirror->GetClass(), TEXT("LeashRadius"))->SetPropertyValue_InContainer(PersonalMirror, 1200);
	auto* PersonalDefense = PersonalMirror->FindComponentByClass<UJTSMirrorDefenseComponent>(); PersonalDefense->bEnabled = true;
	auto* PersonalHealth = PersonalMirror->FindComponentByClass<UJTSHealthComponent>(); PersonalHealth->SetMaxHealth(1000, true);
	AttackTarget->SetActorLocation(PersonalFrame.Location + PersonalFrame.Forward*300 + PersonalFrame.Up*88);
	PersonalHealth->ApplyDamage(1, nullptr, AttackTarget); TickFor(30);
	TestTrue(TEXT("Being hit creates individual reflection far outside the nest radius"), PersonalDefense->IsReflecting());
	bHeldReflection = true;
	for (int32 I = 0; I < 240; ++I) { TickFor(1); bHeldReflection &= PersonalDefense->IsReflecting(); }
	TestTrue(TEXT("Individual reflection also persists while its attacker stays nearby"), bHeldReflection);
	// Move the animal as well: using the first-hit location would incorrectly clear this target.
	FJTSPlanetSurfaceFrame MovedFrame;
	Planet->GetSurfaceFrameAt(PersonalFrame.Location + PersonalFrame.Forward*900, PersonalFrame.Forward, MovedFrame);
	PersonalMirror->SetActorLocationAndRotation(MovedFrame.Location, MovedFrame.Transform.GetRotation());
	AttackTarget->SetActorLocation(MovedFrame.Location + MovedFrame.Forward*1000); TickFor(2);
	TestTrue(TEXT("Individual threat is measured from the current creature rather than the original hit location"), PersonalDefense->IsReflecting());
	auto* SecondAggressor = Scope.World->SpawnActor<APawn>();
	auto* SecondRoot = NewObject<USceneComponent>(SecondAggressor); SecondAggressor->SetRootComponent(SecondRoot); SecondRoot->RegisterComponent();
	SecondAggressor->SetActorLocation(MovedFrame.Location + MovedFrame.Forward*300);
	PersonalHealth->ApplyDamage(1, nullptr, SecondAggressor);
	AttackTarget->SetActorLocation(MovedFrame.Location + MovedFrame.Forward*1300); TickFor(2);
	TestTrue(TEXT("A second nearby attacker keeps individual threat alive when the first leaves"), PersonalDefense->IsReflecting());
	AttackTarget->SetActorLocation(MovedFrame.Location + MovedFrame.Forward*100);
	SecondAggressor->SetActorLocation(MovedFrame.Location + MovedFrame.Forward*1300); TickFor(2);
	TestFalse(TEXT("A forgotten attacker returning during another fight is not automatically added back"), PersonalDefense->IsReflecting());
	const FVector PersonalRecoveryPosition = PersonalMirror->GetActorLocation(); TickFor(60);
	TestTrue(TEXT("The individual mirror also finishes its slow retraction and surface pause before returning"),
		PersonalMirror->GetActorLocation().Equals(PersonalRecoveryPosition, .1f));
	TestFalse(TEXT("Remaining nearby after threat loss cannot restart reflection"), PersonalDefense->IsReflecting());
	PersonalHealth->ApplyDamage(1, nullptr, AttackTarget); TickFor(30);
	TestTrue(TEXT("A fresh hit can establish threat again after disengagement"), PersonalDefense->IsReflecting());
	TargetHealth->ApplyDamage(10000, nullptr, nullptr); TickFor(2);
	TestFalse(TEXT("A dead attacker immediately loses individual threat"), PersonalDefense->IsReflecting());
	PersonalMirror->Destroy(); SecondAggressor->Destroy();
	// Exercise the actual nest alarm: an uninvolved nearby pawn must not keep it alive.
	TargetHealth->RestoreAuthoritativeHealth(1000);
	FJTSPlanetSurfaceFrame AlarmFrame;
	Planet->GetSurfaceFrameAt(Origin + FVector(0, -1800, 0), FVector::YAxisVector, AlarmFrame);
	auto* AlarmHome = Scope.World->SpawnActor<AJTSPlanetEcologyHabitat>(AlarmFrame.Location, AlarmFrame.Transform.Rotator());
	Configure(AlarmHome, 0); AlarmHome->Refuges.Add(AlarmHome);
	AlarmHome->bCanBlockEntrance = true; AlarmHome->BlockHoldDuration = 1000;
	AlarmHome->RefugeMesh->SetStaticMesh(Blockable->RefugeMesh->GetStaticMesh());
	AlarmHome->RefugeMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AlarmHome->RefugeMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	AlarmHome->EntranceInteractionOffset = FVector(0, 0, 100);
	AlarmHome->Blockade->DefensePopulation = 1;
	auto* AlarmGuard = Scope.World->SpawnActor<AJTSPlanetSurfaceCreature>(AlarmFrame.Location, AlarmFrame.Transform.Rotator());
	if (!AlarmGuard->HasActorBegunPlay()) AlarmGuard->DispatchBeginPlay();
	AlarmGuard->InitializeForSettlement_Implementation(Planet, AlarmFrame.Location, AlarmFrame.Location);
	AlarmGuard->SetHabitat(AlarmHome, AlarmFrame.Location);
	auto* AlarmDefense = AlarmGuard->FindComponentByClass<UJTSMirrorDefenseComponent>(); AlarmDefense->bEnabled = true;
	const FVector AlarmWorkPosition = AlarmFrame.Location - AlarmFrame.Forward*115 + AlarmFrame.Up*25;
	Collector->SetActorLocationAndRotation(AlarmWorkPosition, AlarmFrame.Transform.GetRotation());
	AttackTarget->SetActorLocation(AlarmFrame.Location + AlarmFrame.Forward*900 + AlarmFrame.Up*88);
	TestTrue(TEXT("A real nest attack can pass the normal server work validation"), AlarmHome->Blockade->CanWork(Collector));
	AlarmHome->Blockade->SetWorking(Collector, true); AlarmHome->Blockade->SetWorking(Collector, false); TickFor(160);
	TestTrue(TEXT("Validated nest work establishes an alarm against its worker"), AlarmDefense->IsReflecting());
	Collector->SetActorLocation(AlarmFrame.Location - AlarmFrame.Forward*2500); TickFor(4);
	TestFalse(TEXT("An uninvolved player inside the nest radius cannot retain a departed worker's alarm"), AlarmDefense->IsReflecting());
	Collector->SetActorLocation(AlarmWorkPosition); TickFor(30);
	TestFalse(TEXT("Returning to a real nest without attacking does not restart its alarm"), AlarmDefense->IsReflecting());
	AlarmHome->Blockade->SetWorking(Collector, true); AlarmHome->Blockade->SetWorking(Collector, false); TickFor(100);
	TestTrue(TEXT("Attacking the nest again establishes a fresh alarm"), AlarmDefense->IsReflecting());
	auto* SecondInventory = NewObject<UJTSInventoryComponent>(AttackTarget); AttackTarget->AddInstanceComponent(SecondInventory); SecondInventory->RegisterComponent();
	auto* SecondMelee = NewObject<UJTSMeleeComponent>(AttackTarget); AttackTarget->AddInstanceComponent(SecondMelee); SecondMelee->RegisterComponent();
	auto* SecondStrike = NewObject<UJTSMiningStrikeComponent>(AttackTarget); AttackTarget->AddInstanceComponent(SecondStrike); SecondStrike->RegisterComponent();
	SecondInventory->TryAddItemById(EJTSItemId::Pickaxe);
	for (int32 Slot = 0; Slot < SecondInventory->GetItemSlots().Num(); ++Slot)
		if (SecondInventory->GetItemAtSlot(Slot).ItemId == EJTSItemId::Pickaxe) SecondInventory->SelectQuickbarSlot(Slot);
	AttackTarget->SetActorLocationAndRotation(AlarmWorkPosition, AlarmFrame.Transform.GetRotation());
	TestTrue(TEXT("Another expedition member can provoke the same nest"), AlarmHome->Blockade->CanWork(AttackTarget));
	AlarmHome->Blockade->SetWorking(AttackTarget, true); AlarmHome->Blockade->SetWorking(AttackTarget, false); TickFor(4);
	Collector->SetActorLocation(AlarmFrame.Location - AlarmFrame.Forward*2500);
	bHeldReflection = true;
	for (int32 I = 0; I < 40; ++I) { TickFor(1); bHeldReflection &= AlarmDefense->IsReflecting(); }
	TestTrue(TEXT("A second actual nest attacker retains uninterrupted reflection when the first leaves"), bHeldReflection);
	Collector->SetActorLocation(AlarmWorkPosition);
	AttackTarget->SetActorLocation(AlarmFrame.Location - AlarmFrame.Forward*2500); TickFor(4);
	TestFalse(TEXT("A former worker returning during another player's fight remains forgotten"), AlarmDefense->IsReflecting());
	AlarmHome->Blockade->SetWorking(Collector, true); AlarmHome->Blockade->SetWorking(Collector, false); TickFor(100);
	TestTrue(TEXT("A fresh attack restores the alarm before the entrance is destroyed"), AlarmDefense->IsReflecting());
	AlarmHome->CommitEntranceBlockade(); TickFor(4);
	TestTrue(TEXT("Destroying the mouth stops new spawns without clearing an active defender's threat"), AlarmDefense->IsReflecting());
	Collector->SetActorLocation(AlarmFrame.Location - AlarmFrame.Forward*2500); TickFor(4);
	TestFalse(TEXT("A surviving guard of a destroyed mouth still retracts when its attacker leaves"), AlarmDefense->IsReflecting());
	AlarmGuard->Destroy(); AlarmHome->Destroy(); AttackTarget->Destroy();
	// All feeders and temporary guards sharing a mouth consume the same finite reserve.
	auto* FiniteHome = Scope.World->SpawnActor<AJTSPlanetEcologyHabitat>(Origin, Frame);
	Configure(FiniteHome, 0); FiniteHome->Refuges.Add(FiniteHome);
	FiniteHome->RefugeMesh->SetStaticMesh(Blockable->RefugeMesh->GetStaticMesh());
	FiniteHome->Blockade->bFinitePopulation = true;
	FiniteHome->Blockade->PopulationCapacity = 3;
	FiniteHome->Blockade->PopulationRecoveryDelay = 2;
	FiniteHome->Blockade->PopulationRecoveryInterval = 1;
	TArray<AJTSPlanetSurfaceCreature*> Residents;
	for (int32 I = 0; I < 3; ++I)
	{
		auto* Resident = Scope.World->SpawnActor<AJTSPlanetSurfaceCreature>(Origin, Frame);
		if (!Resident->HasActorBegunPlay()) Resident->DispatchBeginPlay();
		Resident->SetHabitat(FiniteHome, Origin);
		Residents.Add(Resident);
	}
	TestEqual(TEXT("Releasing all residents leaves no underground reserve"), FiniteHome->Blockade->GetReservePopulation(), 0);
	TestEqual(TEXT("Emergence does not consume a second copy of a living resident"), FiniteHome->Blockade->GetRemainingPopulation(), 3);
	auto* Overflow = Scope.World->SpawnActor<AJTSPlanetSurfaceCreature>(Origin, Frame);
	Overflow->SetHabitat(FiniteHome, Origin);
	TestTrue(TEXT("Another feeding area cannot bypass an exhausted mouth reserve"), Overflow->IsActorBeingDestroyed());
	FiniteHome->Blockade->UnregisterResident(Residents[0], false);
	Residents[0]->Destroy();
	TestEqual(TEXT("A live returning guard restores one reserve slot"), FiniteHome->Blockade->GetReservePopulation(), 1);
	auto* Replacement = Scope.World->SpawnActor<AJTSPlanetSurfaceCreature>(Origin, Frame);
	if (!Replacement->HasActorBegunPlay()) Replacement->DispatchBeginPlay();
	Replacement->SetHabitat(FiniteHome, Origin);
	Residents[0] = Replacement;
	for (auto* Resident : Residents) Resident->FindComponentByClass<UJTSHealthComponent>()->ApplyDamage(10000, nullptr, nullptr);
	TestEqual(TEXT("Killing the full finite cohort leaves a truly empty nest"), FiniteHome->Blockade->GetRemainingPopulation(), 0);
	TickFor(30);
	TestFalse(TEXT("Repeated defense requests cannot refill the clear window"), FiniteHome->Blockade->CanReleaseResident());
	TickFor(12);
	TestTrue(TEXT("The nest can begin regenerating after its clear window"), FiniteHome->Blockade->CanReleaseResident());
	TestEqual(TEXT("Recovery starts with one resident rather than a full new wave"), FiniteHome->Blockade->GetRemainingPopulation(), 1);
	FiniteHome->bCanBlockEntrance = true; FiniteHome->CommitEntranceBlockade(); TickFor(80);
	TestFalse(TEXT("A demolished entrance cannot regenerate another defender"), FiniteHome->Blockade->CanReleaseResident());
	FindFProperty<FFloatProperty>(Planet->GetClass(), TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet, 10000);
	auto* Returning = Scope.World->SpawnActor<AJTSPlanetSurfaceCreature>(Origin, Frame);
	TestTrue(TEXT("A returning defender starts on the real surface"), Returning->InitializeForSettlement_Implementation(Planet, Origin, Origin));
	Returning->SetHabitat(DefenseHome, Origin);
	Collector->SetActorLocation(Origin + FVector(0, 400, 0));
	Returning->DefendEntrance(Collector, Origin, 1800);
	DefenseHome->CommitEntranceBlockade();
	Collector->SetActorLocation(Origin + FVector(0, 4000, 0)); TickFor(40);
	TestTrue(TEXT("A defender cannot re-enter a blocked refuge when its own leash check runs first"), Returning->IsActorBeingDestroyed());
	// Independent save envelopes preserve irreversible IDs without sharing them between slots.
	auto* SaveA = NewObject<UJTSExpeditionSaveGame>();
	SaveA->Snapshot.BlockedEntranceIds.Add(TEXT("Moon.Entrance.01"));
	TArray<uint8> SaveBytes;
	TestTrue(TEXT("Entrance tombstones serialize"), UGameplayStatics::SaveGameToMemory(SaveA, SaveBytes));
	auto* Restored = Cast<UJTSExpeditionSaveGame>(UGameplayStatics::LoadGameFromMemory(SaveBytes));
	TestTrue(TEXT("Reload preserves a destroyed entrance"), Restored && Restored->Snapshot.BlockedEntranceIds.Contains(TEXT("Moon.Entrance.01")));
	TestTrue(TEXT("A new save starts with intact entrances"), NewObject<UJTSExpeditionSaveGame>()->Snapshot.BlockedEntranceIds.IsEmpty());
	// A frozen layout takes precedence over density, seeds, and runtime random calls.
	auto* Frozen = NewObject<UJTSPlanetDistributionData>();
	auto* FrozenSpawner = Scope.World->SpawnActor<AJTSMoonResourceSpawner>(Origin, Frame);
	FrozenSpawner->SetOwningPlanet(Planet); FrozenSpawner->DistributionData = Frozen;
	FJTSFrozenResourceLayout FrozenLayout; FrozenLayout.Key = FrozenSpawner->GetFName();
	FJTSFrozenResource FrozenNode; FrozenNode.Pose = FTransform(Frame, Origin + FVector(0,0,350));
	FrozenLayout.Resources.Add(FrozenNode);
	FrozenNode.bPickup = true; FrozenNode.Variant = 3;
	FrozenNode.Pose = FTransform(Frame, Origin + FVector(0,0,650)); FrozenLayout.Resources.Add(FrozenNode);
	Frozen->ResourceLayouts.Add(FrozenLayout);
	TestEqual(TEXT("The saved manifest supplies the exact resource capacity"), FrozenSpawner->GenerateResources(), 2);
	auto FindFrozenPickup = [&]() -> AJTSWorldPickupActor*
	{
		for (TActorIterator<AJTSWorldPickupActor> It(Scope.World); It; ++It)
			if (!It->IsActorBeingDestroyed() && It->GetOwner() == FrozenSpawner) return *It;
		return nullptr;
	};
	auto* FirstPickup = FindFrozenPickup();
	const FTransform PickupPose = FirstPickup ? FirstPickup->GetActorTransform() : FTransform::Identity;
	UStaticMesh* PickupMesh = FirstPickup ? FirstPickup->FindComponentByClass<UStaticMeshComponent>()->GetStaticMesh() : nullptr;
	const FGuid PickupId = FirstPickup ? FirstPickup->GetItemInstance().InstanceId : FGuid();
	static_cast<void>(FMath::Rand()); static_cast<void>(FMath::Rand());
	TestEqual(TEXT("Reentry reuses the same manifest"), FrozenSpawner->GenerateResources(), 2);
	auto* NextPickup = FindFrozenPickup();
	TestTrue(TEXT("Loose rocks keep the authored pose and mesh across regeneration"), FirstPickup && NextPickup
		&& NextPickup->GetActorTransform().Equals(PickupPose)
		&& NextPickup->FindComponentByClass<UStaticMeshComponent>()->GetStaticMesh() == PickupMesh);
	TestTrue(TEXT("A replenished pickup still has its own inventory identity"), NextPickup && NextPickup->GetItemInstance().InstanceId != PickupId);
	FrozenSpawner->ClearGeneratedResources();
	// One real collision mesh joins a 60-degree face to flat ground on this translated sphere's side.
	auto* FootActor = Scope.World->SpawnActor<AActor>(Origin, FRotator::ZeroRotator);
	auto* FootMesh = NewObject<UProceduralMeshComponent>(FootActor);
	FootActor->SetRootComponent(FootMesh); FootActor->AddInstanceComponent(FootMesh); FootMesh->RegisterComponent();
	FootMesh->SetWorldLocation(Origin); FootMesh->SetCollisionProfileName(TEXT("BlockAll"));
	const TArray<FVector> FootVertices = {FVector(0,0,-500),FVector(0,500,-500),FVector(0,500,500),FVector(0,0,500),
		FVector(866,-500,-500),FVector(0,0,-500),FVector(0,0,500),FVector(866,-500,500)};
	FootMesh->CreateMeshSection_LinearColor(0, FootVertices, {0,1,2,0,2,3,4,5,6,4,6,7}, {}, {}, {}, {}, true);
	FindFProperty<FObjectProperty>(Planet->GetClass(), TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet, FootActor);
	FindFProperty<FObjectProperty>(Planet->GetClass(), TEXT("GameplaySurfaceComponent"))->SetObjectPropertyValue_InContainer(Planet, FootMesh);
	FJTSPlanetSurfaceHit FootHit;
	TestTrue(TEXT("Slope-foot fixture has an actual steep collision face"), Planet->ProjectPointToSurface(Origin + FVector(70,-40,0), FootHit) && !Planet->IsSurfacePlacementAllowed(FootHit));
	const FVector SteepPoint = FootHit.ImpactPoint;
	TestFalse(TEXT("Strict placement still rejects the steep face"), Planet->FindSurfacePlacementSupport(SteepPoint, 0, FootHit));
	TestTrue(TEXT("A slope-foot placement resolves onto adjacent supported ground"), Planet->FindSurfacePlacementSupport(SteepPoint, 120, FootHit));
	TestTrue(TEXT("Recovered placement satisfies the original slope limit"), Planet->IsSurfacePlacementAllowed(FootHit));
	TestFalse(TEXT("The search cannot reach unrelated flat ground beyond its radius"), Planet->FindSurfacePlacementSupport(SteepPoint, 20, FootHit));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMirrorLeanTest, "JTS.Moon.MirrorDefense.LeanAndCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSMirrorLeanTest::RunTest(const FString&)
{
	for (const FVector Target : {FVector(1,0,.3),FVector(-1,0,.3),FVector(0,1,.3),FVector(0,-1,.3),FVector(1,1,.3),FVector::UpVector})
	{
		const FQuat Lean = UJTSMirrorDefenseComponent::CalculateLean(Target,78);
		TestTrue(TEXT("Shortest lean can present the back toward every azimuth"), FVector::DotProduct(Lean.RotateVector(FVector::UpVector),Target.GetSafeNormal()) > .999);
		TestTrue(TEXT("Mirror presentation remains normalized"),Lean.IsNormalized());
	}
	const FQuat Below = UJTSMirrorDefenseComponent::CalculateLean(FVector(1,0,-1),78);
	TestTrue(TEXT("A low camera cannot flip the animal upside down"),Below.GetAngle() <= FMath::DegreesToRadians(78.01f));
	TestEqual(TEXT("One insect only occludes a local circular region"),UJTSMirrorGlareWidget::FullOcclusionForCount(1),0.f);
	TestEqual(TEXT("Two insects retain unoccluded screen regions"),UJTSMirrorGlareWidget::FullOcclusionForCount(2),0.f);
	TestTrue(TEXT("Even several mirrors preserve visibility for aiming a counterattack"),UJTSMirrorGlareWidget::FullOcclusionForCount(3) < .5f);
	return true;
}
#endif
