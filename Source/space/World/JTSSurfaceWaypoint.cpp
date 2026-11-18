#include "space/World/JTSSurfaceWaypoint.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "space/Core/JTSGameState.h"
AJTSSurfaceWaypoint::AJTSSurfaceWaypoint() { PrimaryActorTick.bCanEverTick=false; SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root"))); }
bool AJTSSurfaceWaypoint::IsDiscovered() const
{
    const auto* S=GetWorld()?GetWorld()->GetGameState<AJTSGameState>():nullptr;
    return S && S->LoadedCoordinateIds.Contains(CoordinateId);
}
