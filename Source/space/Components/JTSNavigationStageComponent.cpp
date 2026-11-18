#include "space/Components/JTSNavigationStageComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "space/Core/JTSGameState.h"
#include "space/Components/JTSCompressedSpaceComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSSpaceWorldManager.h"
UJTSNavigationStageComponent::UJTSNavigationStageComponent() { PrimaryComponentTick.bCanEverTick=false; SetIsReplicatedByDefault(true); }
void UJTSNavigationStageComponent::BeginPlay() { Super::BeginPlay(); if (GetOwner()->HasAuthority()) GetWorld()->GetTimerManager().SetTimer(StageTimer,this,&ThisClass::Update,.2f,true); }
void UJTSNavigationStageComponent::Update()
{
    const auto* M=Cast<AJTSSpaceWorldManager>(GetOwner()); const auto* GS=GetWorld()->GetGameState<AJTSGameState>(); const auto* Ship=GS?GS->GetActiveSpacecraft():nullptr;
    if (!M || !Ship || !M->CompressedSpace || !M->CompressedSpace->IsConfigured()) return;
    const auto* Space=M->CompressedSpace.Get(); const FVector Observer=Space->NavigationPosition(Ship->GetActorLocation());
    const AActor* Near=Space->ContainingNearFlightBody(Ship->GetActorLocation());
    struct FGroup { FName Id; FVector Center; double Radius=0,Extent=0,Range=0; bool bNear=false; };
    TMap<FName,FGroup> Groups;
    for (const auto& B:Space->Bodies)
    {
        if (!IsValid(B.Actor)) continue;
        const FName Id=B.PresentationGroup.IsNone()?B.Actor->GetFName():B.PresentationGroup;
        auto& G=Groups.FindOrAdd(Id); G.Id=Id; G.bNear|=B.Actor==Near;
        if (B.RadiusKilometers>G.Radius) { G.Radius=B.RadiusKilometers; G.Center=B.NavigationPositionKilometers; }
    }
    for (auto& Pair:Groups) Pair.Value.Extent=Pair.Value.Radius*FMath::Max(2.f,M->NavigationMapSettings.MinimumSystemRadii);
    for (const auto& B:Space->Bodies)
    {
        if (!IsValid(B.Actor)) continue;
        const FName Id=B.PresentationGroup.IsNone()?B.Actor->GetFName():B.PresentationGroup;
        auto& G=Groups[Id]; G.Extent=FMath::Max(G.Extent,(FVector::Dist(G.Center,B.NavigationPositionKilometers)+B.RadiusKilometers)*FMath::Max(1.2f,M->NavigationMapSettings.SystemExtentMultiplier));
    }
    FGroup* Best=nullptr; FGroup* Current=Groups.Find(SystemId); double Score=TNumericLimits<double>::Max();
    for (auto& Pair:Groups)
    {
        auto& G=Pair.Value; G.Range=FVector::Dist(Observer,G.Center); const double S=G.Range/FMath::Max(.001,G.Extent);
        if (S<Score) { Best=&G; Score=S; }
    }
    if (!Best) return;
    if (Current && Current->Range/FMath::Max(.001,Current->Extent)<=Score*(1+M->NavigationMapSettings.StageHysteresis)) Best=Current;
    for (auto& Pair:Groups) if (Pair.Value.bNear) { Best=&Pair.Value; break; }
    const auto Next=FJTSNavigationStageRules::Classify(Ship->IsLanded() || (Near && !M->IsCruisePresentationActive(Ship)),Best->Range,Best->Extent,Stage,M->NavigationMapSettings.StageHysteresis);
    if (Stage!=Next || SystemId!=Best->Id) { Stage=Next; SystemId=Best->Id; OnRep_Stage(); GetOwner()->ForceNetUpdate(); }
}
void UJTSNavigationStageComponent::OnRep_Stage() { OnStageChanged.Broadcast(Stage,SystemId); }
void UJTSNavigationStageComponent::EndPlay(const EEndPlayReason::Type R) { if (auto* World=GetWorld()) World->GetTimerManager().ClearTimer(StageTimer); Super::EndPlay(R); }
void UJTSNavigationStageComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UJTSNavigationStageComponent,Stage); DOREPLIFETIME(UJTSNavigationStageComponent,SystemId); }
