// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSCruiseNavigationWidget.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

#include <initializer_list>

namespace
{
	const FLinearColor Ink(0.72f, 0.88f, 0.98f, 0.95f);
	const FLinearColor InkDim(0.48f, 0.68f, 0.82f, 0.72f);
	const FLinearColor PanelFill(0.025f, 0.045f, 0.075f, 0.78f);
	const FLinearColor PanelEdge(0.45f, 0.78f, 0.95f, 0.85f);

	FLinearColor PaletteColor(int32 PaletteIndex)
	{
		switch (PaletteIndex)
		{
		case 0: return FLinearColor(0.45f, 0.86f, 1.0f, 1.0f);
		case 1: return FLinearColor(0.95f, 0.48f, 0.32f, 1.0f);
		default: return FLinearColor(0.96f, 0.78f, 0.38f, 1.0f);
		}
	}

	const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	}

	void FillQuad(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FVector2D& Origin,
		const FVector2D& Size,
		const FLinearColor& Color)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Origin)),
			WhiteBrush(),
			ESlateDrawEffect::None,
			Color);
	}

	void StrokeLine(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FVector2D& From,
		const FVector2D& To,
		const FLinearColor& Color,
		float Thickness)
	{
		TArray<FVector2D> Points;
		Points.Add(From);
		Points.Add(To);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			Color,
			true,
			Thickness);
	}

	void StrokeCircle(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FVector2D& Center,
		float Radius,
		const FLinearColor& Color,
		float Thickness)
	{
		constexpr int32 Segments = 64;
		TArray<FVector2D> Points;
		Points.Reserve(Segments + 1);
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float Angle = (static_cast<float>(Index) / static_cast<float>(Segments)) * 2.0f * PI;
			Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			Color,
			true,
			Thickness);
	}

	void StrokeEllipse(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FVector2D& Center,
		const FVector2D& Radii,
		const FLinearColor& Color,
		float Thickness)
	{
		constexpr int32 Segments = 64;
		TArray<FVector2D> Points;
		Points.Reserve(Segments + 1);
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float Angle = (static_cast<float>(Index) / Segments) * 2.0f * PI;
			Points.Add(Center + FVector2D(FMath::Cos(Angle) * Radii.X, FMath::Sin(Angle) * Radii.Y));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geometry.ToPaintGeometry(),
			Points, ESlateDrawEffect::None, Color, true, Thickness);
	}

	void StrokeRoundedRect(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FVector2D& Origin,
		const FVector2D& Size,
		float Radius,
		const FLinearColor& Color,
		float Thickness)
	{
		constexpr int32 CornerSteps = 6;
		TArray<FVector2D> Points;
		Points.Reserve(CornerSteps * 4 + 1);
		const FVector2D Corners[4] = {
			Origin + FVector2D(Radius, Radius),
			Origin + FVector2D(Size.X - Radius, Radius),
			Origin + FVector2D(Size.X - Radius, Size.Y - Radius),
			Origin + FVector2D(Radius, Size.Y - Radius)
		};
		const float StartAngles[4] = { PI, PI * 1.5f, 0.0f, PI * 0.5f };
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			for (int32 Step = 0; Step < CornerSteps; ++Step)
			{
				const float Angle = StartAngles[Corner] + (PI * 0.5f) * (static_cast<float>(Step) / static_cast<float>(CornerSteps));
				Points.Add(Corners[Corner] + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
			}
		}
		const FVector2D ClosingPoint = Points[0];
		Points.Add(ClosingPoint);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			Color,
			true,
			Thickness);
	}
}

