#include "space/World/JTSSpaceEncounterUnit.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSCompressedSpaceComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSSpaceWorldManager.h"
AJTSSpaceEncounterUnit::AJTSSpaceEncounterUnit()
{
    PrimaryActorTick.bCanEverTick=false; bReplicates=true; SetReplicateMovement(true);
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh")); SetRootComponent(Mesh);
    Mesh->SetCollisionObjectType(ECC_WorldDynamic); Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Mesh->SetCollisionResponseToAllChannels(ECR_Ignore); Mesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Mesh->SetRenderCustomDepth(true);
    Health=CreateDefaultSubobject<UJTSHealthComponent>(TEXT("Health"));
}
void AJTSSpaceEncounterUnit::OnRep_Team() { Mesh->SetCustomDepthStencilValue(TeamId+(bHostile?40:0)); }
void AJTSSpaceEncounterUnit::BeginPlay() { Super::BeginPlay(); AuthoredMeshScale=Mesh->GetRelativeScale3D(); OnRep_Visual(); if (HasAuthority()) Health->OnDeath.AddDynamic(this,&ThisClass::OnDeath); }
void AJTSSpaceEncounterUnit::OnRep_Visual()
{
    const auto* Defaults=GetClass()->GetDefaultObject<AJTSSpaceEncounterUnit>();
    const int32 OriginalTier=Defaults?Defaults->SizeTier:2;
    Mesh->SetRelativeScale3D(AuthoredMeshScale*FMath::Pow(.5f,FMath::Max(0,OriginalTier-SizeTier))*(bIsSalvage?.25f:1.f));
    Mesh->SetCollisionEnabled(bIsSalvage?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryOnly);
    Mesh->SetRenderCustomDepth(!bIsSalvage);
}
void AJTSSpaceEncounterUnit::Advance(float Dt,AJTSSpacecraftActor* Ship)
{
    if (!HasAuthority() || !Ship) return;
    const float Distance=FVector::Dist(Ship->GetActorLocation(),GetActorLocation());
    if (bIsSalvage)
    {
        if (Distance<=3000 && Ship->DepositResourceAmounts(Salvage)) Destroy();
        return;
    }
    const FVector Next=GetActorLocation()+DriftVelocity*Dt;
    for (TActorIterator<AJTSSpaceWorldManager> It(GetWorld());It;++It)
        if (It->CompressedSpace && It->CompressedSpace->ContainingNearFlightBody(Next)) { Destroy(); return; }
    const double Now=GetWorld()->GetTimeSeconds();
    if (!bLightSailRay)
    {
        FVector Center,Extent; Mesh->GetLocalBounds(Center,Extent);
        const float Radius=Mesh->Bounds.SphereRadius;
        if (Now>=CollisionReady && FMath::PointDistToSegment(Ship->GetActorLocation(),GetActorLocation(),Next)<Radius+600)
        {
            const float Relative=(DriftVelocity-Ship->GetVelocity()).Size()/1000;
            UGameplayStatics::ApplyDamage(Ship,ContactDamage*(SizeTier+1)*FMath::Max(.3f,Relative),nullptr,this,nullptr);
            CollisionReady=Now+1;
        }
    }
    else if (Distance<180000)
    {
        if(!bHostile) { bHostile=true;OnRep_Team();ForceNetUpdate(); }
        if (AttackAt==0) { bHeating=true; HeatPoint=Ship->GetActorLocation(); AttackAt=Now+FMath::Max(1.f,WarningSeconds); }
        else if (Now>=AttackAt)
        {
            if (FVector::DistSquared(Ship->GetActorLocation(),HeatPoint)<FMath::Square(HeatRadius)) UGameplayStatics::ApplyDamage(Ship,HeatDamage*Dt,nullptr,this,nullptr);
            if (Now>AttackAt+1.5) { AttackAt=0; bHeating=false; }
        }
    }
    SetActorLocation(Next);
}
float AJTSSpaceEncounterUnit::TakeDamage(float Damage,const FDamageEvent& Event,AController* Controller,AActor* Causer)
{
    if(!HasAuthority() || bIsSalvage || !FMath::IsFinite(Damage) || Damage<=0) return 0;
    if(bLightSailRay && Event.IsOfType(FPointDamageEvent::ClassID))
    {
        const auto& Hit=static_cast<const FPointDamageEvent&>(Event).HitInfo;
        if(FMath::Abs(Mesh->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint).Y)>WingRootHalfWidth) Damage*=FMath::Max(1.f,WingDamageMultiplier);
    }
    return Super::TakeDamage(Damage,Event,Controller,Causer);
}
void AJTSSpaceEncounterUnit::OnDeath(AController*,AActor*)
{
    if (!HasAuthority() || bIsSalvage) return;
    if (!bLightSailRay && SizeTier>0)
    {
        for (int32 I=0;I<2;++I)
        {
            FActorSpawnParameters P; P.Owner=GetOwner(); P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Child=GetWorld()->SpawnActor<AJTSSpaceEncounterUnit>(GetClass(),GetActorLocation()+GetActorRightVector()*(I?150:-150),GetActorRotation(),P);
            if (Child) { Child->TeamId=TeamId; Child->Mesh->SetCustomDepthStencilValue(TeamId); Child->SizeTier=SizeTier-1; Child->OnRep_Visual(); Child->DriftVelocity=DriftVelocity+FMath::VRand()*100; Child->ForceNetUpdate(); }
        }
        Destroy(); return;
    }
    bIsSalvage=true; bHeating=false; bHostile=false; OnRep_Visual(); ForceNetUpdate();
}
void AJTSSpaceEncounterUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AJTSSpaceEncounterUnit,bHostile); DOREPLIFETIME(AJTSSpaceEncounterUnit,bHeating); DOREPLIFETIME(AJTSSpaceEncounterUnit,HeatPoint); DOREPLIFETIME(AJTSSpaceEncounterUnit,DriftVelocity); DOREPLIFETIME(AJTSSpaceEncounterUnit,bIsSalvage);
    DOREPLIFETIME(AJTSSpaceEncounterUnit,TeamId);
    DOREPLIFETIME(AJTSSpaceEncounterUnit,SizeTier);
}
