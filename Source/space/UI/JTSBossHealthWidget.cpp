#include "space/UI/JTSBossHealthWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "space/Characters/JTSPlanetBoss.h"
#include "space/Components/JTSHealthComponent.h"

TSharedRef<SWidget> UJTSBossHealthWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Root;
		Frame = WidgetTree->ConstructWidget<UCanvasPanel>();
		auto* FrameSlot = Root->AddChildToCanvas(Frame);
		FrameSlot->SetAnchors(FAnchors(.5f, .13f));
		FrameSlot->SetAlignment(FVector2D(.5f, 0));
		FrameSlot->SetSize(FVector2D(620, 72));
		NameText = WidgetTree->ConstructWidget<UTextBlock>();
		NameText->SetJustification(ETextJustify::Center);
		auto Font = NameText->GetFont(); Font.Size = 22; NameText->SetFont(Font);
		NameText->SetColorAndOpacity(FSlateColor(FLinearColor(.86f, .91f, .96f)));
		NameText->SetShadowColorAndOpacity(FLinearColor::Black);
		NameText->SetShadowOffset(FVector2D(1, 2));
		auto* NameSlot = Frame->AddChildToCanvas(NameText);
		NameSlot->SetPosition(FVector2D(0, 0)); NameSlot->SetSize(FVector2D(620, 34));
		Bar = WidgetTree->ConstructWidget<UProgressBar>();
		Bar->SetFillColorAndOpacity(FLinearColor(.65f, .14f, .12f));
		auto Style = Bar->GetWidgetStyle();
		Style.BackgroundImage.TintColor = FSlateColor(FLinearColor(.015f, .022f, .03f, .95f));
		Bar->SetWidgetStyle(Style);
		auto* BarSlot = Frame->AddChildToCanvas(Bar);
		BarSlot->SetPosition(FVector2D(0, 43)); BarSlot->SetSize(FVector2D(620, 14));
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return Super::RebuildWidget();
}
void UJTSBossHealthWidget::NativeTick(const FGeometry& Geometry, float Dt)
{
	Super::NativeTick(Geometry, Dt);
	if (!Frame) return;
	const auto* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const bool bNear = Boss.IsValid() && Pawn && FVector::DistSquared(Pawn->GetActorLocation(), Boss->GetActorLocation()) <= FMath::Square(Boss->HudRange);
	if (Boss.IsValid() && Boss->Health->IsDead()) DeathTime += Dt;
	Frame->SetVisibility(bNear && DeathTime < 4 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (Boss.IsValid())
	{
		NameText->SetText(Boss->DisplayName);
		Bar->SetPercent(Boss->Health->GetHealthNormalized());
	}
}
