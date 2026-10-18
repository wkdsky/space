#include "JTSSpaceDebugWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "space/Player/JTSPlayerController.h"
#include "space/World/JTSSpaceDebugTravelManager.h"

void UJTSDebugPlanetButton::Configure(AJTSPlayerController* Controller, FName InPlanetId, bool bDefault)
{
	Player = Controller; PlanetId = InPlanetId; bSetDefault = bDefault;
	InitIsFocusable(false);
	OnClicked.AddUniqueDynamic(this, &UJTSDebugPlanetButton::Activate);
}
void UJTSDebugPlanetButton::Activate()
{
	if (Player.IsValid()) Player->ServerRequestDebugPlanet(PlanetId, bSetDefault);
}
TSharedRef<SWidget> UJTSSpaceDebugWidget::RebuildWidget()
{
	if (Content) return Super::RebuildWidget();
	auto* Border = WidgetTree->ConstructWidget<UBorder>();
	Border->SetBrushColor(FLinearColor(0.015f, 0.035f, 0.055f, 0.9f));
	Border->SetPadding(FMargin(12)); WidgetTree->RootWidget = Border;
	Content = WidgetTree->ConstructWidget<UVerticalBox>(); Border->SetContent(Content);
	Refresh();
	return Super::RebuildWidget();
}
void UJTSSpaceDebugWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}
void UJTSSpaceDebugWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime); RefreshElapsed += InDeltaTime;
	if (RefreshElapsed >= 0.2f) { RefreshElapsed = 0; Refresh(); }
}
void UJTSSpaceDebugWidget::Refresh()
{
	if (!Manager.IsValid()) Manager = AJTSSpaceDebugTravelManager::Find(this);
	const auto* Debug = Manager.Get();
	const bool bAvailable = Debug && Debug->IsAvailable();
	// Keep the owning widget ticking while waiting for the replicated level actor.
	SetRenderOpacity(bAvailable ? 1.0f : 0.0f);
	if (Content) Content->SetVisibility(bAvailable ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (!Debug || !Content) return;
	const auto& Destinations = Debug->GetDestinations();
	auto* PC = Cast<AJTSPlayerController>(GetOwningPlayer());
	if (Buttons.Num() != Destinations.Num() * 2 || !StatusText)
	{
		Content->ClearChildren(); Buttons.Reset(); DefaultLabels.Reset();
		const auto AddText = [this](const FText& Text, int32 Size)
		{
			auto* Label = WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(Text);
			FSlateFontInfo Font = Label->GetFont(); Font.Size = Size; Label->SetFont(Font);
			Content->AddChildToVerticalBox(Label)->SetPadding(FMargin(0, 0, 0, 6)); return Label;
		};
		AddText(NSLOCTEXT("SpaceDebug", "Title", "DEBUG   /   QUICK TRAVEL"), 13);
		for (const auto& Entry : Destinations)
		{
			auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
			Content->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 2));
			for (int32 Action = 0; Action < 2; ++Action)
			{
				auto* Button = WidgetTree->ConstructWidget<UJTSDebugPlanetButton>();
				Button->Configure(PC, Entry.PlanetId, Action == 1);
				auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
				Label->SetText(Action ? FText::FromString(TEXT("Default")) : Entry.DisplayName);
				FSlateFontInfo Font = Label->GetFont(); Font.Size = 12; Label->SetFont(Font);
				Button->AddChild(Label);
				auto* RowSlot = Row->AddChildToHorizontalBox(Button); RowSlot->SetPadding(FMargin(2));
				if (!Action) RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				Buttons.Add(Button); if (Action) DefaultLabels.Add(Label);
			}
		}
		AddText(NSLOCTEXT("SpaceDebug", "Hint", "Hold Alt: cursor  |  Host only"), 10);
		StatusText = AddText(FText::GetEmpty(), 10); StatusText->SetAutoWrapText(true);
	}
	for (const auto& Button : Buttons) Button->SetIsEnabled(Debug->CanUse(PC));
	for (int32 I = 0; I < DefaultLabels.Num(); ++I)
		DefaultLabels[I]->SetText(FText::FromString(Debug->GetDefaultPlanetId() == Destinations[I].PlanetId ? TEXT("Default *") : TEXT("Default")));
	StatusText->SetText(Debug->GetStatus());
}
