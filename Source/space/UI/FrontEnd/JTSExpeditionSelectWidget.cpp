// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/FrontEnd/JTSExpeditionSelectWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/Systems/JTSOnlineSessionSubsystem.h"
#include "space/UI/FrontEnd/JTSFrontEndRootWidget.h"
#include "space/UI/JTSUITheme.h"

namespace
{
	void AddRow(UVerticalBox* Parent, UWidget* Child, const FMargin& Padding = FMargin(0.0f, 7.0f))
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

TSharedRef<SWidget> UJTSExpeditionSelectWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSExpeditionSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UJTSExpeditionSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>())
	{
		Online->OnSessionOperationFinished.RemoveDynamic(this, &UJTSExpeditionSelectWidget::HandleOperationFinished);
		Online->OnSessionOperationFinished.AddDynamic(this, &UJTSExpeditionSelectWidget::HandleOperationFinished);
	}
	RefreshSlots();
}

void UJTSExpeditionSelectWidget::NativeDestruct()
{
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>() : nullptr)
	{
		Online->OnSessionOperationFinished.RemoveDynamic(this, &UJTSExpeditionSelectWidget::HandleOperationFinished);
	}
	Super::NativeDestruct();
}

void UJTSExpeditionSelectWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	UBorder* const Panel = JTSUITheme::MakePanel(WidgetTree, TEXT("ExpeditionSelectPanel"), JTSUITheme::Panel, FMargin(28.0f));
	WidgetTree->RootWidget = Panel;
	UVerticalBox* const Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ExpeditionSelectLayout"));
	Panel->SetContent(Layout);
	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("Title"), TEXT("EXPEDITION SELECT"), 30.0f, JTSUITheme::Ink));
	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("Description"), TEXT("Choose a host-owned expedition save. Each run keeps its own crew snapshot and progress."), 14.0f, JTSUITheme::Muted), FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	UScrollBox* const Slots = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("SaveSlots"));
	Slots->SetOrientation(EOrientation::Orient_Vertical);
	for (int32 SlotIndex = 1; SlotIndex <= 4; ++SlotIndex)
	{
		UButton* const Button = JTSUITheme::MakeButton(WidgetTree, *FString::Printf(TEXT("Slot%d"), SlotIndex), TEXT("LOADING"), JTSUITheme::EButtonTone::Secondary, 16.0f);
		UTextBlock* const Text = Cast<UTextBlock>(Button->GetContent());
		Text->SetJustification(ETextJustify::Left);
		Text->SetAutoWrapText(true);
		if (UScrollBoxSlot* const ScrollSlot = Cast<UScrollBoxSlot>(Slots->AddChild(Button))) ScrollSlot->SetPadding(FMargin(0.0f, 5.0f));
		SlotButtons.Add(Button);
		SlotTexts.Add(Text);
		switch (SlotIndex)
		{
		case 1: Button->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::ChooseSlotOne); break;
		case 2: Button->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::ChooseSlotTwo); break;
		case 3: Button->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::ChooseSlotThree); break;
		default: Button->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::ChooseSlotFour); break;
		}
	}
	AddRow(Layout, Slots, FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	StatusText = JTSUITheme::MakeText(WidgetTree, TEXT("Status"), TEXT(""), 13.0f, JTSUITheme::Muted);
	AddRow(Layout, StatusText);
	UButton* const BackButton = JTSUITheme::MakeButton(WidgetTree, TEXT("Back"), TEXT("BACK"), JTSUITheme::EButtonTone::Secondary);
	BackButton->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::Back);
	AddRow(Layout, BackButton, FMargin(0.0f, 12.0f, 0.0f, 0.0f));
}

void UJTSExpeditionSelectWidget::RefreshSlots()
{
	const UJTSExpeditionSubsystem* const Expedition = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>() : nullptr;
	if (Expedition == nullptr)
	{
		StatusText->SetText(FText::FromString(TEXT("Expedition save subsystem unavailable.")));
		return;
	}

	const TArray<FJTSExpeditionSaveSummary> Summaries = Expedition->GetSaveSlotSummaries();
	for (int32 Index = 0; Index < SlotTexts.Num(); ++Index)
	{
		const FJTSExpeditionSaveSummary* const Summary = Summaries.IsValidIndex(Index) ? &Summaries[Index] : nullptr;
		if (Summary != nullptr && Summary->bOccupied)
		{
			SlotTexts[Index]->SetText(FText::FromString(FString::Printf(
				TEXT("SLOT %02d   %s\nCURRENT PLANET: %s   •   %s\nLAST PLAYED: %s   •   PLAY TIME: %s\n[ CONTINUE EXPEDITION ]"),
				Summary->SaveSlot,
				*Summary->DisplayName,
				*Summary->CurrentPlanet,
				*Summary->CurrentCheckpoint,
				*JTSUITheme::FormatUtcTicks(Summary->LastPlayedUtcTicks),
				*JTSUITheme::FormatPlaytime(Summary->PlaytimeSeconds))));
		}
		else
		{
			SlotTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("SLOT %02d   EMPTY\n[ NEW EXPEDITION ]"), Index + 1)));
		}
	}
	StatusText->SetText(FText::FromString(TEXT("Select an empty slot to configure a new hosted expedition.")));
}

void UJTSExpeditionSelectWidget::ChooseSlot(int32 SlotIndex)
{
	UJTSExpeditionSubsystem* const Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>();
	if (Expedition == nullptr)
	{
		return;
	}
	if (Expedition->HasSaveInSlot(SlotIndex))
	{
		if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>())
		{
			Online->ContinueExpedition(SlotIndex);
		}
		return;
	}
	if (UJTSFrontEndRootWidget* const Root = GetTypedOuter<UJTSFrontEndRootWidget>())
	{
		Root->ShowNewExpedition(SlotIndex);
	}
}

void UJTSExpeditionSelectWidget::ChooseSlotOne() { ChooseSlot(1); }
void UJTSExpeditionSelectWidget::ChooseSlotTwo() { ChooseSlot(2); }
void UJTSExpeditionSelectWidget::ChooseSlotThree() { ChooseSlot(3); }
void UJTSExpeditionSelectWidget::ChooseSlotFour() { ChooseSlot(4); }
void UJTSExpeditionSelectWidget::Back() { if (UJTSFrontEndRootWidget* const Root = GetTypedOuter<UJTSFrontEndRootWidget>()) Root->ShowMainMenu(); }
void UJTSExpeditionSelectWidget::HandleOperationFinished(bool bSucceeded, const FString& Message) { if (StatusText != nullptr) StatusText->SetText(FText::FromString(Message)); }
