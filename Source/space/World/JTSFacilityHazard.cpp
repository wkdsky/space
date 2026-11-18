#include "space/World/JTSFacilityHazard.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "space/Components/JTSFacilityConditionComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
AJTSFacilityHazard::AJTSFacilityHazard()
{
    bReplicates=true; PrimaryActorTick.bCanEverTick=false;
    Bounds=CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds")); SetRootComponent(Bounds); Bounds->SetBoxExtent(FVector(300)); Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual")); Visual->SetupAttachment(Bounds); Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Conditions=CreateDefaultSubobject<UJTSFacilityConditionComponent>(TEXT("Conditions"));
}
void AJTSFacilityHazard::BeginPlay() { Super::BeginPlay(); OnRep_Triggered(); if (HasAuthority()) GetWorldTimerManager().SetTimer(UpdateTimer,this,&ThisClass::Update,.25f,true); }
void AJTSFacilityHazard::Arm() { if(HasAuthority() && !bTriggered && GetWorld()->GetTimeSeconds()>=ResetAt) bForcedTrigger=true; }
void AJTSFacilityHazard::Update()
{
    if (!Conditions->IsSatisfied()) return;
    const double Now=GetWorld()->GetTimeSeconds();
    TArray<AJTSCharacter*> Occupants;
    const FVector Extent=Bounds->GetUnscaledBoxExtent();
    for (TActorIterator<AJTSCharacter> It(GetWorld());It;++It)
    {
        const FVector L=GetActorTransform().InverseTransformPosition(It->GetActorLocation()).GetAbs();
        if (It->IsBoarded() || It->GetHealthComponent()->IsDead() || L.X>Extent.X || L.Y>Extent.Y || L.Z>Extent.Z) continue;
        Occupants.Add(*It);
        if (Kind==EJTSFacilityHazardKind::SporeFog && !It->GetInventoryComponent()->IsWaistLampEquipped()) It->GetHealthComponent()->ApplyDamage(DamagePerSecond*.25f,nullptr,this);
        if (Kind==EJTSFacilityHazardKind::Slag) It->GetHealthComponent()->ApplyDamage(DamagePerSecond*.25f,nullptr,this);
    }
    const bool bOverloaded=Kind==EJTSFacilityHazardKind::IceLoad?Occupants.Num()>MaximumOccupants:!Occupants.IsEmpty();
    if (Kind==EJTSFacilityHazardKind::SporeFog || Kind==EJTSFacilityHazardKind::Slag) return;
    if (!bTriggered && ResetAt<=Now && (Kind==EJTSFacilityHazardKind::DustSweep || bOverloaded || bForcedTrigger))
    {
        if (!TriggerAt) { TriggerAt=Now+WarningSeconds; bWarning=true; OnRep_Warning(); ForceNetUpdate(); }
        if (Now>=TriggerAt)
        {
            bTriggered=true; bWarning=false;bForcedTrigger=false; OnRep_Triggered(); ForceNetUpdate(); ResetAt=Now+RecoverySeconds;
            auto* E=GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>();
            if (!CompletedFlag.IsNone()) E->SetFacilityFlag(CompletedFlag);
            if (Kind==EJTSFacilityHazardKind::DustSweep) { for (FName F:SweepFlags) E->SetFacilityFlag(F); for (auto* C:Occupants) C->LaunchCharacter(GetActorForwardVector()*350+C->GetActorUpVector()*100,true,true); }
            if (Kind==EJTSFacilityHazardKind::IceLoad || Kind==EJTSFacilityHazardKind::FallingIce) for (auto* C:Occupants) C->GetHealthComponent()->ApplyDamage(DamagePerSecond,nullptr,this);
        }
    }
    else if (!bTriggered) { TriggerAt=0; if (bWarning) { bWarning=false; ForceNetUpdate(); } }
    if (bTriggered && Now>=ResetAt) { bTriggered=false; TriggerAt=0; ResetAt=Now+RecoverySeconds; OnRep_Triggered(); ForceNetUpdate(); }
}
void AJTSFacilityHazard::OnRep_Triggered()
{
    if(bTriggered && TriggerSound && GetNetMode()!=NM_DedicatedServer) UGameplayStatics::PlaySoundAtLocation(this,TriggerSound,GetActorLocation());
    if (Kind==EJTSFacilityHazardKind::DustSweep) Visual->SetVisibility(bTriggered,true);
    for (const auto& Floor:AffectedFloors) if (IsValid(Floor)) { Floor->SetActorEnableCollision(!bTriggered); Floor->SetActorHiddenInGame(bTriggered); }
    if (Kind==EJTSFacilityHazardKind::FallingIce)
        Visual->SetRelativeLocation(bTriggered?FVector(0,0,-300):FVector::ZeroVector);
}
void AJTSFacilityHazard::OnRep_Warning()
{
    if(bWarning && WarningSound && GetNetMode()!=NM_DedicatedServer) UGameplayStatics::PlaySoundAtLocation(this,WarningSound,GetActorLocation());
}
void AJTSFacilityHazard::EndPlay(const EEndPlayReason::Type R) { if (auto* World=GetWorld()) World->GetTimerManager().ClearTimer(UpdateTimer); Super::EndPlay(R); }
void AJTSFacilityHazard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AJTSFacilityHazard,bTriggered); DOREPLIFETIME(AJTSFacilityHazard,bWarning); }
