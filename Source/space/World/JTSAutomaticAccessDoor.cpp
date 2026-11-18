#include "space/World/JTSAutomaticAccessDoor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "space/Components/JTSFacilityConditionComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/World/JTSSurfacePlacementBounds.h"
#include "Engine/GameInstance.h"

AJTSAutomaticAccessDoor::AJTSAutomaticAccessDoor()
{
    bReplicates=true;
    PrimaryActorTick.bCanEverTick=false;
    SceneRoot=CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    Frame=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
    LeftLeaf=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftLeaf"));
    RightLeaf=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightLeaf"));
    Reader=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Reader"));
    StatusLight=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatusLight"));
    for (auto* Mesh : {Frame.Get(),LeftLeaf.Get(),RightLeaf.Get(),Reader.Get(),StatusLight.Get()})
    {
        Mesh->SetupAttachment(SceneRoot);
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    }
    StatusLight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftLeaf->SetRelativeLocation(FVector(0,-70,0));
    RightLeaf->SetRelativeLocation(FVector(0,70,0));
    Conditions=CreateDefaultSubobject<UJTSFacilityConditionComponent>(TEXT("Conditions"));
    LockedPrompt=NSLOCTEXT("JTS","AccessDoorLocked","未识别到有效通行许可");
    AuthorizedPrompt=NSLOCTEXT("JTS","AccessDoorGranted","通行许可核验通过 · 靠近自动开门");
    MissingCredentialPrompt=NSLOCTEXT("JTS","AccessDoorMissingPermit","没有许可");
    DisplayName=NSLOCTEXT("JTS","AccessDoorLabel","门");
}

void AJTSAutomaticAccessDoor::InitializePresentation()
{
    if (bPresentationReady) return;
    LeftClosed=LeftLeaf->GetRelativeLocation();
    RightClosed=RightLeaf->GetRelativeLocation();
    bPresentationReady=true;
}

void AJTSAutomaticAccessDoor::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    if (GetWorld()->IsGameWorld()) InitializePresentation();
}

void AJTSAutomaticAccessDoor::BeginPlay()
{
    Super::BeginPlay();
    InitializePresentation();
    OnRep_DoorState();
    if (HasAuthority())
    {
        Refresh();
        GetWorldTimerManager().SetTimer(PresenceTimer,this,&ThisClass::Refresh,.1f,true);
    }
}

void AJTSAutomaticAccessDoor::Refresh()
{
    const auto* State=GetWorld()->GetGameState<AJTSGameState>();
    const bool Latched=!UnlockedFlag.IsNone() && State && State->FacilityFlagIds.Contains(UnlockedFlag);
    const bool Authorized=Conditions->IsSatisfied() && (!bRequireManualActivation || Latched);
    bool Nearby=false, Occupied=false;
    for (TActorIterator<AJTSCharacter> It(GetWorld());It;++It)
    {
        if (It->IsBoarded()) continue;
        const FVector Local=GetActorTransform().InverseTransformPosition(It->GetActorLocation())-DetectionCenter;
        if (FBox(-SafetyHalfExtents,SafetyHalfExtents).IsInsideOrOn(Local)) Occupied=true;
        if (!It->GetHealthComponent()->IsDead() && FBox(-ActivationHalfExtents,ActivationHalfExtents).IsInsideOrOn(Local)) Nearby=true;
    }
    const double Now=GetWorld()->GetTimeSeconds();
    if ((Authorized && Nearby) || (bOpen && Occupied)) LastPresenceTime=Now;
    // The safety volume holds an already open door; it never authorizes a closed door.
    const bool Open=(Authorized && Nearby) || (bOpen && (Occupied || Now-LastPresenceTime<FMath::Max(0.f,CloseDelay)));
    if (bAuthorized!=Authorized || bOpen!=Open)
    {
        bAuthorized=Authorized;
        bOpen=Open;
        OnRep_DoorState();
        ForceNetUpdate();
    }
}

