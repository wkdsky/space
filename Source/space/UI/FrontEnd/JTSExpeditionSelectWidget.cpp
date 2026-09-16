// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/FrontEnd/JTSExpeditionSelectWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
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
	SetIsFocusable(true);
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>() : nullptr)
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

bool UJTSExpeditionSelectWidget::IsDeleteConfirmationOpen() const
{
	return PendingDeleteSlot != INDEX_NONE;
}

void UJTSExpeditionSelectWidget::CancelPendingDeleteConfirmation()
{
	HideDeleteConfirmation();
}

void UJTSExpeditionSelectWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || RootOverlay != nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ExpeditionSelectRoot"));
	WidgetTree->RootWidget = RootOverlay;
	UBorder* const Panel = JTSUITheme::MakePanel(WidgetTree, TEXT("ExpeditionSelectPanel"), JTSUITheme::Panel, FMargin(28.0f));
	if (UOverlaySlot* const PanelSlot = RootOverlay->AddChildToOverlay(Panel))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Fill);
		PanelSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UVerticalBox* const Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ExpeditionSelectLayout"));
	Panel->SetContent(Layout);
	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("Title"), TEXT("EXPEDITION SELECT"), 30.0f, JTSUITheme::Ink));
	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("Description"), TEXT("Choose a host-owned expedition save. Each run keeps its own crew snapshot and progress. Use X to manage an occupied slot."), 14.0f, JTSUITheme::Muted), FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	UScrollBox* const Slots = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("SaveSlots"));
	Slots->SetOrientation(EOrientation::Orient_Vertical);
	for (int32 SlotIndex = 1; SlotIndex <= 4; ++SlotIndex)
	{
		UHorizontalBox* const SlotRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("SlotRow%d"), SlotIndex));
		UButton* const Button = JTSUITheme::MakeButton(WidgetTree, *FString::Printf(TEXT("Slot%d"), SlotIndex), TEXT("LOADING"), JTSUITheme::EButtonTone::Secondary, 16.0f);
		UTextBlock* const Text = Cast<UTextBlock>(Button->GetContent());
		Text->SetJustification(ETextJustify::Left);
		Text->SetAutoWrapText(true);
		AddHorizontal(SlotRow, Button, FMargin(0.0f, 0.0f, 5.0f, 0.0f), ESlateSizeRule::Fill);

		USizeBox* const DeleteContainer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("DeleteSlotContainer%d"), SlotIndex));
		DeleteContainer->SetWidthOverride(42.0f);
		UButton* const DeleteButton = JTSUITheme::MakeButton(WidgetTree, *FString::Printf(TEXT("DeleteSlot%d"), SlotIndex), TEXT("\u00D7"), JTSUITheme::EButtonTone::Danger, 19.0f);
		DeleteButton->SetToolTipText(FText::FromString(TEXT("Delete this host save")));
		DeleteContainer->SetContent(DeleteButton);
		AddHorizontal(SlotRow, DeleteContainer, FMargin(0.0f));
		if (UScrollBoxSlot* const ScrollSlot = Cast<UScrollBoxSlot>(Slots->AddChild(SlotRow))) ScrollSlot->SetPadding(FMargin(0.0f, 5.0f));
		SlotButtons.Add(Button);
		SlotTexts.Add(Text);
		DeleteSlotButtons.Add(DeleteButton);
		DeleteSlotContainers.Add(DeleteContainer);
		switch (SlotIndex)
		{
		case 1: Button->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::ChooseSlotOne); break;
		case 2: Button->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::ChooseSlotTwo); break;
		case 3: Button->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::ChooseSlotThree); break;
		default: Button->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::ChooseSlotFour); break;
		}
		switch (SlotIndex)
		{
		case 1: DeleteButton->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::RequestDeleteSlotOne); break;
		case 2: DeleteButton->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::RequestDeleteSlotTwo); break;
		case 3: DeleteButton->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::RequestDeleteSlotThree); break;
		default: DeleteButton->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::RequestDeleteSlotFour); break;
		}
	}
	AddRow(Layout, Slots, FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	StatusText = JTSUITheme::MakeText(WidgetTree, TEXT("Status"), TEXT(""), 13.0f, JTSUITheme::Muted);
	AddRow(Layout, StatusText);
	UButton* const BackButton = JTSUITheme::MakeButton(WidgetTree, TEXT("Back"), TEXT("BACK"), JTSUITheme::EButtonTone::Secondary);
	BackButton->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::Back);
	AddRow(Layout, BackButton, FMargin(0.0f, 12.0f, 0.0f, 0.0f));

	// This full-screen button prevents clicks from leaking through the confirmation and
	// provides the familiar click-outside-to-cancel affordance.
	DeleteConfirmationBackdrop = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DeleteConfirmationBackdrop"));
	DeleteConfirmationBackdrop->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.76f));
	DeleteConfirmationBackdrop->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::CancelDelete);
	if (UOverlaySlot* const BackdropSlot = RootOverlay->AddChildToOverlay(DeleteConfirmationBackdrop))
	{
		BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
		BackdropSlot->SetVerticalAlignment(VAlign_Fill);
	}

	USizeBox* const ConfirmationSizer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DeleteConfirmationSizer"));
	ConfirmationSizer->SetWidthOverride(470.0f);
	DeleteConfirmationPanel = JTSUITheme::MakePanel(WidgetTree, TEXT("DeleteConfirmationPanel"), JTSUITheme::PanelRaised, FMargin(26.0f));
	ConfirmationSizer->SetContent(DeleteConfirmationPanel);
	UVerticalBox* const ConfirmationLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DeleteConfirmationLayout"));
	DeleteConfirmationPanel->SetContent(ConfirmationLayout);
	AddRow(ConfirmationLayout, JTSUITheme::MakeText(WidgetTree, TEXT("DeleteConfirmationTitle"), TEXT("DELETE EXPEDITION SAVE?"), 23.0f, JTSUITheme::Danger));
	DeleteConfirmationText = JTSUITheme::MakeText(WidgetTree, TEXT("DeleteConfirmationText"), TEXT(""), 14.0f, JTSUITheme::Ink);
	AddRow(ConfirmationLayout, DeleteConfirmationText, FMargin(0.0f, 8.0f, 0.0f, 18.0f));
	UHorizontalBox* const ConfirmationActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DeleteConfirmationActions"));
	UButton* const CancelButton = JTSUITheme::MakeButton(WidgetTree, TEXT("CancelDelete"), TEXT("CANCEL"), JTSUITheme::EButtonTone::Secondary, 15.0f);
	UButton* const ConfirmButton = JTSUITheme::MakeButton(WidgetTree, TEXT("ConfirmDelete"), TEXT("DELETE SAVE"), JTSUITheme::EButtonTone::Danger, 15.0f);
	CancelButton->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::CancelDelete);
	ConfirmButton->OnClicked.AddDynamic(this, &UJTSExpeditionSelectWidget::ConfirmDelete);
	AddHorizontal(ConfirmationActions, CancelButton, FMargin(0.0f, 0.0f, 4.0f, 0.0f), ESlateSizeRule::Fill);
	AddHorizontal(ConfirmationActions, ConfirmButton, FMargin(4.0f, 0.0f, 0.0f, 0.0f), ESlateSizeRule::Fill);
	AddRow(ConfirmationLayout, ConfirmationActions);
	if (UOverlaySlot* const ConfirmationSlot = RootOverlay->AddChildToOverlay(ConfirmationSizer))
	{
		ConfirmationSlot->SetHorizontalAlignment(HAlign_Center);
		ConfirmationSlot->SetVerticalAlignment(VAlign_Center);
	}
	HideDeleteConfirmation();
}

