#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JTSStellarExchangeWidget.generated.h"

class AJTSSpacecraftActor;

/** The same readable instrument is rendered on the real screen and in its inventory view. */
UCLASS()
class SPACE_API UJTSStellarExchangeWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetShip(AJTSSpacecraftActor* InShip, bool bCanClaim) { Ship = InShip; bInteractive = bCanClaim; ForceVolatile(true); }
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
private:
	TWeakObjectPtr<AJTSSpacecraftActor> Ship;
	bool bInteractive = false;
};
