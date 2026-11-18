#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "space/World/JTSSpaceWorldManager.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/Player/JTSCharacter.h"
#include "space/Ships/JTSShipDockActor.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Components/JTSShipDockComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSSpacecraftPresentationComponent.h"
#include "space/Systems/JTSExpeditionSaveGame.h"
#include "space/Items/JTSPortableCredentialDock.h"
#include "Components/BoxComponent.h"
#include "space/Interaction/InteractionComponent.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	struct FDockWorld
	{
		UWorld* World;
		AJTSSpacecraftActor* Ship;
		UJTSShipDockComponent* Modules;
		FDockWorld()
		{
			const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Ship = World->SpawnActor<AJTSSpacecraftActor>(); Modules = Ship->GetDockComponent();
			for (int32 I = 0; I < 2; ++I)
			{
				auto* Def = NewObject<UJTSShipDockDefinition>(Modules);
				Def->ModuleId = I == 0 ? TEXT("Material") : TEXT("Weapon");
				Def->ActorClass = AJTSShipDockActor::StaticClass();
				Def->MountTransform = FTransform(FVector(I * 220, 500, 0)); Modules->Catalog.Add(Def);
			}
		}
		~FDockWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		TArray<AJTSShipDockActor*> Docks() const
		{
			TArray<AJTSShipDockActor*> Result;
			for (TActorIterator<AJTSShipDockActor> It(World); It; ++It) if (It->GetOwner() == Ship) Result.Add(*It);
			return Result;
		}
		void Advance(float Seconds)
		{
			for (float T = 0; T < Seconds; T += 0.05f) { World->Tick(LEVELTICK_All, 0.05f); for (auto* Dock : Docks()) Dock->Tick(0.05f); }
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSDockLoadoutTest, "JTS.Ship.Docks.LoadoutCombinationsOrderAndSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSDockLoadoutTest::RunTest(const FString&)
{
	FDockWorld W;
	for (const TArray<FName>& Ids : TArray<TArray<FName>>{{}, {TEXT("Material")}, {TEXT("Weapon")}, {TEXT("Material"),TEXT("Weapon")}, {TEXT("Weapon"),TEXT("Material")}})
	{
		W.Modules->RestoreInstalledIds(Ids);
		TestEqual(TEXT("Each combination spawns exactly its installed plugins"), W.Docks().Num(), Ids.Num());
		for (auto* Dock : W.Docks())
			TestTrue(TEXT("Hardpoints do not depend on install order"), Dock->GetRootComponent()->GetRelativeLocation().Equals(FVector(0,500,0)) || Dock->GetRootComponent()->GetRelativeLocation().Equals(FVector(220,500,0)));
	}
	W.Modules->RestoreInstalledIds({});
	TestTrue(TEXT("Weapon can be the first upgrade"), W.Modules->InstallDock(TEXT("Weapon")));
	TestFalse(TEXT("Duplicate installation is rejected"), W.Modules->InstallDock(TEXT("Weapon")));
	TestFalse(TEXT("Unknown modules are rejected"), W.Modules->InstallDock(TEXT("Unknown")));
	TestTrue(TEXT("Material can be added later"), W.Modules->InstallDock(TEXT("Material")));
	TestTrue(TEXT("Either plugin can be independently removed"), W.Modules->RemoveDock(TEXT("Weapon")));
	TestEqual(TEXT("Removing one preserves the other"), W.Modules->GetInstalledIds().Num(), 1);
	auto* Envelope = NewObject<UJTSExpeditionSaveGame>(); Envelope->Snapshot.bHasShipDockLoadout = true;
	Envelope->Snapshot.InstalledShipDockIds = {};
	TArray<uint8> Bytes; TestTrue(TEXT("Empty explicit loadout saves without disk IO"), UGameplayStatics::SaveGameToMemory(Envelope, Bytes));
	auto* Loaded = Cast<UJTSExpeditionSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	TestTrue(TEXT("Empty loadout stays distinct from a legacy save"), Loaded && Loaded->Snapshot.bHasShipDockLoadout && Loaded->Snapshot.InstalledShipDockIds.IsEmpty());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSDockInteractionTest, "JTS.Ship.Docks.DeviceProximityAndIndependentSignals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSDockInteractionTest::RunTest(const FString&)
{
	FDockWorld W; W.Modules->RestoreInstalledIds({TEXT("Material"),TEXT("Weapon")});
	auto Docks = W.Docks(); if (!TestEqual(TEXT("Two independently animated actors"), Docks.Num(), 2)) return false;
	APawn* Pawn = W.World->SpawnActor<AJTSCharacter>();
	for (auto* Dock : Docks) { Dock->DeploymentDuration = 0.1f; Dock->RetractionDuration = 0.3f; Dock->SetDeployed(true); }
	W.Advance(0.15f);
	AJTSShipDockActor* Dock = Docks[0]; Dock->Service = EJTSShipDockService::MaterialProcessing;
	Pawn->SetActorLocation(Dock->GetScreenWorldLocation() + Dock->ScreenAnchor->GetRightVector() * 100);
	TestTrue(TEXT("Standing in front of the deployed physical screen permits its service"), W.Modules->CanUseService(EJTSShipDockService::MaterialProcessing, Pawn));
	TestFalse(TEXT("Other service is not supplied by this screen"), W.Modules->CanUseService(EJTSShipDockService::WeaponModification, Pawn));
	Pawn->SetActorLocation(Dock->GetScreenWorldLocation() - Dock->ScreenAnchor->GetRightVector() * 100);
	TestFalse(TEXT("Back of screen cannot be used"), Dock->CanUseDock(Pawn));
	Pawn->SetActorLocation(Dock->GetScreenWorldLocation() + Dock->ScreenAnchor->GetRightVector() * 1000);
	TestFalse(TEXT("Nearby ship does not grant remote workshop access"), Dock->CanUseDock(Pawn));
	Pawn->SetActorLocation(Dock->GetScreenWorldLocation() + Dock->ScreenAnchor->GetRightVector() * 100);
	W.Ship->FindComponentByClass<UJTSSpacecraftPresentationComponent>()->BeginPlay();
	const FTransform ShipBefore = W.Ship->GetActorTransform();
	const auto FlightBefore = W.Ship->GetFlightState();
	const float GearBefore = W.Ship->FindComponentByClass<UJTSSpacecraftPresentationComponent>()->GetGearDeployAlpha();
	W.Modules->NotifyTakeoff();
	TestFalse(TEXT("Interaction closes as soon as retraction is commanded"), Dock->CanUseDock(Pawn));
	TestFalse(TEXT("The signal starts a moving mechanism"), W.Modules->AreAllStowed());
	TestEqual(TEXT("A dock signal cannot change the flight state"), W.Ship->GetFlightState(), FlightBefore);
	TestTrue(TEXT("A dock signal cannot move the spacecraft"), W.Ship->GetActorTransform().Equals(ShipBefore));
	TestEqual(TEXT("A dock signal cannot drive the landing gear"), W.Ship->FindComponentByClass<UJTSSpacecraftPresentationComponent>()->GetGearDeployAlpha(), GearBefore);
	W.Advance(0.4f);
	TestTrue(TEXT("Both modules finish retracting"), W.Modules->AreAllStowed());
	W.Modules->TickComponent(0.05f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Finishing a dock animation cannot change flight state"), W.Ship->GetFlightState(), FlightBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSFolioDockProductionTest, "JTS.Ship.Docks.FolioFitsInsideArmAndCentralConsole",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSFolioDockProductionTest::RunTest(const FString&)
{
	FDockWorld W;
	UClass* ShipClass = LoadClass<AJTSSpacecraftActor>(nullptr, TEXT("/Game/Space/Blueprints/Ships/BP_Spacecraft.BP_Spacecraft_C"));
	AJTSSpacecraftActor* Ship = W.World->SpawnActor<AJTSSpacecraftActor>(ShipClass, FVector(5000,0,0), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Production ship loads"), Ship)) return false;
	Ship->DispatchBeginPlay();
	UJTSShipDockComponent* Modules = Ship->GetDockComponent();
	Modules->SetComponentTickEnabled(false);
	AJTSShipDockActor* Terminal = Modules->GetTerminal();
	if (!TestNotNull(TEXT("Built-in central screen is spawned independently"), Terminal)) return false;
	TestFalse(TEXT("Central screen is not a retractable progression plugin"), Terminal->bDeployable);
	Modules->RestoreInstalledIds({});
	TestTrue(TEXT("An empty module loadout retains the built-in screen"), IsValid(Terminal) && Terminal->IsDeployed());
	Modules->RestoreInstalledIds({TEXT("Weapon"),TEXT("Material")});
	for (TActorIterator<AJTSShipDockActor> It(W.World); It; ++It)
	{
		AJTSShipDockActor* Dock = *It;
		if (Dock->GetOwner() != Ship || !Dock->bDeployable) continue;
		Dock->DeploymentDuration = Dock->RetractionDuration = 1.f;
		Dock->SetDeployed(true); W.Advance(1.05f); Dock->Tick(0.05f);
		const FBox TrayBounds = Dock->Tray->Bounds.GetBox();
		APawn* NearbyPawn = W.World->SpawnActor<AJTSCharacter>();
		const FVector Front=Dock->GetActorRightVector();
		const float TrayDepth=FVector::DotProduct(TrayBounds.GetExtent().GetAbs(),Front.GetAbs());
		NearbyPawn->SetActorLocation(TrayBounds.GetCenter()+Front*(TrayDepth+140));
		TestTrue(TEXT("The front of a protruding device works beyond the old screen-only radius"),
			FVector::Dist(NearbyPawn->GetActorLocation(), Dock->GetScreenWorldLocation()) > Dock->InteractionDistance && Dock->CanUseDock(NearbyPawn));
		TestEqual(TEXT("Expanded equipment blocks walkers"), Dock->Tray->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
		TestEqual(TEXT("Expanded equipment has active collision"), Dock->Tray->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
		NearbyPawn->Destroy();
		Dock->SetDeployed(false); W.Advance(0.20f); Dock->Tick(0.05f);
		TestTrue(TEXT("Folio closes while the service carriage is still fully lowered"),
			Dock->Carriage->GetRelativeLocation().Equals(Dock->DeployedOffset, 0.1f));
		W.Advance(0.85f); Dock->Tick(0.05f);
		TestTrue(TEXT("Retraction finishes before departure"), Dock->IsStowed());
		for (UStaticMeshComponent* Part : {Dock->Body.Get(),Dock->Screen.Get(),Dock->Tray.Get(),Dock->LeftStrut.Get(),Dock->RightStrut.Get()})
		{
			if (!TestNotNull(TEXT("Moving part has a production mesh"), Part->GetStaticMesh().Get())) continue;
			const FTransform Local = Part->GetComponentTransform().GetRelativeTransform(Dock->GetActorTransform());
			const FBox Box = Part->GetStaticMesh()->GetBoundingBox().TransformBy(Local);
			TestTrue(TEXT("Folded hardware is behind the arm skin, including screen and tray"), Box.Max.Y < -25.f);
			TestTrue(TEXT("Folded hardware fits vertically inside the arm bay"), Box.Min.Z > -60.f && Box.Max.Z < 80.f);
		}
	}
	auto* Manager = W.World->SpawnActor<AJTSSpaceWorldManager>();
	auto* Planet = W.World->SpawnActor<AJTSPlanetAnchor>();
	Manager->SetCurrentPlanet(Planet); Manager->SetSurfaceGameplayReady(true); Ship->SetGroundedPlanet(Planet);
	APawn* Pawn = W.World->SpawnActor<AJTSCharacter>();
	const FVector Normal = Terminal->ScreenAnchor->GetRightVector();
	TestEqual(TEXT("The whole-ship flight proxy cannot be an invisible wall for walkers"),
		Ship->GetFlightCollisionComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
	FHitResult PassageHit;
	const FVector ScreenPoint = Terminal->GetScreenWorldLocation();
	const bool bPassageBlocked = W.World->SweepSingleByChannel(PassageHit,
		ScreenPoint + Normal * 350 - FVector(0,0,60), ScreenPoint + Normal * 145 - FVector(0,0,60),
		FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34,88));
	TestFalse(FString::Printf(TEXT("A walking capsule can reach the central console (blocker=%s)"), *GetNameSafe(PassageHit.GetComponent())), bPassageBlocked);
	Pawn->SetActorLocation(Terminal->GetScreenWorldLocation()+Normal*120);
	TestTrue(TEXT("Central screen authorizes ship terminal operations from its front"), Ship->CanUseShipTerminal(Pawn));
	AJTSPortableCredentialDock* Communicator = nullptr;
	for (TActorIterator<AJTSPortableCredentialDock> It(W.World); It; ++It)
		if (It->GetOwner() == Terminal) Communicator = *It;
	if (TestNotNull(TEXT("Central terminal supplies the physical communicator"), Communicator))
	{
		const FVector DevicePoint = Communicator->GetInteractionTargetWorldLocation();
		Pawn->SetActorLocation(DevicePoint + Normal * 120);
		TestTrue(TEXT("Exposed communicator has its own usable target"), Communicator->CanInteract_Implementation(Pawn));
		auto* Occluder = W.World->SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Occluder);
		Occluder->SetRootComponent(Box); Box->RegisterComponent();
		const FVector Screen = Terminal->GetScreenWorldLocation();
		Box->SetWorldLocation((Pawn->GetPawnViewLocation() + Screen) * .5);
		Box->SetBoxExtent(FVector(8)); Box->SetCollisionProfileName(TEXT("BlockAll"));
		TestFalse(TEXT("A blocked main screen is rejected"), Terminal->CanUseDock(Pawn));
		TestTrue(TEXT("An exposed device does not borrow the blocked screen's LOS"), Communicator->CanInteract_Implementation(Pawn));
		TestEqual(TEXT("Communicator aim volume is query-only"), Communicator->InteractionBounds->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
		TestEqual(TEXT("Communicator aim volume never blocks a walking player"), Communicator->InteractionBounds->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
		Occluder->Destroy();
		auto* Inventory=Pawn->FindComponentByClass<UJTSInventoryComponent>();
		Inventory->RestoreItems({UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::Pickaxe),UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::Pickaxe)},0);
		TestFalse(TEXT("Full inventory silently rejects pickup"),Communicator->CanExecuteInteraction_Implementation(Pawn));
		TestEqual(TEXT("Pickup prompt stays concise even when full"),Communicator->GetInteractionPrompt_Implementation(Pawn).ToString(),FString(TEXT("[E] 拿起")));
		Inventory->RestoreItems({},0);
		TestEqual(TEXT("Picking up the communicator requires no hold"),Communicator->GetInteractionHoldDuration_Implementation(Pawn),0.f);
		Communicator->Interact_Implementation(Pawn);
		const FGuid DeviceId=Inventory->GetActiveItem().InstanceId;
		TestTrue(TEXT("One press equips a physical instance"),DeviceId.IsValid() && Inventory->GetActiveItemId()==EJTSItemId::PortableCommunicator);
		TestFalse(TEXT("Taking removes the device from its cradle"),Communicator->bAvailable || Communicator->Device->IsVisible());
		Inventory->RecordActiveCredentials({TEXT("RoundTripPermit")});
		TestEqual(TEXT("An empty cradle offers return"),Communicator->GetInteractionPrompt_Implementation(Pawn).ToString(),FString(TEXT("[E] 放回")));
		Communicator->Interact_Implementation(Pawn);
		TestEqual(TEXT("Returning extracts the held item"),Inventory->GetItemCount(EJTSItemId::PortableCommunicator),0);
		TestTrue(TEXT("Returning restores the visible device"),Communicator->bAvailable && Communicator->Device->IsVisible());
		Communicator->Interact_Implementation(Pawn);
		TestEqual(TEXT("Taking again preserves the same physical identity"),Inventory->GetActiveItem().InstanceId,DeviceId);
		TestTrue(TEXT("Returning preserves the recorded permit"),Inventory->GetActiveItem().CredentialIds.Contains(TEXT("RoundTripPermit")));
		auto* Other=W.World->SpawnActor<AJTSCharacter>();
		Other->SetActorLocation(DevicePoint+Normal*120);
		Other->FindComponentByClass<UJTSInventoryComponent>()->RestoreItems({},0);
		Communicator->Interact_Implementation(Other);
		TestEqual(TEXT("Another player cannot duplicate a carried device"),Other->FindComponentByClass<UJTSInventoryComponent>()->GetItemCount(EJTSItemId::PortableCommunicator),0);
		TestTrue(TEXT("An empty cradle stays quiet without a held communicator"),Communicator->GetInteractionPrompt_Implementation(Other).IsEmpty());
		Other->Destroy();
		Communicator->Interact_Implementation(Pawn);
		Pawn->SetActorLocation(DevicePoint + Normal * 1000);
		TestFalse(TEXT("Device-specific target still rejects remote pickup"), Communicator->CanInteract_Implementation(Pawn));
	}
	Pawn->SetActorLocation(Terminal->GetScreenWorldLocation()-Normal*120);
	TestFalse(TEXT("The opposite side of the hull cannot authorize terminal requests"), Ship->CanUseShipTerminal(Pawn));
	TestFalse(TEXT("The hull itself never executes E interactions"), IInteractable::Execute_CanExecuteInteraction(Ship,Pawn));
	TestFalse(TEXT("The hull prompt does not advertise a global E action"), IInteractable::Execute_GetInteractionPrompt(Ship,Pawn).ToString().Contains(TEXT("[E]")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSDockContinuousAnimationTest, "JTS.Ship.Docks.ContinuousFramesAndParkedJitter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSDockContinuousAnimationTest::RunTest(const FString&)
{
	FDockWorld W; W.Modules->RestoreInstalledIds({TEXT("Material")});
	AJTSShipDockActor* Dock = W.Docks()[0];
	W.Modules->TickComponent(0.7f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Settling starts deployment"), Dock->IsActorTickEnabled());
	int32 FrozenFrames = 0;
	const TArray<USceneComponent*> Parts{Dock->Carriage.Get(), Dock->ScreenHinge.Get(), Dock->TrayHinge.Get(), Dock->UpperBayDoor.Get()};
	for (int32 Frame = 0; Frame < 180; ++Frame)
	{
		TArray<FTransform> Previous;
		for (auto* Part : Parts) Previous.Add(Part->GetRelativeTransform());
		W.World->Tick(LEVELTICK_All, 1.f/60.f); Dock->Tick(1.f/60.f);
		double Motion = 0;
		for (int32 I=0; I<Parts.Num(); ++I)
		{
			const FTransform Now = Parts[I]->GetRelativeTransform();
			Motion += FVector::Dist(Now.GetLocation(),Previous[I].GetLocation())
				+ Now.GetRotation().AngularDistance(Previous[I].GetRotation()) * 50;
		}
		if (Motion < 0.00001) ++FrozenFrames;
		W.Ship->AddActorWorldOffset(FVector(1,0,0));
		W.Modules->TickComponent(1.f/60.f,LEVELTICK_All,nullptr);
	}
	TestEqual(TEXT("Every active frame advances the mechanism; parked jitter never restarts it"),FrozenFrames,0);
	TestTrue(TEXT("Deployment survives small parked-body adjustments"),Dock->GetDeploymentAlpha()>0.9f);
	const float BeforeReverse = Dock->GetDeploymentAlpha();
	W.Modules->NotifyTakeoff();
	TestTrue(TEXT("A retract signal reverses from the current pose without snapping"),FMath::IsNearlyEqual(Dock->GetDeploymentAlpha(),BeforeReverse,0.0001f));
	W.Advance(0.2f);
	const float BeforeDeploy = Dock->GetDeploymentAlpha(); Dock->SetDeployed(true);
	TestTrue(TEXT("A new landing signal can reverse an unfinished retract continuously"),FMath::IsNearlyEqual(Dock->GetDeploymentAlpha(),BeforeDeploy,0.0001f));
	return true;
}
#endif
