#include "space/Systems/JTSWorldPickupRegistrySubsystem.h"

#include "space/Items/JTSWorldPickupActor.h"

void UJTSWorldPickupRegistrySubsystem::RegisterPickup(AJTSWorldPickupActor* Pickup)
{
	if (IsValid(Pickup))
	{
		RegisteredPickups.Add(Pickup);
	}
}

void UJTSWorldPickupRegistrySubsystem::UnregisterPickup(AJTSWorldPickupActor* Pickup)
{
	if (Pickup != nullptr)
	{
		RegisteredPickups.Remove(TWeakObjectPtr<AJTSWorldPickupActor>(Pickup));
	}
}

void UJTSWorldPickupRegistrySubsystem::GetRegisteredPickups(TArray<AJTSWorldPickupActor*>& OutPickups)
{
	OutPickups.Reset();
	for (auto PickupIt = RegisteredPickups.CreateIterator(); PickupIt; ++PickupIt)
	{
		AJTSWorldPickupActor* const Pickup = PickupIt->Get();
		if (!IsValid(Pickup))
		{
			PickupIt.RemoveCurrent();
			continue;
		}

		OutPickups.Add(Pickup);
	}
}