class SJTSCruiseNavigation : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SJTSCruiseNavigation) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
	}

	void SetPresentation(
		const TArray<FJTSCruiseNavigationContact>& InContacts,
		const FString& InLeftCaption,
		const FString& InRightCaption,
		bool bInSurfaceMode,
		float InSurfaceFraction)
	{
		Contacts = InContacts;
		LeftCaption = InLeftCaption;
		RightCaption = InRightCaption;
		bSurfaceMode = bInSurfaceMode;
		SurfaceFraction = FMath::Clamp(InSurfaceFraction, 0.0f, 1.0f);
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(520.0f, 560.0f);
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const FVector2D Size = AllottedGeometry.GetLocalSize();
		const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
		FillQuad(OutDrawElements, LayerId, AllottedGeometry, FVector2D::ZeroVector, Size, PanelFill * Tint);
		StrokeRoundedRect(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2D(2.0f, 2.0f), Size - FVector2D(4.0f, 4.0f), 10.0f, PanelEdge * Tint, 1.5f);
		StrokeLine(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2D(20.0f, 52.0f), FVector2D(Size.X - 20.0f, 52.0f), InkDim * Tint, 1.0f);

		// The HUD renders this dial at half size; 1.4x source fonts keep text at 70% on screen.
		const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 22);
		const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 21);
		DrawText(OutDrawElements, LayerId + 2, AllottedGeometry, TitleFont, LeftCaption, FVector2D(24.0f, 16.0f), Ink * Tint, ETextJustify::Left);
		DrawText(OutDrawElements, LayerId + 2, AllottedGeometry, TitleFont, RightCaption, FVector2D(Size.X - 24.0f, 16.0f), Ink * Tint, ETextJustify::Right);

		const FVector2D DialCenter(Size.X * 0.5f, 68.0f + (Size.Y - 148.0f) * 0.5f);
		const float DialRadius = FMath::Min(Size.X * 0.5f - 28.0f, (Size.Y - 148.0f) * 0.5f - 8.0f);
		if (bSurfaceMode)
		{
			PaintSurface(OutDrawElements, LayerId + 3, AllottedGeometry, DialCenter, DialRadius, Tint);
		}
		else
		{
			PaintCruise(OutDrawElements, LayerId + 3, AllottedGeometry, DialCenter, DialRadius, Tint, LabelFont);
			PaintCruiseMarker(OutDrawElements, LayerId + 6, AllottedGeometry, DialCenter, Tint);
			PaintLegend(OutDrawElements, LayerId + 2, AllottedGeometry, LabelFont, Tint);
		}
		return LayerId + 7;
	}

