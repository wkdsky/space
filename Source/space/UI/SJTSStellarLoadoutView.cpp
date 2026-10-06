#include "space/UI/SJTSStellarLoadoutView.h"
#include "space/Components/JTSStellarLoadoutComponent.h"
#include "space/Items/JTSStellarLootTable.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"

namespace
{
	constexpr float Radius = 38.0f;
	constexpr float ColumnStep = 90.0f;
	constexpr float CircleY = 62.0f;
	constexpr float DiamondY = 150.0f;
	constexpr float SpareY = 202.0f;

	// A triangle fan supports both solid metal faces and a soft halo with transparent edges.
	void PaintShape(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry,
		FVector2D Center, FVector2D Extent, bool bDiamond, FLinearColor CenterColor, FLinearColor EdgeColor)
	{
		const int32 Segments = bDiamond ? 4 : 64;
		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		Vertices.Reserve(Segments + 1); Indices.Reserve(Segments * 3);
		const auto& Transform = Geometry.GetAccumulatedRenderTransform();
		auto AddVertex = [&](FVector2D Position, FLinearColor Color)
		{
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform,
				FVector2f(Position), FVector2f(0.5f, 0.5f), Color.ToFColor(true)));
		};
		AddVertex(Center, CenterColor);
		for (int32 Point = 0; Point < Segments; ++Point)
		{
			const double Angle = 2.0 * PI * Point / Segments - PI / 2.0;
			AddVertex(Center + FVector2D(FMath::Cos(Angle) * Extent.X, FMath::Sin(Angle) * Extent.Y), EdgeColor);
			Indices.Append({0, static_cast<SlateIndex>(Point + 1), static_cast<SlateIndex>((Point + 1) % Segments + 1)});
		}
		const auto Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		FSlateDrawElement::MakeCustomVerts(Elements, Layer, Resource, Vertices, Indices, nullptr, 0, 0);
	}
}

void SJTSStellarLoadoutView::Construct(const FArguments& Args)
{
	ReadLoadout = Args._Loadout;
	// Replicated equipment can change without a Slate attribute update; repaint from its current state.
	ForceVolatile(true);
}

FVector2D SJTSStellarLoadoutView::SlotCenter(int32 Index)
{
	if (Index == FJTSStellarLoadoutRules::SpareSlot) return FVector2D(40.0f, SpareY);
	return FVector2D(90.0f + ((Index - 1) / 2) * ColumnStep,
		FJTSStellarLoadoutRules::IsCoreSlot(Index) ? CircleY : DiamondY);
}

FVector2D SJTSStellarLoadoutView::LayoutSize(int32 Available)
{
	return FVector2D(154.0f + (FMath::Max(1, FJTSStellarLoadoutRules::PairCount(Available)) - 1) * ColumnStep, 252.0f);
}

FVector2D SJTSStellarLoadoutView::ComputeDesiredSize(float Scale) const
{
	const auto* State = ReadLoadout ? ReadLoadout() : nullptr;
	return LayoutSize(State ? State->GetAvailableSlots() : 9);
}

int32 SJTSStellarLoadoutView::FindSlot(FVector2D ScreenPosition) const
{
	const auto* State = ReadLoadout ? ReadLoadout() : nullptr;
	const int32 Available = State ? State->GetAvailableSlots() : 9;
	const FVector2D Local = GetCachedGeometry().AbsoluteToLocal(ScreenPosition);
	for (int32 Index = 0; Index < Available; ++Index)
	{
		const FVector2D Delta = Local - SlotCenter(Index);
		if (FJTSStellarLoadoutRules::IsCoreSlot(Index)
			? Delta.SizeSquared() <= Radius * Radius
			: FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) <= Radius) return Index;
	}
	return INDEX_NONE;
}

