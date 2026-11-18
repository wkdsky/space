#include "space/World/JTSItemSupplyRack.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "space/Components/JTSFacilityConditionComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Systems/JTSExpeditionSubsystem.h"

AJTSItemSupplyRack::AJTSItemSupplyRack()
{
    bReplicates=true; PrimaryActorTick.bCanEverTick=false;
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh")); SetRootComponent(Mesh);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    RefillConditions=CreateDefaultSubobject<UJTSFacilityConditionComponent>(TEXT("RefillConditions"));
}
void AJTSItemSupplyRack::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        int32 Count=InitialCount;
        if (const auto* E=GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>(); E && !CompletionFlags.IsEmpty())
        {
            Count=ReserveCount;
            for (FName Flag:CompletionFlags) Count+=!Flag.IsNone() && !E->GetSnapshot().FacilityFlagIds.Contains(Flag);
        }
        Slots.Init(0,FMath::Clamp(SlotCount,1,32));
        for (int32 I=0;I<FMath::Clamp(Count,0,Slots.Num());++I) Slots[I]=1;
        Refresh();
        GetWorldTimerManager().SetTimer(RefreshTimer,this,&ThisClass::Refresh,.25f,true);
    }
    OnRep_Slots();
}
int32 AJTSItemSupplyRack::GetSupplyCount() const { int32 Count=0; for (uint8 Slot:Slots) Count+=Slot!=0; return Count; }
int32 AJTSItemSupplyRack::GetAimedSlot(APawn* Pawn) const
{
    if (!Pawn) return INDEX_NONE;
    FVector Eye=Pawn->GetActorLocation(); FRotator Rotation=Pawn->GetViewRotation();
    if (const auto* PC=Cast<APlayerController>(Pawn->GetController()); PC && PC->PlayerCameraManager)
    {
        Eye=PC->PlayerCameraManager->GetCameraLocation();
        Rotation=PC->PlayerCameraManager->GetCameraRotation();
    }
    float Best=TNumericLimits<float>::Max(); int32 Selected=INDEX_NONE;
    for (int32 I=0;I<Slots.Num();++I)
    {
        const FVector Position=GetActorTransform().TransformPosition(SlotLocations.IsValidIndex(I)?SlotLocations[I]:FVector(0,0,100));
        const FVector Delta=Position-Eye;
        const float Along=FVector::DotProduct(Delta,Rotation.Vector());
        const float Distance=(Delta-Rotation.Vector()*Along).SizeSquared();
        if (Along>0 && Distance<Best) { Best=Distance; Selected=I; }
    }
    return Selected;
}
bool AJTSItemSupplyRack::CanInteract_Implementation(APawn* Pawn) const
{
    const auto* H=Pawn?Pawn->FindComponentByClass<UJTSHealthComponent>():nullptr;
    return H && !H->IsDead() && ItemId!=EJTSItemId::None && FVector::DistSquared(Pawn->GetActorLocation(),GetActorLocation())<=FMath::Square(250.f);
}
bool AJTSItemSupplyRack::CanExecuteInteraction_Implementation(APawn* Pawn) const
{
    if (!CanInteract_Implementation(Pawn)) return false;
    const int32 Slot=GetAimedSlot(Pawn);
    const auto* Inventory=Pawn->FindComponentByClass<UJTSInventoryComponent>();
    return Slots.IsValidIndex(Slot) && Slots[Slot]!=0 && Inventory && Inventory->CanAddItem(ItemId,1);
}
FText AJTSItemSupplyRack::GetInteractionLabel_Implementation() const { return UJTSItemDefinitionLibrary::GetItemDisplayName(ItemId); }
FText AJTSItemSupplyRack::GetInteractionPrompt_Implementation(APawn* Pawn) const
{
    const int32 Slot=GetAimedSlot(Pawn);
    if (!Slots.IsValidIndex(Slot) || !Slots[Slot]) return NSLOCTEXT("JTS","SupplyEmpty","格位为空");
    return CanExecuteInteraction_Implementation(Pawn)
        ? FText::Format(NSLOCTEXT("JTS","SupplyTakeItem","[E] 拿取{0}"),UJTSItemDefinitionLibrary::GetItemDisplayName(ItemId))
        : NSLOCTEXT("JTS","SupplyFull","物品栏已满");
}
void AJTSItemSupplyRack::Interact_Implementation(APawn* Pawn)
{
    if (!HasAuthority()) return;
    Refresh();
    if (!CanExecuteInteraction_Implementation(Pawn)) return;
    const int32 Slot=GetAimedSlot(Pawn);
    auto* Inventory=Pawn->FindComponentByClass<UJTSInventoryComponent>();
    if (!Inventory->TryAddItemById(ItemId,1)) return;
    Slots[Slot]=0;
    for (int32 I=0;I<Inventory->GetItemSlots().Num();++I)
        if (Inventory->GetItemAtSlot(I).ItemId==ItemId) { Inventory->SelectQuickbarSlot(I); break; }
    OnRep_Slots(); ForceNetUpdate(); Refresh();
}
void AJTSItemSupplyRack::Refresh()
{
    if (!HasAuthority() || ItemId==EJTSItemId::None) return;
    const auto* E=GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>();
    const bool Stopped=E && !StopFlag.IsNone() && E->GetSnapshot().FacilityFlagIds.Contains(StopFlag);
    if (Stopped)
    {
        GetWorldTimerManager().ClearTimer(RefillTimer);
        if (GetSupplyCount()) { for (auto& Slot:Slots) Slot=0; OnRep_Slots(); ForceNetUpdate(); }
        // Keep enforcing the stop condition for late joins and restored inventories as well.
        RemoveSupplies();
    }
    else if (GetSupplyCount()==0 && RefillConditions->IsSatisfied())
    {
        if (!GetWorldTimerManager().IsTimerActive(RefillTimer))
            GetWorldTimerManager().SetTimer(RefillTimer,this,&ThisClass::Refill,FMath::Max(.1f,RefillDelay),false);
    }
    else GetWorldTimerManager().ClearTimer(RefillTimer);
}
void AJTSItemSupplyRack::Refill()
{
    const auto* E=GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>();
    if (!HasAuthority() || GetSupplyCount()!=0 || !RefillConditions->IsSatisfied()
        || (E && !StopFlag.IsNone() && E->GetSnapshot().FacilityFlagIds.Contains(StopFlag))) return;
    if (Slots.Num()) { Slots[0]=1; OnRep_Slots(); ForceNetUpdate(); }
}
void AJTSItemSupplyRack::RemoveSupplies()
{
    for (TActorIterator<APawn> It(GetWorld());It;++It)
    {
        if (auto* Inventory=It->FindComponentByClass<UJTSInventoryComponent>())
        {
            const int32 Count=Inventory->GetItemCount(ItemId);
            if (Count>0) Inventory->TryRemoveItem(ItemId,Count);
        }
    }
    for (TActorIterator<AJTSWorldPickupActor> It(GetWorld());It;++It)
        if (It->GetItemInstance().ItemId==ItemId) It->Destroy();
}
void AJTSItemSupplyRack::OnRep_Slots() { OnSlotsChanged.Broadcast(); }
void AJTSItemSupplyRack::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(RefreshTimer); GetWorldTimerManager().ClearTimer(RefillTimer);
    Super::EndPlay(Reason);
}
void AJTSItemSupplyRack::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AJTSItemSupplyRack,Slots);
}
