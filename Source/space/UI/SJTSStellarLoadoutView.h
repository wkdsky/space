#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class UJTSStellarLoadoutComponent;

/** Shape rendering and hit testing only; mutations stay in the owning UUserWidget. */
class SJTSStellarLoadoutView : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SJTSStellarLoadoutView) {}
		SLATE_ARGUMENT(TFunction<UJTSStellarLoadoutComponent*()>, Loadout)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	int32 FindSlot(FVector2D ScreenPosition) const;
	static FVector2D SlotCenter(int32 Index);
	static FVector2D LayoutSize(int32 Available);
protected:
	virtual FVector2D ComputeDesiredSize(float Scale) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
private:
	TFunction<UJTSStellarLoadoutComponent*()> ReadLoadout;
};
