#include "space/UI/SJTSNavigationMap.h"
#include "space/UI/JTSCruiseNavigationWidget.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
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
		constexpr int32 Segments = 128;
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


namespace
{
	FLinearColor Opacity(FLinearColor Color, float Alpha) { Color.A *= FMath::Clamp(Alpha, 0.0f, 1.0f); return Color; }
	float Ease(float T) { T = FMath::Clamp(T, 0.0f, 1.0f); return T * T * (3.0f - 2.0f * T); }
	void Mesh(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry,
		const TArray<FVector2D>& Points, const TArray<FLinearColor>& Colors, const TArray<SlateIndex>& Indices)
	{
		if (Points.IsEmpty() || Indices.IsEmpty()) return;
		TArray<FSlateVertex> Vertices;
		Vertices.Reserve(Points.Num());
		for (int32 I = 0; I < Points.Num(); ++I)
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),
				FVector2f(Points[I]), FVector2f::ZeroVector, Colors[I].ToFColor(true)));
		FSlateDrawElement::MakeCustomVerts(Elements, Layer, WhiteBrush()->GetRenderingResource(), Vertices, Indices, nullptr, 0, 0);
	}
	void Disc(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry,
		const FVector2D& Center, float Radius, const FLinearColor& Color)
	{
		TArray<FVector2D> Points; TArray<FLinearColor> Colors; TArray<SlateIndex> Indices;
		Points.Add(Center); Colors.Add(Color);
		for (int32 I = 0; I <= 128; ++I)
		{
			const float Angle = 2 * PI * I / 128;
			Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius); Colors.Add(Color);
			if (I > 0) { Indices.Add(0); Indices.Add(I); Indices.Add(I + 1); }
		}
		Mesh(Elements, Layer, Geometry, Points, Colors, Indices);
	}
	void Arrow(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry,
		const FVector2D& Position, FVector2D Direction, float Radius, const FLinearColor& Color)
	{
		if (!Direction.Normalize()) Direction = FVector2D(0, -1);
		const FVector2D Side(-Direction.Y, Direction.X);
		TArray<FVector2D> Points = { Position + Direction * Radius, Position - Direction * Radius * 0.7 + Side * Radius * 0.65,
			Position - Direction * Radius * 0.4, Position - Direction * Radius * 0.7 - Side * Radius * 0.65 };
		TArray<FLinearColor> Colors; Colors.Init(Color, 4);
		Mesh(Elements, Layer, Geometry, Points, Colors, {0, 1, 2, 0, 2, 3});
	}
	FVector2D BoundMarker(FVector2D P, float Reach)
	{
		const double Extent = FMath::Max(FMath::Abs(P.X), FMath::Abs(P.Y));
		return Extent > Reach ? P * (Reach / Extent) : P;
	}
}

