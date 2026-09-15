// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/FrontEnd/JTSNewExpeditionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/Systems/JTSOnlineSessionSubsystem.h"
#include "space/UI/FrontEnd/JTSFrontEndRootWidget.h"
#include "space/UI/JTSUITheme.h"

namespace
{
	void AddRow(UVerticalBox* Parent, UWidget* Child, const FMargin& Padding = FMargin(0.0f, 6.0f))
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

	void AddHorizontal(UHorizontalBox* Parent, UWidget* Child, const FMargin& Padding = FMargin(4.0f), ESlateSizeRule::Type Rule = ESlateSizeRule::Automatic)
	{
		if (Parent != nullptr && Child != nullptr)
		{
			if (UHorizontalBoxSlot* const Slot = Parent->AddChildToHorizontalBox(Child))
			{
				Slot->SetPadding(Padding);
				Slot->SetSize(FSlateChildSize(Rule));
				Slot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}
}

TSharedRef<SWidget> UJTSNewExpeditionWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSNewExpeditionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UJTSNewExpeditionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>())
	{
		Online->OnSessionOperationFinished.RemoveDynamic(this, &UJTSNewExpeditionWidget::HandleOperationFinished);
		Online->OnSessionOperationFinished.AddDynamic(this, &UJTSNewExpeditionWidget::HandleOperationFinished);
		SuggestedJoinCode = Online->GenerateSuggestedJoinCode();
	}
	RefreshPresentation();
}

void UJTSNewExpeditionWidget::NativeDestruct()
{
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>() : nullptr)
	{
		Online->OnSessionOperationFinished.RemoveDynamic(this, &UJTSNewExpeditionWidget::HandleOperationFinished);
	}
	Super::NativeDestruct();
}

void UJTSNewExpeditionWidget::SetSaveSlot(int32 InSaveSlot)
{
	SaveSlot = FMath::Clamp(InSaveSlot, 1, 4);
	if (ExpeditionNameBox != nullptr)
	{
		ExpeditionNameBox->SetText(FText::FromString(FString::Printf(TEXT("Expedition %02d"), SaveSlot)));
	}
	RefreshPresentation();
}

void UJTSNewExpeditionWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	UBorder* const Panel = JTSUITheme::MakePanel(WidgetTree, TEXT("NewExpeditionPanel"), JTSUITheme::Panel, FMargin(28.0f));
	WidgetTree->RootWidget = Panel;
	UVerticalBox* const Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NewExpeditionLayout"));
	Panel->SetContent(Layout);
	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("Title"), TEXT("NEW EXPEDITION"), 30.0f, JTSUITheme::Ink));
	SaveSlotText = JTSUITheme::MakeText(WidgetTree, TEXT("SaveSlot"), TEXT("SAVE SLOT 01"), 14.0f, JTSUITheme::Cyan);
	AddRow(Layout, SaveSlotText, FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("NameLabel"), TEXT("EXPEDITION NAME"), 14.0f, JTSUITheme::Muted));
	ExpeditionNameBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("ExpeditionName"));
	ExpeditionNameBox->SetText(FText::FromString(TEXT("Expedition 01")));
	ExpeditionNameBox->SetHintText(FText::FromString(TEXT("Name your expedition")));
	AddRow(Layout, ExpeditionNameBox, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("VisibilityLabel"), TEXT("LOBBY VISIBILITY"), 14.0f, JTSUITheme::Muted));
	UHorizontalBox* const VisibilityRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("VisibilityRow"));
	UButton* const PublicButton = JTSUITheme::MakeButton(WidgetTree, TEXT("Public"), TEXT("PUBLIC"), JTSUITheme::EButtonTone::Primary, 14.0f);
	UButton* const PrivateButton = JTSUITheme::MakeButton(WidgetTree, TEXT("Private"), TEXT("PRIVATE"), JTSUITheme::EButtonTone::Secondary, 14.0f);
	PublicButton->OnClicked.AddDynamic(this, &UJTSNewExpeditionWidget::SelectPublic);
	PrivateButton->OnClicked.AddDynamic(this, &UJTSNewExpeditionWidget::SelectPrivate);
	AddHorizontal(VisibilityRow, PublicButton, FMargin(0.0f, 3.0f, 4.0f, 3.0f), ESlateSizeRule::Fill);
	AddHorizontal(VisibilityRow, PrivateButton, FMargin(4.0f, 3.0f, 0.0f, 3.0f), ESlateSizeRule::Fill);
	AddRow(Layout, VisibilityRow);
	VisibilityText = JTSUITheme::MakeText(WidgetTree, TEXT("VisibilityDetail"), TEXT("PUBLIC — listed in expedition search."), 12.0f, JTSUITheme::Muted);
	AddRow(Layout, VisibilityText, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("MaxPlayersLabel"), TEXT("MAX PLAYERS"), 14.0f, JTSUITheme::Muted));
	UHorizontalBox* const PlayersRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PlayersRow"));
	UButton* const Minus = JTSUITheme::MakeButton(WidgetTree, TEXT("Minus"), TEXT("−"), JTSUITheme::EButtonTone::Secondary, 22.0f);
	Minus->OnClicked.AddDynamic(this, &UJTSNewExpeditionWidget::DecreasePlayers);
	MaximumPlayersText = JTSUITheme::MakeText(WidgetTree, TEXT("MaximumPlayers"), TEXT("4"), 26.0f, JTSUITheme::Ink, ETextJustify::Center);
	UButton* const Plus = JTSUITheme::MakeButton(WidgetTree, TEXT("Plus"), TEXT("+"), JTSUITheme::EButtonTone::Secondary, 22.0f);
	Plus->OnClicked.AddDynamic(this, &UJTSNewExpeditionWidget::IncreasePlayers);
	AddHorizontal(PlayersRow, Minus);
	AddHorizontal(PlayersRow, MaximumPlayersText, FMargin(14.0f), ESlateSizeRule::Fill);
	AddHorizontal(PlayersRow, Plus);
	AddRow(Layout, PlayersRow, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("PasswordLabel"), TEXT("PASSWORD"), 14.0f, JTSUITheme::Muted));
	PasswordBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Password"));
	PasswordBox->SetHintText(FText::FromString(TEXT("Leave blank for none")));
	PasswordBox->SetIsPassword(true);
	AddRow(Layout, PasswordBox, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("JoinCodeLabel"), TEXT("JOIN CODE"), 14.0f, JTSUITheme::Muted));
	SuggestedJoinCodeText = JTSUITheme::MakeText(WidgetTree, TEXT("SuggestedJoinCode"), TEXT("GENERATED AUTOMATICALLY"), 17.0f, JTSUITheme::Cyan);
	AddRow(Layout, SuggestedJoinCodeText);
	AdvancedJoinCodeButton = JTSUITheme::MakeButton(WidgetTree, TEXT("AdvancedCode"), TEXT("ADVANCED: CUSTOM JOIN CODE"), JTSUITheme::EButtonTone::Secondary, 13.0f);
	AdvancedJoinCodeButton->OnClicked.AddDynamic(this, &UJTSNewExpeditionWidget::ToggleAdvancedJoinCode);
	AddRow(Layout, AdvancedJoinCodeButton);
	CustomJoinCodeBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("CustomJoinCode"));
	CustomJoinCodeBox->SetHintText(FText::FromString(TEXT("Optional custom code")));
	CustomJoinCodeBox->SetVisibility(ESlateVisibility::Collapsed);
	AddRow(Layout, CustomJoinCodeBox);

	StatusText = JTSUITheme::MakeText(WidgetTree, TEXT("Status"), TEXT(""), 13.0f, JTSUITheme::Muted);
	AddRow(Layout, StatusText, FMargin(0.0f, 10.0f, 0.0f, 0.0f));
	UButton* const CreateButton = JTSUITheme::MakeButton(WidgetTree, TEXT("Create"), TEXT("CREATE EXPEDITION"), JTSUITheme::EButtonTone::Primary);
	CreateButton->OnClicked.AddDynamic(this, &UJTSNewExpeditionWidget::CreateExpedition);
	AddRow(Layout, CreateButton, FMargin(0.0f, 10.0f, 0.0f, 0.0f));
	UButton* const BackButton = JTSUITheme::MakeButton(WidgetTree, TEXT("Back"), TEXT("BACK TO SAVES"), JTSUITheme::EButtonTone::Secondary);
	BackButton->OnClicked.AddDynamic(this, &UJTSNewExpeditionWidget::Back);
	AddRow(Layout, BackButton);
}