int32 SJTSStellarLoadoutView::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry,
	const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 Layer,
	const FWidgetStyle& Style, bool bEnabled) const
{
	const auto* State = ReadLoadout ? ReadLoadout() : nullptr;
	const auto* Table = State ? State->GetLootTable() : nullptr;
	const int32 Available = State ? State->GetAvailableSlots() : 9;
	const auto Weapons = State ? State->GetWeapons() : TArray<FJTSStellarWeaponBinding>();
	const FLinearColor Tint = Style.GetColorAndOpacityTint();
	for (const auto& Weapon : Weapons)
	{
		const FLinearColor Color = Table->GetCoreActivationColor(Weapon.CoreId) * Tint;
		const FVector2D Center(SlotCenter(Weapon.CoreSlot).X, (CircleY + DiamondY) * 0.5f);
		// One continuous cloud wraps the circle, its diamond, and the space between them.
		PaintShape(Elements, Layer, Geometry, Center, FVector2D(64, 106), false,
			Color.CopyWithNewOpacity(0.40f * Tint.A), Color.CopyWithNewOpacity(0));
		PaintShape(Elements, Layer + 1, Geometry, Center, FVector2D(51, 92), false,
			Color.CopyWithNewOpacity(0.24f * Tint.A), Color.CopyWithNewOpacity(0));
	}
	const int32 Hovered = FindSlot(FSlateApplication::Get().GetCursorPos());
	for (int32 Index = 0; Index < Available; ++Index)
	{
		const bool bSpare = Index == FJTSStellarLoadoutRules::SpareSlot;
		const bool bDiamond = !FJTSStellarLoadoutRules::IsCoreSlot(Index);
		const FVector2D Center = SlotCenter(Index);
		const auto* Weapon = Weapons.FindByPredicate([Index](const auto& Binding)
			{ return Index == Binding.CoreSlot || Index == Binding.CoreSlot + 1; });
		const bool bSelected = Weapon && State && Weapon->CoreSlot == State->GetActiveCoreSlot();
		const FLinearColor Rim = Weapon ? Table->GetCoreActivationColor(Weapon->CoreId)
			: bSpare ? FLinearColor(1.0f, 0.49f, 0.05f) : FLinearColor(0.70f, 0.68f, 0.63f);
		if (bSelected)
			PaintShape(Elements, Layer + 2, Geometry, Center, FVector2D(Radius + 5), bDiamond,
				FLinearColor::White * Tint, FLinearColor::White * Tint);
		PaintShape(Elements, Layer + 2, Geometry, Center, FVector2D(Radius + 1), bDiamond,
			FLinearColor(0.015f, 0.018f, 0.023f, 0.94f) * Tint, FLinearColor(0.015f, 0.018f, 0.023f, 0.94f) * Tint);
		PaintShape(Elements, Layer + 3, Geometry, Center, FVector2D(Radius), bDiamond,
			Rim * Tint, Rim * Tint);
		PaintShape(Elements, Layer + 4, Geometry, Center, FVector2D(Radius - 2), bDiamond,
			FLinearColor(0.04f, 0.044f, 0.052f, 0.98f) * Tint, FLinearColor(0.20f, 0.20f, 0.19f, 0.94f) * Tint);
		FLinearColor Face = bSpare ? FLinearColor(0.36f, 0.15f, 0.012f, 0.82f) : FLinearColor(0.012f, 0.016f, 0.021f, 0.92f);
		if (Weapon) Face = FMath::Lerp(Face, Rim.CopyWithNewOpacity(0.85f), bSelected ? 0.40f : 0.22f);
		if (Hovered == Index) Face = Face + FLinearColor(0.055f, 0.055f, 0.055f, 0);
		PaintShape(Elements, Layer + 5, Geometry, Center, FVector2D(Radius - 4), bDiamond, Face * Tint, Face * Tint);
		const auto Item = State ? State->GetSlot(Index) : FJTSItemInstance();
		if (Item.IsEmpty()) continue;
		const auto* Entry = Table ? Table->FindEntry(Item.StellarItemId) : nullptr;
		FString Name = Entry ? Entry->DisplayName.ToString() : Item.CustomDisplayName.ToString();
		if (Name.IsEmpty()) Name = Item.StellarItemId.ToString();
		const auto Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18);
		const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		// The shape identifies the role; show a readable compact name and keep the full name in the tooltip.
		Name.ReplaceInline(TEXT("核心"), TEXT(""));
		if (Measure->Measure(Name, Font).X > 50) Name = Name.Left(2);
		const FVector2D TextSize(Measure->Measure(Name, Font));
		FSlateDrawElement::MakeText(Elements, Layer + 6,
			Geometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(Center - TextSize * 0.5f)),
			Name, Font, ESlateDrawEffect::None, FLinearColor(0.91f, 0.95f, 1.0f) * Tint);
	}
	return Layer + 6;
}