void SJTSCruiseNavigation::SetSurfaceTexture(UTextureRenderTarget2D* Texture)
{
	SurfaceBrush.SetResourceObject(Texture);
	SurfaceBrush.DrawAs = ESlateBrushDrawType::Image;
	// Looking down with camera Y=east makes camera Z=south. Match the north-up markers.
	SurfaceBrush.Mirroring = ESlateBrushMirrorType::Vertical;
	SurfaceBrush.ImageSize = FVector2D(512, 512);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SJTSCruiseNavigation::SetPresentation(const FJTSNavigationMapFrame& InFrame)
{
	if (!bInitialized)
	{
		Zoom = static_cast<float>(InFrame.Stage);
		GroundBlend = InFrame.bGroundMap ? 1 : 0;
		bInitialized = true;
	}
	Frame = InFrame;
	if (Frame.bGroundMap) GroundFrame = Frame;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SJTSCruiseNavigation::Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime)
{
	SLeafWidget::Tick(Geometry, CurrentTime, DeltaTime);
	// Editor/background frame throttling can clamp DeltaTime; presentation follows Slate's real clock.
	const double Elapsed = LastTickTime > 0 ? FMath::Max(0.0, CurrentTime - LastTickTime) : DeltaTime;
	LastTickTime = CurrentTime;
	const float OldZoom = Zoom, OldGround = GroundBlend;
	Zoom = FJTSNavigationMapModel::AdvanceZoom(Zoom, static_cast<double>(Frame.Stage), Elapsed, Frame.TransitionSeconds);
	GroundBlend = FMath::FInterpConstantTo(GroundBlend, Frame.bGroundMap ? 1.0f : 0.0f, static_cast<float>(Elapsed), 2.5f);
	if (OldZoom != Zoom || OldGround != GroundBlend) Invalidate(EInvalidateWidgetReason::Paint);
}

int32 SJTSCruiseNavigation::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	const FVector2D Size = Geometry.GetLocalSize();
	const FLinearColor Tint = Style.GetColorAndOpacityTint();
	FillQuad(Elements, Layer, Geometry, FVector2D::ZeroVector, Size, PanelFill * Tint);
	StrokeRoundedRect(Elements, Layer + 1, Geometry, FVector2D(2, 2), Size - FVector2D(4, 4), 12, PanelEdge * Tint, 1.5f);
	const auto TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 22);
	const auto SmallFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18);
	const FString StageName = Frame.bGroundMap ? TEXT("地表定位") : Zoom < 0.5f ? TEXT("近地导航") : Zoom < 1.5f ? TEXT("星域导航") : TEXT("星际导航");
	const FString Context = Frame.bGroundMap ? TEXT("ORTHO / NORTH UP") : Zoom < 0.5f ? Frame.PlanetName : Zoom < 1.5f ? Frame.SystemName : TEXT("EXPEDITION");
	DrawText(Elements, Layer + 2, Geometry, TitleFont, StageName, FVector2D(24, 16), Ink * Tint, ETextJustify::Left);
	DrawText(Elements, Layer + 2, Geometry, SmallFont, Context, FVector2D(Size.X - 24, 20), InkDim * Tint, ETextJustify::Right);
	for (int32 I = 0; I < 3; ++I)
	{
		const float Weight = FMath::Max(0.0f, 1.0f - FMath::Abs(Zoom - I));
		FillQuad(Elements, Layer + 1, Geometry, FVector2D(20 + I * (Size.X - 40) / 3, 52),
			FVector2D((Size.X - 48) / 3, 2), Opacity(PanelEdge * Tint, 0.2f + Weight * 0.8f));
	}
	Elements.PushClip(FSlateClippingZone(Geometry.ToPaintGeometry(FVector2D(Size.X - 24, Size.Y - 148), FSlateLayoutTransform(FVector2D(12, 64)))));
	if (GroundBlend < 1) PaintFlight(Elements, Layer + 3, Geometry, Opacity(Tint, 1 - Ease(GroundBlend)));
	if (GroundBlend > 0) PaintGround(Elements, Layer + 14, Geometry, Opacity(Tint, Ease(GroundBlend)));
	Elements.PopClip();
	StrokeLine(Elements, Layer + 25, Geometry, FVector2D(20, Size.Y - 78), FVector2D(Size.X - 20, Size.Y - 78), Opacity(InkDim * Tint, 0.6f), 1);
	if (Frame.bGroundMap)
	{
		DrawText(Elements, Layer + 26, Geometry, TitleFont, Frame.PlanetName, FVector2D(24, Size.Y - 63), Ink * Tint, ETextJustify::Left);
		DrawText(Elements, Layer + 26, Geometry, SmallFont, Frame.Coordinates, FVector2D(Size.X - 24, Size.Y - 61), Ink * Tint, ETextJustify::Right);
		DrawText(Elements, Layer + 26, Geometry, SmallFont, TEXT("● 玩家   △ 飞船"), FVector2D(24, Size.Y - 31), InkDim * Tint, ETextJustify::Left);
		const FString Range = Frame.bHasShip ? FString::Printf(TEXT("飞船 %.0f m"), Frame.ShipSurfaceDistance / 100) : TEXT("飞船信号未连接");
		DrawText(Elements, Layer + 26, Geometry, SmallFont, Range, FVector2D(Size.X - 24, Size.Y - 31), InkDim * Tint, ETextJustify::Right);
	}
	else
	{
		const FString Footer = Zoom < 0.5f ? Frame.PlanetName : Zoom < 1.5f ? Frame.SystemName + TEXT("星域") : FString::Printf(TEXT("%d 个星域"), Frame.Systems.Num());
		DrawText(Elements, Layer + 26, Geometry, TitleFont, Footer, FVector2D(24, Size.Y - 62), Ink * Tint, ETextJustify::Left);
		DrawText(Elements, Layer + 26, Geometry, SmallFont, Zoom < 1.5f ? TEXT("引力叠加 / 相对强度") : TEXT("主星航向 / 相对俯仰"),
			FVector2D(24, Size.Y - 31), InkDim * Tint, ETextJustify::Left);
		if (Zoom >= 0.5f && Zoom < 1.5f)
			for (int32 I = 0; I < 36; ++I)
				FillQuad(Elements, Layer + 26, Geometry, FVector2D(Size.X - 132 + I * 3, Size.Y - 29), FVector2D(3, 10),
					FMath::Lerp(FLinearColor(0.06f, 0.25f, 0.33f), FLinearColor(0.95f, 0.65f, 0.22f), I / 35.0f) * Tint);
	}
	return Layer + 27;
}

