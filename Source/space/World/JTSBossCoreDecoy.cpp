#include "space/World/JTSBossCoreDecoy.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "space/Characters/JTSPlanetBoss.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Player/JTSCharacter.h"
AJTSBossCoreDecoy::AJTSBossCoreDecoy() { bReplicates=true; SetReplicateMovement(true); Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core")); SetRootComponent(Mesh); Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic")); }
bool AJTSBossCoreDecoy::IsNameplateLit(const APawn* V) const
{
    const auto* I=V?V->FindComponentByClass<UJTSInventoryComponent>():nullptr;
    if (!bOriginal || !I || !I->IsWaistLampEquipped() || FVector::DistSquared(V->GetActorLocation(),GetActorLocation())>FMath::Square(1400.f)) return false;
    const FVector Eye=V->GetPawnViewLocation(), D=(GetActorLocation()-Eye).GetSafeNormal();
    if (FVector::DotProduct(V->GetViewRotation().Vector(),D)<.7f) return false;
    FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(CoreNameplate),false,V); Q.AddIgnoredActor(this); if (Boss) Q.AddIgnoredActor(Boss);
    return !GetWorld()->LineTraceSingleByChannel(H,Eye,GetActorLocation(),ECC_Visibility,Q);
}
float AJTSBossCoreDecoy::TakeDamage(float D,const FDamageEvent& E,AController* C,AActor* Causer)
{
    if (!HasAuthority() || D<=0 || !Boss || Boss->Health->IsDead()) return 0;
    if (bOriginal) return Boss->TakeDamage(D,E,C,this);
    for (TActorIterator<AJTSCharacter> It(GetWorld());It;++It) if (FVector::DistSquared(It->GetActorLocation(),GetActorLocation())<FMath::Square(350.f)) It->GetHealthComponent()->ApplyDamage(ExplosionDamage,C,this);
    Destroy(); return D;
}
bool AJTSBossCoreDecoy::CanReceiveMeleeHit_Implementation(APawn* P) const { const auto* H=P?P->FindComponentByClass<UJTSHealthComponent>():nullptr; return Boss && !Boss->Health->IsDead() && H && !H->IsDead(); }
void AJTSBossCoreDecoy::ReceiveMeleeHit_Implementation(APawn* P,EJTSMeleeAttackType Type) { if (HasAuthority() && CanReceiveMeleeHit_Implementation(P)) TakeDamage(Type==EJTSMeleeAttackType::Tool?24:10,FDamageEvent(),P->GetController(),P); }
FText AJTSBossCoreDecoy::GetMeleeTargetDisplayName_Implementation() const { return NSLOCTEXT("JTS","CorePod","坩埚核心"); }
FText AJTSBossCoreDecoy::GetMeleeTargetPrompt_Implementation(APawn* P) const { return IsNameplateLit(P)?FText::FromString(TEXT("ORIGINAL FABRICATOR · UNIT 000")):NSLOCTEXT("JTS","CoreInspect","腰灯可照亮铭牌"); }
void AJTSBossCoreDecoy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AJTSBossCoreDecoy,Boss); DOREPLIFETIME(AJTSBossCoreDecoy,bOriginal); }
