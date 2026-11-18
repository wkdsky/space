#include "space/Components/JTSBossPhaseComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "space/Characters/JTSPlanetBoss.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSInteriorZone.h"
#include "space/World/JTSPlanetSettlementEnemy.h"
#include "space/Player/JTSCharacter.h"
#include "space/World/JTSBossCoreDecoy.h"
UJTSBossPhaseComponent::UJTSBossPhaseComponent() { PrimaryComponentTick.bCanEverTick=false; SetIsReplicatedByDefault(true); }
int32 UJTSBossPhaseComponent::SelectPhase(const TArray<FJTSBossPhase>& P,float H,int32 Previous)
{
    int32 Result=Previous;
    for (int32 I=0;I<P.Num();++I) if (H<=P[I].EnterBelowHealth) Result=FMath::Max(Result,I);
    return Result;
}
float UJTSBossPhaseComponent::GetDamageMultiplier() const
{
    if (FlippedUntil>GetWorld()->GetTimeSeconds()) return 3;
    return EncounterArmor*(Phases.IsValidIndex(PhaseIndex)?Phases[PhaseIndex].DamageMultiplier:1);
}
bool UJTSBossPhaseComponent::CanFlip(APawn* Pawn) const
{
    const auto* B=Cast<AJTSPlanetBoss>(GetOwner()); const auto* C=Cast<AJTSCharacter>(Pawn);
    return bCooperativeFlip && PhaseIndex==0 && B && C && !C->GetHealthComponent()->IsDead() && !B->Health->IsDead()
        && B->Combat->GetAction()==EJTSBossAction::Stagger && FlippedUntil<=GetWorld()->GetTimeSeconds()
        && FVector::DistSquared(C->GetActorLocation(),B->GetActorLocation())<FMath::Square(400.f);
}
float UJTSBossPhaseComponent::FlipHoldSeconds() const { return GetWorld()->GetGameState() && GetWorld()->GetGameState()->PlayerArray.Num()>1?0:3; }
void UJTSBossPhaseComponent::ContributeFlip(APawn* P)
{
    if (!GetOwner()->HasAuthority() || !CanFlip(P)) return;
    const double Now=GetWorld()->GetTimeSeconds(); Contributions.Add(P,Now);
    for (auto It=Contributions.CreateIterator();It;++It) if (Now-It.Value()>1 || !CanFlip(It.Key().Get())) It.RemoveCurrent();
    const int32 Needed=FlipHoldSeconds()>0?1:2;
    if (Contributions.Num()>=Needed) { FlippedUntil=Now+6; Contributions.Reset(); if (auto* B=Cast<AJTSPlanetBoss>(GetOwner())) B->Combat->HoldStagger(6); OnRep_Flip(); GetOwner()->ForceNetUpdate(); }
}
void UJTSBossPhaseComponent::BeginPlay()
{
    Super::BeginPlay();
    if (auto* B=Cast<AJTSPlanetBoss>(GetOwner())) RestMeshTransform=B->Mesh->GetRelativeTransform();
    if (FlippedUntil>0) OnRep_Flip();
    if (GetOwner()->HasAuthority() && !Phases.IsEmpty()) GetWorld()->GetTimerManager().SetTimer(PhaseTimer,this,&ThisClass::Update,.15f,true);
}
void UJTSBossPhaseComponent::OnRep_Flip()
{
    RefreshFlipPresentation();
    if (bFlipPresented) GetWorld()->GetTimerManager().SetTimer(FlipPresentationTimer,this,&ThisClass::RefreshFlipPresentation,.1f,true);
}
void UJTSBossPhaseComponent::RefreshFlipPresentation()
{
    auto* B=Cast<AJTSPlanetBoss>(GetOwner());if (!B || !GetWorld()) return;
    const auto* State=GetWorld()->GetGameState();
    const double Now=State?State->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
    const bool bFlipped=bCooperativeFlip && FlippedUntil>Now && !B->Health->IsDead();
    if (bFlipPresented!=bFlipped)
    {
        bFlipPresented=bFlipped;
        FTransform Pose=RestMeshTransform;
        if (bFlipped) { Pose.SetRotation(Pose.GetRotation()*FRotator(0,0,180).Quaternion());Pose.AddToTranslation(FlippedMeshOffset); }
        B->Mesh->SetRelativeTransform(Pose);
    }
    if (!bFlipped) GetWorld()->GetTimerManager().ClearTimer(FlipPresentationTimer);
}
void UJTSBossPhaseComponent::Update()
{
    auto* B=Cast<AJTSPlanetBoss>(GetOwner()); if (!B) return;
    if (B->Health->IsDead()) { if(AssemblyPoint.IsValid()) AssemblyPoint->Destroy(); for (auto& A:Cores) if (A.IsValid()) A->Destroy(); for (auto& A:Summons) if (A.IsValid()) A->Destroy(); Cores.Reset(); Summons.Reset(); return; }
    if (!B->Combat->GetPlanet()) return;
    const int32 Next=SelectPhase(Phases,B->Health->GetHealthNormalized(),PhaseIndex);
    if (Next!=PhaseIndex) { if(AssemblyPoint.IsValid()) AssemblyPoint->Destroy(); AssemblyPoint.Reset();AssemblyReadyAt=0;PhaseIndex=Next; NextSummon=0; OnRep_Phase(); B->ForceNetUpdate(); }
    if (Phases.IsValidIndex(PhaseIndex) && GetWorld()->GetTimeSeconds()>=NextSummon) Summon();
    if (Phases.IsValidIndex(PhaseIndex))
    {
        const auto& P=Phases[PhaseIndex];
        if (P.bCoreCopies && !bCoresSpawned) SpawnCores();
        if (P.TrailHazardClass && GetWorld()->GetTimeSeconds()>=NextHazard)
        {
            NextHazard=GetWorld()->GetTimeSeconds()+2;
            if (auto* H=GetWorld()->SpawnActor<AActor>(P.TrailHazardClass,B->GetActorTransform())) { H->SetOwner(B); H->SetLifeSpan(8); Summons.Add(H); }
        }
    }
}
bool UJTSBossPhaseComponent::AllowsDamageFrom(AActor* Causer) const
{
    if (!Phases.IsValidIndex(PhaseIndex) || !Phases[PhaseIndex].bCoreCopies || !bCoresSpawned) return true;
    const auto* Core=Cast<AJTSBossCoreDecoy>(Causer); return Core && Core->bOriginal && Core->Boss==GetOwner();
}
void UJTSBossPhaseComponent::DamageArmor(float D) { if (EncounterArmor<1) { ArmorIntegrity-=D; if (ArmorIntegrity<=0) { EncounterArmor=1; GetOwner()->ForceNetUpdate(); } } }
void UJTSBossPhaseComponent::SpawnCores()
{
    auto* B=Cast<AJTSPlanetBoss>(GetOwner()); const auto& P=Phases[PhaseIndex]; if (!P.CoreClass) return;
    const int32 Real=FMath::RandRange(0,3);
    for (int32 I=0;I<4;++I)
    {
        const FVector Offset=B->GetActorForwardVector().RotateAngleAxis(90.f*I,B->GetActorUpVector())*850;
        FJTSPlanetSurfaceHit H; if (!AJTSInteriorZone::ProjectGameplayGround(B->Combat->GetPlanet(),B->GetActorLocation()+Offset,H)) continue;
        auto* Core=GetWorld()->SpawnActorDeferred<AJTSBossCoreDecoy>(P.CoreClass,FTransform(B->GetActorQuat(),H.ImpactPoint+B->GetActorUpVector()*65),B,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (Core) { Core->Boss=B; Core->bOriginal=I==Real; Core->FinishSpawning(FTransform(B->GetActorQuat(),H.ImpactPoint+B->GetActorUpVector()*65)); Cores.Add(Core); }
    }
    bCoresSpawned=Cores.ContainsByPredicate([](const auto& C){return C.IsValid() && C->bOriginal;});
    if (bCoresSpawned) B->Combat->WalkSpeed=0;
}
void UJTSBossPhaseComponent::OnRep_Phase()
{
    auto* B=Cast<AJTSPlanetBoss>(GetOwner()); if (!B || !Phases.IsValidIndex(PhaseIndex)) return;
    const auto& P=Phases[PhaseIndex];
    if (!P.Attacks.IsEmpty()) B->Combat->Attacks=P.Attacks;
    B->Combat->WalkSpeed=P.WalkSpeed; B->Combat->ChargeSpeed=P.ChargeSpeed;
    if (P.Form) B->Mesh->SetSkeletalMeshAsset(P.Form);
    OnPhaseChanged.Broadcast(PhaseIndex);
}
void UJTSBossPhaseComponent::Summon()
{
    const auto& P=Phases[PhaseIndex];
    const double Now=GetWorld()->GetTimeSeconds();
    NextSummon=Now+FMath::Max(2.f,P.SummonInterval);
    auto* B=Cast<AJTSPlanetBoss>(GetOwner());
    Summons.RemoveAll([](const auto& A){ const auto* H=A.IsValid()?A->FindComponentByClass<UJTSHealthComponent>():nullptr; return !A.IsValid() || (H && H->IsDead()); });
    const auto Class=BonusSummonClass?BonusSummonClass:P.SummonClass;
    if (!Class || Summons.Num()>=24) return;
    if(P.AssemblyPointClass)
    {
        if(AssemblyReadyAt==0)
        {
            FJTSPlanetSurfaceHit Hit;
            if(!AJTSInteriorZone::ProjectGameplayGround(B->Combat->GetPlanet(),B->GetActorLocation()+B->GetActorRightVector()*450,Hit)) return;
            FActorSpawnParameters Params;Params.Owner=B;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AssemblyPoint=GetWorld()->SpawnActor<AActor>(P.AssemblyPointClass,Hit.ImpactPoint,B->GetActorRotation(),Params);
            if(!AssemblyPoint.IsValid()) return;
            AssemblyReadyAt=Now+FMath::Max(.5f,P.AssemblySeconds);NextSummon=Now+.15;return;
        }
        const auto* Health=AssemblyPoint.IsValid()?AssemblyPoint->FindComponentByClass<UJTSHealthComponent>():nullptr;
        const bool bInterrupted=!AssemblyPoint.IsValid() || (Health && Health->IsDead());
        if(!bInterrupted && Now<AssemblyReadyAt) { NextSummon=Now+.15;return; }
        if(AssemblyPoint.IsValid()) AssemblyPoint->Destroy();AssemblyPoint.Reset();AssemblyReadyAt=0;
        if(bInterrupted) return;
    }
    const int32 Players=GetWorld()->GetGameState()?GetWorld()->GetGameState()->PlayerArray.Num():1;
    const int32 Count=FMath::Clamp((BonusSummonClass?BonusSummonCount:P.SummonCount)+FMath::Max(0,Players-1),0,24-Summons.Num());
    for (int32 I=0;I<Count;++I)
    {
        FJTSPlanetSurfaceHit Hit;
        const FVector Offset=B->GetActorForwardVector().RotateAngleAxis(360.f*I/FMath::Max(1,Count),B->GetActorUpVector())*450;
        if (!AJTSInteriorZone::ProjectGameplayGround(B->Combat->GetPlanet(),B->GetActorLocation()+Offset,Hit)) continue;
        FActorSpawnParameters Params; Params.Owner=B; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* A=GetWorld()->SpawnActor<AActor>(Class,Hit.ImpactPoint,B->GetActorRotation(),Params);
        if (!A) continue;
        if (!A->Implements<UJTSPlanetSettlementEnemy>() || !IJTSPlanetSettlementEnemy::Execute_InitializeForSettlement(A,B->Combat->GetPlanet(),B->GetActorLocation(),Hit.ImpactPoint)) { A->Destroy(); continue; }
        Summons.Add(A);
    }
}
void UJTSBossPhaseComponent::EndPlay(const EEndPlayReason::Type R)
{
    if (auto* World=GetWorld()) { World->GetTimerManager().ClearTimer(PhaseTimer);World->GetTimerManager().ClearTimer(FlipPresentationTimer); }
    if(GetOwner()->HasAuthority() && AssemblyPoint.IsValid()) AssemblyPoint->Destroy();
    if (GetOwner()->HasAuthority()) for (auto& A:Summons) if (A.IsValid()) A->Destroy();
    if (GetOwner()->HasAuthority()) for (auto& A:Cores) if (A.IsValid()) A->Destroy();
    Super::EndPlay(R);
}
void UJTSBossPhaseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UJTSBossPhaseComponent,PhaseIndex); DOREPLIFETIME(UJTSBossPhaseComponent,EncounterArmor); DOREPLIFETIME(UJTSBossPhaseComponent,FlippedUntil); }
