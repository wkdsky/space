#include "space/UI/JTSFloatingDamageWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UJTSFloatingDamageWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSFloatingDamageWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || RootCanvas != nullptr)
	{
		return;
	}
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DamageRoot"));
	WidgetTree->RootWidget = RootCanvas;

	CriticalText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CriticalLabel"));
	CriticalText->SetText(FText::FromString(TEXT("CRITICAL")));
	CriticalText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15));
	CriticalText->SetJustification(ETextJustify::Center);
	CriticalText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.68f, 0.13f)));
	CriticalText->SetShadowOffset(FVector2D(1.0f, 2.0f));
	CriticalText->SetShadowColorAndOpacity(FLinearColor(0.10f, 0.02f, 0.0f, 1.0f));
	CriticalText->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(CriticalText))
	{
		CanvasSlot->SetPosition(FVector2D(0.0f, 0.0f));
		CanvasSlot->SetSize(FVector2D(200.0f, 23.0f));
	}

	DamageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DamageValue"));
	DamageText->SetJustification(ETextJustify::Center);
	DamageText->SetShadowOffset(FVector2D(2.0f, 3.0f));
	DamageText->SetShadowColorAndOpacity(FLinearColor(0.01f, 0.02f, 0.03f, 1.0f));
	if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(DamageText))
	{
		CanvasSlot->SetPosition(FVector2D(0.0f, 18.0f));
		CanvasSlot->SetSize(FVector2D(200.0f, 56.0f));
	}
}

void UJTSFloatingDamageWidget::SetDamage(float Damage, bool bCritical)
{
	BuildWidgetTree();
	if (DamageText == nullptr || CriticalText == nullptr)
	{
		return;
	}
	FNumberFormattingOptions Options;
	Options.SetMaximumFractionalDigits(1);
	Options.SetMinimumFractionalDigits(0);
	DamageText->SetText(FText::AsNumber(FMath::Max(0.0f, Damage), &Options));
	DamageText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), bCritical ? 42 : 31));
	DamageText->SetColorAndOpacity(FSlateColor(bCritical
		? FLinearColor(1.0f, 0.68f, 0.12f)
		: FLinearColor(0.94f, 0.98f, 1.0f)));
	CriticalText->SetVisibility(bCritical ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
