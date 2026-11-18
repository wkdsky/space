#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Components/JTSDiscoveryComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSCorpseMeshComponent.h"
#include "space/Components/JTSMeleeComponent.h"
#include "space/Components/JTSRangedWeaponComponent.h"
#include "space/Components/JTSNestBlockadeComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSWeaponVisualComponent.h"
#include "space/Components/JTSStellarTargetComponent.h"
#include "space/World/JTSPlanetEcologyHabitat.h"
#include "space/World/JTSPlanetSurfaceAttachment.h"
#include "space/Characters/JTSPlanetBoss.h"
#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMeshActor.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSurfacePlacementBounds.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Engine/DamageEvents.h"
#include "UObject/UnrealType.h"
#include "space/Components/JTSShipDockComponent.h"
#include "space/Components/JTSShipScannerComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/UI/JTSFieldGuideWidget.h"
#include "InputCoreTypes.h"
#include "space/Systems/JTSExpeditionSaveGame.h"
#include "space/World/JTSSpaceWorldManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
struct FAnalysisWorld
{
	UWorld* World;
	AJTSGameState* State;
	AJTSSpacecraftActor* Ship;
	FAnalysisWorld()
	{
		const auto Settings=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Settings);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
		State=World->SpawnActor<AJTSGameState>(); World->SetGameState(State); State->DispatchBeginPlay();
		Ship=World->SpawnActor<AJTSSpacecraftActor>(); Ship->DispatchBeginPlay(); Ship->SetActorTickEnabled(false); Ship->GetDockComponent()->SetComponentTickEnabled(false);
	}
	~FAnalysisWorld()
	{
		// These fixtures dispatch BeginPlay manually without a GameMode. UWorld::EndPlay
		// intentionally skips that case, so route actor teardown before destroying the world.
		for(FActorIterator It(World);It;++It) It->RouteEndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	}
	void Advance(float Seconds)
	{
		// TimerManager advances once per engine frame, including in manually ticked test worlds.
		TGuardValue<uint64> Frame(GFrameCounter,GFrameCounter);
		for(float T=0;T<Seconds;T+=.05f) { ++GFrameCounter; World->Tick(LEVELTICK_All,.05f); }
	}
	AJTSCharacter* Player(FVector Location)
	{
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<AJTSCharacter>(Location,FRotator::ZeroRotator,P);
	}
	UJTSShipScannerComponent* GroundScanner()
	{
		auto* Floor=World->SpawnActor<AStaticMeshActor>(); auto* Mesh=Floor->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
		Floor->SetActorLocation(FVector(0,0,-55)); Floor->SetActorScale3D(FVector(100,100,1)); Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Ship->Scanner->SetRelativeLocation(FVector(0,1200,100)); Advance(.2f); return Ship->Scanner;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSAnalysisKnowledgeTest,"JTS.Analysis.TypeIdentityLazySourcesAndSave",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSAnalysisKnowledgeTest::RunTest(const FString&)
{
	FAnalysisWorld W; auto* D=W.State->GetDiscoveryComponent();
	auto Rock=UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::Rock,1); Rock.ObservedPlanetIds={TEXT("Moon")}; Rock.SampleSpanCm=12;
	TestFalse(TEXT("Natural material starts unknown"),D->Knows(Rock.ItemId));
	D->ObservePickup(Rock); TestTrue(TEXT("Picking up an unknown sample does not identify it"),D->GetRecords().IsEmpty());
	TestTrue(TEXT("First completed analysis discovers a type"),D->Analyze(Rock));
	TestTrue(TEXT("A different size of the same type is known"),D->Knows(EJTSItemId::Rock));
	TestEqual(TEXT("Only actually collected Moon source is present"),D->GetRecord(Rock.ItemId)->Sources.Num(),1);
	TestFalse(TEXT("Repeated analysis is idempotent"),D->Analyze(Rock));
	Rock.ObservedPlanetIds={TEXT("Mars")}; Rock.SampleSpanCm=170; D->ObservePickup(Rock); D->ObservePickup(Rock);
	TestEqual(TEXT("Actual same-type pickup adds Mars exactly once"),D->GetRecord(Rock.ItemId)->Sources.Num(),2);
	auto Corpse=UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::MoonAntCorpse,1); Corpse.SpecimenId=TEXT("Riftlip"); D->Analyze(Corpse);
	TestTrue(TEXT("Analyzed species is known"),D->Knows(Corpse.ItemId,Corpse.SpecimenId));
	TestFalse(TEXT("Another species does not inherit identification"),D->Knows(Corpse.ItemId,TEXT("MirrorSkimmer")));
	auto* Save=NewObject<UJTSExpeditionSaveGame>(); Save->Snapshot.AnalyzedTypes=D->GetRecords(); Save->Snapshot.AnalysisSample.Item=Rock; Save->Snapshot.AnalysisSample.Elapsed=2;
	TArray<uint8> Bytes; TestTrue(TEXT("Knowledge and held sample serialize"),UGameplayStatics::SaveGameToMemory(Save,Bytes));
	auto* Loaded=Cast<UJTSExpeditionSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	TestTrue(TEXT("Cross-level envelope retains sources and actual sample identity"),Loaded && Loaded->Snapshot.AnalyzedTypes[0].Sources.Num()==2 && Loaded->Snapshot.AnalysisSample.Item.InstanceId==Rock.InstanceId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSAnalysisTransactionTest,"JTS.Analysis.GroundScanAtomicDepositAndTakeoff",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSAnalysisTransactionTest::RunTest(const FString&)
{
	FAnalysisWorld W; auto* D=W.GroundScanner(); auto* P=W.Player(D->GetGroundFrame().GetLocation()+FVector(0,-250,100));
	const FVector FloorLocation=D->GetGroundFrame().GetLocation();
	P->SetActorLocation(FloorLocation+FVector(0,0,90)); W.Advance(.2f);
	TestTrue(TEXT("A player standing in the scan zone cannot move its projection off the floor"),D->GetGroundFrame().GetLocation().Equals(FloorLocation,.1f));
	P->SetActorLocation(FloorLocation+FVector(0,-250,100));
	auto* I=P->GetInventoryComponent(); auto Item=UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::Rock,3); Item.ObservedPlanetIds={TEXT("Moon")}; Item.SampleSpanCm=20;
	int32 Remaining=0; I->TryAddItem(Item,Remaining); int32 Slot=INDEX_NONE;
	for(int32 N=0;N<I->GetItemSlots().Num();++N) if(I->GetItemAtSlot(N).ItemId==Item.ItemId) Slot=N;
	TestTrue(TEXT("Large samples take longer than small ones"),D->DurationForSpan(170)>D->DurationForSpan(12)*3);
	TestTrue(TEXT("A native spacecraft has its scanner without installing a module"),D->IsZoneActive() && W.Ship->GetDockComponent()->GetInstalledIds().IsEmpty());
	TestFalse(TEXT("Stale placement cannot extract a different inventory instance"),D->PlaceCarriedSample(P,Slot,FGuid::NewGuid()));
	P->SetActorLocation(FVector(0,5000,100)); TestFalse(TEXT("Remote placement fails server range validation"),D->PlaceCarriedSample(P,Slot,Item.InstanceId));
	P->SetActorLocation(D->GetGroundFrame().GetLocation()+FVector(0,-250,100));
	if(!TestTrue(TEXT("Placing an exact carried stack produces a real ground item"),D->PlaceCarriedSample(P,Slot,Item.InstanceId))) return false;
	W.Advance(.7f);
	TestEqual(TEXT("The whole stack leaves the backpack once"),I->GetItemCount(Item.ItemId),0);
	TestEqual(TEXT("Ground scan retains stack count"),D->GetScan().Sample.StackCount,3);
	TestTrue(TEXT("Server clock drives partial progress"),D->GetScanProgress()>0 && D->GetScanProgress()<1);
	auto* GroundItem=D->GetScan().Target.Get(); TestNotNull(TEXT("Original ground actor remains throughout analysis"),GroundItem);
	W.State->SetGameplayPhase(EJTSGameplayPhase::Launching); W.Advance(.3f);
	TestFalse(TEXT("Takeoff disables the scan zone"),D->IsZoneActive());
	TestTrue(TEXT("Takeoff leaves the real item on the ground"),IsValid(GroundItem) && GroundItem->IsAvailableForShipScan());
	TestEqual(TEXT("Interrupted analysis cannot deposit items"),W.Ship->GetResourceAmount(EJTSResourceType::Rock),0);
	W.State->SetGameplayPhase(EJTSGameplayPhase::MoonExploration); W.Advance(2.f);
	TestEqual(TEXT("Landing scans and deposits all units exactly once"),W.Ship->GetResourceAmount(EJTSResourceType::Rock),3);
	TestTrue(TEXT("The type is identified for the expedition"),W.State->GetDiscoveryComponent()->Knows(Item.ItemId));
	TestFalse(TEXT("Committed ground actor cannot be picked up during disassembly"),GroundItem->IsAvailableForShipScan());
	W.Advance(2.f); TestEqual(TEXT("Additional scan polls cannot duplicate storage"),W.Ship->GetResourceAmount(EJTSResourceType::Rock),3);
	D->CargoCapacity=1;
	auto Weapon=UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::ShortBlade,1);
	D->RestoreState({UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::PowerHammer,1)},{});
	auto* Equipment=W.World->SpawnActor<AJTSWorldPickupActor>(D->GetGroundFrame().GetLocation()+FVector(0,0,20),FRotator::ZeroRotator); Equipment->InitializeItemInstance(Weapon);
	W.Advance(2.f);
	TestTrue(TEXT("Full cargo retains the real ground item and exposes a blocked state"),Equipment->IsAvailableForShipScan() && D->GetScan().bStorageBlocked);
	TestEqual(TEXT("Full cargo cannot overwrite existing equipment"),D->GetCargo()[0].ItemId,EJTSItemId::PowerHammer);
	D->RestoreState({},{}); W.Advance(.2f);
	TestFalse(TEXT("Clearing space commits the waiting item once"),Equipment->IsAvailableForShipScan());
	TestEqual(TEXT("Equipment enters the shared ship cargo with its exact instance"),D->GetCargo()[0].InstanceId,Weapon.InstanceId);
	TestFalse(TEXT("Completed sample is absent from the pending save envelope"),!D->ExportPendingSample().Item.IsEmpty());
	W.Advance(1.5f);
	FJTSStoredAnalysisSample Legacy; Legacy.Item=UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::Ore,2); Legacy.Duration=5; Legacy.Elapsed=5; Legacy.bComplete=true;
	D->RestoreState(D->GetCargo(),Legacy); W.Advance(.4f);
	TestEqual(TEXT("An old completed table sample migrates and reaches storage exactly once"),W.Ship->GetResourceAmount(EJTSResourceType::Ore),2);
	W.Advance(1.5f);
	auto* CreatureClass=LoadClass<AJTSPlanetSurfaceCreature>(nullptr,TEXT("/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_MirrorSkimmer.BP_MirrorSkimmer_C"));
	auto* Creature=W.World->SpawnActor<AJTSPlanetSurfaceCreature>(CreatureClass,D->GetGroundFrame().GetLocation()+FVector(0,0,30),FRotator::ZeroRotator);
	Creature->DispatchBeginPlay(); Creature->FindComponentByClass<UJTSHealthComponent>()->ApplyDamage(10000,nullptr,P,true);
	W.Advance(6.f);
	TestEqual(TEXT("An animal that dies inside the zone is scanned through its original corpse after the death animation"),W.Ship->GetResourceAmount(EJTSResourceType::MoonAntCorpse),1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSGroundScanSphereTest,"JTS.Analysis.GroundZoneSphericalPlacementAndCancellation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSGroundScanSphereTest::RunTest(const FString&)
{
	FAnalysisWorld W;
	auto* Planet=W.World->SpawnActor<AJTSPlanetAnchor>(); Planet->SetActorLocation(FVector(30000,0,0));
	auto* Sphere=W.World->SpawnActor<AStaticMeshActor>(); auto* Mesh=Sphere->GetStaticMeshComponent(); Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"))); Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Sphere->SetActorLocation(Planet->GetActorLocation()); Sphere->SetActorScale3D(FVector(200));
	FindFProperty<FObjectProperty>(Planet->GetClass(),TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet,Sphere);
	FindFProperty<FFloatProperty>(Planet->GetClass(),TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet,10000);
	const FVector Up=FVector(1,1,1).GetSafeNormal();
	const FRotator Rotation=FRotationMatrix::MakeFromZX(Up,FVector(1,-1,0)).Rotator();
	W.Ship->SetActorLocation(Planet->GetActorLocation()+Up*10100); W.Ship->SetActorRotation(Rotation); W.Ship->SetGroundedPlanet(Planet);
	W.Ship->Scanner->SetRelativeLocation(FVector(0,320,0));
	W.Advance(.3f); auto* Scanner=W.Ship->Scanner.Get();
	if(!TestTrue(TEXT("Ground zone resolves a real tilted spherical surface"),Scanner->IsZoneActive())) return false;
	const FTransform Ground=Scanner->GetGroundFrame();
	FJTSPlanetSurfaceHit GroundHit;
	if(!TestTrue(TEXT("The real surface independently resolves the scanner location"),Planet->ProjectPointToSurface(Scanner->GetComponentLocation(),GroundHit))) return false;
	TestTrue(TEXT("Scanner up follows the actual mesh surface normal instead of world Z"),FVector::DotProduct(Ground.GetUnitAxis(EAxis::Z),GroundHit.ImpactNormal)>.99999);
	TestTrue(TEXT("The scan ring rests at the real mesh surface"),Ground.GetLocation().Equals(GroundHit.ImpactPoint,.1f));
	auto* Pawn=W.Player(Ground.GetLocation()-Ground.GetUnitAxis(EAxis::X)*220+Up*100);
	auto Item=UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::Rock,1); Item.SampleSpanCm=30;
	int32 Left=0; Pawn->GetInventoryComponent()->TryAddItem(Item,Left);
	if(!TestTrue(TEXT("A player can place a sample on the spherical scan zone"),Scanner->PlaceCarriedSample(Pawn,0,Item.InstanceId))) return false;
	W.Advance(.5f); auto* Target=Scanner->GetScan().Target.Get();
	if(!TestNotNull(TEXT("A real spherical pickup is being scanned"),Target)) return false;
	TestTrue(TEXT("Placed sample keeps real planet surface behavior"),Target->IsUsingRealPlanetSurface() && Target->GetSurfacePlanet()==Planet);
	FJTSPlanetSurfaceFrame SampleSurface;
	if(!TestTrue(TEXT("The real surface independently resolves the sample placement"),Planet->GetSurfaceFrameAt(Ground.GetLocation(),Ground.GetUnitAxis(EAxis::X),SampleSurface))) return false;
	TestTrue(TEXT("Placed sample follows its actual mesh surface normal"),FVector::DotProduct(Target->GetActorUpVector(),SampleSurface.Up)>.99999);
	TestEqual(TEXT("Pending save preserves the sample identity"),Scanner->ExportPendingSample().Item.InstanceId,Item.InstanceId);
	Target->SetActorLocation(Ground.GetLocation()+Ground.GetUnitAxis(EAxis::X)*500+Up*30); W.Advance(.2f);
	TestTrue(TEXT("Moving the item out of the ring cancels the pending scan without consuming it"),Scanner->GetScan().Sample.IsEmpty() && Target->IsAvailableForShipScan());
	TestTrue(TEXT("Cancelled sample cannot be duplicated by a save restore"),Scanner->ExportPendingSample().Item.IsEmpty());
	TestEqual(TEXT("Cancelled sample never entered shared storage"),W.Ship->GetResourceAmount(EJTSResourceType::Rock),0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSAnalysisInventoryTest,"JTS.Analysis.ProvenanceStacksAndEarthBypass",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSAnalysisInventoryTest::RunTest(const FString&)
{
	FAnalysisWorld W; auto* P=W.Player(W.Ship->GetBoardingInteractionCenter()); auto* I=P->GetInventoryComponent();
	auto A=UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::Rock,1); A.ObservedPlanetIds={TEXT("Moon")}; A.SampleSpanCm=12;
	auto B=A; B.InstanceId=FGuid::NewGuid(); B.ObservedPlanetIds={TEXT("Mars")}; B.SampleSpanCm=80; B.SampleVisualVariant=2;
	int32 Remaining=0; I->TryAddItem(A,Remaining); I->TryAddItem(B,Remaining);
	for(const auto& Item:I->GetItemSlots()) if(!Item.IsEmpty())
	{ TestEqual(TEXT("Merged unknown stack retains actual observed sources"),Item.ObservedPlanetIds.Num(),2); TestEqual(TEXT("Stack preserves representative sample size"),Item.SampleSpanCm,80.f); TestEqual(TEXT("Largest representative retains its mesh variant"),Item.SampleVisualVariant,2); }
	auto* Dropped=W.World->SpawnActor<AJTSWorldPickupActor>(); Dropped->InitializeItemInstance(B);
	TestTrue(TEXT("Putting an inventory sample back into the world retains its actual size"),FMath::IsNearlyEqual(Dropped->GetVisualBoundsExtent().GetMax()*2,80.f,.5f));
	TestTrue(TEXT("Earth context identifies all Earth gameplay items"),UJTSDiscoveryComponent::IsKnown(W.Ship,EJTSItemId::Rock));
	auto* Manager=W.World->SpawnActor<AJTSSpaceWorldManager>();
	TestFalse(TEXT("Space context enforces analysis before automatic delivery"),UJTSDiscoveryComponent::IsKnown(W.Ship,EJTSItemId::Rock));
	TestFalse(TEXT("Automatic deposit refuses unknown resources"),W.Ship->TryDepositResourcesFromPawn(P));
	TestEqual(TEXT("Unknown stack stays carried"),I->GetItemCount(EJTSItemId::Rock),2);
	W.State->GetDiscoveryComponent()->Analyze(A);
	TestTrue(TEXT("Analysis unlocks delivery for the entire type"),UJTSDiscoveryComponent::IsKnown(W.Ship,EJTSItemId::Rock));
	TestTrue(TEXT("Automatic deposit accepts the identified type"),W.Ship->TryDepositResourcesFromPawn(P));
	TestEqual(TEXT("Identified stack reaches shared storage"),W.Ship->GetResourceAmount(EJTSResourceType::Rock),2);
	Manager->Destroy();
	I->RestoreItems({},0);
	auto Corpse=UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::MoonAntCorpse,1); Corpse.SpecimenId=TEXT("Riftlip");
	I->TryAddItem(Corpse,Remaining); Corpse.InstanceId=FGuid::NewGuid(); Corpse.SpecimenId=TEXT("MirrorSkimmer"); I->TryAddItem(Corpse,Remaining);
	TestEqual(TEXT("Different species never merge into one indistinguishable stack"),I->GetUsedSlotCount(),2);
	Corpse.SpecimenId=TEXT("Riftlip"); TestTrue(TEXT("Capacity preflight matches the complete specimen payload"),I->CanAddItemInstance(Corpse));
	Corpse.SpecimenId=TEXT("SpireSoldier"); TestFalse(TEXT("Full inventory rejects a third species without a partial mutation"),I->CanAddItemInstance(Corpse));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSAnalysisReadoutInputTest,"JTS.Analysis.PersonalFieldGuideAndTagTooltip",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSAnalysisReadoutInputTest::RunTest(const FString&)
{
	FAnalysisWorld W; W.World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	auto* P=W.Player(FVector(0,50000,0));
	auto* PC=W.World->SpawnActor<AJTSPlayerController>(); PC->Possess(P); PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
	auto* Catalog=NewObject<UJTSAnalysisCatalog>(); FJTSAnalysisEntry Entry; Entry.ItemId=EJTSItemId::Rock;
	FJTSAnalysisTag Tag; Tag.Label=FText::FromString(TEXT("Material")); Tag.Explanation=FText::FromString(TEXT("Exchange at a shop"));
	Entry.Tags.Add(Tag); Catalog->Entries.Add(Entry); W.State->GetDiscoveryComponent()->SetCatalog(Catalog);
	W.State->GetDiscoveryComponent()->Analyze(UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::Rock,1));
	auto* Widget=CreateWidget<UJTSFieldGuideWidget>(PC,UJTSFieldGuideWidget::StaticClass());
	if(!TestNotNull(TEXT("Personal field guide constructs far away from any scanner"),Widget)) return false;
	TestEqual(TEXT("Every player's guide reads the replicated expedition catalog"),Widget->GetCatalog(),Catalog);
	TestTrue(TEXT("Slate can route pointer events to the field guide"),Widget->TakeWidget()->GetVisibility().IsHitTestVisible());
	const FGeometry Geometry=FGeometry::MakeRoot(FVector2D(1000,660),FSlateLayoutTransform());
	const FPointerEvent Hover(0,FVector2D(440,402),FVector2D(440,402),TSet<FKey>(),EKeys::Invalid,0,FModifierKeysState());
	Widget->NativeOnMouseMove(Geometry,Hover);
	TestEqual(TEXT("Hovering a usage tag exposes its explanation"),Widget->GetToolTipText().ToString(),Tag.Explanation.ToString());
	Widget->NativeOnMouseLeave(Hover);
	TestTrue(TEXT("Leaving a tag removes its explanation"),Widget->GetToolTipText().IsEmpty());
	const FPointerEvent Click(0,FVector2D(300,136),FVector2D(300,136),TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
	TestTrue(TEXT("Ship cargo tab consumes its pointer event"),Widget->NativeOnMouseButtonDown(Geometry,Click).IsEventHandled());
	TestEqual(TEXT("Reading the guide does not transfer or consume an item"),P->GetInventoryComponent()->GetItemCount(EJTSItemId::Rock)+W.Ship->GetResourceAmount(EJTSResourceType::Rock),0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSCorpseReuseTest,"JTS.Analysis.OriginalCreaturePartsAndStackIdentity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSCorpseReuseTest::RunTest(const FString&)
{
 FAnalysisWorld W;
 auto* Catalog=LoadObject<UJTSAnalysisCatalog>(nullptr,TEXT("/Game/Space/Ships/Analysis/DA_AnalysisCatalog.DA_AnalysisCatalog"));
 if(!TestNotNull(TEXT("Production catalog exists"),Catalog)) return false;
 W.State->GetDiscoveryComponent()->SetCatalog(Catalog);
 auto* P=W.Player(FVector(0,1200,0)); auto* Inventory=P->GetInventoryComponent();
 FindFProperty<FIntProperty>(Inventory->GetClass(),TEXT("BaseInventoryCapacity"))->SetPropertyValue_InContainer(Inventory,4);
 P->DispatchBeginPlay();
 Inventory->RestoreItems({},0);
 for(const auto& Entry:Catalog->Entries)
 {
  if(!Entry.SpecimenMesh) continue;
  TestNull(TEXT("Creature presentation has no separately authored corpse mesh"),Entry.SampleMesh.Get());
  TestNotNull(TEXT("Original death animation is reused"),Entry.DeathAnimation.Get());
  const auto Bounds=UJTSCorpseMeshComponent::GetPartBounds(Entry,W.World);
  if(!TestFalse(TEXT("Original geometry produces usable corpse components"),Bounds.IsEmpty())) continue;
  int32 TriangleCount=0; double SurfaceArea=0;
  auto Area=[](UJTSCorpseMeshComponent* Visual)
  {
   double Result=0;
   for(int32 Section=0;Section<Visual->GetNumSections();++Section)
   {
    const auto* Mesh=Visual->GetProcMeshSection(Section);
    for(int32 I=0;I<Mesh->ProcIndexBuffer.Num();I+=3)
    {
     const FVector A=Mesh->ProcVertexBuffer[Mesh->ProcIndexBuffer[I]].Position;
     const FVector B=Mesh->ProcVertexBuffer[Mesh->ProcIndexBuffer[I+1]].Position;
     const FVector C=Mesh->ProcVertexBuffer[Mesh->ProcIndexBuffer[I+2]].Position;
     Result+=FVector::CrossProduct(B-A,C-A).Size()*.5;
    }
   }
   return Result;
  };
  for(int32 Part=0;Part<Bounds.Num();++Part)
  {
   auto Item=UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::MoonAntCorpse,1);
   Item.SpecimenId=Entry.SpecimenId; Item.CorpsePartIndex=Entry.bSeparateCorpseParts?Part:INDEX_NONE;
   Item.SampleSpanCm=Bounds[Part].GetSize().GetMax();
   auto* Visual=NewObject<UJTSCorpseMeshComponent>(P); Visual->RegisterComponent();
   if(TestTrue(TEXT("Every original corpse component is renderable"),Visual->SetSample(Item,Catalog)))
   {
    for(int32 Section=0;Section<Visual->GetNumSections();++Section) TriangleCount+=Visual->GetProcMeshSection(Section)->ProcIndexBuffer.Num()/3;
    SurfaceArea+=Area(Visual);
   }
   Visual->PlaceAt(FVector::ZeroVector,true);
   TestTrue(TEXT("Ground/tray placement has its bottom at the support plane"),FMath::Abs(Visual->Bounds.GetBox().Min.Z)<.1f);
   Visual->DestroyComponent();
  }
  const int32 OriginalTriangles=Entry.SpecimenMesh->GetResourceForRendering()->LODRenderData[0].GetTotalFaces();
  if(Entry.bSeparateCorpseParts)
  {
   TestTrue(TEXT("Large boss separates into several portable original components"),Bounds.Num()>3);
   for(const auto& B:Bounds) TestTrue(TEXT("Boss pieces fit a carryable span"),B.GetSize().GetMax()<=Entry.MaxCorpsePartSpan+1.f);
   auto* WholeCatalog=NewObject<UJTSAnalysisCatalog>(); auto WholeEntry=Entry; WholeEntry.bSeparateCorpseParts=false; WholeCatalog->Entries.Add(WholeEntry);
   auto* WholeVisual=NewObject<UJTSCorpseMeshComponent>(P); WholeVisual->RegisterComponent();
   auto WholeItem=UJTSItemDefinitionLibrary::MakeInstance(Entry.ItemId,1); WholeItem.SpecimenId=Entry.SpecimenId;
   TestTrue(TEXT("Original whole boss remains available as source geometry"),WholeVisual->SetSample(WholeItem,WholeCatalog));
   const double WholeArea=Area(WholeVisual);
   TestTrue(TEXT("Splitting preserves exactly the original posed surface without loss or duplicate pieces"),WholeArea>0 && FMath::Abs(SurfaceArea-WholeArea)<WholeArea*.0001);
   WholeVisual->DestroyComponent();
  }
  else
  {
   TestEqual(TEXT("Small creature retains its whole authored death pose"),Bounds.Num(),1);
   TestEqual(TEXT("Small corpse retains the exact original triangles"),TriangleCount,OriginalTriangles);
  }
 }
 auto Add=[&](FName Species,int32 Part)
 {
  auto Item=UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::MoonAntCorpse,1); Item.SpecimenId=Species; Item.CorpsePartIndex=Part;
  int32 Remaining=0; TestTrue(TEXT("Corpse stack insert succeeds"),Inventory->TryAddItem(Item,Remaining)&&Remaining==0);
 };
 Add(TEXT("MirrorSkimmer"),INDEX_NONE); Add(TEXT("MirrorSkimmer"),INDEX_NONE);
 Add(TEXT("MirrorBoss"),0); Add(TEXT("MirrorBoss"),1); Add(TEXT("AnotherBoss"),0);
 int32 SmallSlots=0,BossSlots=0,OtherBossSlots=0;
 for(const auto& Item:Inventory->GetItemSlots())
 {
  if(Item.IsEmpty()) continue;
  if(Item.SpecimenId==TEXT("MirrorSkimmer")) { ++SmallSlots; TestEqual(TEXT("Same small species stacks"),Item.StackCount,2); }
  if(Item.SpecimenId==TEXT("MirrorBoss")) { ++BossSlots; TestEqual(TEXT("Different parts of the same boss type stack"),Item.StackCount,2); }
  if(Item.SpecimenId==TEXT("AnotherBoss")) ++OtherBossSlots;
 }
 TestEqual(TEXT("Small species uses one slot"),SmallSlots,1); TestEqual(TEXT("Boss species uses its separate slot"),BossSlots,1); TestEqual(TEXT("Another boss cannot merge into this boss"),OtherBossSlots,1);
 auto Part=UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::MoonAntCorpse,2); Part.SpecimenId=TEXT("MirrorBoss"); Part.CorpsePartIndex=1;
 Inventory->RestoreItems({Part},0);
 bool bHeld=false;
 TArray<UJTSCorpseMeshComponent*> Held; P->GetComponents(Held);
 for(auto* Visual:Held) if(Visual->GetName()==TEXT("HeldOriginalCorpse")) bHeld=Visual->IsVisible() && Visual->GetNumSections()>0;
 TestTrue(TEXT("Selecting a corpse component shows its original geometry in the hand"),bHeld);
 auto* Drop=W.World->SpawnActor<AJTSWorldPickupActor>(); Drop->InitializeItemInstance(Part);
 TestFalse(TEXT("A dropped corpse never displays the generic pickup mesh"),Drop->FindComponentByClass<UStaticMeshComponent>()->IsVisible());
 TestEqual(TEXT("Dropping retains the actual original part identity"),Drop->GetItemInstance().CorpsePartIndex,1);
 auto* Planet=W.World->SpawnActor<AJTSPlanetAnchor>(); Planet->SetActorLocation(FVector(30000,0,0));
 auto* Sphere=W.World->SpawnActor<AStaticMeshActor>(); Sphere->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
 Sphere->SetActorLocation(Planet->GetActorLocation()); Sphere->SetActorScale3D(FVector(200));
 Sphere->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"))); Sphere->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
 FindFProperty<FObjectProperty>(Planet->GetClass(),TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet,Sphere);
 FindFProperty<FFloatProperty>(Planet->GetClass(),TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet,10000);
 FJTSPlanetSurfaceHit Surface;
 if(TestTrue(TEXT("Corpse placement uses a real spherical surface"),Planet->ProjectPointToSurface(Planet->GetActorLocation()+FVector(1,1,1).GetSafeNormal()*10000,Surface)))
 {
  Drop->PlaceOnPlanetSurface(Planet,Surface.ImpactPoint,FVector::ForwardVector);
  FJTSSurfaceVisualProjectionBounds Projection;
  auto* Visual=Drop->FindComponentByClass<UJTSCorpseMeshComponent>();
  JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(Visual,Surface.ImpactPoint,Drop->GetActorUpVector(),Projection);
  TestTrue(TEXT("Original corpse rests on the surface using its actual geometry rather than the hidden generic mesh"),Projection.bIsValid && FMath::Abs(Projection.LowestProjectionFromRoot-JTSSurfacePlacementBounds::DefaultSurfaceClearance)<.1f);
 }
 auto* Save=NewObject<UJTSExpeditionSaveGame>(); Save->Snapshot.AnalysisSample.Item=Part;
 TArray<uint8> Bytes; UGameplayStatics::SaveGameToMemory(Save,Bytes);
 auto* Loaded=Cast<UJTSExpeditionSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
 TestTrue(TEXT("Save/travel retains boss species, component and stack count"),Loaded && Loaded->Snapshot.AnalysisSample.Item.SpecimenId==Part.SpecimenId && Loaded->Snapshot.AnalysisSample.Item.CorpsePartIndex==1 && Loaded->Snapshot.AnalysisSample.Item.StackCount==2);
 Drop->Destroy();
 auto* BossClass=LoadClass<AJTSPlanetBoss>(nullptr,TEXT("/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_MirrorBoss.BP_MirrorBoss_C"));
 auto* Boss=W.World->SpawnActor<AJTSPlanetBoss>(BossClass,FVector(3000,0,0),FRotator::ZeroRotator);
 if(!TestNotNull(TEXT("Production boss spawns for a real death/collection transaction"),Boss)) return false;
 Boss->DispatchBeginPlay(); Boss->Health->ApplyDamage(100000,nullptr,P,true);
 W.Advance(Boss->Combat->GetActionDuration()+.3f);
 const auto* BossEntry=Catalog->Find(EJTSItemId::MoonAntCorpse,TEXT("MirrorBoss"));
 const int32 ExpectedParts=UJTSCorpseMeshComponent::GetPartBounds(*BossEntry,W.World).Num();
 int32 DroppedParts=0; AJTSWorldPickupActor* FirstPart=nullptr;
 for(TActorIterator<AJTSWorldPickupActor> It(W.World);It;++It)
  if(!It->IsActorBeingDestroyed() && It->GetItemInstance().SpecimenId==TEXT("MirrorBoss"))
  { ++DroppedParts; FirstPart=*It; TestTrue(TEXT("Every separated body component remains for collection"),It->GetLifeSpan()>1700.f); }
 TestEqual(TEXT("Boss death produces exactly its original portable components"),DroppedParts,ExpectedParts);
 TestFalse(TEXT("Whole boss disappears only after all body components exist"),Boss->Mesh->IsVisible());
 TestFalse(TEXT("The whole boss cannot also be collected as a miniature"),Boss->CanInteract_Implementation(P));
 if(FirstPart)
 {
  Inventory->RestoreItems({},0); P->SetActorLocation(FirstPart->GetActorLocation()+FVector(0,0,80));
  FirstPart->Interact_Implementation(P);
  TestEqual(TEXT("The original separated component can be carried through normal server pickup"),Inventory->GetItemCount(EJTSItemId::MoonAntCorpse),1);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSNestResearchGateTest,"JTS.Analysis.NestResearchGatesDamageOnly",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSNestResearchGateTest::RunTest(const FString&)
{
 FAnalysisWorld W;
 auto* Nest=W.World->SpawnActor<AJTSPlanetEcologyHabitat>(FVector(300,4000,0),FRotator::ZeroRotator);
 Nest->RequiredAnalyzedSpecimen=TEXT("MirrorSkimmer"); Nest->bDestructibleEntrance=true; Nest->DispatchBeginPlay();
 auto* P=W.Player(FVector(0,4000,0)); P->DispatchBeginPlay();
 TestTrue(TEXT("Unknown nest remains physically hittable"),Nest->CanReceiveMeleeHit_Implementation(P));
 TestEqual(TEXT("Unknown nest name conceals species"),Nest->GetMeleeTargetDisplayName_Implementation().ToString(),FString(TEXT("???的巢")));
 TestTrue(TEXT("The missing analysis is explained"),Nest->GetMeleeTargetPrompt_Implementation(P).ToString().Contains(TEXT("分析")));
 auto* Health=Nest->FindComponentByClass<UJTSHealthComponent>(); const float Before=Health->GetHealth();
 FDamageEvent Event; TestEqual(TEXT("Server rejects direct/projectile/area damage before research"),Nest->TakeDamage(500,Event,nullptr,P),0.f);
 Nest->ReceiveMeleeHit_Implementation(P,EJTSMeleeAttackType::Tool);
 TestEqual(TEXT("Direct ability/execution damage cannot bypass research"),Health->ApplyDamage(500,nullptr,P,true),0.f);
 TestEqual(TEXT("Unknown nest health does not change"),Health->GetHealth(),Before);
 auto* Proxy=W.World->SpawnActor<AJTSPlanetSurfaceAttachment>(); Proxy->AttachToActor(Nest,FAttachmentTransformRules::KeepWorldTransform);
 TestFalse(TEXT("Physical surface proxy allows attack animations before research"),Proxy->BlocksAttackInitiation_Implementation(P));
 TestTrue(TEXT("Physical surface proxy retains the damage research lock"),Proxy->BlocksDamage_Implementation(P));
 TestEqual(TEXT("Hitting the physical proxy cannot bypass server damage validation"),Proxy->TakeDamage(500,Event,nullptr,P),0.f);
 auto* Target=NewObject<UBoxComponent>(Nest); Target->SetupAttachment(Nest->GetRootComponent()); Target->SetBoxExtent(FVector(60)); Target->SetCollisionResponseToAllChannels(ECR_Ignore); Target->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block); Target->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Target->RegisterComponent();
 FVector Origin,Direction; auto* View=P->FindComponentByClass<UJTSRangedWeaponComponent>(); View->GetCameraRay(Origin,Direction);
 Target->SetWorldLocation(Origin+Direction*220.f);
 auto* Melee=P->FindComponentByClass<UJTSMeleeComponent>();
 TestNull(TEXT("Unknown nest never blocks attack input"),Melee->GetAttackBlockerInAim());
 Melee->StartAttack(); TestTrue(TEXT("Player can swing at an unknown nest"),Melee->IsAttacking()); Melee->StopAttack();
 auto Sample=UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::MoonAntCorpse,1); Sample.SpecimenId=TEXT("MirrorBoss"); W.State->GetDiscoveryComponent()->Analyze(Sample);
 TestFalse(TEXT("Analyzing the boss does not unlock its small relative's nest"),Nest->IsSpeciesAnalyzed());
 Sample.SpecimenId=TEXT("MirrorSkimmer"); W.State->GetDiscoveryComponent()->Analyze(Sample);
 TestTrue(TEXT("Shared mirror creature research unlocks nest work immediately"),Nest->CanReceiveMeleeHit_Implementation(P));
 TestFalse(TEXT("Attack initiation is no longer blocked after analysis"),Nest->BlocksAttackInitiation_Implementation(P));
 TestNull(TEXT("Aim releases the attack lock immediately"),Melee->GetAttackBlockerInAim());
 auto* Catalog=NewObject<UJTSAnalysisCatalog>(); FJTSAnalysisEntry E; E.ItemId=Sample.ItemId; E.SpecimenId=Sample.SpecimenId; E.Name=FText::FromString(TEXT("镜虫")); Catalog->Entries.Add(E); W.State->GetDiscoveryComponent()->SetCatalog(Catalog);
 TestEqual(TEXT("Only identified destructible nests advertise destruction"),Nest->GetMeleeTargetDisplayName_Implementation().ToString(),FString(TEXT("镜虫的巢（可破坏）")));
 Melee->StartAttack(); TestTrue(TEXT("The same input starts a real swing after research"),Melee->IsAttacking()); Melee->StopAttack();
 auto* Special=W.World->SpawnActor<AJTSPlanetEcologyHabitat>(FVector(300,5000,0),FRotator::ZeroRotator);
 Special->RequiredAnalyzedSpecimen=TEXT("UnidentifiedSpecies"); Special->bCanBlockEntrance=true;
 Special->RefugeMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
 Special->DispatchBeginPlay();
 TestEqual(TEXT("Unknown marked entrance rejects all weapon damage"),Special->TakeDamage(1000.f,Event,nullptr,P),0.f);
 TestEqual(TEXT("Unknown marked entrance loses no durability"),Special->Blockade->GetProgress(),0.f);
 Special->RequiredAnalyzedSpecimen=TEXT("MirrorSkimmer");
 TestTrue(TEXT("Authored entrance accepts weapon damage targeting"),Special->CanBeDamaged());
 TestTrue(TEXT("Cone, beam and elemental weapons can acquire the identified entrance"),Special->FindComponentByClass<UJTSStellarTargetComponent>()->IsAliveTarget());
 const EJTSMeleeAttackType Types[]={EJTSMeleeAttackType::Knife,EJTSMeleeAttackType::Axe,EJTSMeleeAttackType::Tool,EJTSMeleeAttackType::Improvised};
 for (auto Type:Types)
 {
  Special->Blockade->ResetTransientState();
  Special->ReceiveMeleeHit_Implementation(P,Type);
  TestTrue(TEXT("Each melee weapon decreases authored entrance durability"),Special->Blockade->GetProgress()>0.f);
 }
 Special->Blockade->ResetTransientState();
 FHitResult Point(Special,Special->RefugeMesh,FVector::ZeroVector,FVector::UpVector);
 TestTrue(TEXT("Hitscan/projectile point damage decreases entrance durability"),UGameplayStatics::ApplyPointDamage(Special,10.f,FVector::ForwardVector,Point,nullptr,P,nullptr)>0);
 const float AfterBullet=Special->Blockade->GetProgress();
 TestTrue(TEXT("Generic beam and blast damage reaches the same durability"),UGameplayStatics::ApplyDamage(Special,10.f,nullptr,P,nullptr)>0 && Special->Blockade->GetProgress()>AfterBullet);
 auto* Surface=W.World->SpawnActor<AJTSPlanetSurfaceAttachment>(); Surface->AttachToActor(Special,FAttachmentTransformRules::KeepWorldTransform);
 const float BeforeProxy=Special->Blockade->GetProgress();
 TestTrue(TEXT("Hitting the visible surface forwards damage to the authored entrance"),Surface->TakeDamage(10.f,Event,nullptr,P)>0 && Special->Blockade->GetProgress()>BeforeProxy);
 auto* Peripheral=W.World->SpawnActor<AJTSPlanetEcologyHabitat>(); Peripheral->RequiredAnalyzedSpecimen=TEXT("MirrorSkimmer"); Peripheral->DispatchBeginPlay();
 TestEqual(TEXT("Research never makes a peripheral refuge destructible"),Peripheral->TakeDamage(1000.f,Event,nullptr,P),0.f);
 TestFalse(TEXT("Peripheral refuge name has no destructible label"),Peripheral->GetMeleeTargetDisplayName_Implementation().ToString().Contains(TEXT("可破坏")));
 TestFalse(TEXT("Peripheral refuge cannot enter elemental target queries"),Peripheral->FindComponentByClass<UJTSStellarTargetComponent>()->IsAliveTarget());
 Special->FindComponentByClass<UJTSStellarTargetComponent>()->ApplyFire(P,101.f,0.f,0);
 W.Advance(2.2f);
 TestTrue(TEXT("Elemental ignition damages entrance durability instead of executing its legacy health"),Special->Blockade->GetProgress()>=1.f);
 TestTrue(TEXT("All weapon damage retains the existing entrance collapse transaction"),Special->IsEntranceBlocked());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSCorpseCarryPickupTest,"JTS.Analysis.CorpsePickupAndSizeAwareCarry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSCorpseCarryPickupTest::RunTest(const FString&)
{
 FAnalysisWorld W;
 auto* Catalog=LoadObject<UJTSAnalysisCatalog>(nullptr,TEXT("/Game/Space/Ships/Analysis/DA_AnalysisCatalog.DA_AnalysisCatalog"));
 W.State->GetDiscoveryComponent()->SetCatalog(Catalog);
 auto* PlayerClass=LoadClass<AJTSCharacter>(nullptr,TEXT("/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2.BP_JTSPlayer_Casual_2_C"));
 auto* P=W.World->SpawnActor<AJTSCharacter>(PlayerClass,FVector(-150,8000,100),FRotator::ZeroRotator); P->DispatchBeginPlay();
 P->SetActorTickEnabled(false); auto* Inventory=P->GetInventoryComponent(); Inventory->RestoreItems({},0);
 auto* Visual=P->FindComponentByClass<UJTSWeaponVisualComponent>();
 int32 SmallCount=0,LargeCount=0;
 for (const auto& E:Catalog->Entries)
 {
  if(!E.SpecimenMesh) continue;
  auto Sample=UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::MoonAntCorpse,1); Sample.SpecimenId=E.SpecimenId;
  Inventory->RestoreItems({},0); int32 Remaining=0;
  TestTrue(TEXT("Original specimen can be collected"),Inventory->TryAddItem(Sample,Remaining));
  TestEqual(TEXT("Collected corpse is automatically selected for carrying"),Inventory->GetActiveItem().SpecimenId,E.SpecimenId);
  TestTrue(TEXT("Collecting a specimen immediately displays its original geometry in the hands"),Visual->IsCorpseCarryVisible());
  Visual->RefreshForAttack();
  const FVector Size=Visual->GetCarriedCorpseSize();
  const FVector RightHand=P->GetMesh()->GetBoneLocation(TEXT("Wrist_R"));
  const FVector LeftHand=P->GetMesh()->GetBoneLocation(TEXT("Wrist_L"));
  TestTrue(TEXT("Both carry poses bring the right hand in front of the body"),FVector::DotProduct(RightHand-P->GetActorLocation(),P->GetActorForwardVector())>15.f);
  TestTrue(TEXT("Only a large corpse brings the left hand forward to support it"),Visual->IsTwoHandCorpseCarry()
    ? FVector::DotProduct(LeftHand-P->GetActorLocation(),P->GetActorForwardVector())>15.f
    : FVector::DotProduct(LeftHand-P->GetActorLocation(),P->GetActorForwardVector())<10.f);
  AddInfo(FString::Printf(TEXT("Carry specimen %s original size %s: %s"),*E.SpecimenId.ToString(),*Size.ToString(),Visual->IsTwoHandCorpseCarry()?TEXT("two hands"):TEXT("right hand")));
  if(Visual->IsTwoHandCorpseCarry()) ++LargeCount; else ++SmallCount;
  TArray<UJTSCorpseMeshComponent*> Meshes; P->GetComponents(Meshes);
  for(auto* M:Meshes) if(M->GetName()==TEXT("HeldOriginalCorpse"))
  {
   TestTrue(TEXT("Carry preserves the original real-world corpse scale"),M->GetComponentScale().Equals(FVector::OneVector,.001f));
   TestTrue(TEXT("Held corpse remains within arm reach"),FVector::Dist(M->Bounds.Origin,P->GetActorLocation())<150.f);
   if(!Visual->IsTwoHandCorpseCarry()) TestTrue(TEXT("Small corpse stays in the right-hand grip"),FVector::Dist(M->Bounds.Origin,RightHand)<20.f);
  }
  P->GetMesh()->SetVisibility(false,true);
  TestFalse(TEXT("Boarding hides both carry styles with the player mesh"),Visual->IsCorpseCarryVisible());
  P->GetMesh()->SetVisibility(true,false); Visual->RestoreAfterCharacterMeshShown();
  TestTrue(TEXT("Disembarking restores the corpse instead of the generic tool visual"),Visual->IsCorpseCarryVisible());
 }
 TestTrue(TEXT("Production creatures include graspable right-hand specimens"),SmallCount>0);
 TestTrue(TEXT("Larger production specimens use the two-arm cradle"),LargeCount>0);
 Inventory->RestoreItems({},0);
 auto* Floor=W.World->SpawnActor<AStaticMeshActor>(); auto* FM=Floor->GetStaticMeshComponent(); FM->SetMobility(EComponentMobility::Movable);
 FM->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"))); FM->SetCollisionProfileName(TEXT("BlockAll"));
 Floor->SetActorLocation(FVector(0,8000,-55)); Floor->SetActorScale3D(FVector(20,20,1));
 auto* CreatureClass=LoadClass<AJTSPlanetSurfaceCreature>(nullptr,TEXT("/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_MirrorSkimmer.BP_MirrorSkimmer_C"));
 auto* Creature=W.World->SpawnActor<AJTSPlanetSurfaceCreature>(CreatureClass,FVector(0,8000,-20),FRotator::ZeroRotator); Creature->DispatchBeginPlay();
 auto* CreatureMesh=Creature->FindComponentByClass<USkeletalMeshComponent>(); CreatureMesh->SetRelativeLocation(FVector(0,0,50)); CreatureMesh->UpdateBounds();
 Creature->FindComponentByClass<UJTSHealthComponent>()->ApplyDamage(100000.f,nullptr,P,true);
 TestTrue(TEXT("A visible dead creature exposes its collection prompt"),Creature->CanInteract_Implementation(P));
 auto* Wall=W.World->SpawnActor<AStaticMeshActor>(); auto* WM=Wall->GetStaticMeshComponent(); WM->SetMobility(EComponentMobility::Movable);
 WM->SetStaticMesh(FM->GetStaticMesh()); WM->SetCollisionProfileName(TEXT("BlockAll")); Wall->SetActorScale3D(FVector(.3,2,3));
 Wall->SetActorLocation((P->GetPawnViewLocation()+Creature->GetCorpseInteractionLocation())*.5f);
 Creature->Interact_Implementation(P);
 TestEqual(TEXT("Collection still rejects an intervening solid wall"),Inventory->GetItemCount(EJTSItemId::MoonAntCorpse),0);
 Wall->Destroy();
 Creature->Interact_Implementation(P);
 TestEqual(TEXT("Visible corpse is collectible even when its old body centre is below the floor"),Inventory->GetItemCount(EJTSItemId::MoonAntCorpse),1);
 TestTrue(TEXT("Successful collection consumes the world corpse once"),Creature->IsActorBeingDestroyed());
 return true;
}
#endif
