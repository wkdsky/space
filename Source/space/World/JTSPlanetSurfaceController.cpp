#include "space/World/JTSPlanetSurfaceController.h"
#include "EngineUtils.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetEcologyHabitat.h"
#include "space/World/JTSPlanetEnemySettlement.h"
#include "space/World/JTSMoonResourceSpawner.h"
bool AJTSPlanetSurfaceController::InitializeSurfaceGameplay(const FJTSSurfaceGameplayContext& Context)
{
    if (IsSurfaceGameplayInitialized()) return SupportsPlanet(Context.Planet);
    if (!Super::InitializeSurfaceGameplay(Context)) return false;
    const FName Tag=Context.Planet->GetPlanetId();
    for (TActorIterator<AJTSPlanetEnemySettlement> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(Tag)) { if (auto* H=Cast<AJTSPlanetEcologyHabitat>(*It)) H->ActivateEcology(Context.Planet,this); else It->Activate(Context.Planet,this); Settlements.Add(*It); }
    for (TActorIterator<AJTSMoonResourceSpawner> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(Tag)) { It->SetOwningPlanet(Context.Planet); It->SetSurfaceGameplayController(this); It->GenerateResources(); }
    return true;
}
void AJTSPlanetSurfaceController::RegisterSurfaceRuntimeActor(AActor* Actor) { if (IsValid(Actor)) RuntimeActors.AddUnique(Actor); }
void AJTSPlanetSurfaceController::ShutdownSurfaceGameplay()
{
    for (auto& S:Settlements) if (S.IsValid()) S->Deactivate();
    Settlements.Reset();
    for (auto& A:RuntimeActors) if (A.IsValid()) A->Destroy();
    RuntimeActors.Reset(); Super::ShutdownSurfaceGameplay();
}