private:
	void PaintCruiseMarker(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FVector2D& Anchor,
		const FLinearColor& Tint) const
	{
		const FLinearColor Marker = FLinearColor(0.94f, 0.97f, 1.0f, 1.0f) * Tint;
		TArray<FVector2D> Triangle;
		Triangle.Add(Anchor + FVector2D(0.0f, -16.0f));
		Triangle.Add(Anchor + FVector2D(13.0f, 13.0f));
		Triangle.Add(Anchor + FVector2D(-13.0f, 13.0f));
		const FVector2D ClosingPoint = Triangle[0];
		Triangle.Add(ClosingPoint);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(),
			Triangle,
			ESlateDrawEffect::None,
			Marker,
			true,
			2.2f);
	}

	void PaintShip(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FVector2D& Anchor,
		const FLinearColor& Tint) const
	{
		const FLinearColor HullColor = FLinearColor(0.90f, 0.94f, 0.98f, 1.0f) * Tint;
		const FLinearColor WingColor = FLinearColor(0.55f, 0.62f, 0.70f, 1.0f) * Tint;
		const FLinearColor Gold = FLinearColor(0.92f, 0.72f, 0.28f, 1.0f) * Tint;
		auto Stroke = [&](std::initializer_list<FVector2D> Offsets, const FLinearColor& Color, float Thickness)
		{
			TArray<FVector2D> Points;
			Points.Reserve(static_cast<int32>(Offsets.size()));
			for (const FVector2D& Offset : Offsets)
			{
				Points.Add(Anchor + Offset);
			}
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				Geometry.ToPaintGeometry(),
				Points,
				ESlateDrawEffect::None,
				Color,
				true,
				Thickness);
		};
		Stroke({
			FVector2D(0.0f, -24.0f),
			FVector2D(6.0f, -8.0f),
			FVector2D(6.0f, 14.0f),
			FVector2D(-6.0f, 14.0f),
			FVector2D(-6.0f, -8.0f),
			FVector2D(0.0f, -24.0f)
		}, HullColor, 1.8f);
		Stroke({
			FVector2D(-6.0f, -2.0f),
			FVector2D(-26.0f, 12.0f),
			FVector2D(-16.0f, 16.0f),
			FVector2D(-6.0f, 8.0f)
		}, WingColor, 1.8f);
		Stroke({
			FVector2D(6.0f, -2.0f),
			FVector2D(26.0f, 12.0f),
			FVector2D(16.0f, 16.0f),
			FVector2D(6.0f, 8.0f)
		}, WingColor, 1.8f);
		Stroke({
			FVector2D(-8.0f, 10.0f),
			FVector2D(-8.0f, 20.0f),
			FVector2D(-3.0f, 20.0f),
			FVector2D(-3.0f, 10.0f)
		}, HullColor, 1.6f);
		Stroke({
			FVector2D(8.0f, 10.0f),
			FVector2D(8.0f, 20.0f),
			FVector2D(3.0f, 20.0f),
			FVector2D(3.0f, 10.0f)
		}, HullColor, 1.6f);
		Stroke({
			FVector2D(0.0f, -16.0f),
			FVector2D(0.0f, 12.0f)
		}, Gold, 2.0f);
	}

	void PaintShipPerspective(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FVector2D& Anchor,
		const FLinearColor& Tint) const
	{
		const FLinearColor Hull = FLinearColor(0.90f, 0.94f, 0.98f, 1.0f) * Tint;
		const FLinearColor Wing = FLinearColor(0.58f, 0.66f, 0.74f, 1.0f) * Tint;
		const FLinearColor Gold = FLinearColor(0.92f, 0.72f, 0.28f, 1.0f) * Tint;
		const FLinearColor Shade = FLinearColor(0.28f, 0.34f, 0.42f, 1.0f) * Tint;
		auto Fill = [&](std::initializer_list<FVector2D> Offsets, const FLinearColor& Color)
		{
			TArray<FSlateVertex> Vertices;
			TArray<SlateIndex> Indices;
			Vertices.Reserve(static_cast<int32>(Offsets.size()));
			const FSlateRenderTransform RenderTransform = Geometry.GetAccumulatedRenderTransform();
			const FColor VertexColor = Color.ToFColor(true);
			for (const FVector2D& Offset : Offsets)
			{
				const FVector2D Local = Anchor + Offset;
				Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
					RenderTransform,
					FVector2f(Local),
					FVector2f(0.0f, 0.0f),
					VertexColor));
			}
			for (int32 Index = 1; Index + 1 < Vertices.Num(); ++Index)
			{
				Indices.Add(0);
				Indices.Add(static_cast<SlateIndex>(Index));
				Indices.Add(static_cast<SlateIndex>(Index + 1));
			}
			FSlateDrawElement::MakeCustomVerts(
				OutDrawElements,
				LayerId,
				WhiteBrush()->GetRenderingResource(),
				Vertices,
				Indices,
				nullptr,
				0,
				0,
				ESlateDrawEffect::None);
		};
		auto Stroke = [&](std::initializer_list<FVector2D> Offsets, const FLinearColor& Color, float Thickness)
		{
			TArray<FVector2D> Points;
			Points.Reserve(static_cast<int32>(Offsets.size()));
			for (const FVector2D& Offset : Offsets)
			{
				Points.Add(Anchor + Offset);
			}
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId + 1,
				Geometry.ToPaintGeometry(),
				Points,
				ESlateDrawEffect::None,
				Color,
				true,
				Thickness);
		};

		Fill({
			FVector2D(-46.0f, 16.0f),
			FVector2D(-10.0f, -2.0f),
			FVector2D(-8.0f, 22.0f),
			FVector2D(-28.0f, 30.0f)
		}, Wing);
		Fill({
			FVector2D(46.0f, 16.0f),
			FVector2D(28.0f, 30.0f),
			FVector2D(8.0f, 22.0f),
			FVector2D(10.0f, -2.0f)
		}, Wing);
		Fill({
			FVector2D(0.0f, -38.0f),
			FVector2D(11.0f, -6.0f),
			FVector2D(9.0f, 28.0f),
			FVector2D(0.0f, 34.0f),
			FVector2D(-9.0f, 28.0f),
			FVector2D(-11.0f, -6.0f)
		}, Hull);
		Fill({
			FVector2D(0.0f, -38.0f),
			FVector2D(11.0f, -6.0f),
			FVector2D(4.0f, 8.0f),
			FVector2D(0.0f, -8.0f)
		}, Shade);
		Fill({
			FVector2D(-5.0f, 16.0f),
			FVector2D(-12.0f, 18.0f),
			FVector2D(-11.0f, 32.0f),
			FVector2D(-4.0f, 28.0f)
		}, Shade);
		Fill({
			FVector2D(5.0f, 16.0f),
			FVector2D(4.0f, 28.0f),
			FVector2D(11.0f, 32.0f),
			FVector2D(12.0f, 18.0f)
		}, Shade);
		Stroke({
			FVector2D(0.0f, -38.0f),
			FVector2D(11.0f, -6.0f),
			FVector2D(9.0f, 28.0f),
			FVector2D(0.0f, 34.0f),
			FVector2D(-9.0f, 28.0f),
			FVector2D(-11.0f, -6.0f),
			FVector2D(0.0f, -38.0f)
		}, Hull, 1.6f);
		Stroke({
			FVector2D(-10.0f, -2.0f),
			FVector2D(-46.0f, 16.0f),
			FVector2D(-28.0f, 30.0f),
			FVector2D(-8.0f, 22.0f)
		}, Wing, 1.5f);
		Stroke({
			FVector2D(10.0f, -2.0f),
			FVector2D(46.0f, 16.0f),
			FVector2D(28.0f, 30.0f),
			FVector2D(8.0f, 22.0f)
		}, Wing, 1.5f);
		Stroke({
			FVector2D(0.0f, -28.0f),
			FVector2D(0.0f, 22.0f)
		}, Gold, 2.2f);
	}

	void PaintChevron(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FVector2D& Origin,
		const FVector2D& Direction,
		const FLinearColor& Color) const
	{
		const FVector2D Forward = Direction.GetSafeNormal();
		const FVector2D Side(-Forward.Y, Forward.X);
		TArray<FVector2D> Chevron;
		Chevron.Add(Origin - Forward * 4.0f - Side * 5.0f);
		Chevron.Add(Origin + Forward * 5.0f);
		Chevron.Add(Origin - Forward * 4.0f + Side * 5.0f);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(),
			Chevron,
			ESlateDrawEffect::None,
			Color,
			true,
			1.6f);
	}

	void PaintCruise(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FVector2D& DialCenter,
		float DialRadius,
		const FLinearColor& Tint,
		const FSlateFontInfo& LabelFont) const
	{
		// The flattened ring is the ship's pitch plane. Every contact drops a stalk
		// to its projection on this plane, so pitch alignment is visible without text.
		const FLinearColor Grid = InkDim * Tint;
		StrokeCircle(OutDrawElements, LayerId, Geometry, DialCenter, DialRadius * 0.92f,
			FLinearColor(Grid.R, Grid.G, Grid.B, 0.42f), 1.2f);
		StrokeCircle(OutDrawElements, LayerId, Geometry, DialCenter, DialRadius * 0.49f,
			FLinearColor(Grid.R, Grid.G, Grid.B, 0.25f), 1.0f);
		StrokeEllipse(OutDrawElements, LayerId, Geometry, DialCenter,
			FVector2D(DialRadius * 0.89f, DialRadius * 0.30f),
			FLinearColor(Grid.R, Grid.G, Grid.B, 0.58f), 1.6f);
		StrokeLine(OutDrawElements, LayerId, Geometry,
			DialCenter - FVector2D(DialRadius * 0.86f, 0.0f),
			DialCenter + FVector2D(DialRadius * 0.86f, 0.0f),
			FLinearColor(Grid.R, Grid.G, Grid.B, 0.32f), 1.0f);

		const float InnerReach = DialRadius * 0.42f;
		const float OuterReach = DialRadius * 0.78f;

		for (const FJTSCruiseNavigationContact& Contact : Contacts)
		{
			const float Elevation = FMath::Clamp(Contact.ElevationDegrees, -90.0f, 90.0f);
			const bool bAbove = Elevation > 2.0f;
			const bool bBelow = Elevation < -2.0f;
			const float ScopeFraction = UJTSCruiseNavigationWidget::CruiseRangeRadialFraction(
				Contact.RangeCentimeters);
			const float BearingRadians = FMath::DegreesToRadians(Contact.BearingDegrees);
			const FVector2D Direction(FMath::Sin(BearingRadians), -FMath::Cos(BearingRadians));
			const FVector2D Projection = DialCenter + Direction * FMath::Lerp(InnerReach, OuterReach, ScopeFraction);
			const float Height = FMath::Clamp((FMath::Abs(Elevation) - 2.0f) / 63.0f, 0.0f, 1.0f) * 55.0f;
			const FVector2D Blip(Projection.X,
				FMath::Clamp(Projection.Y + (bAbove ? -Height : bBelow ? Height : 0.0f),
					72.0f, Geometry.GetLocalSize().Y - 82.0f));

			const int32 Palette = FMath::Clamp(Contact.PaletteIndex, 0, 2);
			const FLinearColor BlipColor = PaletteColor(Palette) * Tint;
			const FLinearColor ProjectionColor(BlipColor.R, BlipColor.G, BlipColor.B, 0.35f);
			StrokeCircle(OutDrawElements, LayerId + 1, Geometry, Projection, 8.0f, ProjectionColor, 1.0f);
			FillQuad(OutDrawElements, LayerId + 1, Geometry, Projection - FVector2D(2.0f, 2.0f),
				FVector2D(4.0f, 4.0f), ProjectionColor);

			if (bAbove || bBelow)
			{
				const FVector2D ChevronDirection(0.0f, bAbove ? -1.0f : 1.0f);
				StrokeLine(OutDrawElements, LayerId + 1, Geometry, Projection, Blip,
					FLinearColor(BlipColor.R, BlipColor.G, BlipColor.B, bAbove ? 0.9f : 0.62f), 2.3f);
				StrokeLine(OutDrawElements, LayerId + 1, Geometry,
					Projection - FVector2D(7.0f, 0.0f), Projection + FVector2D(7.0f, 0.0f),
					BlipColor, 1.4f);
				if (FMath::Abs(Blip.Y - Projection.Y) > 15.0f)
				{
					PaintChevron(OutDrawElements, LayerId + 2, Geometry,
						(Blip + Projection) * 0.5f, ChevronDirection, BlipColor);
				}
			}
			if (!bBelow)
			{
				FillQuad(OutDrawElements, LayerId + 2, Geometry, Blip - FVector2D(5.0f, 5.0f),
					FVector2D(10.0f, 10.0f), BlipColor);
			}
			StrokeCircle(OutDrawElements, LayerId + 2, Geometry, Blip,
				bBelow ? 10.0f : 9.0f, BlipColor, bBelow ? 2.1f : 1.5f);

			const FString RangeLabel = UJTSCruiseNavigationWidget::FormatAstronomicalRange(Contact.RangeCentimeters);
			const double BelowWeight = 1.0 - FMath::SmoothStep(95.0, 135.0, Blip.Y);
			const double LabelY = Blip.Y - 22.0 + BelowWeight * 38.0;
			const FVector2D NamePosition(Blip.X - 16.0f, LabelY);
			DrawText(
				OutDrawElements,
				LayerId + 1,
				Geometry,
				LabelFont,
				Contact.DisplayName,
				NamePosition,
				BlipColor,
				ETextJustify::Right);
			DrawText(
				OutDrawElements,
				LayerId + 1,
				Geometry,
				LabelFont,
				RangeLabel,
					NamePosition + FVector2D(0.0f, 28.0f),
				Ink * Tint,
				ETextJustify::Right);
		}
	}

	void PaintSurface(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FVector2D& DialCenter,
		float DialRadius,
		const FLinearColor& Tint) const
	{
		(void)DialCenter;
		(void)DialRadius;
		const FVector2D PanelSize = Geometry.GetLocalSize();
		const FVector2D LeftEdge(10.0f, PanelSize.Y - 8.0f);
		const FVector2D RightEdge(PanelSize.X - 10.0f, PanelSize.Y - 8.0f);
		const FVector2D HorizonControl(PanelSize.X * 0.5f, PanelSize.Y - 150.0f);
		constexpr int32 ArcSteps = 32;
		TArray<FVector2D> Arc;
		Arc.Reserve(ArcSteps + 1);
		for (int32 Step = 0; Step <= ArcSteps; ++Step)
		{
			const float T = static_cast<float>(Step) / static_cast<float>(ArcSteps);
			const float Inverse = 1.0f - T;
			Arc.Add(LeftEdge * (Inverse * Inverse) + HorizonControl * (2.0f * Inverse * T) + RightEdge * (T * T));
		}
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(),
			Arc,
			ESlateDrawEffect::None,
			PanelEdge * Tint,
			true,
			2.2f);

		const FVector2D ShipAnchor(PanelSize.X * 0.5f, 168.0f);
		PaintShipPerspective(OutDrawElements, LayerId + 2, Geometry, ShipAnchor, Tint);

		const FString Distance = Contacts.Num() > 0 ? FormatSurfaceRange(Contacts[0].RangeCentimeters) : FString(TEXT("0 m"));
		const FSlateFontInfo DistanceFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 56);
		DrawText(
			OutDrawElements,
			LayerId + 1,
			Geometry,
			DistanceFont,
			Distance,
			FVector2D(PanelSize.X * 0.5f, 248.0f),
			FLinearColor(0.90f, 0.96f, 1.0f) * Tint,
			ETextJustify::Center);
	}

	void PaintLegend(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FSlateFontInfo& LabelFont,
		const FLinearColor& Tint) const
	{
		struct FLegend
		{
			int32 Palette;
			const TCHAR* Label;
		};
		const FLegend Entries[] = {
			{0, TEXT("MOON")},
			{1, TEXT("MAR II")},
			{2, TEXT("OTHER")}
		};
		const FVector2D PanelSize = Geometry.GetLocalSize();
		float X = 36.0f;
		const float LegendY = PanelSize.Y - 42.0f;
		for (const FLegend& Entry : Entries)
		{
			const FLinearColor Swatch = PaletteColor(Entry.Palette) * Tint;
			FillQuad(OutDrawElements, LayerId, Geometry, FVector2D(X, LegendY + 4.0f), FVector2D(14.0f, 14.0f), Swatch);
			DrawText(OutDrawElements, LayerId, Geometry, LabelFont, Entry.Label, FVector2D(X + 22.0f, LegendY), Ink * Tint, ETextJustify::Left);
			X += 156.0f;
		}
	}

	static FString FormatSurfaceRange(float Centimeters)
	{
		const float Meters = FMath::Max(0.0f, Centimeters) / 100.0f;
		if (Meters < 1000.0f)
		{
			return FString::Printf(TEXT("%d m"), FMath::RoundToInt(Meters));
		}
		return FString::Printf(TEXT("%.2f km"), Meters / 1000.0f);
	}

	void DrawText(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geometry,
		const FSlateFontInfo& Font,
		const FString& Text,
		const FVector2D& Position,
		const FLinearColor& Color,
		ETextJustify::Type Justify) const
	{
		FVector2D DrawPosition = Position;
		if (Justify != ETextJustify::Left)
		{
			const FVector2D Measured = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
			DrawPosition.X -= Justify == ETextJustify::Center ? Measured.X * 0.5f : Measured.X;
		}
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(FVector2D(360.0f, 56.0f), FSlateLayoutTransform(DrawPosition)),
			Text,
			Font,
			ESlateDrawEffect::None,
			Color);
	}

	TArray<FJTSCruiseNavigationContact> Contacts;
	FString LeftCaption;
	FString RightCaption;
	bool bSurfaceMode = false;
	float SurfaceFraction = 0.0f;
};

