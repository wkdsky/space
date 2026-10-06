#pragma once

#include "CoreMinimal.h"

/** Layout in UMG local units, after the project DPI curve has been applied. */
struct FJTSInventoryDockLayout
{
	FVector2D InventoryPosition;
	FVector2D InventorySize;
	FVector2D StellarPosition;
	FVector2D StellarSize;
	float SlotWidth = 96;
	float SlotHeight = 88;
	float SlotGap = 8;
	float StellarScale = 1;
	float Top = 0;
};

struct FJTSGameUILayout
{
	static FJTSInventoryDockLayout InventoryDock(FVector2D Viewport, int32 VisibleItems,
		FVector2D StellarSize, float SpareCenterY);
	static FBox2D TerminalBounds(FVector2D Viewport, float DockTop);
};
