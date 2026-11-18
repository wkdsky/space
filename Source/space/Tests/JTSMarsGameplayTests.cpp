#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/NetDriver.h"
#include "HAL/IConsoleManager.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "UObject/UnrealType.h"
#include "space/Core/JTSGameState.h"
#include "space/Core/JTSExpeditionTypes.h"
#include "space/Components/JTSFacilityConditionComponent.h"
#include "space/Components/JTSBossPhaseComponent.h"
#include "space/Characters/JTSPlanetBoss.h"
#include "space/Components/JTSPlanetSurfaceSteeringComponent.h"
#include "space/World/JTSNavigationStage.h"
#include "space/World/JTSInteriorZone.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSNestBossEncounter.h"
#include "space/World/JTSPlanetEcologyHabitat.h"
#include "space/World/JTSSpaceEncounterUnit.h"
#include "space/World/JTSFacilityHazard.h"
#include "space/World/JTSBreakableObstacle.h"
#include "space/World/JTSInteriorTransit.h"
#include "space/World/JTSFacilityController.h"
#include "space/World/JTSAutomaticAccessDoor.h"
#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "space/Components/JTSCreatureTacticsComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Systems/JTSCoordinateSettings.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/Systems/JTSExpeditionSaveGame.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Components/JTSInteriorStateComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Interaction/InteractionComponent.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMarsProgressionRules,"JTS.Mars.ProgressionRules",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSMarsProgressionRules::RunTest(const FString&)
{
    const auto* Settings=GetDefault<UJTSCoordinateSettings>();
    const auto* Access=Settings->KeyCoordinateGrants.Find(TEXT("MarsEcologyAccess"));
    TestTrue(TEXT("Project configuration grants both the planet and station"),Access && Access->CoordinateIds.Contains(TEXT("Mars")) && Access->CoordinateIds.Contains(TEXT("Mars_Station")));
    const auto* Collar=Settings->KeyCoordinateGrants.Find(TEXT("Mars_TrackingCollar"));
    TestTrue(TEXT("Collar coordinates load from actual project INI"),Collar && Collar->CoordinateIds.Contains(TEXT("Mars_Noctis")));
    TestEqual(TEXT("Original resource values retain save identity"),int32(EJTSResourceType::InorganicCarbon),8);
    TestEqual(TEXT("Special materials append"),int32(EJTSResourceType::ManufacturedPart),9);
    for (auto R:{EJTSResourceType::ManufacturedPart,EJTSResourceType::HighGradeAlloy}) { EJTSResourceType Back; TestTrue(TEXT("Material is an inventory resource"),UJTSItemDefinitionLibrary::TryGetResourceType(UJTSItemDefinitionLibrary::GetItemIdForResource(R),Back)); TestEqual(TEXT("Resource round trip"),Back,R); }
    TestFalse(TEXT("Two badges do not open a three-badge door"),UJTSFacilityConditionComponent::EvaluateCount(2,4,EJTSConditionMode::AtLeast,3));
    TestTrue(TEXT("Any three badges suffice"),UJTSFacilityConditionComponent::EvaluateCount(3,4,EJTSConditionMode::AtLeast,3));
    TestTrue(TEXT("Two of three nests suffice"),UJTSFacilityConditionComponent::EvaluateCount(2,3,EJTSConditionMode::AtLeast,2));
    TestFalse(TEXT("An empty threshold cannot unlock"),UJTSFacilityConditionComponent::EvaluateCount(0,0,EJTSConditionMode::AtLeast,1));
    TestTrue(TEXT("Unconditional transit remains usable"),UJTSFacilityConditionComponent::EvaluateCount(0,0,EJTSConditionMode::All,1));
    TArray<FJTSBossPhase> P; for(float T:{1.f,.66f,.33f}) { FJTSBossPhase V; V.EnterBelowHealth=T; P.Add(V); }
    TestEqual(TEXT("First phase"),UJTSBossPhaseComponent::SelectPhase(P,1,INDEX_NONE),0);
    TestEqual(TEXT("Threshold crossing"),UJTSBossPhaseComponent::SelectPhase(P,.65f,0),1);
    TestEqual(TEXT("Burst damage crosses multiple phases"),UJTSBossPhaseComponent::SelectPhase(P,.2f,0),2);
    TestEqual(TEXT("Healing never reverses phase"),UJTSBossPhaseComponent::SelectPhase(P,.9f,2),2);
    TestEqual(TEXT("Near any planet excludes space encounters"),FJTSNavigationStageRules::Classify(true,10,100,EJTSNavigationMapStage::System,.1f),EJTSNavigationMapStage::Surface);
    TestEqual(TEXT("Exit hysteresis avoids boundary flicker"),FJTSNavigationStageRules::Classify(false,105,100,EJTSNavigationMapStage::System,.1f),EJTSNavigationMapStage::System);
    TestEqual(TEXT("Outside extent is interstellar"),FJTSNavigationStageRules::Classify(false,120,100,EJTSNavigationMapStage::System,.1f),EJTSNavigationMapStage::Interstellar);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMarsIndoorProjection,"JTS.Mars.IndoorFloorAndNestThreshold",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSMarsIndoorProjection::RunTest(const FString&)
{
    const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    W->InitializeActorsForPlay(FURL());
    auto* P=W->SpawnActor<AJTSPlanetAnchor>(FVector(100000,200000,300000),FRotator::ZeroRotator);
    const FVector Ground=P->GetActorLocation()+FVector(8000,0,0);
    auto* Floor=W->SpawnActor<AStaticMeshActor>();Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Floor->SetActorLocation(Ground-FVector(20,0,0));Floor->SetActorScale3D(FVector(.4,20,20));Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    auto* Z=W->SpawnActor<AJTSInteriorZone>(Ground+FVector(300,0,0),FRotator(0,0,0));Z->Bounds->SetBoxExtent(FVector(350,1000,1000));Z->Planet=P;Z->FloorActors.Add(Floor);
    TestTrue(TEXT("Pocket query is inside its configured bounds"),Z->Contains(Ground+FVector(50,0,0)));
    TestEqual(TEXT("Pocket lookup selects this zone"),AJTSInteriorZone::FindAt(W,Ground+FVector(50,0,0)),Z);
    FJTSPlanetSurfaceHit Hit;
    TestTrue(TEXT("An indoor floor works without a celestial surface mesh"),AJTSInteriorZone::ProjectGameplayGround(P,Ground+FVector(50,0,0),Hit));
    TestTrue(TEXT("Projection stays at the pocket, not approximate outer radius"),Hit.ImpactPoint.Equals(Ground,.5));
    TestFalse(TEXT("Outside indoor bounds never uses an unrelated room floor"),AJTSInteriorZone::ProjectGameplayGround(P,Ground+FVector(0,3000,0),Hit));
    auto* E=W->SpawnActor<AJTSNestBossEncounter>(); E->MinimumDestroyedNests=2;
    for (int I=0;I<3;++I) { auto* N=W->SpawnActor<AJTSPlanetEcologyHabitat>();N->bDestructibleEntrance=true;E->Nests.Add(N); }
    E->Nests[0]->RestoreDestroyedEntrance();TestFalse(TEXT("One nest cannot open gate"),E->AreAllNestsDestroyed());E->Nests[1]->RestoreDestroyedEntrance();TestTrue(TEXT("Second nest opens gate while third stays live"),E->AreAllNestsDestroyed());
    W->DestroyWorld(false);GEngine->DestroyWorldContext(W); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMarsInteriorRestore,"JTS.Mars.FourPlayerInteriorRestore",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSMarsInteriorRestore::RunTest(const FString&)
{
    const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    auto* GI=NewObject<UGameInstance>(GEngine);W->SetGameInstance(GI);W->InitializeActorsForPlay(FURL());
    auto* E=NewObject<UJTSExpeditionSubsystem>(GI);
    auto* P=W->SpawnActor<AJTSPlanetAnchor>();
    auto* Zone=W->SpawnActor<AJTSInteriorZone>(FVector(8000,0,0),FRotator::ZeroRotator);Zone->ZoneId=TEXT("TestPocket");Zone->Planet=P;Zone->Bounds->SetBoxExtent(FVector(1000,1000,1000));
    auto* State=W->SpawnActor<AJTSGameState>();W->SetGameState(State);State->SetGameplayPhase(EJTSGameplayPhase::SpaceFlight);State->SetCurrentPlanetId(P->GetPlanetId().ToString());
    TArray<AJTSCharacter*> Characters;TArray<AJTSPlayerState*> PlayerStates;
    for (int32 I=0;I<4;++I)
    {
        auto* PC=W->SpawnActor<APlayerController>();auto* PS=W->SpawnActor<AJTSPlayerState>();PS->SetPlayerId(I+1);PS->SetOwner(PC);PC->SetPlayerState(PS);State->AddPlayerState(PS);
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* C=W->SpawnActor<AJTSCharacter>(FVector(I%2?8000:14000,I*250,0),FRotator::ZeroRotator,Params);PC->Possess(C);Characters.Add(C);PlayerStates.Add(PS);
    }
    E->CaptureWorldState(State,nullptr);
    auto* Envelope=NewObject<UJTSExpeditionSaveGame>();Envelope->Snapshot=E->GetSnapshot();TArray<uint8> Bytes;
    TestTrue(TEXT("Real save envelope serializes four player snapshots"),UGameplayStatics::SaveGameToMemory(Envelope,Bytes));
    auto* Loaded=Cast<UJTSExpeditionSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (TestNotNull(TEXT("Real save envelope reloads"),Loaded))
    {
        E->GetMutableSnapshot()=Loaded->Snapshot;
        TestEqual(TEXT("All four players captured"),Loaded->Snapshot.Players.Num(),4);
        for(int32 I=0;I<4;++I)
        {
            Characters[I]->SetActorLocation(FVector(14000,I*250,0));E->RestorePlayerState(PlayerStates[I],Characters[I]);
            TestEqual(TEXT("Only the saved underground players return to the pocket"),Zone->Contains(Characters[I]->GetActorLocation()),bool(I%2));
            if(I%2) TestEqual(TEXT("Replicated interior state is restored"),Characters[I]->FindComponentByClass<UJTSInteriorStateComponent>()->Zone.Get(),Zone);
        }
    }
    W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMarsBossFlip,"JTS.Mars.BossCooperativeFlip",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSMarsBossFlip::RunTest(const FString&)
{
    const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());
    auto* GS=W->SpawnActor<AJTSGameState>();W->SetGameState(GS);
    auto* Planet=W->SpawnActor<AJTSPlanetAnchor>();auto* Boss=W->SpawnActor<AJTSPlanetBoss>();Boss->InitializeOnPlanet(Planet);
    auto* Phase=Boss->FindComponentByClass<UJTSBossPhaseComponent>();Phase->PhaseIndex=0;Phase->bCooperativeFlip=true;Boss->Combat->HoldStagger(10);
    TArray<AJTSCharacter*> Players;TArray<AJTSPlayerState*> States;
    for(int32 I=0;I<4;++I)
    {
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Players.Add(W->SpawnActor<AJTSCharacter>(FVector(120,I*65,0),FRotator::ZeroRotator,Params));States.Add(W->SpawnActor<AJTSPlayerState>());GS->AddPlayerState(States.Last());
    }
    TestEqual(TEXT("Multiplayer uses simultaneous contributions"),Phase->FlipHoldSeconds(),0.f);
    Phase->ContributeFlip(Players[0]);Phase->ContributeFlip(Players[0]);TestEqual(TEXT("One player cannot contribute twice"),Phase->FlippedUntil,0.);
    Phase->ContributeFlip(Players[1]);TestTrue(TEXT("Two distinct nearby players flip a staggered boss"),Phase->FlippedUntil>W->GetTimeSeconds());TestEqual(TEXT("Flipped belly takes triple damage"),Phase->GetDamageMultiplier(),3.f);
    TestTrue(TEXT("A successful flip visibly turns the shell upside down"),Boss->Mesh->GetRelativeRotation().Quaternion().Equals(FRotator(0,0,180).Quaternion(),.01f));
    Phase->FlippedUntil=0;Players[1]->SetActorLocation(FVector(2000,0,0));Phase->ContributeFlip(Players[1]);Phase->ContributeFlip(Players[0]);TestEqual(TEXT("A distant contributor cannot complete a flip"),Phase->FlippedUntil,0.);
    for(int32 I=1;I<4;++I)GS->RemovePlayerState(States[I]);
    TestEqual(TEXT("Solo alternative holds for three seconds"),Phase->FlipHoldSeconds(),3.f);Phase->ContributeFlip(Players[0]);TestTrue(TEXT("Solo completion flips the boss"),Phase->FlippedUntil>W->GetTimeSeconds());
    W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMarsSpaceProjectiles,"JTS.Mars.SpaceProjectileDamage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSMarsSpaceProjectiles::RunTest(const FString&)
{
    const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());W->BeginPlay();W->GetWorldSettings()->NotifyBeginPlay();
    auto* Ray=W->SpawnActor<AJTSSpaceEncounterUnit>();Ray->bLightSailRay=true;Ray->Health->SetMaxHealth(45,true);
    FPointDamageEvent Point;Point.HitInfo.ImpactPoint=Ray->Mesh->GetComponentTransform().TransformPosition(FVector(0,0,0));
    Ray->TakeDamage(18,Point,nullptr,nullptr);TestEqual(TEXT("Pulse laser body hit removes eighteen health"),Ray->Health->GetHealth(),27.f);
    Point.HitInfo.ImpactPoint=Ray->Mesh->GetComponentTransform().TransformPosition(FVector(0,25,0));Ray->TakeDamage(18,Point,nullptr,nullptr);
    TestTrue(TEXT("Membrane hit destroys a fragile ray and creates salvage"),Ray->bIsSalvage);
    auto* Owner=W->SpawnActor<AActor>();FActorSpawnParameters Params;Params.Owner=Owner;
    auto* Rock=W->SpawnActor<AJTSSpaceEncounterUnit>(FVector(3000,0,0),FRotator::ZeroRotator,Params);Rock->TakeDamage(999,FDamageEvent(),nullptr,nullptr);
    auto Children=[&](){TArray<AJTSSpaceEncounterUnit*> Result;for(TActorIterator<AJTSSpaceEncounterUnit> It(W);It;++It)if(It->GetOwner()==Owner)Result.Add(*It);return Result;};
    auto Medium=Children();TestEqual(TEXT("A large asteroid splits into two medium units"),Medium.Num(),2);for(auto* A:Medium){TestEqual(TEXT("Medium tier"),A->SizeTier,1);A->TakeDamage(999,FDamageEvent(),nullptr,nullptr);}
    auto Small=Children();TestEqual(TEXT("Medium asteroids split into four small units"),Small.Num(),4);for(auto* A:Small){TestEqual(TEXT("Small tier"),A->SizeTier,0);A->TakeDamage(999,FDamageEvent(),nullptr,nullptr);}
    auto Salvage=Children();TestEqual(TEXT("Small fragments become four salvage pickups"),Salvage.Num(),4);for(auto* A:Salvage)TestTrue(TEXT("Salvage no longer takes damage"),A->bIsSalvage && A->TakeDamage(18,FDamageEvent(),nullptr,nullptr)==0);
    W->EndPlay(EEndPlayReason::Quit);W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMarsIceAmbush,"JTS.Mars.IceAmbushBreaksIce",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSMarsIceAmbush::RunTest(const FString&)
{
    const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    W->SetGameInstance(NewObject<UGameInstance>(GEngine));W->InitializeActorsForPlay(FURL());W->BeginPlay();W->GetWorldSettings()->NotifyBeginPlay();
    auto* Planet=W->SpawnActor<AJTSPlanetAnchor>();
    auto* Ice=W->SpawnActor<AJTSFacilityHazard>(FVector(8000,0,0),FRotator::ZeroRotator);Ice->Kind=EJTSFacilityHazardKind::IceLoad;Ice->WarningSeconds=.5f;
    auto* Floor=W->SpawnActor<AStaticMeshActor>();Ice->AffectedFloors.Add(Floor);
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Player=W->SpawnActor<AJTSCharacter>(FVector(8000,100,0),FRotator::ZeroRotator,Params);
    auto* Fish=W->SpawnActor<AJTSPlanetSurfaceCreature>(FVector(8000,0,0),FRotator::ZeroRotator,Params);
    auto* Tactic=Fish->FindComponentByClass<UJTSCreatureTacticsComponent>();Tactic->Tactic=EJTSCreatureTactic::IceAmbush;
    TestFalse(TEXT("One player does not overload the ice"),Ice->bTriggered);
    Tactic->Update(Planet,.1f);TestEqual(TEXT("Nearby player starts fish warning"),Tactic->Posture,EJTSCreaturePosture::Warning);
    const uint64 OriginalFrame=GFrameCounter;
    for(int32 I=0;I<30;++I) { ++GFrameCounter;W->Tick(LEVELTICK_All,.05f); }
    TestTrue(TEXT("Fish ambush breaks ice below the occupancy threshold"),Ice->bTriggered);
    TestFalse(TEXT("Broken ice no longer blocks the pool"),Floor->GetActorEnableCollision());
    TestTrue(TEXT("Ambush does not kill the player outright"),!Player->GetHealthComponent()->IsDead());
    W->EndPlay(EEndPlayReason::Quit);W->DestroyWorld(false);GEngine->DestroyWorldContext(W);GFrameCounter=OriginalFrame;return true;
}

// Explicit editor fixture setup; production admission remains closed throughout expeditions.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMarsCameraReset,"JTS.Mars.CameraShutdownAndRefresh",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSMarsCameraReset::RunTest(const FString&)
{
    const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    W->SetGameInstance(NewObject<UGameInstance>(GEngine));W->InitializeActorsForPlay(FURL());W->BeginPlay();W->GetWorldSettings()->NotifyBeginPlay();
    const uint64 OriginalFrame=GFrameCounter;
    auto Advance=[&](int32 Frames){for(int32 I=0;I<Frames;++I){++GFrameCounter;W->Tick(LEVELTICK_All,.05f);}};
    auto* Room=W->SpawnActor<AJTSInteriorZone>(FVector(8000,0,0),FRotator::ZeroRotator);Room->Bounds->SetBoxExtent(FVector(1000));
    auto* Camera=W->SpawnActorDeferred<AJTSBreakableObstacle>(AJTSBreakableObstacle::StaticClass(),FTransform(FVector(8000,0,0)));
    Camera->bRequiresMiningTool=false;Camera->ShutdownSeconds=.3f;Camera->RefreshOnEntry=Room;Camera->FinishSpawning(FTransform(FVector(8000,0,0)));
    Camera->TakeDamage(999,FDamageEvent(),nullptr,nullptr);
    TestTrue(TEXT("Destruction starts the short shutdown presentation"),Camera->bPropBroken && Camera->Mesh->IsVisible());
    TestEqual(TEXT("A destroyed camera immediately stops blocking"),Camera->Mesh->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
    Advance(4);TestTrue(TEXT("The screen collapses during shutdown"),Camera->Mesh->GetRelativeScale3D().Z<.8f);
    Advance(6);TestFalse(TEXT("The camera disappears after shutdown"),Camera->Mesh->IsVisible());
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    W->SpawnActor<AJTSCharacter>(FVector(8000,150,0),FRotator::ZeroRotator,Params);Advance(11);
    TestFalse(TEXT("A new room entry resets the broken camera"),Camera->bPropBroken);
    TestTrue(TEXT("Room entry restores health, scale and visibility"),!Camera->Health->IsDead() && Camera->Mesh->IsVisible() && Camera->Mesh->GetRelativeScale3D().Equals(FVector::OneVector));
    W->EndPlay(EEndPlayReason::Quit);W->DestroyWorld(false);GEngine->DestroyWorldContext(W);GFrameCounter=OriginalFrame;return true;
}

static void ConnectMarsPIEClients()
{
    UWorld* Server=nullptr;
    for(const auto& Context:GEngine->GetWorldContexts())
        if(Context.WorldType==EWorldType::PIE && Context.World() && Context.World()->GetNetMode()==NM_ListenServer) Server=Context.World();
    if(!Server) return;
    auto* GS=Server->GetGameState<AJTSGameState>();if(!GS)return;
    GS->SetGameplayPhase(EJTSGameplayPhase::WaitingToStart);GS->SetAcceptingNewPlayers(true);
    const FString Address=Server->GetNetDriver()?Server->GetNetDriver()->LowLevelGetNetworkNumber():FString();
    int32 Colon=INDEX_NONE;if(!Address.FindLastChar(':',Colon))return;
    const FString URL=TEXT("127.0.0.1:")+Address.Mid(Colon+1)+TEXT("?JTSBuild=JTS-1");
    for(const auto& Context:GEngine->GetWorldContexts())
        if(Context.WorldType==EWorldType::PIE && Context.World() && Context.World()!=Server)
            if(auto* PC=Context.World()->GetFirstPlayerController()) PC->ClientTravel(URL,TRAVEL_Absolute);
}
static void ResumeMarsPIE()
{
    for(const auto& Context:GEngine->GetWorldContexts())
        if(Context.WorldType==EWorldType::PIE && Context.World() && Context.World()->GetNetMode()==NM_ListenServer)
            if(auto* GS=Context.World()->GetGameState<AJTSGameState>()) { GS->SetGameplayPhase(EJTSGameplayPhase::SpaceFlight);GS->SetAcceptingNewPlayers(false);UE_LOG(LogTemp,Display,TEXT("Mars PIE fixture: %d authoritative players"),GS->PlayerArray.Num()); }
}
static FAutoConsoleCommand MarsPIEConnect(TEXT("jts.Editor.Mars.ConnectClients"),TEXT("Connect editor PIE clients for replication validation."),FConsoleCommandDelegate::CreateStatic(&ConnectMarsPIEClients));
static FAutoConsoleCommand MarsPIEResume(TEXT("jts.Editor.Mars.Resume"),TEXT("Close admission after editor PIE clients connected."),FConsoleCommandDelegate::CreateStatic(&ResumeMarsPIE));
// Python's editor execution guard forces RPCs local. Run input on the next native game frame
// so this fixture exercises the same owner RPC path as actual player input.
static void QueueMarsPIEInput(const TArray<FString>& Args)
{
    if (Args.Num()<2) return;
    const FString Action=Args[0];const int32 PlayerId=FCString::Atoi(*Args[1]);
    const int32 Slot=Args.Num()>2?FCString::Atoi(*Args[2]):INDEX_NONE;
    for (const auto& Context:GEngine->GetWorldContexts())
    {
        UWorld* World=Context.World();if(Context.WorldType!=EWorldType::PIE || !World)continue;
        for(TActorIterator<AJTSCharacter> It(World);It;++It)
        {
            auto* Pawn=*It;auto* State=Pawn->GetPlayerState<AJTSPlayerState>();
            if(!Pawn->IsLocallyControlled() || !State || State->GetPlayerId()!=PlayerId)continue;
            World->GetTimerManager().SetTimerForNextTick([Weak=TWeakObjectPtr<AJTSCharacter>(Pawn),Action,Slot,PlayerId]()
            {
                auto* P=Weak.Get();if(!P)return;bool Dispatched=false;
                if(Action==TEXT("press"))
                {
                    if(auto* I=P->FindComponentByClass<UInteractionComponent>())
                    {
                        I->RefreshInteractable();
                        UE_LOG(LogTemp,Display,TEXT("Mars PIE input: player %d authority %d target %s prompt %s"),PlayerId,P->HasAuthority(),*GetNameSafe(I->GetCurrentInteractable()),*I->GetCurrentInteractionPrompt().ToString());
                        Dispatched=I->TryInteract();
                    }
                }
                else if(auto* I=P->FindComponentByClass<UJTSInventoryComponent>())
                {
                    if(Action==TEXT("select_slot") || Action==TEXT("drop_slot"))Dispatched=I->SelectQuickbarSlot(Slot);
                    if(Action==TEXT("drop_slot"))Dispatched=I->DropItemAtSlot(Slot);
                    if(Action==TEXT("drop"))Dispatched=I->DropItemAtSlot(I->GetSelectedQuickbarSlot());
                }
                UE_LOG(LogTemp,Display,TEXT("Mars PIE input: %s player %d authority %d dispatched %d"),*Action,PlayerId,P->HasAuthority(),Dispatched);
            });
            return;
        }
    }
}
static FAutoConsoleCommand MarsPIEInput(TEXT("jts.Editor.Mars.Input"),TEXT("Defer a local PIE player's input to a native game frame: action playerId [slot]."),FConsoleCommandWithArgsDelegate::CreateStatic(&QueueMarsPIEInput));
static void GrantMarsPIEStationPermit()
{
    for(const auto& Context:GEngine->GetWorldContexts())
    {
        auto* World=Context.World();
        if(Context.WorldType!=EWorldType::PIE || !World || World->GetNetMode()!=NM_ListenServer)continue;
        auto* Expedition=World->GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>();
        // Never mutate a user's expedition: this fixture uses its own physical save prefix.
        if(!Expedition || Expedition->SaveSlotPrefix!=TEXT("MarsAccessValidation") || Expedition->GetActiveSaveSlot()!=4)continue;
        Expedition->RecordImportantItem(TEXT("MarsEcologyAccess"));
    }
}
static FAutoConsoleCommand MarsPIEStationPermit(TEXT("jts.Editor.Mars.GrantStationPermit"),TEXT("Grant the station permit only inside the isolated MarsAccessValidation PIE fixture."),FConsoleCommandDelegate::CreateStatic(&GrantMarsPIEStationPermit));
static void SaveReloadMarsPIE()
{
    for(const auto& Context:GEngine->GetWorldContexts())
    {
        auto* W=Context.World();if(Context.WorldType!=EWorldType::PIE || !W || W->GetNetMode()!=NM_ListenServer)continue;
        auto* E=W->GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>();auto* GS=W->GetGameState<AJTSGameState>();
        if(!E || !GS || !E->SaveSlotPrefix.StartsWith(TEXT("MarsValidation")) || E->GetActiveSaveSlot()!=4)return;
        if(!E->SaveNow()){UE_LOG(LogTemp,Error,TEXT("Mars PIE save/load failed writing fixture save"));return;}
        const auto Before=E->GetSnapshot();E->GetMutableSnapshot()=FJTSExpeditionSnapshot();
        if(!E->LoadExpeditionSlot(4)){E->GetMutableSnapshot()=Before;UE_LOG(LogTemp,Error,TEXT("Mars PIE save/load failed reading fixture save"));return;}
        const auto& After=E->GetSnapshot();
        const bool bSame=Before.LoadedCoordinateIds.Num()==After.LoadedCoordinateIds.Num() && Before.LoadedCoordinateIds.Difference(After.LoadedCoordinateIds).IsEmpty()
            && Before.FacilityFlagIds.Num()==After.FacilityFlagIds.Num() && Before.FacilityFlagIds.Difference(After.FacilityFlagIds).IsEmpty()
            && Before.ImportantItemIds.Difference(After.ImportantItemIds).IsEmpty() && Before.BlockedEntranceIds.Difference(After.BlockedEntranceIds).IsEmpty()
            && Before.DefeatedEncounterIds.Difference(After.DefeatedEncounterIds).IsEmpty() && Before.MerchantStateIds.Difference(After.MerchantStateIds).IsEmpty() && Before.Players.Num()==After.Players.Num();
        E->SyncProgression();E->RestoreSpacecraft(GS->GetActiveSpacecraft());bool bPositionsSame=true;
        for(auto It=W->GetPlayerControllerIterator();It;++It)if(auto* PC=It->Get())if(auto* C=Cast<AJTSCharacter>(PC->GetPawn()))
        {
            const auto* Saved=After.Players.FindByPredicate([PC](const FJTSPlayerSnapshot& P){return P.PlayerId==PC->GetPlayerState<AJTSPlayerState>()->GetOnlineIdentityString();});
            // Surface players use the normal arrival checkpoint; only interiors save exact positions.
            if(!Saved || Saved->InteriorZoneId.IsNone())continue;
            const FVector Expected=C->GetActorLocation();C->SetActorLocation(Expected+FVector(5000,5000,5000));E->RestorePlayerState(PC->GetPlayerState<AJTSPlayerState>(),C);bPositionsSame&=C->GetActorLocation().Equals(Expected,3);
        }
        UE_LOG(LogTemp,Display,TEXT("Mars PIE disk save/load: %s, %d players, %d coordinates, %d facility flags, positions %s"),bSame&&bPositionsSame?TEXT("PASS"):TEXT("FAIL"),After.Players.Num(),After.LoadedCoordinateIds.Num(),After.FacilityFlagIds.Num(),bPositionsSame?TEXT("PASS"):TEXT("FAIL"));
    }
}
static FAutoConsoleCommand MarsPIESaveReload(TEXT("jts.Editor.Mars.SaveReload"),TEXT("Round-trip the isolated MarsValidation PIE snapshot through disk."),FConsoleCommandDelegate::CreateStatic(&SaveReloadMarsPIE));

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMarsSnapshotRoundTrip,"JTS.Mars.ProgressionSnapshotRoundTrip",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSMarsSnapshotRoundTrip::RunTest(const FString&)
{
    FJTSExpeditionSnapshot Before;Before.LoadedCoordinateIds={TEXT("Mars"),TEXT("Mars_Noctis"),TEXT("Titan")};Before.FacilityFlagIds={TEXT("Mars_Power"),TEXT("Mars_SeedVault_Open")};Before.BlockedEntranceIds={TEXT("Noctis_Nest_IceVein")};Before.DefeatedEncounterIds={TEXT("MarsSeedVaultMatriarch")};Before.MerchantStateIds={TEXT("Deimos_Merchant_Gifted")};Before.bHasSpacecraftHull=true;Before.SpacecraftHull=0;
    for(int I=0;I<4;++I) { auto& P=Before.Players.AddDefaulted_GetRef();P.InteriorZoneId=I%2?FName(TEXT("B2")):NAME_None;P.InteriorLocalPosition=FVector(I*100,200,150); }
    TArray<uint8> Bytes; FMemoryWriter Writer(Bytes);FObjectAndNameAsStringProxyArchive Out(Writer,false);FJTSExpeditionSnapshot::StaticStruct()->SerializeItem(Out,&Before,nullptr);
    FJTSExpeditionSnapshot After;FMemoryReader Reader(Bytes);FObjectAndNameAsStringProxyArchive In(Reader,true);FJTSExpeditionSnapshot::StaticStruct()->SerializeItem(In,&After,nullptr);
    TestEqual(TEXT("Coordinate tombstones survive"),After.LoadedCoordinateIds.Num(),3);TestTrue(TEXT("Door state survives"),After.FacilityFlagIds.Contains(TEXT("Mars_SeedVault_Open")));TestTrue(TEXT("Merchant cannot grant twice after load"),After.MerchantStateIds.Contains(TEXT("Deimos_Merchant_Gifted")));TestTrue(TEXT("Disabled hull is distinguishable from an old save"),After.bHasSpacecraftHull && After.SpacecraftHull==0);TestEqual(TEXT("Four independent player positions survive"),After.Players.Num(),4);TestEqual(TEXT("Underground/surface split survives"),After.Players[1].InteriorZoneId,FName(TEXT("B2")));TestTrue(TEXT("Old saves have no invalid interior destination"),FJTSPlayerSnapshot().InteriorZoneId.IsNone());TestFalse(TEXT("Old saves initialize full hull"),FJTSExpeditionSnapshot().bHasSpacecraftHull);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMarsFacilityFeedback,"JTS.Mars.LockedFacilityFeedback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSMarsFacilityFeedback::RunTest(const FString&)
{
    const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    W->SetGameInstance(NewObject<UGameInstance>(GEngine));W->InitializeActorsForPlay(FURL());W->BeginPlay();W->GetWorldSettings()->NotifyBeginPlay();
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Player=W->SpawnActor<AJTSCharacter>(FVector(8000,100,0),FRotator::ZeroRotator,Params);
    auto* Facility=W->SpawnActor<AJTSFacilityController>(FVector(8000,0,0),FRotator::ZeroRotator);
    Facility->CompletedFlag=TEXT("FixtureRepair");Facility->LockedPrompt=FText::FromString(TEXT("Restore power first"));
    FJTSFacilityRequirement Requirement;Requirement.Id=TEXT("FixturePower");Facility->Conditions->Requirements.Add(Requirement);
    TestTrue(TEXT("Locked equipment remains discoverable by the interaction finder"),Facility->CanInteract_Implementation(Player));
    TestEqual(TEXT("Locked equipment explains its prerequisite"),Facility->GetInteractionPrompt_Implementation(Player).ToString(),FString(TEXT("Restore power first")));
    TestFalse(TEXT("A visible locked prompt does not authorize execution"),Facility->CanExecuteInteraction_Implementation(Player));
    Facility->Interact_Implementation(Player);TestFalse(TEXT("Direct interaction cannot bypass the prerequisite"),Facility->bActive);
    Facility->Conditions->Requirements.Reset();Facility->RepairCosts.Add(EJTSResourceType::Ore,6);
    TestTrue(TEXT("Missing materials do not hide the target"),Facility->CanInteract_Implementation(Player));
    TestFalse(TEXT("Missing storage cannot begin a repair hold"),Facility->CanExecuteInteraction_Implementation(Player));
    TestTrue(TEXT("The prompt exposes stock and required quantity"),Facility->GetInteractionPrompt_Implementation(Player).ToString().Contains(TEXT("0/6")));
    Facility->bActive=true;Facility->CompletedPrompt=FText::FromString(TEXT("Recovered"));
    TestEqual(TEXT("Completed machinery stays inspectable"),Facility->GetInteractionPrompt_Implementation(Player).ToString(),FString(TEXT("Recovered")));
    TestFalse(TEXT("Completed machinery cannot grant a second reward"),Facility->CanExecuteInteraction_Implementation(Player));
    auto* Transit=W->SpawnActor<AJTSInteriorTransit>(FVector(8000,0,0),FRotator::ZeroRotator);
    Transit->Destination=Facility;Transit->Conditions->Requirements.Add(Requirement);Transit->LockedPrompt=FText::FromString(TEXT("Lift offline"));
    TestTrue(TEXT("An unpowered lift remains discoverable"),Transit->CanInteract_Implementation(Player));
    TestFalse(TEXT("An unpowered lift cannot teleport"),Transit->CanExecuteInteraction_Implementation(Player));
    TestEqual(TEXT("Lift prompt explains the lock"),Transit->GetInteractionPrompt_Implementation(Player).ToString(),FString(TEXT("Lift offline")));
    Player->SetActorLocation(FVector(9000,0,0));TestFalse(TEXT("Inspection still enforces proximity"),Transit->CanInteract_Implementation(Player));
    W->EndPlay(EEndPlayReason::Quit);W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMarsAutomaticDoor,"JTS.Mars.AutomaticAccessDoor",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSMarsAutomaticDoor::RunTest(const FString&)
{
    const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    W->InitializeActorsForPlay(FURL());
    auto* State=W->SpawnActor<AJTSGameState>();W->SetGameState(State);
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FTransform DoorTransform(FRotator(45,31,85),FVector(8000,9000,10000));
    auto* Door=W->SpawnActor<AJTSAutomaticAccessDoor>(DoorTransform.GetLocation(),DoorTransform.Rotator(),Params);
    FJTSFacilityRequirement Requirement;Requirement.Source=EJTSFacilityFact::ImportantItem;Requirement.Id=TEXT("TestAccess");
    Door->Conditions->Requirements.Add(Requirement);
    auto* Member=W->SpawnActor<AJTSCharacter>(DoorTransform.TransformPosition(FVector(250,0,180)),FRotator::ZeroRotator,Params);
    Member->GetCharacterMovement()->SetComponentTickEnabled(false);
    W->BeginPlay();W->GetWorldSettings()->NotifyBeginPlay();
    const uint64 PreviousFrame=GFrameCounter;
    auto Advance=[&](float Seconds) { for (int32 I=0;I<FMath::CeilToInt(Seconds/.05f);++I) { ++GFrameCounter;W->Tick(LEVELTICK_All,.05f); } };
    Advance(1);
    TestFalse(TEXT("A nearby member cannot open a door without shared authorization"),Door->bOpen);
    TestFalse(TEXT("Press interaction cannot bypass automatic authentication"),Door->CanExecuteInteraction_Implementation(Member));
    State->SetImportantItems({TEXT("UnrelatedBadge")});Advance(.2f);
    TestFalse(TEXT("Another badge does not satisfy the configured permit"),Door->bOpen);
    State->SetImportantItems({TEXT("TestAccess")});Advance(1.1f);
    TestTrue(TEXT("A shared credential opens for a member without a personal inventory card"),Door->bAuthorized && Door->bOpen);
    TestTrue(TEXT("Leaf slides in door space on a tilted planet"),Door->LeftLeaf->GetRelativeLocation().Equals(FVector(0,-220,0),.1f));
    TestFalse(TEXT("Open, stationary doors do not keep ticking"),Door->IsActorTickEnabled());
    Member->SetActorLocation(DoorTransform.TransformPosition(FVector(0,0,180)));
    State->SetImportantItems({});Advance(2.5f);
    TestTrue(TEXT("Revoking authorization does not close onto an occupant"),Door->bOpen);
    Member->SetActorLocation(DoorTransform.TransformPosition(FVector(1500,0,180)));Advance(2.6f);
    TestFalse(TEXT("The entrance closes after the last occupant leaves"),Door->bOpen);
    TestTrue(TEXT("Leaves return to their actual blocking positions"),Door->LeftLeaf->GetRelativeLocation().Equals(FVector(0,-70,0),.1f));
    TestFalse(TEXT("Closed doors stop animation tick"),Door->IsActorTickEnabled());
    Member->SetActorLocation(DoorTransform.TransformPosition(FVector(250,0,180)));
    Member->GetHealthComponent()->ApplyDamage(100000,nullptr,nullptr,true);
    State->SetImportantItems({TEXT("TestAccess")});Advance(.5f);
    TestFalse(TEXT("A dead member does not trigger access"),Door->bOpen);
    W->EndPlay(EEndPlayReason::Quit);W->DestroyWorld(false);GEngine->DestroyWorldContext(W);GFrameCounter=PreviousFrame;
    return true;
}
#endif