UJTSCruiseNavigationWidget::UJTSCruiseNavigationWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FString UJTSCruiseNavigationWidget::FormatAstronomicalRange(float Centimeters)
{
	constexpr double AstronomicalUnitCentimeters = 1.495978707e13;
	const double AstronomicalUnits = static_cast<double>(FMath::Max(0.0f, Centimeters)) / AstronomicalUnitCentimeters;
	if (AstronomicalUnits < 0.01)
	{
		return TEXT("<0.01 AU");
	}
	return FString::Printf(TEXT("%.2f AU"), AstronomicalUnits);
}

float UJTSCruiseNavigationWidget::CruiseRangeRadialFraction(float Centimeters)
{
	constexpr double AstronomicalUnitCentimeters = 1.495978707e13;
	constexpr double HalfDialRangeAU = 0.35;
	const double RangeAU = FMath::Max(0.0, static_cast<double>(Centimeters))
		/ AstronomicalUnitCentimeters;
	// A fixed, soft AU scale keeps each contact continuous when two bodies exchange
	// nearest/farthest order. It also leaves useful space on the dial past one AU.
	return static_cast<float>(RangeAU / (RangeAU + HalfDialRangeAU));
}

FString UJTSCruiseNavigationWidget::FormatNavigationSpeed(float CentimetersPerSecond)
{
	const double SpeedCentimetersPerSecond = FMath::Max(0.0f, CentimetersPerSecond);
	constexpr double AstronomicalUnitCentimeters = 1.495978707e13;
	if (SpeedCentimetersPerSecond >= 0.01 * AstronomicalUnitCentimeters)
	{
		return FString::Printf(TEXT("%.3f AU/s"), SpeedCentimetersPerSecond / AstronomicalUnitCentimeters);
	}
	const double KilometersPerSecond = SpeedCentimetersPerSecond / 100000.0;
	if (KilometersPerSecond >= 1000.0)
	{
		return FString::Printf(TEXT("%.0f km/s"), KilometersPerSecond);
	}
	if (KilometersPerSecond >= 1.0)
	{
		return FString::Printf(TEXT("%.1f km/s"), KilometersPerSecond);
	}
	return FString::Printf(TEXT("%d m/s"), FMath::RoundToInt(SpeedCentimetersPerSecond / 100.0));
}

void UJTSCruiseNavigationWidget::SetCruisePresentation(
	const TArray<FJTSCruiseNavigationContact>& InContacts,
	const FString& InLeftCaption,
	const FString& InRightCaption,
	bool bInSurfaceMode,
	float InSurfaceFraction)
{
	Contacts = InContacts;
	LeftCaption = InLeftCaption;
	RightCaption = InRightCaption;
	bSurfaceMode = bInSurfaceMode;
	SurfaceFraction = InSurfaceFraction;
	PushPresentation();
}

TSharedRef<SWidget> UJTSCruiseNavigationWidget::RebuildWidget()
{
	NavigationSlate = SNew(SJTSCruiseNavigation);
	PushPresentation();
	return NavigationSlate.ToSharedRef();
}

void UJTSCruiseNavigationWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	NavigationSlate.Reset();
}

void UJTSCruiseNavigationWidget::PushPresentation()
{
	if (NavigationSlate.IsValid())
	{
		NavigationSlate->SetPresentation(Contacts, LeftCaption, RightCaption, bSurfaceMode, SurfaceFraction);
	}
}
