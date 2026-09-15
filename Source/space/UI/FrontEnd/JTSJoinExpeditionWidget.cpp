// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/FrontEnd/JTSJoinExpeditionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
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
}

TSharedRef<SWidget> UJTSJoinExpeditionWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSJoinExpeditionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UJTSJoinExpeditionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>() : nullptr)
	{
		Online->OnSessionListingsChanged.RemoveDynamic(this, &UJTSJoinExpeditionWidget::RefreshListings);
		Online->OnSessionOperationFinished.RemoveDynamic(this, &UJTSJoinExpeditionWidget::HandleOperationFinished);
		Online->OnSessionListingsChanged.AddDynamic(this, &UJTSJoinExpeditionWidget::RefreshListings);
		Online->OnSessionOperationFinished.AddDynamic(this, &UJTSJoinExpeditionWidget::HandleOperationFinished);
		StatusText->SetText(FText::FromString(Online->GetProviderStatus()));
	}
	RefreshListings();
}

void UJTSJoinExpeditionWidget::NativeDestruct()
{
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>() : nullptr)
	{
		Online->OnSessionListingsChanged.RemoveDynamic(this, &UJTSJoinExpeditionWidget::RefreshListings);
		Online->OnSessionOperationFinished.RemoveDynamic(this, &UJTSJoinExpeditionWidget::HandleOperationFinished);
	}
	Super::NativeDestruct();
}

void UJTSJoinExpeditionWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	UBorder* const Panel = JTSUITheme::MakePanel(WidgetTree, TEXT("JoinPanel"), JTSUITheme::Panel, FMargin(28.0f));
	WidgetTree->RootWidget = Panel;
	UVerticalBox* const Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("JoinLayout"));
	Panel->SetContent(Layout);
	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("Title"), TEXT("JOIN EXPEDITION"), 30.0f, JTSUITheme::Ink));
	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("Description"), TEXT("Search public expeditions, or enter the code shared by your host."), 14.0f, JTSUITheme::Muted), FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("CodeLabel"), TEXT("JOIN CODE"), 14.0f, JTSUITheme::Muted));
	JoinCodeBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("JoinCode"));
	JoinCodeBox->SetHintText(FText::FromString(TEXT("Leave blank to browse public expeditions")));
	AddRow(Layout, JoinCodeBox);
	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("PasswordLabel"), TEXT("PASSWORD"), 14.0f, JTSUITheme::Muted));
	PasswordBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Password"));
	PasswordBox->SetHintText(FText::FromString(TEXT("Only required by password-protected expeditions")));
	PasswordBox->SetIsPassword(true);
	AddRow(Layout, PasswordBox);
	UButton* const SearchButton = JTSUITheme::MakeButton(WidgetTree, TEXT("Search"), TEXT("SEARCH EXPEDITIONS"), JTSUITheme::EButtonTone::Primary);
	SearchButton->OnClicked.AddDynamic(this, &UJTSJoinExpeditionWidget::Search);
	AddRow(Layout, SearchButton, FMargin(0.0f, 10.0f, 0.0f, 6.0f));
	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("ResultsLabel"), TEXT("AVAILABLE EXPEDITIONS"), 14.0f, JTSUITheme::Muted));
	UScrollBox* const Listings = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Listings"));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UButton* const ListingButton = JTSUITheme::MakeButton(WidgetTree, *FString::Printf(TEXT("Listing%d"), Index + 1), TEXT("NO EXPEDITION"), JTSUITheme::EButtonTone::Secondary, 14.0f);
		UTextBlock* const ListingLabel = Cast<UTextBlock>(ListingButton->GetContent());
		ListingLabel->SetJustification(ETextJustify::Left);
		ListingButton->SetVisibility(ESlateVisibility::Collapsed);
		if (UScrollBoxSlot* const ScrollSlot = Cast<UScrollBoxSlot>(Listings->AddChild(ListingButton))) ScrollSlot->SetPadding(FMargin(0.0f, 3.0f));
		ListingButtons.Add(ListingButton);
		ListingButtonTexts.Add(ListingLabel);
		switch (Index)
		{
		case 0: ListingButton->OnClicked.AddDynamic(this, &UJTSJoinExpeditionWidget::SelectListingOne); break;
		case 1: ListingButton->OnClicked.AddDynamic(this, &UJTSJoinExpeditionWidget::SelectListingTwo); break;
		case 2: ListingButton->OnClicked.AddDynamic(this, &UJTSJoinExpeditionWidget::SelectListingThree); break;
		default: ListingButton->OnClicked.AddDynamic(this, &UJTSJoinExpeditionWidget::SelectListingFour); break;
		}
	}
	AddRow(Layout, Listings, FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	ListingText = JTSUITheme::MakeText(WidgetTree, TEXT("ListingStatus"), TEXT("Search to find a crew."), 13.0f, JTSUITheme::Muted);
	AddRow(Layout, ListingText);
	UButton* const JoinButton = JTSUITheme::MakeButton(WidgetTree, TEXT("JoinSelected"), TEXT("JOIN SELECTED EXPEDITION"), JTSUITheme::EButtonTone::Ready);
	JoinButton->OnClicked.AddDynamic(this, &UJTSJoinExpeditionWidget::JoinSelectedListing);
	AddRow(Layout, JoinButton, FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	UButton* const BackButton = JTSUITheme::MakeButton(WidgetTree, TEXT("Back"), TEXT("BACK"), JTSUITheme::EButtonTone::Secondary);
	BackButton->OnClicked.AddDynamic(this, &UJTSJoinExpeditionWidget::ReturnToMain);
	AddRow(Layout, BackButton);
	StatusText = JTSUITheme::MakeText(WidgetTree, TEXT("Status"), TEXT(""), 12.0f, JTSUITheme::Muted);
	AddRow(Layout, StatusText);
}

void UJTSJoinExpeditionWidget::Search()
{
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>())
	{
		SelectedListingIndex = INDEX_NONE;
		Online->FindExpeditions(JoinCodeBox->GetText().ToString());
	}
}