void AJTSAutomaticAccessDoor::OnRep_DoorState()
{
    // Initial network state can arrive before a client's BeginPlay.
    InitializePresentation();
    OnRep_ReaderState();
    if (!FMath::IsNearlyEqual(OpenFraction,bOpen?1.f:0.f) && !GetWorldTimerManager().IsTimerActive(SlideTimer))
    {
        LastSlideTime=GetWorld()->GetTimeSeconds();
        GetWorldTimerManager().SetTimer(SlideTimer,this,&ThisClass::AdvanceSlide,.03f,true);
    }
}

void AJTSAutomaticAccessDoor::AdvanceSlide()
{
    const double Now=GetWorld()->GetTimeSeconds();
    const float DeltaSeconds=FMath::Max(0.,Now-LastSlideTime);
    LastSlideTime=Now;
    const float Target=bOpen?1.f:0.f;
    OpenFraction=FMath::FInterpConstantTo(OpenFraction,Target,DeltaSeconds,1.f/FMath::Max(.1f,SlideSeconds));
    const FVector Offset=LeafTravel*FMath::SmoothStep(0.f,1.f,OpenFraction);
    LeftLeaf->SetRelativeLocation(LeftClosed-Offset);
    RightLeaf->SetRelativeLocation(RightClosed+Offset);
    if (OpenFraction==Target)
    {
        GetWorldTimerManager().ClearTimer(SlideTimer);
    }
}

FVector AJTSAutomaticAccessDoor::GetInteractionTargetWorldLocation() const
{
    // A side-mounted swipe reader is the target, rather than the center of the whole doorway.
    return bRequireManualActivation && Reader->GetStaticMesh()
        ? Reader->Bounds.Origin : GetComponentsBoundingBox(true).GetCenter();
}

FVector AJTSAutomaticAccessDoor::GetInteractionPromptAnchor_Implementation() const
{
    FVector Top;
    return bRequireManualActivation && JTSSurfacePlacementBounds::GetVisualTopCenter(Reader,GetActorUpVector(),16.f,Top)
        ? Top : IInteractable::GetInteractionPromptAnchor_Implementation();
}

bool AJTSAutomaticAccessDoor::CanInteract_Implementation(APawn* Pawn) const
{
    const auto* Character=Cast<AJTSCharacter>(Pawn);
    if (!Character || Character->IsBoarded() || Character->GetHealthComponent()->IsDead()) return false;
    const FVector Local=GetActorTransform().InverseTransformPosition(Character->GetActorLocation())-DetectionCenter;
    return FBox(-ActivationHalfExtents,ActivationHalfExtents).ExpandBy(100).IsInsideOrOn(Local);
}