void UJTSExpeditionSelectWidget::RefreshSlots()
{
	const UJTSExpeditionSubsystem* const Expedition = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>() : nullptr;
	if (Expedition == nullptr)
	{
		if (StatusText != nullptr)
		{
			StatusText->SetText(FText::FromString(TEXT("Expedition save subsystem unavailable.")));
		}
		return;
	}

	HideDeleteConfirmation();
	const TArray<FJTSExpeditionSaveSummary> Summaries = Expedition->GetSaveSlotSummaries();
	for (int32 Index = 0; Index < SlotTexts.Num(); ++Index)
	{
		const FJTSExpeditionSaveSummary* const Summary = Summaries.IsValidIndex(Index) ? &Summaries[Index] : nullptr;
		const bool bOccupied = Summary != nullptr && Summary->bOccupied;
		if (DeleteSlotContainers.IsValidIndex(Index) && DeleteSlotContainers[Index] != nullptr)
		{
			DeleteSlotContainers[Index]->SetVisibility(bOccupied ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		if (DeleteSlotButtons.IsValidIndex(Index) && DeleteSlotButtons[Index] != nullptr)
		{
			DeleteSlotButtons[Index]->SetIsEnabled(bOccupied);
		}
		if (bOccupied)
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
	if (PendingDeleteSlot != INDEX_NONE || GetGameInstance() == nullptr)
	{
		return;
	}

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

void UJTSExpeditionSelectWidget::RequestDeleteSlot(int32 SlotIndex)
{
	if (GetGameInstance() == nullptr || SlotIndex < 1 || SlotIndex > UJTSExpeditionSubsystem::MaximumSaveSlots)
	{
		return;
	}

	UJTSExpeditionSubsystem* const Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>();
	if (Expedition == nullptr || !Expedition->HasSaveInSlot(SlotIndex))
	{
		return;
	}

	const TArray<FJTSExpeditionSaveSummary> Summaries = Expedition->GetSaveSlotSummaries();
	const FJTSExpeditionSaveSummary* const Summary = Summaries.IsValidIndex(SlotIndex - 1) ? &Summaries[SlotIndex - 1] : nullptr;
	ShowDeleteConfirmation(SlotIndex, Summary != nullptr ? Summary->DisplayName : FString::Printf(TEXT("Expedition %02d"), SlotIndex));
}

void UJTSExpeditionSelectWidget::ShowDeleteConfirmation(int32 SlotIndex, const FString& DisplayName)
{
	if (DeleteConfirmationBackdrop == nullptr || DeleteConfirmationPanel == nullptr || DeleteConfirmationText == nullptr)
	{
		return;
	}

	PendingDeleteSlot = SlotIndex;
	DeleteConfirmationText->SetText(FText::FromString(FString::Printf(
		TEXT("Delete \"%s\" in save slot %02d?\n\nThis permanently removes the expedition from this host. This action cannot be undone."),
		*DisplayName,
		SlotIndex)));
	DeleteConfirmationBackdrop->SetVisibility(ESlateVisibility::Visible);
	DeleteConfirmationPanel->SetVisibility(ESlateVisibility::Visible);
}

void UJTSExpeditionSelectWidget::HideDeleteConfirmation()
{
	PendingDeleteSlot = INDEX_NONE;
	if (DeleteConfirmationBackdrop != nullptr)
	{
		DeleteConfirmationBackdrop->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (DeleteConfirmationPanel != nullptr)
	{
		DeleteConfirmationPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UJTSExpeditionSelectWidget::ChooseSlotOne() { ChooseSlot(1); }
void UJTSExpeditionSelectWidget::ChooseSlotTwo() { ChooseSlot(2); }
void UJTSExpeditionSelectWidget::ChooseSlotThree() { ChooseSlot(3); }
void UJTSExpeditionSelectWidget::ChooseSlotFour() { ChooseSlot(4); }
void UJTSExpeditionSelectWidget::RequestDeleteSlotOne() { RequestDeleteSlot(1); }
void UJTSExpeditionSelectWidget::RequestDeleteSlotTwo() { RequestDeleteSlot(2); }
void UJTSExpeditionSelectWidget::RequestDeleteSlotThree() { RequestDeleteSlot(3); }
void UJTSExpeditionSelectWidget::RequestDeleteSlotFour() { RequestDeleteSlot(4); }

void UJTSExpeditionSelectWidget::ConfirmDelete()
{
	if (PendingDeleteSlot == INDEX_NONE || GetGameInstance() == nullptr)
	{
		return;
	}

	const int32 SlotToDelete = PendingDeleteSlot;
	UJTSExpeditionSubsystem* const Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>();
	if (Expedition != nullptr && Expedition->DeleteExpeditionSlot(SlotToDelete))
	{
		HideDeleteConfirmation();
		RefreshSlots();
		if (StatusText != nullptr)
		{
			StatusText->SetText(FText::FromString(FString::Printf(TEXT("Host save slot %02d deleted."), SlotToDelete)));
		}
		return;
	}

	if (StatusText != nullptr)
	{
		StatusText->SetText(FText::FromString(TEXT("Could not delete that host save. It may already be unavailable.")));
	}
}

void UJTSExpeditionSelectWidget::CancelDelete()
{
	HideDeleteConfirmation();
}

void UJTSExpeditionSelectWidget::Back() { if (UJTSFrontEndRootWidget* const Root = GetTypedOuter<UJTSFrontEndRootWidget>()) Root->ShowMainMenu(); }
void UJTSExpeditionSelectWidget::HandleOperationFinished(bool bSucceeded, const FString& Message) { if (StatusText != nullptr) StatusText->SetText(FText::FromString(Message)); }
