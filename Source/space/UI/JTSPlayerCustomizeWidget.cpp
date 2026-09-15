// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSPlayerCustomizeWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/UI/JTSPreLaunchLobbyWidget.h"
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

	void AddColourButton(UHorizontalBox* Parent, UButton* Button)
	{
		if (Parent != nullptr && Button != nullptr)
		{
			if (UHorizontalBoxSlot* const Slot = Parent->AddChildToHorizontalBox(Button))
			{
				Slot->SetPadding(FMargin(4.0f));
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
	}
}

TSharedRef<SWidget> UJTSPlayerCustomizeWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSPlayerCustomizeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UJTSPlayerCustomizeWidget::SetLobbyOwner(UJTSPreLaunchLobbyWidget* InOwner)
{
	LobbyOwner = InOwner;
	if (const AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		if (const AJTSPlayerState* const State = Controller->GetPlayerState<AJTSPlayerState>())
		{
			CurrentColorText->SetText(FText::FromString(FString::Printf(TEXT("CURRENT COLOUR: %s"), *StaticEnum<EJTSAvatarColor>()->GetNameStringByValue(static_cast<int64>(State->GetAvatarColor())).ToUpper())));
		}
	}
}

void UJTSPlayerCustomizeWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	UBorder* const Panel = JTSUITheme::MakePanel(WidgetTree, TEXT("CustomizePanel"), JTSUITheme::Panel, FMargin(26.0f));
	WidgetTree->RootWidget = Panel;
	UVerticalBox* const Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CustomizeLayout"));
	Panel->SetContent(Layout);
	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("Title"), TEXT("CUSTOMIZE PLAYER"), 27.0f, JTSUITheme::Ink, ETextJustify::Center), FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("ColourLabel"), TEXT("COLOUR"), 14.0f, JTSUITheme::Muted));
	CurrentColorText = JTSUITheme::MakeText(WidgetTree, TEXT("CurrentColour"), TEXT("CURRENT COLOUR"), 13.0f, JTSUITheme::Ink);
	AddRow(Layout, CurrentColorText, FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	UHorizontalBox* const Colours = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ColourChoices"));
	UButton* const Blue = JTSUITheme::MakeButton(WidgetTree, TEXT("Blue"), TEXT("BLUE"), JTSUITheme::EButtonTone::Secondary, 13.0f);
	UButton* const Orange = JTSUITheme::MakeButton(WidgetTree, TEXT("Orange"), TEXT("ORANGE"), JTSUITheme::EButtonTone::Secondary, 13.0f);
	UButton* const Green = JTSUITheme::MakeButton(WidgetTree, TEXT("Green"), TEXT("GREEN"), JTSUITheme::EButtonTone::Secondary, 13.0f);
	UButton* const Purple = JTSUITheme::MakeButton(WidgetTree, TEXT("Purple"), TEXT("PURPLE"), JTSUITheme::EButtonTone::Secondary, 13.0f);
	Blue->SetBackgroundColor(FLinearColor(0.10f, 0.45f, 1.0f, 1.0f));
	Orange->SetBackgroundColor(FLinearColor(1.0f, 0.34f, 0.06f, 1.0f));
	Green->SetBackgroundColor(FLinearColor(0.18f, 0.85f, 0.28f, 1.0f));
	Purple->SetBackgroundColor(FLinearColor(0.58f, 0.25f, 0.90f, 1.0f));
	Blue->OnClicked.AddDynamic(this, &UJTSPlayerCustomizeWidget::SelectBlue);
	Orange->OnClicked.AddDynamic(this, &UJTSPlayerCustomizeWidget::SelectOrange);
	Green->OnClicked.AddDynamic(this, &UJTSPlayerCustomizeWidget::SelectGreen);
	Purple->OnClicked.AddDynamic(this, &UJTSPlayerCustomizeWidget::SelectPurple);
	AddColourButton(Colours, Blue); AddColourButton(Colours, Orange); AddColourButton(Colours, Green); AddColourButton(Colours, Purple);
	AddRow(Layout, Colours, FMargin(0.0f, 0.0f, 0.0f, 18.0f));

	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("FutureLabel"), TEXT("HEAD / HAIR     FACE     OUTFIT     ACCESSORY"), 14.0f, JTSUITheme::Muted));
	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("FutureDetail"), TEXT("Additional avatar parts will unlock here as their replicated equipment data is added."), 13.0f, JTSUITheme::Muted));
	UButton* const CloseButton = JTSUITheme::MakeButton(WidgetTree, TEXT("CloseButton"), TEXT("DONE"), JTSUITheme::EButtonTone::Primary);
	CloseButton->OnClicked.AddDynamic(this, &UJTSPlayerCustomizeWidget::Close);
	AddRow(Layout, CloseButton, FMargin(0.0f, 18.0f, 0.0f, 0.0f));
}

void UJTSPlayerCustomizeWidget::SelectColor(EJTSAvatarColor NewColor)
{
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		Controller->RequestAvatarColor(NewColor);
	}
}

void UJTSPlayerCustomizeWidget::SelectBlue() { SelectColor(EJTSAvatarColor::Blue); }
void UJTSPlayerCustomizeWidget::SelectOrange() { SelectColor(EJTSAvatarColor::Orange); }
void UJTSPlayerCustomizeWidget::SelectGreen() { SelectColor(EJTSAvatarColor::Green); }
void UJTSPlayerCustomizeWidget::SelectPurple() { SelectColor(EJTSAvatarColor::Purple); }

void UJTSPlayerCustomizeWidget::Close()
{
	if (UJTSPreLaunchLobbyWidget* const Owner = LobbyOwner.Get())
	{
		Owner->ClosePlayerCustomization();
	}
}