bool AJTSAutomaticAccessDoor::HasHeldCredential(APawn* Pawn) const
{
    if (RequiredHeldItem==EJTSItemId::None && RequiredCredentialId.IsNone()) return true;
    const auto* Inventory=Pawn?Pawn->FindComponentByClass<UJTSInventoryComponent>():nullptr;
    if (!Inventory) return false;
    const auto Item=Inventory->GetActiveItem();
    return !Item.IsEmpty() && (RequiredHeldItem==EJTSItemId::None || Item.ItemId==RequiredHeldItem)
        && (RequiredCredentialId.IsNone() || Item.CredentialIds.Contains(RequiredCredentialId));
}
bool AJTSAutomaticAccessDoor::HasRequiredDevice(APawn* Pawn) const
{
    const auto* Inventory=Pawn?Pawn->FindComponentByClass<UJTSInventoryComponent>():nullptr;
    if (RequiredHeldItem==EJTSItemId::None)
        return RequiredCredentialId.IsNone() || (Inventory && !Inventory->GetActiveItem().IsEmpty());
    return Inventory && Inventory->GetActiveItemId()==RequiredHeldItem;
}
bool AJTSAutomaticAccessDoor::CanExecuteInteraction_Implementation(APawn* Pawn) const
{
    return bRequireManualActivation && !bAuthorized && !UnlockedFlag.IsNone()
        && ReaderState==EJTSAccessReaderState::Idle
        && CanInteract_Implementation(Pawn) && Conditions->IsSatisfied() && HasRequiredDevice(Pawn);
}
void AJTSAutomaticAccessDoor::Interact_Implementation(APawn* Pawn)
{
    if (!HasAuthority() || !CanExecuteInteraction_Implementation(Pawn)) return;
    if (RequiredHeldItem!=EJTSItemId::None || !RequiredCredentialId.IsNone())
    {
        ScanningPawn=Pawn;
        ScanningItemId=Pawn->FindComponentByClass<UJTSInventoryComponent>()->GetActiveItem().InstanceId;
        ReaderState=EJTSAccessReaderState::Reading;
        OnRep_ReaderState();ForceNetUpdate();
        GetWorldTimerManager().SetTimer(ReaderTimer,this,&ThisClass::CompleteReaderScan,FMath::Max(.1f,SwipeSeconds),false);
        return;
    }
    ActivateDoor();
}
void AJTSAutomaticAccessDoor::ActivateDoor()
{
    if (auto* Expedition=GetGameInstance()?GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>():nullptr)
    {
        Expedition->SetFacilityFlag(UnlockedFlag);
        Refresh();
        // A reader may sit outside the doorway's proximity volume. A successful press opens it immediately.
        if (bAuthorized)
        {
            LastPresenceTime=GetWorld()->GetTimeSeconds();bOpen=true;
            OnRep_DoorState();ForceNetUpdate();
        }
    }
}
void AJTSAutomaticAccessDoor::CompleteReaderScan()
{
    APawn* Pawn=ScanningPawn.Get();
    const auto* Inventory=Pawn?Pawn->FindComponentByClass<UJTSInventoryComponent>():nullptr;
    // Read this physical item on the server. A shared expedition permit cannot substitute for its payload.
    if (!Pawn || !CanInteract_Implementation(Pawn) || !Conditions->IsSatisfied() || !HasRequiredDevice(Pawn)
        || !Inventory || Inventory->GetActiveItem().InstanceId!=ScanningItemId)
    {
        ResetReader();return;
    }
    if (HasHeldCredential(Pawn))
    {
        ResetReader();ActivateDoor();
    }
    else
    {
        ReaderState=EJTSAccessReaderState::Denied;OnRep_ReaderState();ForceNetUpdate();
        GetWorldTimerManager().SetTimer(ReaderTimer,this,&ThisClass::ResetReader,1.4f,false);
    }
}
void AJTSAutomaticAccessDoor::ResetReader()
{
    ScanningPawn.Reset();ScanningItemId.Invalidate();
    ReaderState=EJTSAccessReaderState::Idle;OnRep_ReaderState();ForceNetUpdate();
}
void AJTSAutomaticAccessDoor::OnRep_ReaderState()
{
    UMaterialInterface* Material=bAuthorized?AuthorizedMaterial.Get():LockedMaterial.Get();
    if (ReaderState==EJTSAccessReaderState::Reading && ReadingMaterial) Material=ReadingMaterial;
    if (ReaderState==EJTSAccessReaderState::Denied && DeniedMaterial) Material=DeniedMaterial;
    if (Material) StatusLight->SetMaterial(0,Material);
}
FText AJTSAutomaticAccessDoor::GetInteractionPrompt_Implementation(APawn* Pawn) const
{
    if (bRequireManualActivation && !bAuthorized)
    {
        if (!Conditions->IsSatisfied()) return LockedPrompt;
        if (!HasRequiredDevice(Pawn)) return MissingDevicePrompt;
        if (ReaderState==EJTSAccessReaderState::Reading) return NSLOCTEXT("JTS","AccessReaderReading","读取中");
        if (ReaderState==EJTSAccessReaderState::Denied) return MissingCredentialPrompt;
        return ActivationPrompt;
    }
    return bAuthorized?AuthorizedPrompt:LockedPrompt;
}

void AJTSAutomaticAccessDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AJTSAutomaticAccessDoor,bOpen);
    DOREPLIFETIME(AJTSAutomaticAccessDoor,bAuthorized);
    DOREPLIFETIME(AJTSAutomaticAccessDoor,ReaderState);
}

void AJTSAutomaticAccessDoor::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(PresenceTimer);
    GetWorldTimerManager().ClearTimer(SlideTimer);
    GetWorldTimerManager().ClearTimer(ReaderTimer);
    Super::EndPlay(Reason);
}
