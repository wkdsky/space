#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "JTSWorldPickupRegistrySubsystem.generated.h"

class AJTSWorldPickupActor;

/** Keeps a small live set of manual Moon world pickups for crosshair targeting. */
UCLASS()
class SPACE_API UJTSWorldPickupRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterPickup(AJTSWorldPickupActor* Pickup);
	void UnregisterPickup(AJTSWorldPickupActor* Pickup);
	void GetRegisteredPickups(TArray<AJTSWorldPickupActor*>& OutPickups);

private:
	TSet<TWeakObjectPtr<AJTSWorldPickupActor>> RegisteredPickups;
};
