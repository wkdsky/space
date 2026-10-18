#include "space/UI/JTSCelestialMarkerWidget.h"
#include "space/UI/JTSCelestialNavigationLibrary.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

class SJTSCelestialMarkers : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SJTSCelestialMarkers) {} SLATE_END_ARGS()
	void Construct(const FArguments&) {}
	TArray<FJTSCelestialScreenMarker> Markers;
	FVector2D ProjectionSize = FVector2D(1, 1);
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool) const override
	{
		const FVector2D Size = Geometry.GetLocalSize();
		const FVector2D Scale(Size.X / ProjectionSize.X, Size.Y / ProjectionSize.Y);
		const auto Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14);
		TArray<FBox2D> Labels;
		for (const auto& Marker : Markers)
		{
			const FVector2D Low = Marker.Min * Scale - FVector2D(8, 8);
			const FVector2D High = Marker.Max * Scale + FVector2D(8, 8);
			const FLinearColor Accent = (Marker.PaletteIndex == 1 ? FLinearColor(1.0f, 0.62f, 0.42f, 0.88f)
				: Marker.PaletteIndex == 0 ? FLinearColor(0.52f, 0.88f, 1.0f, 0.88f)
				: FLinearColor(1.0f, 0.83f, 0.48f, 0.88f)) * Style.GetColorAndOpacityTint();
			auto Stroke = [&](const TArray<FVector2D>& Points, int32 Depth, FLinearColor Color, float Width)
			{
				FSlateDrawElement::MakeLines(Elements, Depth, Geometry.ToPaintGeometry(), Points,
					ESlateDrawEffect::None, Color, true, Width);
			};
			const double Length = FMath::Clamp(FMath::Min(High.X - Low.X, High.Y - Low.Y) * 0.12, 12.0, 28.0);
			for (int32 X = 0; X < 2; ++X) for (int32 Y = 0; Y < 2; ++Y)
			{
				const FVector2D Corner(X ? High.X : Low.X, Y ? High.Y : Low.Y);
				// Keep corners attached to the disk; don't pin a false box to the viewport rim.
				if (Corner.X < 4 || Corner.Y < 4 || Corner.X > Size.X - 4 || Corner.Y > Size.Y - 4) continue;
				const TArray<FVector2D> Points = {Corner + FVector2D(X ? -Length : Length, 0),
					Corner, Corner + FVector2D(0, Y ? -Length : Length)};
				Stroke(Points, Layer, FLinearColor(0.005f, 0.012f, 0.02f, 0.85f), 3.5f);
				Stroke(Points, Layer + 1, Accent, 1.2f);
			}
			const FString Name = Marker.DisplayName.ToString();
			const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Name, Font);
			const double LabelX = FMath::Clamp(Low.X, 16.0, FMath::Max(16.0, Size.X - TextSize.X - 16.0));
			FVector2D LabelPosition(LabelX, Low.Y - TextSize.Y - 10.0);
			if (LabelPosition.Y < 48.0) LabelPosition.Y = FMath::Min(High.Y + 10.0, Size.Y - TextSize.Y - 20.0);
			FBox2D LabelBounds(LabelPosition, LabelPosition + TextSize + FVector2D(12, 8));
			for (const FBox2D& Other : Labels)
			{
				if (LabelBounds.Intersect(Other))
				{
					LabelPosition.Y = Other.Max.Y + 5.0;
					LabelBounds = FBox2D(LabelPosition, LabelPosition + TextSize + FVector2D(12, 8));
				}
			}
			if (LabelBounds.Max.Y > Size.Y - 16.0) continue;
			Labels.Add(LabelBounds);
			FSlateDrawElement::MakeBox(Elements, Layer + 2,
				Geometry.ToPaintGeometry(LabelBounds.GetSize(), FSlateLayoutTransform(LabelPosition - FVector2D(6, 4))),
				FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, FLinearColor(0.012f, 0.025f, 0.04f, 0.76f));
			FSlateDrawElement::MakeText(Elements, Layer + 3,
				Geometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(LabelPosition)), Name, Font, ESlateDrawEffect::None, Accent);
		}
		return Layer + 4;
	}
};

void UJTSCelestialMarkerWidget::UpdateMarkers(APlayerController* Viewer,
	const TArray<FJTSCelestialNavigationBody>& Bodies, bool bCruise)
{
	Markers.Reset();
	const ULocalPlayer* Local = IsValid(Viewer) ? Viewer->GetLocalPlayer() : nullptr;
	FSceneViewProjectionData Projection;
	if (bCruise && Local && Local->ViewportClient && Local->ViewportClient->Viewport
		&& Local->GetProjectionData(Local->ViewportClient->Viewport, Projection))
	{
		const FIntRect Rect = Projection.GetConstrainedViewRect();
		const FIntRect PlayerRect = Projection.GetViewRect();
		const FMatrix Matrix = Projection.ComputeViewProjectionMatrix();
		const double ShortSide = FMath::Max(1, FMath::Min(Rect.Width(), Rect.Height()));
		if (MarkerSlate.IsValid()) MarkerSlate->ProjectionSize = FVector2D(PlayerRect.Size());
		for (const auto& Body : Bodies)
		{
			if (!UJTSCelestialNavigationLibrary::IsWithinMarkerDistance(Body)) continue;
			FBox2D Bounds;
			if (!UJTSCelestialNavigationLibrary::ProjectBody(Body, Projection.ViewOrigin, Matrix, Rect, Bounds)) continue;
			const float Fraction = FMath::Min(Bounds.GetSize().X, Bounds.GetSize().Y) / ShortSide;
			if (UJTSCelestialNavigationLibrary::IsOccluded(Body, Projection.ViewOrigin, Bodies)) continue;
			FJTSCelestialScreenMarker& Marker = Markers.AddDefaulted_GetRef();
			Marker.DisplayName = Body.DisplayName;
			const FVector2D LetterboxOffset(Rect.Min - PlayerRect.Min);
			Marker.Min = Bounds.Min + LetterboxOffset;
			Marker.Max = Bounds.Max + LetterboxOffset;
			Marker.ScreenDiameterFraction = Fraction;
			Marker.PaletteIndex = Body.PaletteIndex;
		}
	}
	if (MarkerSlate.IsValid())
	{
		MarkerSlate->Markers = Markers;
		MarkerSlate->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

TSharedRef<SWidget> UJTSCelestialMarkerWidget::RebuildWidget()
{
	MarkerSlate = SNew(SJTSCelestialMarkers);
	return MarkerSlate.ToSharedRef();
}

void UJTSCelestialMarkerWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MarkerSlate.Reset();
}
