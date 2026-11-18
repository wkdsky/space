#include "space/World/JTSFuelDepot.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "space/Core/JTSGameState.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSPlanetAnchor.h"
void AJTSFuelDepot::BeginPlay() { Super::BeginPlay(); if (HasAuthority()) GetWorldTimerManager().SetTimer(LandingTimer,this,&ThisClass::CheckLanding,.5f,true); }
void AJTSFuelDepot::CheckLanding()
{
    const auto* GS=GetWorld()->GetGameState<AJTSGameState>(); const auto* S=GS?GS->GetActiveSpacecraft():nullptr;
    if (!S || !S->IsLanded() || S->GetLandedPlanet()!=Planet) bUsedThisLanding=false;
}
bool AJTSFuelDepot::CanInteract_Implementation(APawn* P) const
{
    if (!bActive) return Super::CanInteract_Implementation(P);
    const auto* H=P?P->FindComponentByClass<UJTSHealthComponent>():nullptr;
    const auto* GS=GetWorld()->GetGameState<AJTSGameState>(); const auto* S=GS?GS->GetActiveSpacecraft():nullptr;
    return H && !H->IsDead() && !bUsedThisLanding && S && S->IsLanded() && S->GetLandedPlanet()==Planet && FVector::DistSquared(P->GetActorLocation(),GetActorLocation())<FMath::Square(350.f);
}
FText AJTSFuelDepot::GetInteractionPrompt_Implementation(APawn* P) const { return bActive?NSLOCTEXT("JTS","RefuelDepot","[E] 补给燃料（每次着陆一次）"):Super::GetInteractionPrompt_Implementation(P); }
void AJTSFuelDepot::Interact_Implementation(APawn* P)
{
    if (!HasAuthority() || !CanInteract_Implementation(P)) return;
    if (!bActive) { Super::Interact_Implementation(P); return; }
    auto* GS=GetWorld()->GetGameState<AJTSGameState>();
    if (auto* S=GS?GS->GetActiveSpacecraft():nullptr) { S->DepositResourceAmounts({{EJTSResourceType::Fuel,FMath::Max(0,FuelAmount)}}); bUsedThisLanding=true; ForceNetUpdate(); }
}
void AJTSFuelDepot::EndPlay(const EEndPlayReason::Type R) { if (auto* World=GetWorld()) World->GetTimerManager().ClearTimer(LandingTimer); Super::EndPlay(R); }
void AJTSFuelDepot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AJTSFuelDepot,bUsedThisLanding); }
