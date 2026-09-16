// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/FrontEnd/JTSFrontEndRootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SafeZone.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "InputCoreTypes.h"
#include "space/UI/FrontEnd/JTSCreditsWidget.h"
#include "space/UI/FrontEnd/JTSExpeditionSelectWidget.h"
#include "space/UI/FrontEnd/JTSJoinExpeditionWidget.h"
#include "space/UI/FrontEnd/JTSMainMenuWidget.h"
#include "space/UI/FrontEnd/JTSNewExpeditionWidget.h"
#include "space/UI/FrontEnd/JTSSettingsWidget.h"
#include "space/UI/JTSUITheme.h"

namespace
{
	UCanvasPanelSlot* AddCanvas(UCanvasPanel* Parent, UWidget* Child, const FAnchors& Anchors, const FMargin& Offsets, const FVector2D& Alignment = FVector2D::ZeroVector)
	{
		if (Parent == nullptr || Child == nullptr) return nullptr;
		UCanvasPanelSlot* const Slot = Parent->AddChildToCanvas(Child);
		Slot->SetAnchors(Anchors);
		Slot->SetOffsets(Offsets);
		Slot->SetAlignment(Alignment);
		return Slot;
	}

	void AddVertical(UVerticalBox* Parent, UWidget* Child, const FMargin& Padding = FMargin(0.0f, 3.0f))
	{
		if (Parent != nullptr && Child != nullptr)
		{
			if (UVerticalBoxSlot* const Slot = Parent->AddChildToVerticalBox(Child))
			{
				Slot->SetPadding(Padding);
				Slot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
	}
}

TSharedRef<SWidget> UJTSFrontEndRootWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSFrontEndRootWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UJTSFrontEndRootWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (MainMenu != nullptr)
	{
		MainMenu->OnPageRequested.RemoveAll(this);
		MainMenu->OnPageRequested.AddUObject(this, &UJTSFrontEndRootWidget::ShowPage);
	}
	ShowMainMenu();
}

void UJTSFrontEndRootWidget::NativeDestruct()
{
	if (MainMenu != nullptr)
	{
		MainMenu->OnPageRequested.RemoveAll(this);
	}
	Super::NativeDestruct();
}

FReply UJTSFrontEndRootWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape
		&& PageSwitcher != nullptr
		&& PageSwitcher->GetActiveWidget() == ExpeditionSelectPage
		&& ExpeditionSelectPage != nullptr
		&& ExpeditionSelectPage->IsDeleteConfirmationOpen())
	{
		ExpeditionSelectPage->CancelPendingDeleteConfirmation();
		return FReply::Handled();
	}

	if (InKeyEvent.GetKey() == EKeys::Escape && PageSwitcher != nullptr && PageSwitcher->GetActiveWidget() != MainMenu)
	{
		ShowMainMenu();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UJTSFrontEndRootWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || RootCanvas != nullptr)
	{
		return;
	}

	USafeZone* const SafeZone = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("SafeZone"));
	WidgetTree->RootWidget = SafeZone;
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FrontEndRootCanvas"));
	SafeZone->SetContent(RootCanvas);

	// Deliberately transparent enough for L_Entry's launch/space backdrop to remain part of the menu.
	UBorder* const Background = JTSUITheme::MakePanel(WidgetTree, TEXT("Backdrop"), FLinearColor(0.004f, 0.015f, 0.035f, 0.58f), FMargin(0.0f));
	AddCanvas(RootCanvas, Background, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f));
	UBorder* const Glow = JTSUITheme::MakePanel(WidgetTree, TEXT("OrbitGlow"), FLinearColor(0.05f, 0.32f, 0.52f, 0.10f), FMargin(0.0f));
	AddCanvas(RootCanvas, Glow, FAnchors(0.64f, 0.16f, 0.97f, 0.83f), FMargin(0.0f));

	UVerticalBox* const Brand = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Brand"));
	AddVertical(Brand, JTSUITheme::MakeText(WidgetTree, TEXT("BrandJump"), TEXT("JUMP"), 56.0f, JTSUITheme::Ink));
	AddVertical(Brand, JTSUITheme::MakeText(WidgetTree, TEXT("BrandToSpace"), TEXT("TO SPACE"), 56.0f, JTSUITheme::Cyan), FMargin(0.0f, -14.0f, 0.0f, 5.0f));
	AddVertical(Brand, JTSUITheme::MakeText(WidgetTree, TEXT("BrandSub"), TEXT("COOPERATIVE EXPEDITION"), 13.0f, JTSUITheme::Muted));
	AddCanvas(RootCanvas, Brand, FAnchors(0.065f, 0.055f), FMargin(0.0f, 0.0f, 400.0f, 150.0f));

	PageSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("FrontEndPageSwitcher"));
	AddCanvas(RootCanvas, PageSwitcher, FAnchors(0.065f, 0.265f, 0.585f, 0.93f), FMargin(0.0f));
	MainMenu = WidgetTree->ConstructWidget<UJTSMainMenuWidget>(ResolveMainMenuClass(), TEXT("MainMenuPage"));
	ExpeditionSelectPage = WidgetTree->ConstructWidget<UJTSExpeditionSelectWidget>(ResolveExpeditionSelectClass(), TEXT("ExpeditionSelectPage"));
	NewExpeditionPage = WidgetTree->ConstructWidget<UJTSNewExpeditionWidget>(ResolveNewExpeditionClass(), TEXT("NewExpeditionPage"));
	JoinPage = WidgetTree->ConstructWidget<UJTSJoinExpeditionWidget>(ResolveJoinClass(), TEXT("JoinExpeditionPage"));
	SettingsPage = WidgetTree->ConstructWidget<UJTSSettingsWidget>(ResolveSettingsClass(), TEXT("SettingsPage"));
	CreditsPage = WidgetTree->ConstructWidget<UJTSCreditsWidget>(ResolveCreditsClass(), TEXT("CreditsPage"));
	PageSwitcher->AddChild(MainMenu);
	PageSwitcher->AddChild(ExpeditionSelectPage);
	PageSwitcher->AddChild(NewExpeditionPage);
	PageSwitcher->AddChild(JoinPage);
	PageSwitcher->AddChild(SettingsPage);
	PageSwitcher->AddChild(CreditsPage);
}

