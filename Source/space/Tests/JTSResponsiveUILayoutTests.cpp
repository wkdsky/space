#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/UserInterfaceSettings.h"
#include "space/UI/JTSGameUILayout.h"
#include "space/UI/SJTSStellarLoadoutView.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSResponsiveInventoryLayoutTest, "JTS.UI.ResponsiveInventoryLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSResponsiveInventoryLayoutTest::RunTest(const FString& Parameters)
{
	const FIntPoint Resolutions[] = {{1280,720}, {1280,800}, {1920,1080}, {2560,1440},
		{3840,2160}, {2560,1080}, {1440,1080}};
	for (const auto Resolution : Resolutions)
	{
		const float DPI = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Resolution);
		const FVector2D Viewport = FVector2D(Resolution) / DPI;
		for (int32 Items = 1; Items <= 9; ++Items)
		{
			for (const int32 Available : {3,5,7,9})
			{
				const FString Context = FString::Printf(TEXT("%dx%d / %d items / %d stellar slots"),
					Resolution.X, Resolution.Y, Items, Available);
				const auto Dock = FJTSGameUILayout::InventoryDock(Viewport, Items,
					SJTSStellarLoadoutView::LayoutSize(Available), SJTSStellarLoadoutView::SlotCenter(0).Y);
				const FBox2D Menu = FJTSGameUILayout::TerminalBounds(Viewport, Dock.Top);
				TestTrue(Context + TEXT(" quickbar inside safe edges"), Dock.InventoryPosition.X >= 24
					&& Dock.InventoryPosition.Y >= 24
					&& Dock.InventoryPosition.X + Dock.InventorySize.X <= Viewport.X - 24
					&& Dock.InventoryPosition.Y + Dock.InventorySize.Y <= Viewport.Y - 24);
				TestTrue(Context + TEXT(" stellar dock inside safe edges"), Dock.StellarPosition.X >= 24
					&& Dock.StellarPosition.Y >= 24
					&& Dock.StellarPosition.X + Dock.StellarSize.X <= Viewport.X - 24
					&& Dock.StellarPosition.Y + Dock.StellarSize.Y <= Viewport.Y - 24);
				TestTrue(Context + TEXT(" separate quickbar and stellar dock"),
					Dock.InventoryPosition.X + Dock.InventorySize.X + 8 <= Dock.StellarPosition.X);
				TestTrue(Context + TEXT(" menu clear of equipment and screen edges"),
					Menu.Min.X >= 24 && Menu.Min.Y >= 24 && Menu.Max.X <= Viewport.X - 24
					&& Menu.Max.Y + 23.9 <= Dock.Top);
				TestTrue(Context + TEXT(" readable slot dimensions"), Dock.SlotWidth >= 90 && Dock.SlotHeight >= 80);
				TestTrue(Context + TEXT(" weapon ammo and reload footer inside safe edges"),
					Dock.InventoryPosition.Y + Dock.InventorySize.Y + 39 * Dock.StellarScale <= Viewport.Y - 24);
				// The orange slot is centered on the ordinary quickbar, not below its bottom edge.
				const float SpareY = Dock.StellarPosition.Y + SJTSStellarLoadoutView::SlotCenter(0).Y * Dock.StellarScale;
				TestTrue(Context + TEXT(" spare aligned to quickbar"), FMath::IsNearlyEqual(SpareY,
					static_cast<float>(Dock.InventoryPosition.Y + Dock.InventorySize.Y * 0.5), 0.01f));
			}
		}
	}
	return true;
}
#endif
