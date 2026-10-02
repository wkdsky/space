#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "JTSCriticalHitTarget.generated.h"

/** Optional server-side weak point rule for a ranged hit. A return value above one is critical. */
UINTERFACE(MinimalAPI)
class UJTSCriticalHitTarget : public UInterface
{
	GENERATED_BODY()
};

class SPACE_API IJTSCriticalHitTarget
{
	GENERATED_BODY()

public:
	virtual float GetCriticalHitMultiplier(const FHitResult& Hit) const = 0;
};
