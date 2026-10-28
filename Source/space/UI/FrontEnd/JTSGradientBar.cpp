#include "space/UI/FrontEnd/JTSGradientBar.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"

class SJTSGradientBar : public SLeafWidget
{
public:
 SLATE_BEGIN_ARGS(SJTSGradientBar) {}
 SLATE_END_ARGS()
 void Construct(const FArguments&) {}
 void Set(const TArray<FLinearColor>& InStops,float InRadius) { Stops=InStops; Radius=InRadius; Invalidate(EInvalidateWidgetReason::Paint); }
 virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(64,12); }
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& Geometry,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool bEnabled) const override
 {
  if (Stops.Num()<2) return Layer;
  const FVector2f Size=Geometry.GetLocalSize();
  TArray<FSlateGradientStop> Gradient;
  for (int32 I=0;I<Stops.Num();++I) Gradient.Add(FSlateGradientStop(FVector2f(Size.X*I/(Stops.Num()-1),0),Stops[I]));
  // Slate names gradients by stop axis: vertical stops run left to right.
  FSlateDrawElement::MakeGradient(Out,Layer,Geometry.ToPaintGeometry(),Gradient,Orient_Vertical,bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,FVector4f(Radius));
  return Layer;
 }
private:
 TArray<FLinearColor> Stops;
 float Radius=0;
};

TSharedRef<SWidget> UJTSGradientBar::RebuildWidget() { Bar=SNew(SJTSGradientBar); return Bar.ToSharedRef(); }
void UJTSGradientBar::SynchronizeProperties() { Super::SynchronizeProperties(); if (Bar) Bar->Set(Stops,CornerRadius); }
void UJTSGradientBar::SetStops(const TArray<FLinearColor>& InStops) { Stops=InStops; if (Bar) Bar->Set(Stops,CornerRadius); }
void UJTSGradientBar::ReleaseSlateResources(bool bReleaseChildren) { Super::ReleaseSlateResources(bReleaseChildren); Bar.Reset(); }
