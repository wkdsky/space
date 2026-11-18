#include "space/Components/JTSCreatureTacticsComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSPlanetSurfaceSteeringComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSInteriorZone.h"
#include "space/World/JTSFacilityHazard.h"
UJTSCreatureTacticsComponent::UJTSCreatureTacticsComponent() { SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=false; }
void UJTSCreatureTacticsComponent::BeginPlay()
{
    Super::BeginPlay(); if (Tactic==EJTSCreatureTactic::None || !BodyMesh) return;
    auto* C=Cast<AJTSPlanetSurfaceCreature>(GetOwner()); if (!C) return;
    C->Mesh->SetVisibility(false);
    Visual=NewObject<UStaticMeshComponent>(GetOwner(),TEXT("TacticVisual")); Visual->SetupAttachment(C->GetRootComponent());
    Visual->SetStaticMesh(BodyMesh); Visual->SetRelativeScale3D(BodySize); Visual->SetRelativeLocation(BodyOffset);
    if (BodyMaterial) Visual->SetMaterial(0,BodyMaterial);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visual->RegisterComponent();
    if (Tactic==EJTSCreatureTactic::LightFlock)
    {
        Visual->SetVisibility(false);
        Flock=NewObject<UInstancedStaticMeshComponent>(C,TEXT("Flock")); Flock->SetupAttachment(C->GetRootComponent()); Flock->SetStaticMesh(BodyMesh);
        if (BodyMaterial) Flock->SetMaterial(0,BodyMaterial); Flock->SetCollisionEnabled(ECollisionEnabled::NoCollision); Flock->RegisterComponent();
        for (int32 I=0;I<18;++I) { const float Angle=I*2.39996f; Flock->AddInstance(FTransform(FRotator(0,I*43.f,0),FVector(FMath::Cos(Angle)*90,FMath::Sin(Angle)*90,55+(I%5)*18),BodySize*.2f)); }
    }
    OnRep_Posture();
}
void UJTSCreatureTacticsComponent::SetPosture(EJTSCreaturePosture P,float Duration)
{
    Posture=P; StateUntil=GetWorld()->GetTimeSeconds()+Duration; bHit=false; OnRep_Posture(); GetOwner()->ForceNetUpdate();
    if(Tactic==EJTSCreatureTactic::IceAmbush && P==EJTSCreaturePosture::Warning)
        for(TActorIterator<AJTSFacilityHazard> It(GetWorld());It;++It)
            if(It->Kind==EJTSFacilityHazardKind::IceLoad)
            {
                const FVector Local=It->GetActorTransform().InverseTransformPosition(GetOwner()->GetActorLocation()).GetAbs();
                const FVector Extent=It->Bounds->GetUnscaledBoxExtent();
                if(Local.X<=Extent.X && Local.Y<=Extent.Y && Local.Z<=Extent.Z) It->Arm();
            }
}
void UJTSCreatureTacticsComponent::OnRep_Posture()
{
    if (!Visual) return;
    Visual->SetRelativeRotation(Posture==EJTSCreaturePosture::Stunned?FRotator(0,0,180):FRotator::ZeroRotator);
    const float Squash=Tactic==EJTSCreatureTactic::Roll && Posture==EJTSCreaturePosture::Attack?.8f:1.f;
    Visual->SetRelativeScale3D(BodySize*Squash);
    Visual->SetRelativeLocation(BodyOffset+(Posture==EJTSCreaturePosture::Stunned?StunnedBodyOffset:FVector::ZeroVector));
    if (Tactic==EJTSCreatureTactic::CeilingRunner) Visual->SetRelativeLocation(BodyOffset+(Posture==EJTSCreaturePosture::Rest?FVector(0,0,250):FVector::ZeroVector));
    RefreshPresentation();
}
void UJTSCreatureTacticsComponent::RefreshPresentation()
{
    auto* C=Cast<AJTSPlanetSurfaceCreature>(GetOwner());
    if (!C || !Visual || Tactic==EJTSCreatureTactic::None) return;
    C->Mesh->SetVisibility(false);
    const bool Dead=C->Health->IsDead();
    const bool Buried=(Tactic==EJTSCreatureTactic::IceAmbush || Tactic==EJTSCreatureTactic::BurrowBite)
        && (Posture==EJTSCreaturePosture::Rest || Posture==EJTSCreaturePosture::Retreat);
    Visual->SetVisibility((Dead || !Buried) && (Tactic!=EJTSCreatureTactic::LightFlock || Dead));
    if (Flock) Flock->SetVisibility(!Dead);
    if (Dead) Visual->SetRelativeRotation(FRotator(0,0,90));
}
float UJTSCreatureTacticsComponent::DamageMultiplier() const
{
    if (Tactic==EJTSCreatureTactic::HeavyMiner) return .5f;
    if (Tactic==EJTSCreatureTactic::Roll) return Posture==EJTSCreaturePosture::Stunned?2.f:1.f;
    return 1;
}
bool UJTSCreatureTacticsComponent::Update(AJTSPlanetAnchor* P,float Dt)
{
    auto* C=Cast<AJTSPlanetSurfaceCreature>(GetOwner()); if (!C || Tactic==EJTSCreatureTactic::None || !P) return false;
    if (!bInitialized) { Home=C->GetActorLocation(); bInitialized=true; }
    const double Now=GetWorld()->GetTimeSeconds();
    AJTSCharacter* Target=nullptr; float Best=FMath::Square(DetectRadius);
    for (TActorIterator<AJTSCharacter> It(GetWorld());It;++It)
    {
        if (It->IsBoarded() || It->GetHealthComponent()->IsDead()) continue;
        if (AJTSInteriorZone::FindAt(GetWorld(),It->GetActorLocation())!=AJTSInteriorZone::FindAt(GetWorld(),C->GetActorLocation())) continue;
        if (Tactic==EJTSCreatureTactic::LightFlock && (!It->GetInventoryComponent()->IsWaistLampEquipped() || bPassive)) continue;
        const float D=FVector::DistSquared(It->GetActorLocation(),C->GetActorLocation());
        if (D<Best) { Best=D; Target=*It; }
    }
    auto Hurt=[&](float Reach)
    {
        if (Target && Now>=NextHit && Best<FMath::Square(Reach))
        {
            FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(CreatureAttack),false,C); Q.AddIgnoredActor(Target);
            if (!GetWorld()->LineTraceSingleByChannel(Hit,C->GetActorLocation()+C->GetActorUpVector()*60,Target->GetActorLocation(),ECC_Visibility,Q))
            { Target->GetHealthComponent()->ApplyDamage(Damage,nullptr,C); NextHit=Now+(Tactic==EJTSCreatureTactic::LightFlock?1:1.8); }
        }
    };
    if (Tactic==EJTSCreatureTactic::LightFlock)
    {
        if (Target) { C->MoveTowards(Target->GetActorLocation(),AttackSpeed,Dt); Hurt(170); }
        else C->MoveTowards(Home+C->GetActorForwardVector().RotateAngleAxis(Now*25,P->GetRadialUpVector(Home))*180,50,Dt);
        return true;
    }
    if (Posture==EJTSCreaturePosture::Stunned) { if (Now>=StateUntil) SetPosture(EJTSCreaturePosture::Rest,2); return true; }
    if (Tactic==EJTSCreatureTactic::FeignWreck || Tactic==EJTSCreatureTactic::CeilingRunner)
    {
        bAwake|=(Target && Best<FMath::Square(450.f)) || C->Health->GetHealthNormalized()<1;
        if (!bAwake) return true;
        if (Posture==EJTSCreaturePosture::Rest) { SetPosture(EJTSCreaturePosture::Warning,1); return true; }
        if (Posture==EJTSCreaturePosture::Warning && Now<StateUntil) return true;
        if (Posture!=EJTSCreaturePosture::Attack) SetPosture(EJTSCreaturePosture::Attack,60);
        if (Target) { C->MoveTowards(Target->GetActorLocation(),AttackSpeed,Dt); Hurt(170); }
        return true;
    }
    if (Tactic==EJTSCreatureTactic::HeavyMiner) { if (Target) { C->MoveTowards(Target->GetActorLocation(),AttackSpeed,Dt); Hurt(300); } return true; }
    if (Posture==EJTSCreaturePosture::Rest && Target && Now>=StateUntil)
    {
        LockedHeading=FVector::VectorPlaneProject(Target->GetActorLocation()-C->GetActorLocation(),P->GetRadialUpVector(C->GetActorLocation())).GetSafeNormal();
        SetPosture(EJTSCreaturePosture::Warning,Tactic==EJTSCreatureTactic::Roll?.7f:1.25f);
    }
    else if (Posture==EJTSCreaturePosture::Warning && Now>=StateUntil) SetPosture(EJTSCreaturePosture::Attack,Tactic==EJTSCreatureTactic::BurrowBite?1.f:2.5f);
    else if (Posture==EJTSCreaturePosture::Attack)
    {
        C->SetState(EJTSForagerState::Attack,0);
        if (Tactic==EJTSCreatureTactic::Roll)
        {
            FVector Ground,Heading;
            C->bMovementBlocked=!C->Steering->ConstrainDisplacement(P,C->GetActorLocation(),LockedHeading*AttackSpeed*Dt,Ground,Heading);
            if (!C->bMovementBlocked) C->SetActorLocationAndRotation(Ground,FRotationMatrix::MakeFromXZ(Heading,P->GetRadialUpVector(Ground)).ToQuat());
        }
        else if (Tactic!=EJTSCreatureTactic::BurrowBite) C->MoveTowards(C->GetActorLocation()+LockedHeading*700,AttackSpeed,Dt);
        Hurt(Tactic==EJTSCreatureTactic::BurrowBite?350:180);
        if (C->bMovementBlocked && Tactic==EJTSCreatureTactic::Roll) SetPosture(EJTSCreaturePosture::Stunned,6);
        else if (Now>=StateUntil) SetPosture(EJTSCreaturePosture::Retreat,3);
    }
    else if (Posture==EJTSCreaturePosture::Retreat)
    {
        if (Tactic!=EJTSCreatureTactic::BurrowBite) C->MoveTowards(Home,AttackSpeed*.6f,Dt);
        if (Now>=StateUntil) SetPosture(EJTSCreaturePosture::Rest,3);
    }
    return true;
}
void UJTSCreatureTacticsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UJTSCreatureTacticsComponent,Posture); }
