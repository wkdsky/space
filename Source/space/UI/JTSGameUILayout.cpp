#include "space/UI/JTSGameUILayout.h"

FJTSInventoryDockLayout FJTSGameUILayout::InventoryDock(FVector2D Viewport, int32 VisibleItems,
	FVector2D StellarSize, float SpareCenterY)
{
	Viewport.X = FMath::Max(1.0, Viewport.X);
	Viewport.Y = FMath::Max(1.0, Viewport.Y);
	VisibleItems = FMath::Clamp(VisibleItems, 1, 9);
	FJTSInventoryDockLayout Result;
	const float SideMargin = FMath::Clamp(Viewport.Y * 0.045, 32.0, 64.0);
	const float BottomMargin = FMath::Clamp(Viewport.Y * 0.065, 48.0, 96.0);
	const float BaseRowWidth = VisibleItems * Result.SlotWidth + (VisibleItems - 1) * Result.SlotGap;
	const float GroupGap = 16;
	const float FullWidth = BaseRowWidth + GroupGap + StellarSize.X;
	const float Scale = FMath::Min(1.0, FMath::Max(0.1, (Viewport.X - 2 * SideMargin) / FullWidth));
	Result.SlotWidth *= Scale;
	Result.SlotHeight *= Scale;
	Result.SlotGap *= Scale;
	Result.StellarScale = Scale;
	Result.InventorySize = FVector2D(BaseRowWidth * Scale, Result.SlotHeight);
	Result.StellarSize = StellarSize * Scale;
	// Keep the quickbar centered whenever possible. On narrower aspect ratios shift the
	// whole dock left together, preserving readable cells and the right-hand equipment pairs.
	const float IdealLeft = (Viewport.X - Result.InventorySize.X) * 0.5;
	const float RightLimit = Viewport.X - SideMargin - FullWidth * Scale;
	const float Left = FMath::Max(SideMargin, FMath::Min(IdealLeft, RightLimit));
	Result.InventoryPosition = FVector2D(Left, Viewport.Y - BottomMargin - Result.SlotHeight);
	Result.StellarPosition = FVector2D(Left + Result.InventorySize.X + GroupGap * Scale,
		Viewport.Y - BottomMargin - Result.SlotHeight * 0.5 - SpareCenterY * Scale);
	Result.Top = FMath::Min(Result.InventoryPosition.Y, Result.StellarPosition.Y);
	return Result;
}

FBox2D FJTSGameUILayout::TerminalBounds(FVector2D Viewport, float DockTop)
{
	const float Margin = FMath::Clamp(Viewport.Y * 0.04, 24.0, 64.0);
	const FVector2D Available(FMath::Max(1.0, Viewport.X - 2 * Margin),
		FMath::Max(1.0f, DockTop - Margin - 24));
	const FVector2D DesignSize(1320, 720);
	const float Scale = FMath::Min(1.0, FMath::Min(Available.X / DesignSize.X, Available.Y / DesignSize.Y));
	const FVector2D Size = DesignSize * Scale;
	const FVector2D Position((Viewport.X - Size.X) * 0.5, Margin + (Available.Y - Size.Y) * 0.5);
	return FBox2D(Position, Position + Size);
}
