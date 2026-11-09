#include "space/UI/JTSExpeditionNoticeWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UJTSExpeditionNoticeWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget = Root;
		auto* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NoticePanel"));
		Panel->SetBrushColor(FLinearColor(.012f, .022f, .035f, .76f));
		Panel->SetPadding(FMargin(36, 22));
		auto* PanelSlot = Root->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(.5f, 1.f / 3.f)); PanelSlot->SetAlignment(FVector2D(.5f)); PanelSlot->SetAutoSize(true);
		auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
		Text->SetText(PendingMessage); Text->SetJustification(ETextJustify::Center);
		auto Font = Text->GetFont(); Font.Size = 24; Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(FLinearColor(.80f, .94f, 1)));
		Panel->SetContent(Text);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}
void UJTSExpeditionNoticeWidget::NativeTick(const FGeometry& Geometry, float Dt)
{
	Super::NativeTick(Geometry, Dt); Age += Dt;
	SetRenderOpacity(FMath::Min(FMath::Clamp(Age / .18f, 0.f, 1.f), FMath::Clamp((4.2f - Age) / .6f, 0.f, 1.f)));
	if (Age >= 4.2f) RemoveFromParent();
}
