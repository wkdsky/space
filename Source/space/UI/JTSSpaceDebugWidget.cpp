#include "JTSSpaceDebugWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/LocalPlayer.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Systems/JTSDebugViewSubsystem.h"
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
void UJTSSpaceDebugWidget::ToggleDarkVisibility()
{
	if (auto* LocalPlayer = GetOwningLocalPlayer())
	{
		if (auto* View = LocalPlayer->GetSubsystem<UJTSDebugViewSubsystem>())
			View->SetDarkVisibilityEnabled(!View->IsDarkVisibilityEnabled());
	}
	Refresh();
}
void UJTSSpaceDebugWidget::ToggleInvulnerability()
{
	if (auto* PC = Cast<AJTSPlayerController>(GetOwningPlayer()))
		PC->ServerSetDebugInvulnerable(!PC->IsDebugInvulnerable());
	Refresh();
}
void UJTSSpaceDebugWidget::ToggleOneHitKill()
{
	if (auto* PC = Cast<AJTSPlayerController>(GetOwningPlayer()))
		PC->ServerSetDebugOneHitKill(!PC->IsDebugOneHitKillEnabled());
	Refresh();
}
void UJTSSpaceDebugWidget::Refresh()
{
	if (!Manager.IsValid()) Manager = AJTSSpaceDebugTravelManager::Find(this);
	const auto* Debug = Manager.Get();
	const bool bAvailable = Debug && Debug->IsAvailable();
	auto* LocalPlayer = GetOwningLocalPlayer();
	auto* View = LocalPlayer ? LocalPlayer->GetSubsystem<UJTSDebugViewSubsystem>() : nullptr;
	if (!bAvailable && View) View->SetDarkVisibilityEnabled(false);
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
		AddText(NSLOCTEXT("SpaceDebug", "Title", "DEBUG   /   SPACE"), 13);
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
		DarkVisibilityButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DarkVisibilityToggle"));
		DarkVisibilityButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ToggleDarkVisibility);
		DarkVisibilityButton->SetToolTipText(NSLOCTEXT("SpaceDebug", "DarkVisibilityTooltip",
			"切换无光照材质视图，看清星球背光面和无灯光区域。仅影响本机；关闭后恢复原来的渲染设置。"));
		DarkVisibilityLabel = WidgetTree->ConstructWidget<UTextBlock>();
		FSlateFontInfo Font = DarkVisibilityLabel->GetFont(); Font.Size = 12; DarkVisibilityLabel->SetFont(Font);
		DarkVisibilityLabel->SetJustification(ETextJustify::Center);
		DarkVisibilityButton->AddChild(DarkVisibilityLabel);
		Content->AddChildToVerticalBox(DarkVisibilityButton)->SetPadding(FMargin(2, 6, 2, 6));
		InvulnerabilityButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PlayerInvulnerabilityToggle"));
		InvulnerabilityButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ToggleInvulnerability);
		InvulnerabilityButton->SetToolTipText(NSLOCTEXT("SpaceDebug", "InvulnerabilityTooltip",
			"仅切换自己角色的伤害免疫；重生后保持开关状态，关闭后恢复正常扣血。"));
		InvulnerabilityLabel = WidgetTree->ConstructWidget<UTextBlock>();
		InvulnerabilityLabel->SetFont(Font);
		InvulnerabilityLabel->SetJustification(ETextJustify::Center);
		InvulnerabilityButton->AddChild(InvulnerabilityLabel);
		Content->AddChildToVerticalBox(InvulnerabilityButton)->SetPadding(FMargin(2, 0, 2, 6));
		OneHitKillButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("OneHitKillToggle"));
		OneHitKillButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ToggleOneHitKill);
		OneHitKillButton->SetToolTipText(NSLOCTEXT("SpaceDebug", "OneHitKillTooltip",
			"自己的攻击命中其他生物时一击必杀，包含首领。玩家和队友承伤不变；关闭后恢复正常攻击伤害。"));
		OneHitKillLabel = WidgetTree->ConstructWidget<UTextBlock>();
		OneHitKillLabel->SetFont(Font);
		OneHitKillLabel->SetJustification(ETextJustify::Center);
		OneHitKillButton->AddChild(OneHitKillLabel);
		Content->AddChildToVerticalBox(OneHitKillButton)->SetPadding(FMargin(2, 0, 2, 6));
		AddText(NSLOCTEXT("SpaceDebug", "Hint", "Hold Alt: cursor | Travel: host only"), 10);
		StatusText = AddText(FText::GetEmpty(), 10); StatusText->SetAutoWrapText(true);
	}
	DarkVisibilityButton->SetIsEnabled(View && View->CanUseDarkVisibility());
	const bool bDarkVisible = View && View->IsDarkVisibilityEnabled();
	DarkVisibilityLabel->SetText(bDarkVisible
		? NSLOCTEXT("SpaceDebug", "DarkVisibilityOn", "暗区可视：开（无光照）")
		: NSLOCTEXT("SpaceDebug", "DarkVisibilityOff", "暗区可视：关"));
	DarkVisibilityButton->SetBackgroundColor(bDarkVisible
		? FLinearColor(0.1f, 0.45f, 0.35f, 1.0f) : FLinearColor::White);
	InvulnerabilityButton->SetIsEnabled(bAvailable && PC);
	const bool bInvulnerable = PC && PC->IsDebugInvulnerable();
	InvulnerabilityLabel->SetText(bInvulnerable
		? NSLOCTEXT("SpaceDebug", "InvulnerabilityOn", "玩家无敌：开")
		: NSLOCTEXT("SpaceDebug", "InvulnerabilityOff", "玩家无敌：关"));
	InvulnerabilityButton->SetBackgroundColor(bInvulnerable
		? FLinearColor(0.1f, 0.45f, 0.35f, 1.0f) : FLinearColor::White);
	OneHitKillButton->SetIsEnabled(bAvailable && PC);
	const bool bOneHitKill = PC && PC->IsDebugOneHitKillEnabled();
	OneHitKillLabel->SetText(bOneHitKill
		? NSLOCTEXT("SpaceDebug", "OneHitKillOn", "一击必杀：开")
		: NSLOCTEXT("SpaceDebug", "OneHitKillOff", "一击必杀：关"));
	OneHitKillButton->SetBackgroundColor(bOneHitKill
		? FLinearColor(0.1f, 0.45f, 0.35f, 1.0f) : FLinearColor::White);
	for (const auto& Button : Buttons) Button->SetIsEnabled(Debug->CanUse(PC));
	for (int32 I = 0; I < DefaultLabels.Num(); ++I)
		DefaultLabels[I]->SetText(FText::FromString(Debug->GetDefaultPlanetId() == Destinations[I].PlanetId ? TEXT("Default *") : TEXT("Default")));
	StatusText->SetText(Debug->GetStatus());
}