void UJTSJoinExpeditionWidget::JoinSelectedListing()
{
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>())
	{
		if (SelectedListingIndex == INDEX_NONE)
		{
			StatusText->SetText(FText::FromString(TEXT("Select an expedition result first.")));
			return;
		}
		Online->JoinExpedition(SelectedListingIndex, PasswordBox->GetText().ToString());
	}
}

void UJTSJoinExpeditionWidget::RefreshListings()
{
	const UJTSOnlineSessionSubsystem* const Online = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>() : nullptr;
	const TArray<FJTSSessionListing> Listings = Online != nullptr ? Online->GetDiscoveredListings() : TArray<FJTSSessionListing>();
	if (!Listings.IsValidIndex(SelectedListingIndex)) SelectedListingIndex = INDEX_NONE;
	for (int32 Index = 0; Index < ListingButtons.Num(); ++Index)
	{
		const bool bVisible = Listings.IsValidIndex(Index);
		ListingButtons[Index]->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bVisible)
		{
			const FJTSSessionListing& Listing = Listings[Index];
			ListingButtonTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("%s  •  HOST: %s  •  %d/%d  •  %s%s"),
				*Listing.JoinCode, *Listing.HostName, Listing.CurrentPlayers, Listing.MaximumPlayers,
				Listing.Visibility == EJTSLobbyVisibility::Private ? TEXT("PRIVATE") : TEXT("PUBLIC"),
				Index == SelectedListingIndex ? TEXT("  ✓ SELECTED") : TEXT(""))));
			ListingButtons[Index]->SetBackgroundColor(Index == SelectedListingIndex ? JTSUITheme::Cyan : JTSUITheme::PanelRaised);
		}
	}
	ListingText->SetText(FText::FromString(Listings.IsEmpty() ? TEXT("No matching expedition found.") : TEXT("Select a crew, then join its pre-launch lobby.")));
}

void UJTSJoinExpeditionWidget::SelectListing(int32 Index) { SelectedListingIndex = Index; RefreshListings(); }
void UJTSJoinExpeditionWidget::SelectListingOne() { SelectListing(0); }
void UJTSJoinExpeditionWidget::SelectListingTwo() { SelectListing(1); }
void UJTSJoinExpeditionWidget::SelectListingThree() { SelectListing(2); }
void UJTSJoinExpeditionWidget::SelectListingFour() { SelectListing(3); }
void UJTSJoinExpeditionWidget::ReturnToMain() { if (UJTSFrontEndRootWidget* const Root = GetTypedOuter<UJTSFrontEndRootWidget>()) Root->ShowMainMenu(); }
void UJTSJoinExpeditionWidget::HandleOperationFinished(bool bSucceeded, const FString& Message) { if (StatusText != nullptr) StatusText->SetText(FText::FromString(Message)); }
