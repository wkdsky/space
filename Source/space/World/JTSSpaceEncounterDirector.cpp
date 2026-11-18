#include "space/World/JTSSpaceEncounterDirector.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "space/World/JTSSpaceEncounterUnit.h"
#include "space/World/JTSSpaceWorldManager.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/Components/JTSCompressedSpaceComponent.h"
#include "space/Components/JTSNavigationStageComponent.h"
#include "space/Components/JTSSpacecraftDurabilityComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
AJTSSpaceEncounterDirector::AJTSSpaceEncounterDirector() { PrimaryActorTick.bCanEverTick=false; }
void AJTSSpaceEncounterDirector::BeginPlay() { Super::BeginPlay(); if (HasAuthority()) GetWorldTimerManager().SetTimer(DirectorTimer,this,&ThisClass::Update,.1f,true); }
void AJTSSpaceEncounterDirector::Update()
{
    auto* GS=GetWorld()->GetGameState<AJTSGameState>(); auto* Ship=GS?GS->GetActiveSpacecraft():nullptr;
    if (!Ship) return;
    AJTSSpaceWorldManager* M=nullptr; for (TActorIterator<AJTSSpaceWorldManager> It(GetWorld());It;++It) { M=*It; break; }
    const auto* Stage=M?M->FindComponentByClass<UJTSNavigationStageComponent>():nullptr;
    if (!Stage) return;
    const auto* Hull=Ship->FindComponentByClass<UJTSSpacecraftDurabilityComponent>();
    const bool Active=Stage->Stage==EJTSNavigationMapStage::System && Stage->SystemId==SystemId && (!Hull || !Hull->bDisabled);
    TArray<AJTSSpaceEncounterUnit*> Units;
    TSet<int32> Teams;
    for (TActorIterator<AJTSSpaceEncounterUnit> It(GetWorld());It;++It)
    {
        if (It->GetOwner()!=this) continue;
        if (!Active || FVector::DistSquared(Ship->GetActorLocation(),It->GetActorLocation())>FMath::Square(RecycleDistance)) { It->Destroy(); continue; }
        Units.Add(*It); Teams.Add(It->TeamId);
    }
    for (auto* U:Units) if (IsValid(U)) U->Advance(.1f,Ship);
    if (Active && Teams.Num()<FMath::Clamp(ActiveTeams,3,5) && !UnitClasses.IsEmpty())
    {
        const FVector Center=Ship->GetActorLocation()+FMath::VRand()*FMath::FRandRange(SpawnRingMinimum,SpawnRingMaximum);
        if (M->CompressedSpace && M->CompressedSpace->ContainingNearFlightBody(Center)) return;
        auto Class=UnitClasses[FMath::RandRange(0,UnitClasses.Num()-1)]; if (!Class) return;
        const bool Ray=Class.GetDefaultObject()->bLightSailRay; const int32 Count=Ray?FMath::RandRange(3,6):FMath::RandRange(2,3);
        int32 Stencil=Ray?20:10; while (Teams.Contains(Stencil)) ++Stencil;
        for (int32 I=0;I<Count;++I)
        {
            const FVector Position=Center+FMath::VRand()*4000;
            if (M->CompressedSpace && M->CompressedSpace->ContainingNearFlightBody(Position)) continue;
            FActorSpawnParameters P; P.Owner=this; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* U=GetWorld()->SpawnActor<AJTSSpaceEncounterUnit>(Class,Position,FRotator::ZeroRotator,P);
            if (U) { U->TeamId=Stencil; U->Mesh->SetCustomDepthStencilValue(Stencil); U->DriftVelocity=Ship->GetActorForwardVector()*FMath::FRandRange(100.f,300.f); }
        }
    }
    if (Stage->Stage==EJTSNavigationMapStage::Surface && !ApproachCoordinateId.IsNone())
    {
        const auto* Near=M->CompressedSpace?M->CompressedSpace->ContainingNearFlightBody(Ship->GetActorLocation()):nullptr;
        const auto* Planet=Cast<AJTSPlanetAnchor>(Near);
        auto* E=GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>();
        if (Planet && Planet->GetPlanetId()==ApproachPlanetId && E->GetSnapshot().LoadedCoordinateIds.Contains(ApproachPlanetId))
        {
            E->LoadCoordinate(ApproachCoordinateId);
        }
    }
}
void AJTSSpaceEncounterDirector::EndPlay(const EEndPlayReason::Type R) { if (auto* World=GetWorld()) World->GetTimerManager().ClearTimer(DirectorTimer); Super::EndPlay(R); }
