#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Ships/JTSStellarExchangeDock.h"
#include "space/Ships/JTSShipWeaponModule.h"
#include "space/Components/JTSStellarExchangeComponent.h"
#include "space/Components/JTSShipArmamentComponent.h"
#include "space/Components/JTSShipCabinCouplingComponent.h"
#include "space/Components/JTSSpacecraftLandingSupportComponent.h"
#include "space/Components/JTSShipDockComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Items/JTSStellarLootTable.h"
#include "space/Systems/JTSExpeditionSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Blueprint/UserWidget.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSStellarLoadoutComponent.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Items/JTSStellarProgression.h"
#include "space/World/JTSMoonCubeEnemy.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetSurfaceAttachment.h"

namespace
{
struct FRefitWorld
{
	UWorld* World;
	uint64 SavedFrame=GFrameCounter;
	AJTSSpacecraftActor* Ship;
	FRefitWorld()
	{
		const auto V=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&V);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
		auto* Class=LoadClass<AJTSSpacecraftActor>(nullptr,TEXT("/Game/Space/Blueprints/Ships/BP_Spacecraft.BP_Spacecraft_C"));
		Ship=World->SpawnActor<AJTSSpacecraftActor>(Class);Ship->DispatchBeginPlay();Ship->GetDockComponent()->SetComponentTickEnabled(false);
	}
	~FRefitWorld()
	{
		for(TActorIterator<AJTSStellarExchangeDock> It(World);It;++It)
		{
			if(auto* Widget=It->Display->GetUserWidgetObject()) Widget->ReleaseSlateResources(true);
			It->Display->SetWidget(nullptr); It->Display->SetWidgetClass(nullptr); It->Display->DestroyComponent();
		}
		World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GFrameCounter=SavedFrame;
	}
	void Step(float Seconds)
	{
		for(int32 I=0;I<FMath::CeilToInt(Seconds/.05f);++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,.05f); }
	}
	AJTSCharacter* Player(AJTSPlayerState*& State)
	{
		State=World->SpawnActor<AJTSPlayerState>();State->SetPlayerId(World->GetNumPlayerControllers()+1);auto* PC=World->SpawnActor<AJTSPlayerController>();
		FindFProperty<FObjectProperty>(PC->GetClass(),TEXT("PlayerState"))->SetObjectPropertyValue_InContainer(PC,State);
		FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Pawn=World->SpawnActor<AJTSCharacter>(FVector(2000,0,0),FRotator::ZeroRotator,Params);PC->Possess(Pawn);
		FindFProperty<FObjectProperty>(Pawn->GetClass(),TEXT("PlayerState"))->SetObjectPropertyValue_InContainer(Pawn,State);return Pawn;
	}
	AJTSStellarExchangeDock* Dock()
	{for(TActorIterator<AJTSStellarExchangeDock> I(World);I;++I)if(I->GetOwner()==Ship)return *I;return nullptr;}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSSharedExchangeTest,"JTS.Ship.Refit.PrivateRollStackAndTravel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSSharedExchangeTest::RunTest(const FString&)
{
	FRefitWorld W;auto* Dock=W.Dock();if(!TestNotNull(TEXT("Production exchange is installed"),Dock))return false;
	auto* E=W.Ship->GetStellarExchange();auto* Table=NewObject<UJTSStellarLootTable>(W.World);
	FJTSStellarLootEntry Entry;Entry.ItemId=TEXT("SharedTestCore");Entry.DisplayName=FText::FromString(TEXT("Shared Test Core"));Entry.bCore=true;Table->Entries.Add(Entry);
	FJTSItemCost Cost;Cost.ResourceType=EJTSResourceType::Rock;Cost.Amount=3;Table->SpinCosts.Add(Cost);
	FindFProperty<FSoftObjectProperty>(W.Ship->GetClass(),TEXT("StellarLootTable"))->SetObjectPropertyValue_InContainer(W.Ship,Table);
	AJTSPlayerState *AState,*BState;auto* A=W.Player(AState);auto* B=W.Player(BState);
	AState->GetStellarLoadout()->ConfigureLootTable(Table); BState->GetStellarLoadout()->ConfigureLootTable(Table);
	const FVector Front=Dock->GetScreenWorldLocation()+Dock->GetActorRightVector()*110;
	A->SetActorLocation(Front);B->SetActorLocation(Front+Dock->GetActorForwardVector()*20);
	TestTrue(TEXT("Both players can use the same screen"),Dock->CanUseDock(A)&&Dock->CanUseDock(B));
	W.Ship->DepositResourceAmounts({{EJTSResourceType::Rock,30}});
	const int32 Before=W.Ship->GetResourceAmount(EJTSResourceType::Rock);
	if(!TestEqual(TEXT("Physical machine accepts one pull"),E->TryPull(A),EJTSStellarRollResult::Succeeded))return false;
	TestEqual(TEXT("Concurrent second player cannot start a second roll"),E->TryPull(B),EJTSStellarRollResult::CoolingDown);
	TestEqual(TEXT("Cost is deducted once"),W.Ship->GetResourceAmount(EJTSResourceType::Rock),Before-3);
	TestTrue(TEXT("Prize remains hidden while the real reels spin"),E->GetState().Stock.IsEmpty());
	const auto Snapshot=E->CaptureSnapshot();if(!TestEqual(TEXT("Travel mid-spin retains the charged prize"),Snapshot.Stock.Num(),1))return false;
	auto* Save=NewObject<UJTSExpeditionSaveGame>();Save->Snapshot.StellarExchange=Snapshot;TArray<uint8> Bytes;
	TestTrue(TEXT("Shared stock serializes through the expedition save"),UGameplayStatics::SaveGameToMemory(Save,Bytes));
	auto* Reload=Cast<UJTSExpeditionSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if(!TestNotNull(TEXT("Expedition save reloads"),Reload))return false;
	E->RestoreSnapshot(Reload->Snapshot.StellarExchange);
	TestEqual(TEXT("Pending prize records pulling player's identity"),Snapshot.Stock[0].RecipientId,AState->GetOnlineIdentityString());
	TestFalse(TEXT("Public claim RPC cannot redirect a private prize"),E->TryClaim(B,Snapshot.Stock[0].Token));
	W.Step(.7f);
	TestEqual(TEXT("Pulling player's personal core grid receives reward"),AState->GetStellarLoadout()->GetSlot(0).StellarItemId,Entry.ItemId);
	TestTrue(TEXT("Other player receives no duplicate"),BState->GetStellarLoadout()->GetSlot(0).IsEmpty());
	TestTrue(TEXT("Delivery clears pending queue"),E->GetState().Stock.IsEmpty());
	auto Item=Table->MakeTextItem(Entry.ItemId);
	TestTrue(TEXT("Same kind stacks in personal grid"),AState->GetStellarLoadout()->TryStoreItem(Item));
	TestEqual(TEXT("Stack count survives delivery"),AState->GetStellarLoadout()->GetSlot(0).StackCount,2);
	Item.StellarCoreLevel=2;
	TestTrue(TEXT("Different upgrade level retains separate slot"),AState->GetStellarLoadout()->TryStoreItem(Item));
	TestEqual(TEXT("Existing stack keeps original level"),AState->GetStellarLoadout()->GetSlot(0).StellarCoreLevel,1);
	TestEqual(TEXT("Circled stack badge"),FJTSItemInstance::StackBadge(2),FString(TEXT("②")));
	// Upgrading one physical copy must not upgrade every copy in the stack.
	TArray<FJTSItemInstance> UpgradeSlots; UpgradeSlots.SetNum(9); UpgradeSlots[0]=Table->MakeTextItem(Entry.ItemId); UpgradeSlots[0].StackCount=3;
	auto* Loadout=AState->GetStellarLoadout(); Loadout->RestoreState(UpgradeSlots,100); AState->RestoreStellarFirmwareUnits(100);
	Loadout->ServerUpgradeCore(0,Loadout->GetSlot(0).InstanceId,Loadout->GetRevision());
	TestEqual(TEXT("Upgrade mutates a single copy"),Loadout->GetSlot(0).StackCount,1);
	TestEqual(TEXT("Upgrade increases only selected level"),Loadout->GetSlot(0).StellarCoreLevel,2);
	TestEqual(TEXT("Unmodified copies remain stacked"),Loadout->GetSlot(1).StackCount,2);
	TestEqual(TEXT("Remainder retains its original level"),Loadout->GetSlot(1).StellarCoreLevel,1);
	// An entirely full personal grid still accepts an existing compatible stack.
	for (int32 I=0;I<9;++I) { UpgradeSlots[I]=Table->MakeTextItem(Entry.ItemId); UpgradeSlots[I].StellarCoreLevel=I+2; }
	Loadout->RestoreState(UpgradeSlots,100); Item=Table->MakeTextItem(Entry.ItemId); Item.StellarCoreLevel=2;
	TestTrue(TEXT("Full grid merges compatible reward"),Loadout->TryStoreItem(Item));
	TestEqual(TEXT("Full grid merged count"),Loadout->GetSlot(0).StackCount,2);
	// Without a compatible stack, delivery ejects one real replicated pickup from the authored chute.
	auto* Ground=W.World->SpawnActor<AStaticMeshActor>(); Ground->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Ground->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
	Ground->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll")); Ground->SetActorLocation(FVector(0,0,-200));Ground->SetActorScale3D(FVector(50,50,.2));
	FJTSStellarExchangeSnapshot Overflow; auto& Prize=Overflow.Stock.AddDefaulted_GetRef();Prize.ItemId=Entry.ItemId;Prize.Token=FGuid::NewGuid();Prize.RecipientId=AState->GetOnlineIdentityString();
	E->RestoreSnapshot(Overflow); W.Step(.55f);
	AJTSWorldPickupActor* Drop=nullptr;for(TActorIterator<AJTSWorldPickupActor> It(W.World);It;++It)if(It->GetItemInstance().InstanceId==Prize.Token)Drop=*It;
	if(TestNotNull(TEXT("Full grid ejects exactly one physical prize"),Drop))
	{
		if (!Drop->HasActorBegunPlay()) Drop->DispatchBeginPlay();
		const auto* Glint=Drop->FindComponentByClass<UMaterialBillboardComponent>();
		TestTrue(TEXT("Physical reward uses the authored pickup sparkle"),Glint && Glint->Elements.Num()==1 && Glint->Elements[0].Material);
		const FVector Start=Drop->GetActorLocation(); W.Step(.1f);
		TestTrue(TEXT("Prize follows an upward arc instead of teleporting onto floor"),Drop->GetActorLocation().Z>Start.Z);
	}
	TestTrue(TEXT("Successful overflow clears durable delivery queue"),E->GetState().Stock.IsEmpty());


	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSRefitHardpointsTest,"JTS.Ship.Refit.EightIndependentHardpointsAndAisles",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSRefitHardpointsTest::RunTest(const FString&)
{
	FRefitWorld W;auto* Arms=W.Ship->GetArmament();
	TestEqual(TEXT("Production configuration has eight mounts"),Arms->Hardpoints.Num(),8);
	TestEqual(TEXT("All eight independently replicated laser actors are installed"),Arms->GetModules().Num(),8);
	for(const TCHAR* Id:{TEXT("L1"),TEXT("L2"),TEXT("L3")})
	{
		const auto* L=Arms->Hardpoints.FindByPredicate([Id](const auto& H){return H.Id==Id;});
		FString RId(Id);RId[0]='R';const auto* R=Arms->Hardpoints.FindByPredicate([RId](const auto& H){return H.Id==*RId;});
		TestTrue(TEXT("Port/starboard mounts are mirrored"),L&&R&&L->MountTransform.GetLocation().Equals(R->MountTransform.GetLocation()*FVector(1,-1,1),.1));
	}
	const auto* Turret=Arms->Hardpoints.FindByPredicate([](const auto& H){return H.Id==TEXT("Turret");});
	TestTrue(TEXT("Roof turret allows full horizontal traversal"),Turret&&Turret->bFullRotation);
	TestTrue(TEXT("One plugin can be removed independently"),Arms->ReplaceWeapon(TEXT("L2"),nullptr));
	TestEqual(TEXT("Other seven weapons survive removal"),Arms->GetModules().Num(),7);
	TestFalse(TEXT("Unknown hardpoint cannot spawn an arbitrary extra weapon"),Arms->ReplaceWeapon(TEXT("Missing"),AJTSShipWeaponModule::StaticClass()));
	// These are the real physical hull proxies, not the large empty flight envelope.
	for(double Y:{-295.0,295.0})
	{
		FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(RefitAisle),false);Q.AddIgnoredComponent(W.Ship->GetFlightCollisionComponent());
		TestFalse(TEXT("Widened aisles remain physically empty"),W.World->LineTraceSingleByChannel(Hit,FVector(180,Y,-30),FVector(430,Y,-30),ECC_Visibility,Q));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSRefitLaserTest,"JTS.Ship.Refit.LaserDamageCooldownAndHullOcclusion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSRefitLaserTest::RunTest(const FString&)
{
	FRefitWorld W;
	FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for(auto* Weapon:W.Ship->GetArmament()->GetModules())
	{
		const FVector Start=Weapon->Muzzle->GetComponentLocation(),Direction=Weapon->Muzzle->GetForwardVector();
		auto* Target=W.World->SpawnActor<AJTSMoonCubeEnemy>(Start+Direction*3000,FRotator::ZeroRotator,P);
		if(!TestNotNull(TEXT("Laser target spawns"),Target))return false;
		Target->DispatchBeginPlay();auto* Health=Target->GetHealthComponent();const float Before=Health->GetHealth();
		Weapon->FirePulse();
		TestEqual(TEXT("Each installed muzzle delivers its configured server damage"),Health->GetHealth(),Before-Weapon->Damage);
		const float After=Health->GetHealth();Weapon->FirePulse();
		TestEqual(TEXT("A repeated request in the same fire interval cannot apply damage twice"),Health->GetHealth(),After);
		Target->Destroy();
	}
	// An independently replaced module has a fresh fire interval. Its own hull must still block it.
	auto* Arms=W.Ship->GetArmament();
	const auto* H=Arms->Hardpoints.FindByPredicate([](const auto& Mount){return Mount.Id==TEXT("L1");});
	if(!TestNotNull(TEXT("Front hardpoint exists"),H))return false;
	Arms->ReplaceWeapon(H->Id,H->WeaponClass);
	AJTSShipWeaponModule* Front=nullptr;
	for(auto* M:Arms->GetModules())if(M->GetRootComponent()->GetRelativeLocation().Equals(H->MountTransform.GetLocation(),.1f))Front=M;
	if(!TestNotNull(TEXT("Replacement front gun is installed"),Front))return false;
	const FVector Start=Front->Muzzle->GetComponentLocation(),Direction=Front->Muzzle->GetForwardVector();
	auto* Target=W.World->SpawnActor<AJTSMoonCubeEnemy>(Start+Direction*1000,FRotator::ZeroRotator,P);Target->DispatchBeginPlay();
	auto* Cover=NewObject<UStaticMeshComponent>(W.Ship);W.Ship->AddInstanceComponent(Cover);Cover->SetupAttachment(W.Ship->GetRootComponent());
	Cover->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
	Cover->SetCollisionProfileName(TEXT("BlockAll"));Cover->RegisterComponent();Cover->SetWorldLocation(Start+Direction*100);Cover->SetWorldScale3D(FVector(.4));
	const float Before=Target->GetHealthComponent()->GetHealth();Front->FirePulse();
	TestEqual(TEXT("A physical hull section blocks the ship's own laser"),Target->GetHealthComponent()->GetHealth(),Before);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSLandedDefenseTest,"JTS.Ship.Refit.LandedAutomaticDefense",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSLandedDefenseTest::RunTest(const FString&)
{
	FRefitWorld W;
	auto* Arms = W.Ship->GetArmament();
	const auto Modules = Arms->GetModules();
	AJTSShipWeaponModule* Roof = nullptr;
	for (int32 I = 0; I < Modules.Num(); ++I)
		if (Arms->Hardpoints.IsValidIndex(I) && Arms->Hardpoints[I].bFullRotation && !Modules[I]->bFixedForward) { Roof = Modules[I]; break; }
	if (!TestNotNull(TEXT("Production roof turret exists"), Roof)) return false;
	W.Ship->SetGroundedPlanet(W.World->SpawnActor<AJTSPlanetAnchor>());
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Point = Roof->PitchPivot->GetComponentLocation() + W.Ship->GetActorRightVector() * 2200 + W.Ship->GetActorUpVector() * 100;
	auto* Target = W.World->SpawnActor<AJTSMoonCubeEnemy>(Point, FRotator::ZeroRotator, Params);
	Target->DispatchBeginPlay(); Target->GetHealthComponent()->SetMaxHealth(10000);
	Target->SetActorTickEnabled(false);
	auto* Neutral = W.World->SpawnActor<AJTSPlanetSurfaceAttachment>();
	Neutral->DispatchBeginPlay();
	Neutral->SetActorLocation(Roof->GetActorLocation() - W.Ship->GetActorRightVector() * 1200);
	auto* NeutralHealth = Neutral->FindComponentByClass<UJTSHealthComponent>();
	NeutralHealth->SetInvulnerable(false);
	auto* NeutralCollision = NewObject<UBoxComponent>(Neutral);
	Neutral->AddInstanceComponent(NeutralCollision); NeutralCollision->SetupAttachment(Neutral->GetRootComponent());
	NeutralCollision->SetBoxExtent(FVector(80)); NeutralCollision->SetCollisionProfileName(TEXT("BlockAllDynamic")); NeutralCollision->RegisterComponent();
	const float NeutralBefore = NeutralHealth->GetHealth();
	const float Before = Target->GetHealthComponent()->GetHealth();
	W.Step(2);
	TestTrue(TEXT("Unoccupied landed ship automatically turns and damages a visible enemy"), Target->GetHealthComponent()->GetHealth() < Before);
	TestTrue(TEXT("Roof turret tracks the side target rather than hull forward"), FVector::DotProduct(Roof->Muzzle->GetForwardVector(), W.Ship->GetActorRightVector()) > .95f);
	const FRotator Aim = Roof->YawPivot->GetRelativeRotation();
	AJTSPlayerState* DriverState = nullptr;
	auto* Character = W.Player(DriverState);
	auto* Driver = Cast<APlayerController>(Character->GetController());
	FindFProperty<FObjectProperty>(W.Ship->GetClass(), TEXT("DriverPlayerState"))->SetObjectPropertyValue_InContainer(W.Ship, DriverState);
	Driver->Possess(W.Ship);
	Arms->ServerSetWeaponIntent(-W.Ship->GetActorRightVector(), true);
	W.Step(.1f);
	TestTrue(TEXT("Landed camera intent cannot override automatic tracking"), Roof->YawPivot->GetRelativeRotation().Equals(Aim, .5f));
	auto* Cover = W.World->SpawnActor<AStaticMeshActor>();
	Cover->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Cover->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
	Cover->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Cover->SetActorLocation(Point - W.Ship->GetActorRightVector() * 400); Cover->SetActorScale3D(FVector(40, 1, 40));
	const float Covered = Target->GetHealthComponent()->GetHealth();
	W.Step(1);
	TestEqual(TEXT("Terrain or structures prevent automatic fire through cover"), Target->GetHealthComponent()->GetHealth(), Covered);
	Cover->Destroy(); Target->Destroy();
	W.Step(1);
	TestTrue(TEXT("Turret returns to its installed heading without a target"), Roof->YawPivot->GetRelativeRotation().IsNearlyZero());
	TestEqual(TEXT("Damageable surface attachments are not hostile targets"), NeutralHealth->GetHealth(), NeutralBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMechanicalInterlockTest,"JTS.Ship.Mechanics.CouplingInterlocks",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSMechanicalInterlockTest::RunTest(const FString&)
{
	for(int32 I=0;I<=1000;++I)
	{
		const float P=I/1000.f;
		for(bool Capture:{false,true})
		{
			const auto M=UJTSShipCabinCouplingComponent::EvaluateMotion(P,Capture);
			if(M.CabinTravel>.0001f)TestTrue(TEXT("Cabin travels only with both side links withdrawn"),M.SideExtension<.0001f);
			if(M.SideExtension<.9999f)TestTrue(TEXT("Pins clear before side carriage moves"),M.Lock<.0001f);
			if(M.Lock>.0001f)TestTrue(TEXT("Pins engage only on fully extended carriages"),M.SideExtension>.9999f);
		}
	}
	const auto Released=UJTSShipCabinCouplingComponent::EvaluateMotion(1,false);
	TestEqual(TEXT("Released cabin clears frame"),Released.CabinTravel,1.f);
	TestEqual(TEXT("Ejector returns after release"),Released.RearExtension,0.f);
	const auto Locked=UJTSShipCabinCouplingComponent::EvaluateMotion(1,true);
	TestEqual(TEXT("Captured cabin seats home"),Locked.CabinTravel,0.f);
	TestEqual(TEXT("Captured assembly is pinned"),Locked.Lock,1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMechanicalProductionRigTest,"JTS.Ship.Mechanics.ProductionGearAndFixedCannon",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSMechanicalProductionRigTest::RunTest(const FString&)
{
	FRefitWorld W;
	const auto* Support=W.Ship->GetLandingSupportComponent();
	TInlineComponentArray<USceneComponent*> Components(W.Ship);
	for(const auto& Definition:Support->Feet)
	{
		USceneComponent *Foot=nullptr,*Strut=nullptr;
		for(auto* C:Components){if(C->GetFName()==Definition.FootComponentName)Foot=C;if(C->GetFName()==Definition.StrutComponentName)Strut=C;}
		if(!TestNotNull(TEXT("Configured foot exists"),Foot)||!TestNotNull(TEXT("Configured shin exists"),Strut))return false;
		const FVector Sole=W.Ship->GetActorTransform().InverseTransformPosition(Foot->GetComponentTransform().TransformPosition(Definition.SoleCentreLocal));
		TestTrue(TEXT("Actual solid sole coincides with the terrain solver frame"),Sole.Equals(Definition.DeployedContactFrame.GetLocation(),.1f));
		TestTrue(TEXT("No reflected hull scale leaks into landing rig"),Foot->GetComponentScale().GetMin()>0);
		TestTrue(TEXT("Foot remains mechanically attached to its shin"),Foot->GetAttachParent()==Strut);
		TestTrue(TEXT("Wide load spreading pads"),Definition.PadHalfExtent.X>=45 && Definition.PadHalfExtent.Y>=30);
	}
	AJTSShipWeaponModule* Main=nullptr;
	for(auto* M:W.Ship->GetArmament()->GetModules())if(M->bFixedForward)Main=M;
	if(!TestNotNull(TEXT("Fixed axial main cannon installed"),Main))return false;
	Main->Aim(FVector::RightVector,true);Main->Tick(1);
	TestTrue(TEXT("Camera aim cannot traverse fixed main cannon"),Main->Muzzle->GetForwardVector().Equals(W.Ship->GetActorForwardVector(),.001f));
	TestTrue(TEXT("Main muzzle starts at the recessed front aperture"),Main->GetActorTransform().InverseTransformPosition(Main->Muzzle->GetComponentLocation()).Equals(FVector(282,0,-37),.01f));
	TestTrue(TEXT("Main cannon follows detachable cabin"),Main->GetAttachParentActor()==W.Ship && Main->GetRootComponent()->GetAttachParent()==W.Ship->CabinAssemblyRoot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMechanicalReleaseTest,"JTS.Ship.Mechanics.ReleaseCaptureAndObstruction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSMechanicalReleaseTest::RunTest(const FString&)
{
	FRefitWorld W;auto* C=W.Ship->CabinCoupling.Get();
	TestTrue(TEXT("Cabin releases on server"),C->ReleaseCabin());
	TestFalse(TEXT("Cannot release twice"),C->ReleaseCabin());
	TestFalse(TEXT("Cannot capture before release completes"),C->CaptureCabin());
	for(int32 I=0;I<FMath::CeilToInt((C->ReleaseDuration+.2f)/.05f);++I)
	{W.World->Tick(LEVELTICK_All,.05f);C->TickComponent(.05f,LEVELTICK_All,nullptr);}
	TestEqual(TEXT("Release reaches detached state"),C->GetCouplingState(),EJTSCabinCouplingState::Detached);
	const FTransform Free=C->GetCabinWorldTransform();
	W.Ship->SetActorLocation(FVector(50,20,0));
	TestTrue(TEXT("Detached cabin stays in its own world frame"),C->GetCabinWorldTransform().Equals(Free,.01f));
	TestFalse(TEXT("Detached cabin rejects invalid scale"),C->SetDetachedCabinTransform(FTransform(FQuat::Identity,Free.GetLocation(),FVector::ZeroVector)));
	TestTrue(TEXT("Clear nearby cabin can begin capture"),C->CaptureCabin());
	for(int32 I=0;I<FMath::CeilToInt((C->CaptureDuration+.2f)/.05f);++I)
	{W.World->Tick(LEVELTICK_All,.05f);C->TickComponent(.05f,LEVELTICK_All,nullptr);}
	TestTrue(TEXT("Capture completes all mechanical locks"),C->IsCabinDocked());
	TestTrue(TEXT("Captured cabin follows frame again"),C->GetCabinWorldTransform().Equals(W.Ship->GetActorTransform(),.01f));
	auto* Obstacle=W.World->SpawnActor<AStaticMeshActor>();
	Obstacle->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Obstacle->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
	Obstacle->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Obstacle->SetActorLocation(FVector(800,20,0));Obstacle->SetActorScale3D(FVector(3));
	TestFalse(TEXT("Obstructed cabin cannot eject through another object"),C->ReleaseCabin());
	TestTrue(TEXT("Rejected release keeps all locks engaged"),C->IsCabinDocked());
	return true;
}
#endif