void SJTSCruiseNavigation::PaintFlight(FSlateWindowElementList& Elements, int32 Layer,
	const FGeometry& Geometry, const FLinearColor& Tint) const
{
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D Center(Size.X * 0.5, (Size.Y - 20) * 0.5);
	const float Reach = FMath::Min(Size.X * 0.5f - 32, (Size.Y - 148) * 0.5f);
	const float Local = Ease(Zoom), Deep = Ease(Zoom - 1);
	const auto Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 21);
	const auto Small = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18);
	const auto* Active = Frame.Systems.FindByPredicate([](const FJTSNavigationMapSystem& S) { return S.bActive; });
	const FVector2D Destination = Center + (Active ? Active->Position : FVector2D::ZeroVector) * Reach;
	TArray<FVector2D> BodyPositions;
	TArray<float> BodyRadii;
	for (const auto& Body : Frame.Bodies)
	{
		const FVector2D LocalPosition = Center + Body.Position * Reach;
		const float NearRadius = Body.bFocus ? 750 : Body.Radius * Reach;
		const float Radius = FMath::Exp(FMath::Lerp(FMath::Loge(NearRadius), FMath::Loge(Body.Radius * Reach), Local));
		// Interpolate the visible limb, then derive the centre from the shrinking radius.
		// Interpolating the far-below-panel centre directly makes the planet disappear midway.
		const FVector2D FocusPosition(FMath::Lerp(Size.X * 0.5, LocalPosition.X, static_cast<double>(Local)),
			FMath::Lerp(Size.Y - 130, LocalPosition.Y - Body.Radius * Reach, static_cast<double>(Local)) + Radius);
		BodyPositions.Add(Body.bFocus ? FocusPosition : LocalPosition);
		BodyPositions.Last() = FMath::Lerp(BodyPositions.Last(), Destination, Deep);
		BodyRadii.Add(FMath::Lerp(Radius, Body.bPrimary ? 6.0f : 0.0f, Deep));
	}
	// A batched, smoothly interpolated scalar field. Contributions add even for uncharted bodies.
	// This is a map visualization of authored gravity, never a simulation force.
	if (Local > 0.01f && Deep < 0.99f)
	{
		constexpr int32 NX = 40, NY = 34;
		TArray<FVector2D> Points; TArray<FLinearColor> Colors; TArray<SlateIndex> Indices;
		for (int32 Y = 0; Y <= NY; ++Y)
			for (int32 X = 0; X <= NX; ++X)
			{
				const FVector2D P(12 + (Size.X - 24) * X / NX, 64 + (Size.Y - 148) * Y / NY);
				double Strength = 0;
				for (int32 I = 0; I < Frame.Bodies.Num(); ++I)
				{
					const double R = BodyRadii[I];
					Strength += Frame.Bodies[I].Gravity * R * R / FMath::Max(R * R, FVector2D::DistSquared(P, BodyPositions[I])
						+ FMath::Square(Frame.Bodies[I].Height * Reach * Local * (1 - Deep)));
				}
				const float Heat = FMath::FloorToFloat((1 - FMath::Exp(-Strength * 2.3)) * 8) / 8;
				FLinearColor Color = Heat < 0.55f ? FMath::Lerp(FLinearColor(0.04f, 0.17f, 0.28f), FLinearColor(0.16f, 0.61f, 0.61f), Heat / 0.55f)
					: FMath::Lerp(FLinearColor(0.16f, 0.61f, 0.61f), FLinearColor(0.97f, 0.66f, 0.22f), (Heat - 0.55f) / 0.45f);
				Color.A = FMath::Min(0.65f, Heat * 0.85f) * Local * (1 - Deep);
				Points.Add(P); Colors.Add(Color * Tint);
				if (X < NX && Y < NY)
				{
					const SlateIndex I = Y * (NX + 1) + X;
					Indices.Append({ I, static_cast<SlateIndex>(I + 1), static_cast<SlateIndex>(I + NX + 1),
						static_cast<SlateIndex>(I + 1), static_cast<SlateIndex>(I + NX + 2), static_cast<SlateIndex>(I + NX + 1) });
				}
			}
		Mesh(Elements, Layer, Geometry, Points, Colors, Indices);
	}
	if (Local > 0)
	{
		const float Grid = FMath::Lerp(Local * 0.65f, 1.0f, Deep);
		StrokeCircle(Elements, Layer, Geometry, Center, Reach * 0.92, Opacity(InkDim * Tint, Grid * 0.4f), 1);
		StrokeEllipse(Elements, Layer, Geometry, Center, FVector2D(Reach * 0.89, Reach * 0.48), Opacity(InkDim * Tint, Grid * 0.6f), 1.3f);
		StrokeLine(Elements, Layer, Geometry, Center - FVector2D(Reach * 0.86, 0), Center + FVector2D(Reach * 0.86, 0), Opacity(InkDim * Tint, Grid * 0.3f), 1);
		StrokeEllipse(Elements, Layer, Geometry, Center, FVector2D(Reach * 0.32, Reach * 0.92), Opacity(InkDim * Tint, Grid * 0.25f), 1);
	}
	TArray<FBox2D> Labels;
	auto Label = [&](const FString& Name, FVector2D Position, const FLinearColor& Color)
	{
		const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Name, Font);
		const double HalfWidth = FMath::Min(TextSize.X * 0.5 + 7, (Size.X - 40) * 0.5);
		Position.X = FMath::Clamp(Position.X, 20 + HalfWidth, Size.X - 20 - HalfWidth);
		Position.Y = FMath::Clamp(Position.Y, 76.0, Size.Y - 114);
		FBox2D Box;
		for (int32 Try = 0; Try < 12; ++Try)
		{
			Box = FBox2D(Position - FVector2D(HalfWidth, 2), Position + FVector2D(HalfWidth, 28));
			if (!Labels.ContainsByPredicate([&](const FBox2D& Other) { return Other.Intersect(Box); })) break;
			Position.Y += 31;
			if (Position.Y > Size.Y - 114) Position.Y = 76;
		}
		Labels.Add(Box);
		FillQuad(Elements, Layer + 6, Geometry, Box.Min, Box.GetSize(), Opacity(PanelFill * Tint, Color.A / FMath::Max(Tint.A, 0.001f)));
		DrawText(Elements, Layer + 7, Geometry, Font, Name, Position, Color, ETextJustify::Center);
	};
	for (int32 I = 0; I < Frame.Bodies.Num(); ++I)
	{
		const auto& Body = Frame.Bodies[I];
		if (!Body.bCharted && !(Body.bFocus && Local < 1)) continue;
		const float Alpha = Body.bCharted ? (Body.bFocus ? 1.0f : Local) * (Body.bPrimary ? 1.0f : 1 - Deep) : 1 - Local;
		if (Alpha < 0.01f) continue;
		const FVector2D P = BodyPositions[I];
		const float R = BodyRadii[I];
		const FLinearColor Color = Opacity(PaletteColor(Body.Palette) * Tint, Alpha);
		Disc(Elements, Layer + 2, Geometry, P, R, Opacity(FLinearColor(0.035f, 0.075f, 0.105f, 1) * Tint, Alpha));
		StrokeCircle(Elements, Layer + 3, Geometry, P, R, Color, 1.7f);
		if (Body.bCharted && Local > 0.8f && Deep < 1)
		{
			const FVector2D Foot = FMath::Lerp(Center + Body.PlanePosition * Reach, Destination, static_cast<double>(Deep));
			const float StemAlpha = Ease((Local - 0.8f) / 0.2f) * (1 - Deep);
			StrokeLine(Elements, Layer + 4, Geometry, Foot, P, Opacity(Color, StemAlpha * 0.65f), 1.4f);
			StrokeCircle(Elements, Layer + 4, Geometry, Foot, 3, Opacity(Color, StemAlpha * 0.5f), 1);
		}
		if (Body.bCharted && Local > 0.15f)
			Label(Body.Name, P + FVector2D(0, R + 8), Opacity(Color, Ease((Local - 0.15f) / 0.6f)));
	}
	if (Deep > 0)
		for (const auto& System : Frame.Systems)
		{
			if (System.bActive) continue; // The primary itself continuously becomes this contact.
			const FVector2D P = Center + System.Position * Reach;
			const FLinearColor Color = Opacity(PaletteColor(System.Palette) * Tint, Deep);
			Disc(Elements, Layer + 3, Geometry, P, 5, Color);
			StrokeCircle(Elements, Layer + 3, Geometry, P, 10, Opacity(Color, 0.5f), 1);
			Label(System.Name, P + FVector2D(0, 17), Color);
		}
	if (Deep > 0.05f)
		for (const auto& System : Frame.Systems)
		{
			const FVector2D P = Center + System.Position * Reach;
			const float Height = FMath::Clamp(System.Elevation / 70.0f, -1.0f, 1.0f) * 32 * Deep;
			if (FMath::Abs(Height) < 3) continue;
			StrokeLine(Elements, Layer + 4, Geometry, P, P - FVector2D(0, Height), Opacity(PaletteColor(System.Palette) * Tint, Deep * 0.65f), 1.5f);
			Arrow(Elements, Layer + 4, Geometry, P - FVector2D(0, Height), FVector2D(0, Height > 0 ? -1 : 1), 4, Opacity(Ink * Tint, Deep));
		}
	const FVector2D LocalShip = Center;
	const FVector2D ShipPosition = FMath::Lerp(FMath::Lerp(FVector2D(Size.X * 0.5, 168), LocalShip, Local), Center, Deep);
	if (Local < 1) PaintShipPerspective(Elements, Layer + 8, Geometry, ShipPosition, Opacity(Tint, 1 - Local));
	const float Heading = FMath::DegreesToRadians(Frame.ShipBearing * (1 - Deep));
	if (Local > 0) Arrow(Elements, Layer + 10, Geometry, ShipPosition, FVector2D(FMath::Sin(Heading), -FMath::Cos(Heading)), 13, Opacity(Ink * Tint, Local));
	if (Local < 1)
	{
		const float Meters = FMath::Max(0.0f, Frame.Altitude) / 100;
		const FString Altitude = Meters < 1000 ? FString::Printf(TEXT("%.0f m"), Meters) : FString::Printf(TEXT("%.2f km"), Meters / 1000);
		DrawText(Elements, Layer + 10, Geometry, FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 56), Altitude,
			FVector2D(Size.X * 0.5, 248), Opacity(Ink * Tint, 1 - Local), ETextJustify::Center);
	}
	if (Deep > 0.8f)
		DrawText(Elements, Layer + 10, Geometry, Small, TEXT("△ 飞船  ·  主星 / 星域"), FVector2D(Size.X * 0.5, Size.Y - 112),
			Opacity(InkDim * Tint, Ease((Deep - 0.8f) / 0.2f)), ETextJustify::Center);
}

