// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSInventoryQuantityDialogWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Player/JTSPlayerController.h"

namespace
{
	UTextBlock* MakeDialogText(UWidgetTree* Tree, const FName Name, const FString& Text, const float FontSize, const FLinearColor& Color)
	{
		if (Tree == nullptr)
		{
			return nullptr;
		}

		UTextBlock* const Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		if (Result != nullptr)
		{
			Result->SetText(FText::FromString(Text));
			Result->SetFont(FCoreStyle::GetDefaultFontStyle(FName(TEXT("Bold")), FontSize));
			Result->SetColorAndOpacity(FSlateColor(Color));
			Result->SetJustification(ETextJustify::Center);
			Result->SetAutoWrapText(true);
		}
		return Result;
	}

	UButton* MakeDialogButton(UWidgetTree* Tree, const FName Name, const FString& Label)
	{
		if (Tree == nullptr)
		{
			return nullptr;
		}

		UButton* const Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		if (Button != nullptr)
		{
			Button->SetBackgroundColor(FLinearColor(0.10f, 0.28f, 0.40f, 1.0f));
			Button->SetContent(MakeDialogText(Tree, *FString::Printf(TEXT("%sLabel"), *Name.ToString()), Label, 15.0f, FLinearColor::White));
		}
		return Button;
	}

	void AddVertical(UVerticalBox* Parent, UWidget* Child, const FMargin Padding)
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

TSharedRef<SWidget> UJTSInventoryQuantityDialogWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

bool UJTSInventoryQuantityDialogWidget::OpenForItem(int32 SlotIndex, const FJTSItemInstance& Item, bool bInDestroyMode)
{
	if (SlotIndex < 0 || Item.IsEmpty() || Item.StackCount <= 1)
	{
		return false;
	}

	BuildWidgetTree();
	ActiveSlotIndex = SlotIndex;
	ActiveItem = Item;
	MaximumQuantity = Item.StackCount;
	bDestroyMode = bInDestroyMode;
	bDialogOpen = true;
	SetVisibility(ESlateVisibility::Visible);
	RefreshText();
	if (QuantityInput != nullptr)
	{
		QuantityInput->SetKeyboardFocus();
	}
	return true;
}

void UJTSInventoryQuantityDialogWidget::CloseDialog()
{
	bDialogOpen = false;
	ActiveSlotIndex = INDEX_NONE;
	MaximumQuantity = 0;
	ActiveItem.Clear();
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UJTSInventoryQuantityDialogWidget::IsDialogOpen() const
{
	return bDialogOpen;
}

FReply UJTSInventoryQuantityDialogWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bDialogOpen)
	{
		return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
	}

	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
		{
			Controller->CloseInventoryQuantityDialog();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Enter)
	{
		ConfirmSelection();
		return FReply::Handled();
	}
	if (Key == EKeys::Up || Key == EKeys::Right)
	{
		AdjustQuantity(1);
		return FReply::Handled();
	}
	if (Key == EKeys::Down || Key == EKeys::Left)
	{
		AdjustQuantity(-1);
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UJTSInventoryQuantityDialogWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDialogOpen && !FMath::IsNearlyZero(InMouseEvent.GetWheelDelta()))
	{
		AdjustQuantity(InMouseEvent.GetWheelDelta() > 0.0f ? 1 : -1);
		return FReply::Handled();
	}

	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void UJTSInventoryQuantityDialogWidget::BuildWidgetTree()
{
	if (RootCanvas != nullptr || WidgetTree == nullptr)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("QuantityDialogRoot"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* const Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QuantityBackdrop"));
	Backdrop->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.04f, 0.78f));
	if (UCanvasPanelSlot* const BackdropSlot = RootCanvas->AddChildToCanvas(Backdrop))
	{
		BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackdropSlot->SetOffsets(FMargin(0.0f));
	}

	DialogFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QuantityDialogFrame"));
	DialogFrame->SetBrushColor(FLinearColor(0.035f, 0.075f, 0.12f, 0.98f));
	DialogFrame->SetPadding(FMargin(28.0f));
	if (UCanvasPanelSlot* const FrameSlot = RootCanvas->AddChildToCanvas(DialogFrame))
	{
		FrameSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		FrameSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		FrameSlot->SetSize(FVector2D(500.0f, 320.0f));
	}

