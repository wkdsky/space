#pragma once
#include "CoreMinimal.h"
#include "JTSNavigationStage.generated.h"
UENUM(BlueprintType)
enum class EJTSNavigationMapStage : uint8 { Surface, System, Interstellar };
struct SPACE_API FJTSNavigationStageRules
{
    static EJTSNavigationMapStage Classify(bool bNear,double Range,double Extent,EJTSNavigationMapStage Previous,float Hysteresis)
    {
        if (bNear) return EJTSNavigationMapStage::Surface;
        return Range<=Extent*(Previous==EJTSNavigationMapStage::System?1+FMath::Max(0.f,Hysteresis):1)?EJTSNavigationMapStage::System:EJTSNavigationMapStage::Interstellar;
    }
};
