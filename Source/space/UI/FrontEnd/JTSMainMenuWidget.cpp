// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/FrontEnd/JTSMainMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/KismetSystemLibrary.h"
#include "space/Systems/JTSOnlineSessionSubsystem.h"
#include "space/UI/JTSUITheme.h"

namespace
{
	void AddMenuRow(UVerticalBox* Layout, UWidget* Child, const FMargin& Padding = FMargin(0.0f, 6.0f))
	{
		if (Layout != nullptr && Child != nullptr)
		{
			if (UVerticalBoxSlot* const Slot = Layout->AddChildToVerticalBox(Child))
			{
				Slot->SetPadding(Padding);
				Slot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
	}
}

TSharedRef<SWidget> UJTSMainMenuWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UJTSMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>() : nullptr)
	{
		Online->OnSessionOperationFinished.RemoveDynamic(this, &UJTSMainMenuWidget::HandleOperationFinished);
		Online->OnSessionOperationFinished.AddDynamic(this, &UJTSMainMenuWidget::HandleOperationFinished);
		StatusText->SetText(FText::FromString(Online->GetLastError().IsEmpty() ? Online->GetProviderStatus() : Online->GetLastError()));
	}
}

void UJTSMainMenuWidget::NativeDestruct()
{
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>() : nullptr)
	{
		Online->OnSessionOperationFinished.RemoveDynamic(this, &UJTSMainMenuWidget::HandleOperationFinished);
	}
	Super::NativeDestruct();
}

void UJTSMainMenuWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	UBorder* const MenuPanel = JTSUITheme::MakePanel(WidgetTree, TEXT("MainMenuPanel"), FLinearColor(0.025f, 0.065f, 0.115f, 0.76f), FMargin(28.0f));
	WidgetTree->RootWidget = MenuPanel;
	UVerticalBox* const Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainMenuLayout"));
	MenuPanel->SetContent(Layout);
	AddMenuRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("MainMenuPrompt"), TEXT("ASSEMBLE A CREW. JUMP FARTHER."), 16.0f, JTSUITheme::Cyan), FMargin(0.0f, 0.0f, 0.0f, 18.0f));

	PlayButton = JTSUITheme::MakeButton(WidgetTree, TEXT("PlayButton"), TEXT("PLAY"), JTSUITheme::EButtonTone::Primary, 25.0f);
	PlayButton->OnClicked.AddDynamic(this, &UJTSMainMenuWidget::RequestPlay);
	AddMenuRow(Layout, PlayButton);
	JoinButton = JTSUITheme::MakeButton(WidgetTree, TEXT("JoinButton"), TEXT("JOIN EXPEDITION"), JTSUITheme::EButtonTone::Secondary, 20.0f);
	JoinButton->OnClicked.AddDynamic(this, &UJTSMainMenuWidget::RequestJoin);
	AddMenuRow(Layout, JoinButton);
	SettingsButton = JTSUITheme::MakeButton(WidgetTree, TEXT("SettingsButton"), TEXT("SETTINGS"), JTSUITheme::EButtonTone::Secondary, 20.0f);
	SettingsButton->OnClicked.AddDynamic(this, &UJTSMainMenuWidget::RequestSettings);
	AddMenuRow(Layout, SettingsButton);
	CreditsButton = JTSUITheme::MakeButton(WidgetTree, TEXT("CreditsButton"), TEXT("CREDITS"), JTSUITheme::EButtonTone::Secondary, 20.0f);
	CreditsButton->OnClicked.AddDynamic(this, &UJTSMainMenuWidget::RequestCredits);
	AddMenuRow(Layout, CreditsButton);
	QuitButton = JTSUITheme::MakeButton(WidgetTree, TEXT("QuitButton"), TEXT("QUIT"), JTSUITheme::EButtonTone::Secondary, 20.0f);
	QuitButton->OnClicked.AddDynamic(this, &UJTSMainMenuWidget::RequestQuit);
	AddMenuRow(Layout, QuitButton);
	StatusText = JTSUITheme::MakeText(WidgetTree, TEXT("StatusText"), TEXT(""), 12.0f, JTSUITheme::Muted);
	AddMenuRow(Layout, StatusText, FMargin(0.0f, 18.0f, 0.0f, 0.0f));
}

void UJTSMainMenuWidget::RequestPlay() { OnPageRequested.Broadcast(TEXT("ExpeditionSelect")); }
void UJTSMainMenuWidget::RequestJoin() { OnPageRequested.Broadcast(TEXT("Join")); }
void UJTSMainMenuWidget::RequestSettings() { OnPageRequested.Broadcast(TEXT("Settings")); }
void UJTSMainMenuWidget::RequestCredits() { OnPageRequested.Broadcast(TEXT("Credits")); }

void UJTSMainMenuWidget::RequestQuit()
{
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>() : nullptr)
	{
		Online->RequestApplicationQuit();
		return;
	}

	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UJTSMainMenuWidget::HandleOperationFinished(bool bSucceeded, const FString& Message)
{
	if (StatusText != nullptr) StatusText->SetText(FText::FromString(Message));
}