TSubclassOf<UJTSMainMenuWidget> UJTSFrontEndRootWidget::ResolveMainMenuClass() const
{
	const TSubclassOf<UJTSMainMenuWidget> LoadedClass = MainMenuWidgetClass.LoadSynchronous();
	if (LoadedClass != nullptr)
	{
		return LoadedClass;
	}
	return UJTSMainMenuWidget::StaticClass();
}

TSubclassOf<UJTSExpeditionSelectWidget> UJTSFrontEndRootWidget::ResolveExpeditionSelectClass() const
{
	const TSubclassOf<UJTSExpeditionSelectWidget> LoadedClass = ExpeditionSelectWidgetClass.LoadSynchronous();
	if (LoadedClass != nullptr)
	{
		return LoadedClass;
	}
	return UJTSExpeditionSelectWidget::StaticClass();
}

TSubclassOf<UJTSNewExpeditionWidget> UJTSFrontEndRootWidget::ResolveNewExpeditionClass() const
{
	const TSubclassOf<UJTSNewExpeditionWidget> LoadedClass = NewExpeditionWidgetClass.LoadSynchronous();
	if (LoadedClass != nullptr)
	{
		return LoadedClass;
	}
	return UJTSNewExpeditionWidget::StaticClass();
}

TSubclassOf<UJTSJoinExpeditionWidget> UJTSFrontEndRootWidget::ResolveJoinClass() const
{
	const TSubclassOf<UJTSJoinExpeditionWidget> LoadedClass = JoinExpeditionWidgetClass.LoadSynchronous();
	if (LoadedClass != nullptr)
	{
		return LoadedClass;
	}
	return UJTSJoinExpeditionWidget::StaticClass();
}

TSubclassOf<UJTSSettingsWidget> UJTSFrontEndRootWidget::ResolveSettingsClass() const
{
	const TSubclassOf<UJTSSettingsWidget> LoadedClass = SettingsWidgetClass.LoadSynchronous();
	if (LoadedClass != nullptr)
	{
		return LoadedClass;
	}
	return UJTSSettingsWidget::StaticClass();
}

TSubclassOf<UJTSCreditsWidget> UJTSFrontEndRootWidget::ResolveCreditsClass() const
{
	const TSubclassOf<UJTSCreditsWidget> LoadedClass = CreditsWidgetClass.LoadSynchronous();
	if (LoadedClass != nullptr)
	{
		return LoadedClass;
	}
	return UJTSCreditsWidget::StaticClass();
}

void UJTSFrontEndRootWidget::ShowMainMenu()
{
	ShowPage(TEXT("Main"));
}

void UJTSFrontEndRootWidget::ShowExpeditionSelect()
{
	ShowPage(TEXT("ExpeditionSelect"));
	if (ExpeditionSelectPage != nullptr) ExpeditionSelectPage->RefreshSlots();
}

void UJTSFrontEndRootWidget::ShowNewExpedition(int32 SaveSlot)
{
	if (NewExpeditionPage != nullptr) NewExpeditionPage->SetSaveSlot(SaveSlot);
	ShowPage(TEXT("NewExpedition"));
}

void UJTSFrontEndRootWidget::ShowPage(FName PageName)
{
	if (PageSwitcher == nullptr) return;
	if (PageName == TEXT("ExpeditionSelect")) PageSwitcher->SetActiveWidget(ExpeditionSelectPage);
	else if (PageName == TEXT("NewExpedition")) PageSwitcher->SetActiveWidget(NewExpeditionPage);
	else if (PageName == TEXT("Join")) PageSwitcher->SetActiveWidget(JoinPage);
	else if (PageName == TEXT("Settings")) PageSwitcher->SetActiveWidget(SettingsPage);
	else if (PageName == TEXT("Credits")) PageSwitcher->SetActiveWidget(CreditsPage);
	else PageSwitcher->SetActiveWidget(MainMenu);
}
