#include "space/UI/JTSHealthBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"

void UJTSHealthBarWidget::SetHealth(float CurrentHealth, float MaxHealth)
{
	CachedMaxHealth = FMath::Max(0.0f, MaxHealth);
	CachedCurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, CachedMaxHealth);
	RefreshPresentation();
}

TSharedRef<SWidget> UJTSHealthBarWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSHealthBarWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || RootCanvas != nullptr)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	if (RootCanvas == nullptr)
	{
		return;
	}
	WidgetTree->RootWidget = RootCanvas;

	Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HealthBackground"));
	if (Background != nullptr)
	{
		Background->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.035f, 0.88f));
		Background->SetPadding(FMargin(2.0f));
		if (UCanvasPanelSlot* const BackgroundSlot = RootCanvas->AddChildToCanvas(Background))
		{
			BackgroundSlot->SetPosition(FVector2D::ZeroVector);
			BackgroundSlot->SetSize(FVector2D(110.0f, 14.0f));
		}
	}

	HealthProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthProgressBar"));
	if (HealthProgressBar != nullptr)
	{
		HealthProgressBar->SetFillColorAndOpacity(FLinearColor(0.92f, 0.26f, 0.16f, 1.0f));
		if (Background != nullptr)
		{
			Background->SetContent(HealthProgressBar);
		}
	}

	RefreshPresentation();
}

void UJTSHealthBarWidget::RefreshPresentation()
{
	if (HealthProgressBar != nullptr)
	{
		const float HealthPercent = CachedMaxHealth > KINDA_SMALL_NUMBER
			? FMath::Clamp(CachedCurrentHealth / CachedMaxHealth, 0.0f, 1.0f)
			: 0.0f;
		HealthProgressBar->SetPercent(HealthPercent);
	}
}
