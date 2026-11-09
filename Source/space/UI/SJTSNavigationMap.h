#pragma once

#include "Widgets/SLeafWidget.h"
#include "Styling/SlateBrush.h"
#include "space/UI/JTSNavigationMapModel.h"

/** Slate drawing and reversible animations; data collection belongs to FJTSNavigationMapModel. */
class SJTSCruiseNavigation : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SJTSCruiseNavigation) {}
	SLATE_END_ARGS()
	void Construct(const FArguments&) { SetCanTick(true); }
	void SetPresentation(const FJTSNavigationMapFrame& InFrame);
	void SetSurfaceTexture(UObject* Resource);
	float GetZoom() const { return Zoom; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(520, 560); }
	virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;

private:
	FJTSNavigationMapFrame Frame;
	FJTSNavigationMapFrame GroundFrame;
	float Zoom = 2.0f;
	float GroundBlend = 0.0f;
	bool bInitialized = false;
	double LastTickTime = 0.0;
	FSlateBrush SurfaceBrush;
	void PaintFlight(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const FLinearColor& Tint) const;
	void PaintGround(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const FLinearColor& Tint) const;
	void PaintShipPerspective(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry,
		const FVector2D& Anchor, const FLinearColor& Tint) const;
	void DrawText(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const FSlateFontInfo& Font,
		const FString& Text, const FVector2D& Position, const FLinearColor& Color, ETextJustify::Type Justify) const;
};
