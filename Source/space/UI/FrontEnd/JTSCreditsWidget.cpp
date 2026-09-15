// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/FrontEnd/JTSCreditsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "space/UI/FrontEnd/JTSFrontEndRootWidget.h"
#include "space/UI/JTSUITheme.h"

TSharedRef<SWidget> UJTSCreditsWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSCreditsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UJTSCreditsWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr) return;
	UBorder* const Panel = JTSUITheme::MakePanel(WidgetTree, TEXT("CreditsPanel"), JTSUITheme::Panel, FMargin(28.0f));
	WidgetTree->RootWidget = Panel;
	UVerticalBox* const Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CreditsLayout"));
	Panel->SetContent(Layout);
	auto Add = [Layout](UWidget* Child, const FMargin& Margin = FMargin(0.0f, 8.0f))
	{
		if (UVerticalBoxSlot* const VerticalSlot = Layout->AddChildToVerticalBox(Child)) { VerticalSlot->SetPadding(Margin); VerticalSlot->SetHorizontalAlignment(HAlign_Fill); }
	};
	Add(JTSUITheme::MakeText(WidgetTree, TEXT("Title"), TEXT("CREDITS"), 30.0f, JTSUITheme::Ink));
	Add(JTSUITheme::MakeText(WidgetTree, TEXT("Copy"), TEXT("JUMP TO SPACE\nA cooperative expedition prototype.\n\nBuilt with Unreal Engine and the project art/content credited in their respective source packages."), 16.0f, JTSUITheme::Muted));
	UButton* const BackButton = JTSUITheme::MakeButton(WidgetTree, TEXT("Back"), TEXT("BACK"), JTSUITheme::EButtonTone::Secondary);
	BackButton->OnClicked.AddDynamic(this, &UJTSCreditsWidget::Back);
	Add(BackButton, FMargin(0.0f, 18.0f, 0.0f, 0.0f));
}

void UJTSCreditsWidget::Back()
{
	if (UJTSFrontEndRootWidget* const Root = GetTypedOuter<UJTSFrontEndRootWidget>()) Root->ShowMainMenu();
}
