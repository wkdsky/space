#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "JTSCriticalDamageType.generated.h"

/** Identifies a server-validated weak-point hit to damage presentation. */
UCLASS()
class SPACE_API UJTSCriticalDamageType : public UDamageType
{
	GENERATED_BODY()
};
