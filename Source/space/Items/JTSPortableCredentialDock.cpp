#include "space/Items/JTSPortableCredentialDock.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Ships/JTSShipDockActor.h"
#include "space/World/JTSSurfacePlacementBounds.h"

AJTSPortableCredentialDock::AJTSPortableCredentialDock()
{
    bReplicates = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = false;
    Mount = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mount")); SetRootComponent(Mount);
    Device = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Device")); Device->SetupAttachment(Mount);
    Status = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Status")); Status->SetupAttachment(Mount);
    InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
    InteractionBounds->SetupAttachment(Mount);
    InteractionBounds->SetRelativeLocation(FVector(0,0,20));
    InteractionBounds->SetBoxExtent(FVector(22,28,26));
    InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBounds->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Mount->SetCollisionProfileName(TEXT("BlockAll"));
    Device->SetCollisionProfileName(TEXT("BlockAll"));
    Status->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AJTSPortableCredentialDock::BeginPlay()
{
    Super::BeginPlay(); OnRep_Available();
    Refresh(); GetWorldTimerManager().SetTimer(RefreshTimer,this,&ThisClass::Refresh,1.f,true);
}
TArray<FName> AJTSPortableCredentialDock::EarnedPermits() const
{
    TArray<FName> Result;
    const auto* State = GetWorld()->GetGameState<AJTSGameState>();
    for (FName Id : ScannablePermitIds) if (State && State->HasImportantItem(Id)) Result.AddUnique(Id);
    return Result;
}
void AJTSPortableCredentialDock::Refresh()
{
    if (auto* Material = EarnedPermits().IsEmpty()?EmptyMaterial.Get():ReadyMaterial.Get()) Status->SetMaterial(0,Material);
    if (!HasAuthority()) return;
    // Taking a device removes it from the cradle. A destroyed device can be replaced at the ship.
    bool Available = true;
    for (TActorIterator<AJTSCharacter> It(GetWorld()); It; ++It)
        if (const auto* Inventory = It->FindComponentByClass<UJTSInventoryComponent>(); Inventory && Inventory->GetItemCount(DeviceItemId)>0) { Available=false; break; }
    if (Available) for (TActorIterator<AJTSWorldPickupActor> It(GetWorld()); It; ++It)
        if (It->IsAvailableForShipScan() && It->GetItemInstance().ItemId == DeviceItemId) { Available=false; break; }
    if (Available) for (TActorIterator<AJTSPlayerState> It(GetWorld()); It; ++It)
        for (const auto& Slot : It->GetShipLockerSlots())
            if (!Slot.StandardItem.IsEmpty() && Slot.StandardItem.ItemId==DeviceItemId) { Available=false; break; }
    if (Available)
    {
        if (DockedItem.IsEmpty()) DockedItem=UJTSItemDefinitionLibrary::MakeInstance(DeviceItemId);
        for (FName Id : EarnedPermits()) DockedItem.CredentialIds.AddUnique(Id);
    }
    if (bAvailable != Available) { bAvailable=Available; OnRep_Available(); ForceNetUpdate(); }
}
FVector AJTSPortableCredentialDock::GetInteractionTargetWorldLocation() const
{
    return bAvailable && Device->GetStaticMesh() ? Device->Bounds.Origin : InteractionBounds->GetComponentLocation();
}
bool AJTSPortableCredentialDock::CanInteract_Implementation(APawn* Pawn) const
{
    const auto* Character = Cast<AJTSCharacter>(Pawn);
    const auto* Dock = Cast<AJTSShipDockActor>(GetOwner());
    return Character && !Character->IsBoarded() && !Character->GetHealthComponent()->IsDead()
        && Dock && Dock->CanUseDockAt(Pawn,GetInteractionTargetWorldLocation())
        && FVector::DistSquared(Pawn->GetActorLocation(),GetInteractionTargetWorldLocation()) <= FMath::Square(260.f);
}
bool AJTSPortableCredentialDock::CanExecuteInteraction_Implementation(APawn* Pawn) const
{
    if (!CanInteract_Implementation(Pawn)) return false;
    const auto* Inventory = Pawn->FindComponentByClass<UJTSInventoryComponent>();
    return Inventory && ((!bAvailable && Inventory->GetActiveItemId()==DeviceItemId)
        || (bAvailable && Inventory->CanAddItem(DeviceItemId)));
}
FText AJTSPortableCredentialDock::GetInteractionPrompt_Implementation(APawn* Pawn) const
{
    const auto* Inventory = Pawn?Pawn->FindComponentByClass<UJTSInventoryComponent>():nullptr;
    if (!bAvailable)
        return Inventory && Inventory->GetActiveItemId()==DeviceItemId
            ?NSLOCTEXT("JTS","CommsReturn","[E] 放回"):FText::GetEmpty();
    return NSLOCTEXT("JTS","CommsTake","[E] 拿起");
}
FText AJTSPortableCredentialDock::GetInteractionLabel_Implementation() const
{
    return UJTSItemDefinitionLibrary::GetItemDisplayName(DeviceItemId);
}
FVector AJTSPortableCredentialDock::GetInteractionPromptAnchor_Implementation() const
{
    FVector Top;
    const auto* Visual = bAvailable ? Device.Get() : Mount.Get();
    return JTSSurfacePlacementBounds::GetVisualTopCenter(Visual, GetActorUpVector(), 16.f, Top)
        ? Top : GetInteractionTargetWorldLocation() + GetActorUpVector() * 30.f;
}
void AJTSPortableCredentialDock::Interact_Implementation(APawn* Pawn)
{
    if (!HasAuthority()) return;
    Refresh();
    if (!CanExecuteInteraction_Implementation(Pawn)) return;
    auto* Inventory = Pawn->FindComponentByClass<UJTSInventoryComponent>();
    if (Inventory->GetActiveItemId()==DeviceItemId)
    {
        FJTSItemInstance Returned;
        if (!Inventory->TryExtractItemAtSlot(Inventory->GetSelectedQuickbarSlot(),Inventory->GetActiveItem().InstanceId,Returned)) return;
        DockedItem=MoveTemp(Returned);
    }
    else
    {
        const auto Item = DockedItem;
        int32 Remaining=1;
        if (!Inventory->TryAddItem(Item,Remaining) || Remaining) return;
        DockedItem.Clear();
        const auto& Slots=Inventory->GetItemSlots();
        for (int32 I=0; I<Slots.Num(); ++I) if (Slots[I].InstanceId==Item.InstanceId) { Inventory->SelectQuickbarSlot(I); break; }
    }
    Refresh();
}
void AJTSPortableCredentialDock::OnRep_Available() { Device->SetVisibility(bAvailable); Device->SetCollisionEnabled(bAvailable?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision); }
void AJTSPortableCredentialDock::EndPlay(EEndPlayReason::Type Reason) { GetWorldTimerManager().ClearTimer(RefreshTimer); Super::EndPlay(Reason); }
void AJTSPortableCredentialDock::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AJTSPortableCredentialDock,bAvailable); }
