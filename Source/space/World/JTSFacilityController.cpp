#include "space/World/JTSFacilityController.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSFacilityConditionComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
AJTSFacilityController::AJTSFacilityController()
{
    bReplicates=true; PrimaryActorTick.bCanEverTick=false;
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh")); SetRootComponent(Mesh);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    StatusLight=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatusLight"));
    StatusLight->SetupAttachment(Mesh);
    StatusLight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Conditions=CreateDefaultSubobject<UJTSFacilityConditionComponent>(TEXT("Conditions"));
}
void AJTSFacilityController::BeginPlay()
{
    Super::BeginPlay();
    OnRep_Active();
    if (HasAuthority()) { Refresh(); GetWorldTimerManager().SetTimer(RefreshTimer,this,&ThisClass::Refresh,.25f,true); }
}
void AJTSFacilityController::Refresh()
{
    auto* E=GetGameInstance()?GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>():nullptr;
    if (!E || CompletedFlag.IsNone()) return;
    const bool bFreshActivation=bAutomatic && !E->GetSnapshot().FacilityFlagIds.Contains(CompletedFlag) && Conditions->IsSatisfied();
    if (bFreshActivation) E->SetFacilityFlag(CompletedFlag);
    const bool Next=E->GetSnapshot().FacilityFlagIds.Contains(CompletedFlag);
    if (Next!=bActive) { bActive=Next; OnRep_Active(); ForceNetUpdate(); }
    if (bFreshActivation) MulticastRuntimeActivated();
}
void AJTSFacilityController::MulticastRuntimeActivated_Implementation() { OnRuntimeActivated.Broadcast(true); }
bool AJTSFacilityController::HasRequiredHeldItem(APawn* Pawn) const
{
    if (RequiredHeldItem==EJTSItemId::None) return true;
    const auto* Inventory=Pawn?Pawn->FindComponentByClass<UJTSInventoryComponent>():nullptr;
    const auto Item=Inventory?Inventory->GetActiveItem():FJTSItemInstance();
    return !Item.IsEmpty() && Item.ItemId==RequiredHeldItem && RequiredHeldItemCount>0 && Item.StackCount>=RequiredHeldItemCount;
}
bool AJTSFacilityController::CanInteract_Implementation(APawn* Pawn) const
{
    const auto* H=Pawn?Pawn->FindComponentByClass<UJTSHealthComponent>():nullptr;
    return H && !H->IsDead() && !bAutomatic && !(bActive && bHideMeshWhenActive) && !CompletedFlag.IsNone()
        && FVector::DistSquared(Pawn->GetActorLocation(),GetActorLocation())<=FMath::Square(InteractionDistance);
}
bool AJTSFacilityController::CanExecuteInteraction_Implementation(APawn* Pawn) const
{
    if (!CanInteract_Implementation(Pawn) || bActive || !Conditions->IsSatisfied() || !HasRequiredHeldItem(Pawn)) return false;
    const auto* State=GetWorld()?GetWorld()->GetGameState<AJTSGameState>():nullptr;
    const auto* Ship=State?State->GetActiveSpacecraft():nullptr;
    if (!CompletionRewards.IsEmpty() && !Ship) return false;
    for (const auto& Cost:RepairCosts)
        if (Cost.Value<0 || (Cost.Value>0 && (!Ship || Ship->GetResourceAmount(Cost.Key)<Cost.Value))) return false;
    return true;
}
FText AJTSFacilityController::GetInteractionPrompt_Implementation(APawn* Pawn) const
{
    if (bActive) return CompletedPrompt.IsEmpty()?NSLOCTEXT("JTS","FacilityComplete","已完成"):CompletedPrompt;
    if (!Conditions->IsSatisfied()) return LockedPrompt.IsEmpty()?NSLOCTEXT("JTS","FacilityUnavailable","未解锁"):LockedPrompt;
    if (!HasRequiredHeldItem(Pawn)) return MissingHeldItemPrompt.IsEmpty()?NSLOCTEXT("JTS","FacilityHeldMissing","需要手持指定物品"):MissingHeldItemPrompt;
    if (!CanExecuteInteraction_Implementation(Pawn)) return NSLOCTEXT("JTS","FacilityMissing","材料不足");
    if (RequiredHeldItem!=EJTSItemId::None) return HeldItemActionPrompt.IsEmpty()?NSLOCTEXT("JTS","FacilityReplaceAction","[E] 更换组件"):HeldItemActionPrompt;
    return CompletionRewards.IsEmpty()?NSLOCTEXT("JTS","FacilityRepairAction","[长按 E] 修复")
        :NSLOCTEXT("JTS","FacilityClaimAction","[长按 E] 领取");
}
void AJTSFacilityController::Interact_Implementation(APawn* Pawn)
{
    if (!HasAuthority() || !CanExecuteInteraction_Implementation(Pawn)) return;
    auto* Expedition=GetGameInstance()?GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>():nullptr;
    if (!Expedition || Expedition->GetSnapshot().FacilityFlagIds.Contains(CompletedFlag)) return;
    auto* S=GetWorld()->GetGameState<AJTSGameState>();
    auto* Ship=S?S->GetActiveSpacecraft():nullptr;
    for (const auto& Cost:RepairCosts) if (Cost.Value<0) return;
    for (const auto& Reward:CompletionRewards)
        if (Reward.Value<=0 || !Ship || int64(Ship->GetResourceAmount(Reward.Key))+Reward.Value>MAX_int32) return;
    if (!RepairCosts.IsEmpty() && (!Ship || !Ship->TryConsumeResourceAmounts(RepairCosts))) return;
    if (RequiredHeldItem!=EJTSItemId::None && !Pawn->FindComponentByClass<UJTSInventoryComponent>()->TryConsumeActiveItem(RequiredHeldItem,RequiredHeldItemCount))
    {
        if (Ship && !RepairCosts.IsEmpty()) Ship->DepositResourceAmounts(RepairCosts);
        return;
    }
    if (!CompletionRewards.IsEmpty() && !Ship->DepositResourceAmounts(CompletionRewards)) return;
    Expedition->SetFacilityFlag(CompletedFlag);
    Refresh();
    MulticastRuntimeActivated();
}
void AJTSFacilityController::OnRep_Active()
{
    GetWorldTimerManager().ClearTimer(StatusTimer);
    if (bActive && bPresentationInitialized && StatusActivationDelay>0)
        GetWorldTimerManager().SetTimer(StatusTimer,this,&ThisClass::ApplyStatusPresentation,StatusActivationDelay,false);
    else ApplyStatusPresentation();
    bPresentationInitialized=true;
    OnStateChanged.Broadcast(bActive);
}
void AJTSFacilityController::ApplyStatusPresentation()
{
    for (const auto& A:PoweredActors) if (IsValid(A)) A->SetActorHiddenInGame(!bActive);
    if (bHideMeshWhenActive) { Mesh->SetVisibility(!bActive,true); Mesh->SetCollisionEnabled(bActive?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics); }
    if (auto* Material=bActive?ActiveMaterial.Get():InactiveMaterial.Get()) StatusLight->SetMaterial(0,Material);
}
void AJTSFacilityController::EndPlay(const EEndPlayReason::Type R) { if (auto* World=GetWorld()) { World->GetTimerManager().ClearTimer(RefreshTimer); World->GetTimerManager().ClearTimer(StatusTimer); } Super::EndPlay(R); }
void AJTSFacilityController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AJTSFacilityController,bActive); }
