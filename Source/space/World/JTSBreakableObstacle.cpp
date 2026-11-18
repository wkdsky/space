#include "space/World/JTSBreakableObstacle.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSFacilityConditionComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/World/JTSInteriorZone.h"
AJTSBreakableObstacle::AJTSBreakableObstacle() { Health=CreateDefaultSubobject<UJTSHealthComponent>(TEXT("Health")); bHideMeshWhenActive=true; }
void AJTSBreakableObstacle::BeginPlay()
{
    Super::BeginPlay();
    IntactMeshScale=Mesh->GetRelativeScale3D();
    if (HasAuthority()) { Health->OnDeath.AddDynamic(this,&ThisClass::Broken); if (RefreshOnEntry) GetWorldTimerManager().SetTimer(EntryTimer,this,&ThisClass::CheckEntry,.5f,true); }
}
float AJTSBreakableObstacle::TakeDamage(float Damage,const FDamageEvent& Event,AController* Controller,AActor* Causer)
{ return bRequiresMiningTool?0:Super::TakeDamage(Damage,Event,Controller,Causer); }
bool AJTSBreakableObstacle::BlocksAttackInitiation_Implementation(APawn* Pawn) const
{ const auto* C=Cast<AJTSCharacter>(Pawn); return bRequiresMiningTool && (!C || C->GetInventoryComponent()->GetActiveItemId()!=EJTSItemId::Pickaxe); }
bool AJTSBreakableObstacle::CanReceiveMeleeHit_Implementation(APawn* P) const
{
    const auto* C=Cast<AJTSCharacter>(P);
    return C && !C->GetHealthComponent()->IsDead() && !Health->IsDead() && !bActive && Conditions->IsSatisfied() && !BlocksAttackInitiation_Implementation(P);
}
void AJTSBreakableObstacle::ReceiveMeleeHit_Implementation(APawn* P,EJTSMeleeAttackType Type)
{
    if (HasAuthority() && CanReceiveMeleeHit_Implementation(P) && (!bRequiresMiningTool || Type==EJTSMeleeAttackType::Tool)) Health->ApplyDamage(Type==EJTSMeleeAttackType::Tool?25:8,P->GetController(),P);
}
FText AJTSBreakableObstacle::GetMeleeTargetPrompt_Implementation(APawn*) const { return bRequiresMiningTool?NSLOCTEXT("JTS","BreakWall","用采矿镐敲碎"):NSLOCTEXT("JTS","BreakProp","攻击"); }
void AJTSBreakableObstacle::Broken(AController*,AActor*)
{
    if (!HasAuthority()) return;
    if (!CompletedFlag.IsNone()) GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>()->SetFacilityFlag(CompletedFlag);
    bPropBroken=true; OnRep_Broken();
    if (const auto* GS=GetWorld()->GetGameState<AJTSGameState>(); GS && GS->GetActiveSpacecraft()) GS->GetActiveSpacecraft()->DepositResourceAmounts(Salvage);
    ForceNetUpdate();
}
void AJTSBreakableObstacle::CheckEntry()
{
    bool bNow=false;
    for (TActorIterator<AJTSCharacter> It(GetWorld());It;++It) if (!It->IsBoarded() && !It->GetHealthComponent()->IsDead() && RefreshOnEntry->Contains(It->GetActorLocation())) { bNow=true; break; }
    if (bNow && !bOccupied && Health->IsDead()) { Health->RestoreAuthoritativeHealth(Health->GetMaxHealth()); bPropBroken=false; OnRep_Broken(); ForceNetUpdate(); }
    bOccupied=bNow;
}
void AJTSBreakableObstacle::EndPlay(const EEndPlayReason::Type R) { if (auto* World=GetWorld()) { World->GetTimerManager().ClearTimer(EntryTimer);World->GetTimerManager().ClearTimer(ShutdownTimer); } Super::EndPlay(R); }
void AJTSBreakableObstacle::OnRep_Broken()
{
    GetWorldTimerManager().ClearTimer(ShutdownTimer);Mesh->SetRelativeScale3D(IntactMeshScale);
    Mesh->SetCollisionEnabled(bPropBroken?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics);
    const bool Animate=bPropBroken && ShutdownSeconds>0 && GetNetMode()!=NM_DedicatedServer;
    Mesh->SetVisibility(!bPropBroken || Animate,true);
    if(Animate)
    {
        ShutdownAt=GetWorld()->GetTimeSeconds();
        if(ShutdownSound) UGameplayStatics::PlaySoundAtLocation(this,ShutdownSound,GetActorLocation());
        GetWorldTimerManager().SetTimer(ShutdownTimer,this,&ThisClass::AnimateShutdown,.03f,true);
    }
}
void AJTSBreakableObstacle::AnimateShutdown()
{
    const float Fraction=FMath::Clamp(float((GetWorld()->GetTimeSeconds()-ShutdownAt)/FMath::Max(.03f,ShutdownSeconds)),0.f,1.f);
    Mesh->SetRelativeScale3D(IntactMeshScale*FVector(1.f,1.f,FMath::Max(.015f,1.f-Fraction*1.5f)));
    if(Fraction>=1) { Mesh->SetVisibility(false,true);GetWorldTimerManager().ClearTimer(ShutdownTimer); }
}
void AJTSBreakableObstacle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AJTSBreakableObstacle,bPropBroken); }