	UVerticalBox* const Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("QuantityDialogContent"));
	DialogFrame->SetContent(Content);
	TitleText = MakeDialogText(WidgetTree, TEXT("QuantityTitle"), TEXT("DROP QUANTITY"), 28.0f, FLinearColor(0.55f, 0.88f, 1.0f, 1.0f));
	ItemText = MakeDialogText(WidgetTree, TEXT("QuantityItem"), TEXT("ITEM"), 19.0f, FLinearColor::White);
	HintText = MakeDialogText(WidgetTree, TEXT("QuantityHint"), TEXT("Mouse wheel / type a number / Enter"), 14.0f, FLinearColor(0.70f, 0.78f, 0.86f, 1.0f));
	AddVertical(Content, TitleText, FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	AddVertical(Content, ItemText, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	UHorizontalBox* const QuantityRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("QuantityRow"));
	DecreaseButton = MakeDialogButton(WidgetTree, TEXT("DecreaseQuantity"), TEXT("−"));
	IncreaseButton = MakeDialogButton(WidgetTree, TEXT("IncreaseQuantity"), TEXT("+"));
	QuantityInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("QuantityInput"));
	QuantityInput->SetJustification(ETextJustify::Center);
	QuantityInput->SetSelectAllTextWhenFocused(true);
	QuantityInput->SetClearKeyboardFocusOnCommit(false);
	if (UHorizontalBoxSlot* const MinusSlot = QuantityRow->AddChildToHorizontalBox(DecreaseButton))
	{
		MinusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		MinusSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	}
	if (UHorizontalBoxSlot* const InputSlot = QuantityRow->AddChildToHorizontalBox(QuantityInput))
	{
		InputSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	if (UHorizontalBoxSlot* const PlusSlot = QuantityRow->AddChildToHorizontalBox(IncreaseButton))
	{
		PlusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		PlusSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
	}
	AddVertical(Content, QuantityRow, FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	AddVertical(Content, HintText, FMargin(0.0f, 0.0f, 0.0f, 16.0f));

	UHorizontalBox* const ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("QuantityActions"));
	ConfirmButton = MakeDialogButton(WidgetTree, TEXT("ConfirmQuantity"), TEXT("CONFIRM"));
	CancelButton = MakeDialogButton(WidgetTree, TEXT("CancelQuantity"), TEXT("CANCEL"));
	if (UHorizontalBoxSlot* const ConfirmSlot = ActionRow->AddChildToHorizontalBox(ConfirmButton))
	{
		ConfirmSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ConfirmSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
	}
	if (UHorizontalBoxSlot* const CancelSlot = ActionRow->AddChildToHorizontalBox(CancelButton))
	{
		CancelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CancelSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
	}
	AddVertical(Content, ActionRow, FMargin(0.0f));

	DecreaseButton->OnClicked.AddDynamic(this, &UJTSInventoryQuantityDialogWidget::HandleDecreaseClicked);
	IncreaseButton->OnClicked.AddDynamic(this, &UJTSInventoryQuantityDialogWidget::HandleIncreaseClicked);
	ConfirmButton->OnClicked.AddDynamic(this, &UJTSInventoryQuantityDialogWidget::HandleConfirmClicked);
	CancelButton->OnClicked.AddDynamic(this, &UJTSInventoryQuantityDialogWidget::HandleCancelClicked);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UJTSInventoryQuantityDialogWidget::RefreshText()
{
	if (TitleText != nullptr)
	{
		TitleText->SetText(FText::FromString(bDestroyMode ? TEXT("DESTROY QUANTITY") : TEXT("DROP QUANTITY")));
	}
	if (ItemText != nullptr)
	{
		ItemText->SetText(FText::FromString(FString::Printf(
			TEXT("%s  x %d"),
			*UJTSItemDefinitionLibrary::GetItemDisplayName(ActiveItem.ItemId).ToString(),
			MaximumQuantity)));
	}
	if (HintText != nullptr)
	{
		HintText->SetText(FText::FromString(bDestroyMode
			? TEXT("Permanent. Mouse wheel / type a number / Enter")
			: TEXT("Mouse wheel / type a number / Enter")));
	}
	if (QuantityInput != nullptr)
	{
		QuantityInput->SetText(FText::AsNumber(MaximumQuantity));
	}
}

void UJTSInventoryQuantityDialogWidget::AdjustQuantity(int32 Delta)
{
	if (QuantityInput == nullptr || MaximumQuantity <= 0)
	{
		return;
	}

	const int32 CurrentQuantity = FMath::Clamp(FCString::Atoi(*QuantityInput->GetText().ToString()), 1, MaximumQuantity);
	QuantityInput->SetText(FText::AsNumber(FMath::Clamp(CurrentQuantity + Delta, 1, MaximumQuantity)));
}

void UJTSInventoryQuantityDialogWidget::ConfirmSelection()
{
	if (!bDialogOpen || QuantityInput == nullptr)
	{
		return;
	}

	const int32 Quantity = FMath::Clamp(FCString::Atoi(*QuantityInput->GetText().ToString()), 1, MaximumQuantity);
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		Controller->ServerRequestInventoryQuantityAction(ActiveSlotIndex, Quantity, bDestroyMode);
		Controller->CloseInventoryQuantityDialog();
	}
}

void UJTSInventoryQuantityDialogWidget::HandleDecreaseClicked()
{
	AdjustQuantity(-1);
}

void UJTSInventoryQuantityDialogWidget::HandleIncreaseClicked()
{
	AdjustQuantity(1);
}

void UJTSInventoryQuantityDialogWidget::HandleConfirmClicked()
{
	ConfirmSelection();
}

void UJTSInventoryQuantityDialogWidget::HandleCancelClicked()
{
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		Controller->CloseInventoryQuantityDialog();
	}
}
