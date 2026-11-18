#include "space/World/JTSInteriorTransit.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "space/World/JTSInteriorZone.h"
#include "space/World/JTSPlanetEnemySettlement.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "space/Components/JTSInteriorStateComponent.h"
#include "space/Components/JTSFacilityConditionComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
AJTSInteriorTransit::AJTSInteriorTransit()
{
    bReplicates=true; PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bStartWithTickEnabled=false; PrimaryActorTick.TickInterval=.03f;
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh")); SetRootComponent(Mesh);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Conditions=CreateDefaultSubobject<UJTSFacilityConditionComponent>(TEXT("Conditions"));
}
bool AJTSInteriorTransit::CanInteract_Implementation(APawn* Pawn) const
{
    auto* C=Cast<AJTSCharacter>(Pawn);
    return C && !C->IsBoarded() && !C->GetHealthComponent()->IsDead() && IsValid(Destination)
        && !Pending.Contains(Pawn) && FVector::DistSquared(Pawn->GetActorLocation(),GetActorLocation())<FMath::Square(350.f);
}
bool AJTSInteriorTransit::CanExecuteInteraction_Implementation(APawn* Pawn) const { return CanInteract_Implementation(Pawn) && Conditions->IsSatisfied(); }
FText AJTSInteriorTransit::GetInteractionPrompt_Implementation(APawn*) const
{
    if (!Conditions->IsSatisfied()) return LockedPrompt.IsEmpty()?NSLOCTEXT("JTS","TransitLocked","通道锁定 · 前置条件尚未满足"):LockedPrompt;
    return FText::Format(NSLOCTEXT("JTS","Transit","[E] {0}"),DisplayName);
}
void AJTSInteriorTransit::Interact_Implementation(APawn* Pawn)
{
    if (!HasAuthority() || !CanExecuteInteraction_Implementation(Pawn)) return;
    Pending.Add(Pawn);
    if (RideSeconds>0)
    {
        if (!bRiding) { bRiding=true; OnRep_Riding(); if (RideGuards) if (auto* Z=AJTSInteriorZone::FindAt(GetWorld(),GetActorLocation())) RideGuards->Activate(Z->Planet,this); ForceNetUpdate(); }
        if (auto* PC=Cast<AJTSPlayerController>(Pawn->GetController())) PC->ClientPlayExpeditionSound(RideSound);
        auto* C=Cast<AJTSCharacter>(Pawn); C->GetCharacterMovement()->StopMovementImmediately(); C->GetCharacterMovement()->DisableMovement();
        if (auto* PC=Cast<AJTSPlayerController>(C->GetController())) PC->ClientShowExpeditionNotice(NSLOCTEXT("JTS","CageRide","货笼正在下降 · 守住笼门"));
    }
    else if (auto* S=Pawn->FindComponentByClass<UJTSInteriorStateComponent>()) S->ClientFade(true);
    FTimerHandle Handle;
    GetWorldTimerManager().SetTimer(Handle,FTimerDelegate::CreateWeakLambda(this,[this,Weak=TWeakObjectPtr<APawn>(Pawn)]()
    {
        if (RideSeconds>0 && Weak.IsValid())
        {
            if (auto* S=Weak->FindComponentByClass<UJTSInteriorStateComponent>()) S->ClientFade(true);
            FTimerHandle Fade; GetWorldTimerManager().SetTimer(Fade,FTimerDelegate::CreateWeakLambda(this,[this,Weak](){CompleteTransit(Weak);}),.3f,false);
        }
        else CompleteTransit(Weak);
    }),FMath::Max(.26f,TransitSeconds+RideSeconds),false);
}
void AJTSInteriorTransit::CompleteTransit(TWeakObjectPtr<APawn> Weak)
{
    Pending.Remove(Weak); if (Pending.IsEmpty() && bRiding) { bRiding=false; OnRep_Riding(); if (RideGuards) RideGuards->Deactivate(); ForceNetUpdate(); }
    auto* C=Cast<AJTSCharacter>(Weak.Get()); if (!C) return;
    C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    if (CanExecuteInteraction_Implementation(C))
    {
        C->GetCharacterMovement()->StopMovementImmediately();
        C->TeleportTo(Destination->GetActorLocation(),Destination->GetActorRotation(),false,false);
        if (auto* PC=C->GetController()) PC->SetControlRotation(Destination->GetActorRotation());
    }
    if (auto* S=C->FindComponentByClass<UJTSInteriorStateComponent>()) S->ClientFade(false);
}
void AJTSInteriorTransit::EndPlay(const EEndPlayReason::Type R)
{
    for (auto Weak:Pending) if (auto* C=Cast<AJTSCharacter>(Weak.Get())) { C->GetCharacterMovement()->SetMovementMode(MOVE_Walking); if (auto* S=C->FindComponentByClass<UJTSInteriorStateComponent>()) S->ClientFade(false); }
    Super::EndPlay(R);
}

void AJTSInteriorTransit::OnRep_Riding()
{
    SetActorTickEnabled(bRiding);
    for (auto A:RideWalls) if (IsValid(A))
    {
        if (bRiding) WallOrigins.FindOrAdd(A)=A->GetActorLocation();
        else if (const auto* P=WallOrigins.Find(A)) A->SetActorLocation(*P,false);
    }
    if (!bRiding) WallOrigins.Reset();
}
void AJTSInteriorTransit::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Offset=FMath::Fmod(GetWorld()->GetTimeSeconds()*450.f,1000.f);
    for (auto& Pair:WallOrigins) if (Pair.Key.IsValid()) Pair.Key->SetActorLocation(Pair.Value+GetActorUpVector()*Offset,false);
}
void AJTSInteriorTransit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AJTSInteriorTransit,bRiding);
}
