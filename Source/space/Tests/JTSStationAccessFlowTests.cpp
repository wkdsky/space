#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "space/World/JTSAutomaticAccessDoor.h"
#include "space/Components/JTSFacilityConditionComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Components/TextBlock.h"
#include "Widgets/SWidget.h"
#include "UObject/UnrealType.h"
#include "space/Player/JTSPlayerController.h"
#include "space/UI/JTSPrototypeHUDWidget.h"
#include "space/Interaction/InteractionComponent.h"
#include "space/World/JTSFacilityController.h"
#include "space/Items/JTSReadableDocument.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "space/Components/JTSMeleeComponent.h"
#include "space/World/JTSPlanetEcologyHabitat.h"
#include "Components/StaticMeshComponent.h"
#include "space/World/JTSSurfacePlacementBounds.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSStationPhysicalCredential,"JTS.Mars.PhysicalCredentialAndPowerLocks",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSStationPhysicalCredential::RunTest(const FString&)
{
    const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W); W->InitializeActorsForPlay(FURL());
    auto* State=W->SpawnActor<AJTSGameState>(); W->SetGameState(State);
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FTransform Frame(FRotator(31,18,75),FVector(8000,9000,10000));
    auto* Gate=W->SpawnActor<AJTSAutomaticAccessDoor>(Frame.GetLocation(),Frame.Rotator(),Params);
    Gate->bRequireManualActivation=true; Gate->RequiredHeldItem=EJTSItemId::PortableCommunicator;
    Gate->RequiredCredentialId=TEXT("Permit"); Gate->UnlockedFlag=TEXT("GateOpened");
    Gate->MissingDevicePrompt=FText::FromString(TEXT("拿通讯器"));
    Gate->ActivationPrompt=FText::FromString(TEXT("[E] 刷门禁"));Gate->SwipeSeconds=.2f;
    Gate->Reader->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Gate->Reader->SetRelativeLocation(FVector(45,-220,145));Gate->Reader->SetRelativeScale3D(FVector(.2,.4,.6));
    FVector ReaderTop;JTSSurfacePlacementBounds::GetVisualTopCenter(Gate->Reader,Gate->GetActorUpVector(),16.f,ReaderTop);
    TestTrue(TEXT("Reader prompt is above the side reader on a tilted planet"),Gate->GetInteractionPromptAnchor_Implementation().Equals(ReaderTop,.1));
    auto* Member=W->SpawnActor<AJTSCharacter>(Frame.TransformPosition(FVector(250,0,150)),FRotator::ZeroRotator,Params);
    Member->GetCharacterMovement()->SetComponentTickEnabled(false);
    auto* Inventory=Member->FindComponentByClass<UJTSInventoryComponent>();
    W->BeginPlay();W->GetWorldSettings()->NotifyBeginPlay();
    const uint64 PreviousFrame=GFrameCounter;
    auto Advance=[&](float Seconds){for(int I=0;I<FMath::CeilToInt(Seconds/.05f);++I){++GFrameCounter;W->Tick(LEVELTICK_All,.05f);}};
    State->SetImportantItems({TEXT("Permit")});Advance(1);
    TestFalse(TEXT("Shared lunar permit and proximity cannot open a manual gate"),Gate->bOpen);
    TestFalse(TEXT("An empty hand cannot swipe"),Gate->CanExecuteInteraction_Implementation(Member));
    TestEqual(TEXT("An empty hand only asks for the device"),Gate->GetInteractionPrompt_Implementation(Member).ToString(),FString(TEXT("拿通讯器")));
    auto Device=UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::PortableCommunicator);
    Inventory->RestoreItems({Device},0);
    TestTrue(TEXT("An unprogrammed physical communicator can be presented to the reader"),Gate->CanExecuteInteraction_Implementation(Member));
    TestEqual(TEXT("Before a scan the reader offers E, without revealing the item's permit"),Gate->GetInteractionPrompt_Implementation(Member).ToString(),FString(TEXT("[E] 刷门禁")));
    TestEqual(TEXT("A single press starts reading; holding E is unnecessary"),Gate->GetInteractionHoldDuration_Implementation(Member),0.f);
    Gate->Interact_Implementation(Member);
    TestTrue(TEXT("A deliberate press starts the reader"),Gate->ReaderState==EJTSAccessReaderState::Reading);
    TestFalse(TEXT("Starting a read cannot open the gate"),Gate->bOpen);
    Advance(.3f);
    TestTrue(TEXT("The missing permit is detected only after the read"),Gate->ReaderState==EJTSAccessReaderState::Denied);
    TestEqual(TEXT("A rejected scan gives concise feedback"),Gate->GetInteractionPrompt_Implementation(Member).ToString(),FString(TEXT("没有许可")));
    TestFalse(TEXT("The expedition's shared permit cannot replace the physical item payload"),Gate->bOpen || Gate->bAuthorized);
    Advance(1.5f);
    TestEqual(TEXT("After feedback the reader offers another deliberate scan"),Gate->GetInteractionPrompt_Implementation(Member).ToString(),FString(TEXT("[E] 刷门禁")));
    Device.CredentialIds={TEXT("OtherPermit")};Inventory->RestoreItems({Device},0);
    Gate->Interact_Implementation(Member);Advance(.3f);
    TestTrue(TEXT("An unrelated embedded permit is rejected after reading"),Gate->ReaderState==EJTSAccessReaderState::Denied);
    Advance(1.5f);
    Device.CredentialIds={TEXT("Permit")};Inventory->RestoreItems({Device},0);
    TestTrue(TEXT("Holding the programmed communicator enables the swipe"),Gate->CanExecuteInteraction_Implementation(Member));
    Advance(1);TestFalse(TEXT("Even the programmed item never opens from proximity alone"),Gate->bOpen);
    Gate->Interact_Implementation(Member);Inventory->RestoreItems({},0);Advance(.3f);
    TestTrue(TEXT("Putting away the device during a read cancels it"),Gate->ReaderState==EJTSAccessReaderState::Idle && !Gate->bOpen);
    Inventory->RestoreItems({Device},0);
    State->FacilityFlagIds.Add(TEXT("GateOpened"));Advance(1.2f);
    TestTrue(TEXT("The server latch opens the physical tilted-world gate"),Gate->bOpen && Gate->bAuthorized);
    auto* Lift=W->SpawnActor<AJTSAutomaticAccessDoor>(Frame.GetLocation(),Frame.Rotator(),Params);
    Lift->bRequireManualActivation=true;Lift->UnlockedFlag=TEXT("LiftOpened");
    FJTSFacilityRequirement Power;Power.Id=TEXT("Power");Lift->Conditions->Requirements.Add(Power);
    Advance(1);TestFalse(TEXT("A programmed communicator cannot bypass an unpowered lift"),Lift->CanExecuteInteraction_Implementation(Member));
    State->FacilityFlagIds.Add(TEXT("Power"));Advance(1);
    TestFalse(TEXT("Restoring power does not automatically open the lift door"),Lift->bOpen);
    TestTrue(TEXT("Power enables a deliberate lift-door interaction"),Lift->CanExecuteInteraction_Implementation(Member));
    TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FObjectAndNameAsStringProxyArchive Out(Writer,false);FJTSItemInstance::StaticStruct()->SerializeItem(Out,&Device,nullptr);
    FJTSItemInstance Loaded;FMemoryReader Reader(Bytes);FObjectAndNameAsStringProxyArchive In(Reader,true);FJTSItemInstance::StaticStruct()->SerializeItem(In,&Loaded,nullptr);
    TestEqual(TEXT("Physical identity survives serialization"),Loaded.InstanceId,Device.InstanceId);
    TestTrue(TEXT("The recorded permit survives drops, transfers and save serialization"),Loaded.CredentialIds.Contains(TEXT("Permit")));
    Loaded.Clear();TestTrue(TEXT("Clearing an item removes its authorization"),Loaded.CredentialIds.IsEmpty());
    W->EndPlay(EEndPlayReason::Quit);W->DestroyWorld(false);GEngine->DestroyWorldContext(W);GFrameCounter=PreviousFrame;
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSGenericInteractableHud,"JTS.UI.GenericInteractablePrompt",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSGenericInteractableHud::RunTest(const FString&)
{
    const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());
    W->SetGameInstance(NewObject<UGameInstance>(GEngine));
    auto* State=W->SpawnActor<AJTSGameState>();W->SetGameState(State);State->SetGameplayPhase(EJTSGameplayPhase::EarthCollection);
    auto* Controller=W->SpawnActor<AJTSPlayerController>();auto* LocalPlayer=NewObject<ULocalPlayer>(GEngine);
    Controller->Player=LocalPlayer;LocalPlayer->PlayerController=Controller;
    LocalPlayer->Origin=FVector2D::ZeroVector;LocalPlayer->Size=FVector2D(1,1);
    auto* ViewportClient=NewObject<UGameViewportClient>(GEngine);
    const TSharedRef<SViewport> ViewportWidget=SNew(SViewport);
    FSceneViewport Viewport(ViewportWidget);
    Viewport.SetViewportClient(TStrongPtrVariant<FViewportClient>(ViewportClient));
    Viewport.SetInitialSize(FIntPoint(1280,720));
    ViewportClient->Viewport=&Viewport;LocalPlayer->ViewportClient=ViewportClient;
    auto* Character=W->SpawnActor<AJTSCharacter>();Character->DispatchBeginPlay();Controller->Possess(Character);
    auto* Widget=CreateWidget<UJTSPrototypeHUDWidget>(Controller);
    auto Cleanup=[&]{if(Widget)Widget->ConditionalBeginDestroy();ViewportClient->Viewport=nullptr;LocalPlayer->ViewportClient=nullptr;Controller->Player=nullptr;LocalPlayer->PlayerController=nullptr;W->DestroyWorld(false);GEngine->DestroyWorldContext(W);};
    if(!TestNotNull(TEXT("The actual HUD constructs"),Widget)){Cleanup();return false;}
    const auto SlateWidget=Widget->TakeWidget();
    auto* Prompt=Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("InteractionPromptText")));
    if(!TestNotNull(TEXT("The actual HUD prompt exists"),Prompt)){Cleanup();return false;}
    auto* Interaction=Character->FindComponentByClass<UInteractionComponent>();
    auto* TargetProperty=FindFProperty<FObjectProperty>(Interaction->GetClass(),TEXT("CurrentInteractable"));
    auto* Facility=W->SpawnActor<AJTSFacilityController>();Facility->SetActorLocation(Character->GetActorLocation()+FVector(120,0,0));
    Facility->CompletedFlag=TEXT("HudTestRepair");Facility->DisplayName=FText::FromString(TEXT("Repair combiner"));
    auto* Melee=Character->FindComponentByClass<UJTSMeleeComponent>();
    auto* MeleeTargetProperty=FindFProperty<FObjectProperty>(Melee->GetClass(),TEXT("CurrentMeleeTarget"));
    auto* NearbyNest=W->SpawnActor<AJTSPlanetEcologyHabitat>();NearbyNest->SetActorLocation(FVector(180,80,0));
    MeleeTargetProperty->SetObjectPropertyValue_InContainer(Melee,NearbyNest);
    TargetProperty->SetObjectPropertyValue_InContainer(Interaction,Facility);SlateWidget->Tick(FGeometry(),1.,.016f);
    TestEqual(TEXT("The selected facility shows name then action"),Prompt->GetText().ToString(),Facility->DisplayName.ToString()+TEXT("\n")+Interaction->GetCurrentInteractionPrompt().ToString());
    TestFalse(TEXT("Generic interaction text is not collapsed"),Prompt->GetVisibility()==ESlateVisibility::Collapsed);
    auto* PromptSlot=Cast<UCanvasPanelSlot>(Prompt->Slot);
    FVector2D FacilityScreen;
    UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller,IInteractable::Execute_GetInteractionPromptAnchor(Facility),FacilityScreen,true);
    TestTrue(TEXT("Facility prompt is anchored above its own world target"),PromptSlot && PromptSlot->GetPosition().Equals(FacilityScreen,.1));
    TestTrue(TEXT("The prompt uses a world projection rather than a fixed screen centre"),PromptSlot && PromptSlot->GetAnchors().Minimum.IsNearlyZero());
    auto* Folder=W->SpawnActor<AJTSReadableDocument>();Folder->SetActorLocation(Character->GetActorLocation()+FVector(120,0,0));Folder->Document=NewObject<UJTSReadableDocumentData>();
    TargetProperty->SetObjectPropertyValue_InContainer(Interaction,Folder);SlateWidget->Tick(FGeometry(),2.,.016f);
    TestEqual(TEXT("Switching target replaces the name and action together"),Prompt->GetText().ToString(),Folder->DisplayName.ToString()+TEXT("\n")+Interaction->GetCurrentInteractionPrompt().ToString());
    FVector2D FolderScreen;
    UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller,IInteractable::Execute_GetInteractionPromptAnchor(Folder),FolderScreen,true);
    TestTrue(TEXT("Switching target also moves the one shared prompt"),PromptSlot && PromptSlot->GetPosition().Equals(FolderScreen,.1));
    MeleeTargetProperty->SetObjectPropertyValue_InContainer(Melee,nullptr);
    TargetProperty->SetObjectPropertyValue_InContainer(Interaction,nullptr);SlateWidget->Tick(FGeometry(),3.,.016f);
    TestTrue(TEXT("Looking away clears the previous generic prompt"),Prompt->GetText().IsEmpty());
    Cleanup();return true;
}
#endif