void SJTSCruiseNavigation::PaintGround(FSlateWindowElementList& Elements, int32 Layer,
	const FGeometry& Geometry, const FLinearColor& Tint) const
{
	const auto& Data = GroundFrame;
	if (!Data.Terrain) return;
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D Center(Size.X * 0.5, (Size.Y - 20) * 0.5);
	const FVector2D HalfSize((Size.X - 48) * 0.5, (Size.Y - 172) * 0.5);
	const float Reach = FMath::Min(HalfSize.X, HalfSize.Y);
	const FVector2D Origin = Center - FVector2D(Reach, Reach);
	if (SurfaceBrush.GetResourceObject())
		FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(FVector2D(Reach * 2, Reach * 2),
			FSlateLayoutTransform(Origin)), &SurfaceBrush, ESlateDrawEffect::IgnoreTextureAlpha, Tint);
	StrokeRoundedRect(Elements, Layer + 1, Geometry, Origin, FVector2D(Reach * 2, Reach * 2), 2, Opacity(InkDim * Tint, 0.5f), 1);
	const auto Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18);
	const FVector2D Player = Center + BoundMarker(Data.PlayerPosition, 0.94f) * Reach;
	if (Data.bHasShip)
	{
		const FVector2D Ship = Center + BoundMarker(Data.ShipPosition, 0.91f) * Reach;
		StrokeLine(Elements, Layer + 3, Geometry, Player, Ship, Opacity(PaletteColor(2) * Tint, 0.5f), 1);
		StrokeCircle(Elements, Layer + 4, Geometry, Ship, 17, Opacity(PaletteColor(2) * Tint, 0.3f), 1);
		Arrow(Elements, Layer + 5, Geometry, Ship, Data.ShipPosition.Size() > 1 ? Ship - Center : Data.ShipHeading, 12, PaletteColor(2) * Tint);
		if (FVector2D::Distance(Player, Ship) > 36)
			DrawText(Elements, Layer + 5, Geometry, Font, TEXT("飞船"), Ship + FVector2D(0, 20), PaletteColor(2) * Tint, ETextJustify::Center);
	}
	if (Data.bPlayerOnFoot)
	{
		Disc(Elements, Layer + 6, Geometry, Player, 7, Ink * Tint);
		StrokeCircle(Elements, Layer + 6, Geometry, Player, 13, Opacity(Ink * Tint, 0.65f), 1.6f);
		Arrow(Elements, Layer + 6, Geometry, Player + Data.PlayerHeading * 24, Data.PlayerHeading, 7, Ink * Tint);
	}
	DrawText(Elements, Layer + 7, Geometry, Font, TEXT("N ↑"), Origin + FVector2D(10, 9), Ink * Tint, ETextJustify::Left);
	const FVector2D ScaleStart = Origin + FVector2D(12, Reach * 2 - 26);
	StrokeLine(Elements, Layer + 7, Geometry, ScaleStart, ScaleStart + FVector2D(Reach * 0.5, 0), Ink * Tint, 2);
	DrawText(Elements, Layer + 7, Geometry, Font, FString::Printf(TEXT("%.0f m"), Data.GroundHalfWidth / 200), ScaleStart + FVector2D(0, -27), Ink * Tint, ETextJustify::Left);
	if (!SurfaceBrush.GetResourceObject())
		DrawText(Elements, Layer + 7, Geometry, Font, TEXT("地表扫描中"), Center, InkDim * Tint, ETextJustify::Center);
}

void SJTSCruiseNavigation::PaintShipPerspective(
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


void SJTSCruiseNavigation::DrawText(
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
