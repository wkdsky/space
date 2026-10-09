#pragma once

#include "CoreMinimal.h"
#include "Components/LineBatchComponent.h"
#include "JTSMoonResourcePreviewComponent.generated.h"

/** Editor-only coverage lines with constant screen width and no spline handles. */
UCLASS()
class SPACE_API UJTSMoonResourcePreviewComponent : public ULineBatchComponent
{
	GENERATED_BODY()

public:
	UJTSMoonResourcePreviewComponent(const FObjectInitializer& ObjectInitializer);
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
};
