#pragma once

#include "CollisionQueryParams.h"
#include "GameFramework/Actor.h"

namespace JTSSpacecraftCollision
{
	/** Equipment is part of the vehicle, never terrain for that vehicle's own hull/foot sweeps. */
	inline void IgnoreSelfAndAttachments(FCollisionQueryParams& Query, const AActor* Ship)
	{
		if (!IsValid(Ship)) return;
		Query.AddIgnoredActor(Ship);
		TArray<AActor*> Attachments;
		Ship->GetAttachedActors(Attachments, true, true);
		Query.AddIgnoredActors(Attachments);
	}
}