void UJTSNewExpeditionWidget::RefreshPresentation()
{
	if (SaveSlotText == nullptr)
	{
		return;
	}
	SaveSlotText->SetText(FText::FromString(FString::Printf(TEXT("SAVE SLOT %02d"), SaveSlot)));
	MaximumPlayersText->SetText(FText::AsNumber(MaximumPlayers));
	VisibilityText->SetText(FText::FromString(LobbyVisibility == EJTSLobbyVisibility::Public
		? TEXT("PUBLIC — listed in expedition search.")
		: TEXT("PRIVATE — joinable by code, not shown in public browse.")));
	SuggestedJoinCodeText->SetText(FText::FromString(FString::Printf(TEXT("%s  (AUTOMATIC)"), SuggestedJoinCode.IsEmpty() ? TEXT("GENERATING") : *SuggestedJoinCode)));
}

void UJTSNewExpeditionWidget::DecreasePlayers() { MaximumPlayers = FMath::Clamp(MaximumPlayers - 1, 1, 4); RefreshPresentation(); }
void UJTSNewExpeditionWidget::IncreasePlayers() { MaximumPlayers = FMath::Clamp(MaximumPlayers + 1, 1, 4); RefreshPresentation(); }
void UJTSNewExpeditionWidget::SelectPublic() { LobbyVisibility = EJTSLobbyVisibility::Public; RefreshPresentation(); }
void UJTSNewExpeditionWidget::SelectPrivate() { LobbyVisibility = EJTSLobbyVisibility::Private; RefreshPresentation(); }

void UJTSNewExpeditionWidget::ToggleAdvancedJoinCode()
{
	const bool bWasVisible = CustomJoinCodeBox->GetVisibility() == ESlateVisibility::Visible;
	CustomJoinCodeBox->SetVisibility(bWasVisible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	AdvancedJoinCodeButton->SetContent(JTSUITheme::MakeText(WidgetTree, TEXT("AdvancedCodeLabelRefresh"), bWasVisible ? TEXT("ADVANCED: CUSTOM JOIN CODE") : TEXT("USE AUTOMATIC JOIN CODE"), 13.0f, JTSUITheme::Ink, ETextJustify::Center));
}

void UJTSNewExpeditionWidget::CreateExpedition()
{
	UJTSExpeditionSubsystem* const Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>();
	UJTSOnlineSessionSubsystem* const Online = GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>();
	if (Expedition == nullptr || Online == nullptr)
	{
		StatusText->SetText(FText::FromString(TEXT("Required expedition services are unavailable.")));
		return;
	}

	const FString ExpeditionName = ExpeditionNameBox->GetText().ToString().TrimStartAndEnd();
	if (!Expedition->BeginNewExpeditionInSlot(SaveSlot, ExpeditionName))
	{
		StatusText->SetText(FText::FromString(TEXT("Could not prepare that save slot.")));
		return;
	}
	// A selected slot becomes durable before session creation, so backing out of the first lobby
	// never makes the expedition disappear from Expedition Select.
	if (!Expedition->SaveNow())
	{
		StatusText->SetText(FText::FromString(TEXT("Could not write the new expedition save slot.")));
		return;
	}
	const FString CustomCode = CustomJoinCodeBox->GetVisibility() == ESlateVisibility::Visible ? CustomJoinCodeBox->GetText().ToString() : FString();
	Online->CreateExpedition(CustomCode.IsEmpty() ? SuggestedJoinCode : CustomCode, PasswordBox->GetText().ToString(), MaximumPlayers, LobbyVisibility);
}

void UJTSNewExpeditionWidget::Back()
{
	if (UJTSFrontEndRootWidget* const Root = GetTypedOuter<UJTSFrontEndRootWidget>()) Root->ShowExpeditionSelect();
}

void UJTSNewExpeditionWidget::HandleOperationFinished(bool bSucceeded, const FString& Message)
{
	if (StatusText != nullptr) StatusText->SetText(FText::FromString(Message));
}
