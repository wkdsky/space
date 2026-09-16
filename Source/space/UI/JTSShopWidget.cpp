// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSShopWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSPlayerEquipmentComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/World/JTSShopTerminalActor.h"

namespace
{
	UCanvasPanelSlot* AddCanvas(
		UCanvasPanel* Parent,
		UWidget* Child,
		const FAnchors& Anchors,
		const FVector2D& Position,
		const FVector2D& Size,
		const FVector2D& Alignment = FVector2D::ZeroVector)
	{
		if (Parent == nullptr || Child == nullptr) return nullptr;
		UCanvasPanelSlot* const Slot = Parent->AddChildToCanvas(Child);
		if (Slot != nullptr)
		{
			Slot->SetAnchors(Anchors);
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
			Slot->SetAlignment(Alignment);
		}
		return Slot;
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const FName Name, const FString& Text, const float Size, const FLinearColor& Color, const ETextJustify::Type Justify = ETextJustify::Left)
	{
		if (Tree == nullptr) return nullptr;
		UTextBlock* const Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		if (Result != nullptr)
		{
			Result->SetText(FText::FromString(Text));
			Result->SetFont(FCoreStyle::GetDefaultFontStyle(FName(TEXT("Bold")), Size));
			Result->SetColorAndOpacity(FSlateColor(Color));
			Result->SetAutoWrapText(true);
			Result->SetJustification(Justify);
		}
		return Result;
	}

	UBorder* MakeBorder(UWidgetTree* Tree, const FName Name, const FLinearColor Color, const float Padding = 0.0f)
	{
		if (Tree == nullptr) return nullptr;
		UBorder* const Result = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		if (Result != nullptr)
		{
			Result->SetBrushColor(Color);
			Result->SetPadding(FMargin(Padding));
		}
		return Result;
	}

	UButton* MakeButton(UWidgetTree* Tree, const FName Name, const FString& Label, const FLinearColor Color, const float FontSize = 17.0f)
	{
		if (Tree == nullptr) return nullptr;
		UButton* const Result = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		if (Result != nullptr)
		{
			Result->SetBackgroundColor(Color);
			if (UTextBlock* const Text = MakeText(Tree, *FString::Printf(TEXT("%s_Label"), *Name.ToString()), Label, FontSize, FLinearColor::White, ETextJustify::Center))
			{
				Result->SetContent(Text);
			}
		}
		return Result;
	}

	void AddVertical(UVerticalBox* Parent, UWidget* Child, const FMargin Padding, const EHorizontalAlignment HorizontalAlignment = HAlign_Fill)
	{
		if (Parent == nullptr || Child == nullptr) return;
		if (UVerticalBoxSlot* const Slot = Parent->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(Padding);
			Slot->SetHorizontalAlignment(HorizontalAlignment);
		}
	}

	FString GetShopCategoryLabel(const EJTSShopCategory Category)
	{
		switch (Category)
		{
		case EJTSShopCategory::Weapons: return TEXT("WEAPONS");
		case EJTSShopCategory::Mining: return TEXT("MINING");
		case EJTSShopCategory::Utility: return TEXT("UTILITY");
		case EJTSShopCategory::Resources: return TEXT("RESOURCES / MATERIALS");
		case EJTSShopCategory::Wearables: return TEXT("WEARABLES");
		default: return TEXT("ALL SUPPLIES");
		}
	}

	FString GetSupplyCardPlaceholderLabel(const UJTSItemDefinition* Definition)
	{
		if (!IsValid(Definition)) return TEXT("SUPPLY");
		if (Definition->IsWearable()) return TEXT("WEARABLE  //  CAPACITY");
		if (Definition->IsRangedWeapon()) return TEXT("RANGED  //  PROTOTYPE");
		if (Definition->PrimaryCategory == EJTSItemCategory::Mining) return TEXT("MINING  //  TOOL");
		if (Definition->PrimaryCategory == EJTSItemCategory::Weapons) return TEXT("MELEE  //  WEAPON");
		return TEXT("EXPEDITION  //  SUPPLY");
	}
}

TSharedRef<SWidget> UJTSShopWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

FReply UJTSShopWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bShopOpen && (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::E))
	{
		if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
		{
			Controller->CloseSpaceShop();
			return FReply::Handled();
		}
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

bool UJTSShopWidget::OpenForTerminal(AJTSShopTerminalActor* Terminal)
{
	if (!IsValid(Terminal))
	{
		return false;
	}
	BuildWidgetTree();
	SetIsFocusable(true);
	ActiveTerminal = Terminal;
	bShopOpen = true;
	SetVisibility(ESlateVisibility::Visible);
	RefreshAll();
	return true;
}

void UJTSShopWidget::CloseShop()
{
	bShopOpen = false;
	ActiveTerminal.Reset();
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UJTSShopWidget::IsShopOpen() const
{
	return bShopOpen && ActiveTerminal.IsValid();
}

void UJTSShopWidget::NotifyPurchaseResult(EJTSShopPurchaseResult Result)
{
	if (StatusText == nullptr)
	{
		return;
	}
	switch (Result)
	{
	case EJTSShopPurchaseResult::Succeeded:
		StatusText->SetText(FText::FromString(TEXT("Purchase authorized. Supply transferred to your inventory.")));
		break;
	case EJTSShopPurchaseResult::SucceededDropped:
		StatusText->SetText(FText::FromString(TEXT("Purchase authorized. No destination slot: supply deployed beside the terminal.")));
		break;
	case EJTSShopPurchaseResult::InsufficientResources:
		StatusText->SetText(FText::FromString(TEXT("Purchase denied: the shared ship wallet lacks the required materials.")));
		break;
	case EJTSShopPurchaseResult::InvalidItem:
		StatusText->SetText(FText::FromString(TEXT("Purchase denied: this catalog entry is no longer valid.")));
		break;
	case EJTSShopPurchaseResult::DeliveryFailed:
	default:
		StatusText->SetText(FText::FromString(TEXT("Purchase could not be delivered. No shared materials were spent.")));
		break;
	}
	RefreshAll();
}

void UJTSShopWidget::NotifyDepositResult(bool bSucceeded)
{
	if (StatusText != nullptr)
	{
		StatusText->SetText(FText::FromString(bSucceeded
			? TEXT("Carried Rock and Ore deposited into the shared ship wallet.")
			: TEXT("No carried Rock or Ore could be deposited from this terminal.")));
	}
	RefreshAll();
}

void UJTSShopWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bShopOpen) return;
	if (!ActiveTerminal.IsValid())
	{
		// Closing through the controller also restores viewport/game input. A local
		// collapse here would leave a UI-only input mode active after a terminal is removed.
		if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
		{
			Controller->CloseSpaceShop();
		}
		else
		{
			CloseShop();
		}
		return;
	}
	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.15f)
	{
		RefreshAccumulator = 0.0f;
		RefreshWallet();
		RefreshDetail();
	}
}

void UJTSShopWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || RootCanvas != nullptr) return;
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShopRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* const Dimmer = MakeBorder(WidgetTree, TEXT("ShopDimmer"), FLinearColor(0.004f, 0.012f, 0.030f, 0.94f));
	AddCanvas(RootCanvas, Dimmer, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);
	ShopFrame = MakeBorder(WidgetTree, TEXT("ExpeditionSupplyFrame"), FLinearColor(0.025f, 0.065f, 0.115f, 0.995f), 18.0f);
	AddCanvas(RootCanvas, ShopFrame, FAnchors(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(1420.0f, 800.0f), FVector2D(0.5f, 0.5f));
	UCanvasPanel* const FrameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShopFrameCanvas"));
	ShopFrame->SetContent(FrameCanvas);

	AddCanvas(FrameCanvas, MakeText(WidgetTree, TEXT("ShopKicker"), TEXT("JUMP TO SPACE  /  SHARED EXPEDITION MARKET"), 14.0f, FLinearColor(0.35f, 0.78f, 1.0f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(28.0f, 20.0f), FVector2D(620.0f, 24.0f));
	AddCanvas(FrameCanvas, MakeText(WidgetTree, TEXT("ShopTitle"), TEXT("EXPEDITION SUPPLY TERMINAL"), 32.0f, FLinearColor(0.90f, 0.96f, 1.0f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(28.0f, 44.0f), FVector2D(760.0f, 48.0f));
	WalletText = MakeText(WidgetTree, TEXT("SharedWallet"), TEXT("SHIP WALLET  ROCK 0  |  ORE 0"), 18.0f, FLinearColor(0.35f, 1.0f, 0.72f, 1.0f), ETextJustify::Right);
	AddCanvas(FrameCanvas, WalletText, FAnchors(1.0f, 0.0f), FVector2D(-270.0f, 35.0f), FVector2D(240.0f, 46.0f), FVector2D(1.0f, 0.0f));

	UBorder* const CategoryPanel = MakeBorder(WidgetTree, TEXT("ShopCategories"), FLinearColor(0.035f, 0.105f, 0.175f, 1.0f), 12.0f);
	AddCanvas(FrameCanvas, CategoryPanel, FAnchors(0.0f, 0.0f), FVector2D(28.0f, 112.0f), FVector2D(200.0f, 630.0f));
	CategoryList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CategoryList"));
	CategoryPanel->SetContent(CategoryList);
	AddVertical(CategoryList, MakeText(WidgetTree, TEXT("CategoryLabel"), TEXT("SUPPLY FILTER"), 14.0f, FLinearColor(0.38f, 0.70f, 0.93f, 1.0f)), FMargin(6.0f, 4.0f, 6.0f, 15.0f));

	const TArray<FString> CategoryLabels = { TEXT("ALL"), TEXT("WEAPONS"), TEXT("MINING"), TEXT("UTILITY"), TEXT("MATERIALS"), TEXT("WEARABLES") };
	for (int32 Index = 0; Index < CategoryLabels.Num(); ++Index)
	{
		UButton* const Button = MakeButton(WidgetTree, *FString::Printf(TEXT("Category_%d"), Index), CategoryLabels[Index], FLinearColor(0.06f, 0.16f, 0.25f, 1.0f));
		CategoryButtons.Add(Button);
		AddVertical(CategoryList, Button, FMargin(4.0f, 3.0f));
	}
	CategoryButtons[0]->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleAllCategory);
	CategoryButtons[1]->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleWeaponsCategory);
	CategoryButtons[2]->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleMiningCategory);
	CategoryButtons[3]->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleUtilityCategory);
	CategoryButtons[4]->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleResourcesCategory);
	CategoryButtons[5]->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleWearablesCategory);
	AddVertical(CategoryList, MakeText(WidgetTree, TEXT("FilterFootnote"), TEXT("Tags describe combat, mining, capacity and expedition roles. More catalogs can be attached without changing this terminal."), 12.0f, FLinearColor(0.48f, 0.62f, 0.75f, 1.0f)), FMargin(6.0f, 22.0f, 6.0f, 0.0f));

	UBorder* const CatalogPanel = MakeBorder(WidgetTree, TEXT("ShopCatalogPanel"), FLinearColor(0.018f, 0.050f, 0.090f, 1.0f), 12.0f);
	AddCanvas(FrameCanvas, CatalogPanel, FAnchors(0.0f, 0.0f), FVector2D(245.0f, 112.0f), FVector2D(725.0f, 630.0f));
	CatalogScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ShopCatalogScroll"));
	CatalogWrap = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("ShopCatalogCards"));
	CatalogWrap->SetInnerSlotPadding(FVector2D(10.0f, 10.0f));
	CatalogScroll->AddChild(CatalogWrap);
	CatalogPanel->SetContent(CatalogScroll);

	DetailPanel = MakeBorder(WidgetTree, TEXT("ShopDetailPanel"), FLinearColor(0.045f, 0.125f, 0.190f, 1.0f), 16.0f);
	AddCanvas(FrameCanvas, DetailPanel, FAnchors(1.0f, 0.0f), FVector2D(-28.0f, 112.0f), FVector2D(390.0f, 630.0f), FVector2D(1.0f, 0.0f));
	UVerticalBox* const DetailBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopDetailBox"));
	DetailPanel->SetContent(DetailBox);
	AddVertical(DetailBox, MakeText(WidgetTree, TEXT("DetailKicker"), TEXT("SELECTED SUPPLY"), 13.0f, FLinearColor(0.36f, 0.78f, 1.0f, 1.0f)), FMargin(2.0f, 2.0f, 2.0f, 4.0f));
	DetailNameText = MakeText(WidgetTree, TEXT("DetailName"), TEXT("PICKAXE"), 27.0f, FLinearColor::White);
	DetailTagsText = MakeText(WidgetTree, TEXT("DetailTags"), TEXT("MINING  /  MELEE  /  UTILITY"), 13.0f, FLinearColor(1.0f, 0.76f, 0.26f, 1.0f));
	DetailDescriptionText = MakeText(WidgetTree, TEXT("DetailDescription"), TEXT(""), 15.0f, FLinearColor(0.80f, 0.87f, 0.95f, 1.0f));
	DetailStatsText = MakeText(WidgetTree, TEXT("DetailStats"), TEXT(""), 15.0f, FLinearColor(0.62f, 0.88f, 1.0f, 1.0f));
	DetailCostText = MakeText(WidgetTree, TEXT("DetailCost"), TEXT(""), 20.0f, FLinearColor(0.35f, 1.0f, 0.72f, 1.0f));
	DeliveryHintText = MakeText(WidgetTree, TEXT("DeliveryHint"), TEXT("NO OPEN INVENTORY OR WEARABLE SLOT? PURCHASED SUPPLIES DEPLOY AS A WORLD PICKUP BESIDE THIS TERMINAL."), 12.0f, FLinearColor(1.0f, 0.80f, 0.34f, 1.0f));
	StatusText = MakeText(WidgetTree, TEXT("ShopStatus"), TEXT("Ready for expedition requisition."), 13.0f, FLinearColor(0.72f, 0.87f, 1.0f, 1.0f));
	AddVertical(DetailBox, DetailNameText, FMargin(2.0f, 0.0f, 2.0f, 3.0f));
	AddVertical(DetailBox, DetailTagsText, FMargin(2.0f, 0.0f, 2.0f, 14.0f));
	AddVertical(DetailBox, DetailDescriptionText, FMargin(2.0f, 0.0f, 2.0f, 16.0f));
	AddVertical(DetailBox, DetailStatsText, FMargin(2.0f, 0.0f, 2.0f, 16.0f));
	AddVertical(DetailBox, DetailCostText, FMargin(2.0f, 0.0f, 2.0f, 15.0f));
	PurchaseButton = MakeButton(WidgetTree, TEXT("PurchaseSelected"), TEXT("AUTHORIZE PURCHASE"), FLinearColor(0.06f, 0.54f, 0.42f, 1.0f), 20.0f);
	DepositButton = MakeButton(WidgetTree, TEXT("DepositMaterials"), TEXT("DEPOSIT CARRIED ROCK + ORE"), FLinearColor(0.16f, 0.31f, 0.66f, 1.0f), 15.0f);
	CloseButton = MakeButton(WidgetTree, TEXT("CloseSupplyTerminal"), TEXT("CLOSE [E]"), FLinearColor(0.20f, 0.25f, 0.33f, 1.0f), 15.0f);
	PurchaseButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandlePurchaseClicked);
	DepositButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleDepositClicked);
	CloseButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleCloseClicked);
	AddVertical(DetailBox, PurchaseButton, FMargin(2.0f, 0.0f, 2.0f, 8.0f));
	AddVertical(DetailBox, DepositButton, FMargin(2.0f, 0.0f, 2.0f, 12.0f));
	AddVertical(DetailBox, DeliveryHintText, FMargin(2.0f, 0.0f, 2.0f, 10.0f));
	AddVertical(DetailBox, StatusText, FMargin(2.0f, 0.0f, 2.0f, 10.0f));
	AddVertical(DetailBox, CloseButton, FMargin(2.0f, 0.0f, 2.0f, 0.0f));

	SetVisibility(ESlateVisibility::Collapsed);
}

void UJTSShopWidget::RefreshAll()
{
	RefreshWallet();
	RefreshCatalog();
	RefreshDetail();
}

bool UJTSShopWidget::IsItemVisible(EJTSItemId ItemId) const
{
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
	return IsValid(Definition) && Definition->MatchesShopCategory(ActiveCategory);
}

void UJTSShopWidget::RefreshCatalog()
{
	if (CatalogWrap == nullptr) return;
	CatalogWrap->ClearChildren();
	CatalogButtons.Reset();
	bool bHasVisibleCatalogItem = false;
	for (const EJTSItemId ItemId : UJTSItemDefinitionLibrary::GetDefaultShopCatalog())
	{
		if (!IsItemVisible(ItemId)) continue;
		bHasVisibleCatalogItem = true;
		const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
		const bool bSelected = ItemId == SelectedItemId;
		UButton* const Card = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			*FString::Printf(TEXT("SupplyCard_%d"), static_cast<int32>(ItemId)));
		Card->SetBackgroundColor(bSelected
			? Definition->AccentColor.CopyWithNewOpacity(0.72f)
			: FLinearColor(0.055f, 0.13f, 0.20f, 1.0f));
		UVerticalBox* const CardContents = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			*FString::Printf(TEXT("SupplyCardContents_%d"), static_cast<int32>(ItemId)));
		UBorder* const PlaceholderIcon = MakeBorder(
			WidgetTree,
			*FString::Printf(TEXT("SupplyCardIcon_%d"), static_cast<int32>(ItemId)),
			Definition->AccentColor.CopyWithNewOpacity(0.82f),
			3.0f);
		PlaceholderIcon->SetContent(MakeText(
			WidgetTree,
			*FString::Printf(TEXT("SupplyCardIconLabel_%d"), static_cast<int32>(ItemId)),
			GetSupplyCardPlaceholderLabel(Definition),
			11.0f,
			FLinearColor(0.015f, 0.035f, 0.065f, 1.0f),
			ETextJustify::Center));
		AddVertical(CardContents, PlaceholderIcon, FMargin(5.0f, 5.0f, 5.0f, 4.0f));
		AddVertical(CardContents, MakeText(WidgetTree, *FString::Printf(TEXT("SupplyCardName_%d"), static_cast<int32>(ItemId)), Definition->DisplayName.ToString(), 17.0f, FLinearColor::White), FMargin(6.0f, 0.0f, 6.0f, 2.0f));
		AddVertical(CardContents, MakeText(WidgetTree, *FString::Printf(TEXT("SupplyCardTags_%d"), static_cast<int32>(ItemId)), FormatTags(ItemId), 11.0f, FLinearColor(0.70f, 0.84f, 0.97f, 1.0f)), FMargin(6.0f, 0.0f, 6.0f, 2.0f));
		AddVertical(CardContents, MakeText(WidgetTree, *FString::Printf(TEXT("SupplyCardCost_%d"), static_cast<int32>(ItemId)), FormatCosts(ItemId), 13.0f, FLinearColor(0.38f, 1.0f, 0.70f, 1.0f)), FMargin(6.0f, 0.0f, 6.0f, 2.0f));
		AddVertical(CardContents, MakeText(WidgetTree, *FString::Printf(TEXT("SupplyCardOwnership_%d"), static_cast<int32>(ItemId)), FormatOwnership(ItemId), 11.0f, FLinearColor(0.78f, 0.84f, 0.91f, 1.0f)), FMargin(6.0f, 0.0f, 6.0f, 4.0f));
		Card->SetContent(CardContents);
		Card->SetToolTipText(Definition->Description);
		CatalogButtons.Add(Card);
		switch (ItemId)
		{
		case EJTSItemId::Pickaxe: Card->OnClicked.AddDynamic(this, &UJTSShopWidget::HandlePickaxeSelected); break;
		case EJTSItemId::Knife: Card->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleKnifeSelected); break;
		case EJTSItemId::Pistol: Card->OnClicked.AddDynamic(this, &UJTSShopWidget::HandlePistolSelected); break;
		case EJTSItemId::MachineGun: Card->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleMachineGunSelected); break;
		case EJTSItemId::Backpack: Card->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleBackpackSelected); break;
		default: break;
		}
		USizeBox* const CardFrame = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("SupplyCardFrame_%d"), static_cast<int32>(ItemId)));
		CardFrame->SetMinDesiredWidth(218.0f);
		CardFrame->SetMinDesiredHeight(178.0f);
		CardFrame->SetContent(Card);
		if (UWrapBoxSlot* const CardSlot = CatalogWrap->AddChildToWrapBox(CardFrame))
		{
			CardSlot->SetPadding(FMargin(4.0f));
			CardSlot->SetFillEmptySpace(false);
		}
	}
	if (!bHasVisibleCatalogItem)
	{
		USizeBox* const EmptyCatalogCard = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EmptyCatalogCard"));
		EmptyCatalogCard->SetMinDesiredWidth(665.0f);
		EmptyCatalogCard->SetMinDesiredHeight(180.0f);
		EmptyCatalogCard->SetContent(MakeText(
			WidgetTree,
			TEXT("EmptyCatalogText"),
			TEXT("MATERIAL CATALOG\nNo raw materials are sold by this terminal yet. This category is reserved for future expedition supply contracts."),
			18.0f,
			FLinearColor(0.48f, 0.67f, 0.82f, 1.0f),
			ETextJustify::Center));
		CatalogWrap->AddChildToWrapBox(EmptyCatalogCard);
	}
	for (int32 Index = 0; Index < CategoryButtons.Num(); ++Index)
	{
		const bool bSelected = static_cast<int32>(ActiveCategory) == Index;
		CategoryButtons[Index]->SetBackgroundColor(bSelected ? FLinearColor(0.12f, 0.55f, 0.78f, 1.0f) : FLinearColor(0.06f, 0.16f, 0.25f, 1.0f));
	}
}

void UJTSShopWidget::RefreshWallet()
{
	if (WalletText == nullptr) return;
	if (const AJTSShopTerminalActor* const Terminal = ActiveTerminal.Get())
	{
		WalletText->SetText(FText::FromString(FString::Printf(TEXT("SHIP WALLET\nROCK %d   |   ORE %d"), Terminal->GetSharedResourceAmount(EJTSResourceType::Rock), Terminal->GetSharedResourceAmount(EJTSResourceType::Ore))));
	}
}

void UJTSShopWidget::RefreshDetail()
{
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, SelectedItemId);
	if (!IsValid(Definition))
	{
		DetailNameText->SetText(FText::FromString(TEXT("CATALOG EXPANSION READY")));
		DetailNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.48f, 0.72f, 0.94f, 1.0f)));
		DetailTagsText->SetText(FText::FromString(TEXT("MATERIALS  /  FUTURE SUPPLY")));
		DetailDescriptionText->SetText(FText::FromString(TEXT("This terminal reserves room for resource and material contracts. Collect Rock and Ore in the field, deposit them here, and use the current tool, weapon and wearable catalog to equip the expedition.")));
		DetailStatsText->SetText(FText::FromString(TEXT("TYPE  CATALOG PLACEHOLDER\nNEW DATA-DRIVEN ENTRIES APPEAR HERE AUTOMATICALLY.")));
		DetailCostText->SetText(FText::FromString(TEXT("NO PURCHASE SELECTED")));
		PurchaseButton->SetIsEnabled(false);
		PurchaseButton->SetBackgroundColor(FLinearColor(0.20f, 0.24f, 0.28f, 1.0f));
		return;
	}
	DetailNameText->SetText(Definition->DisplayName);
	DetailNameText->SetColorAndOpacity(FSlateColor(Definition->AccentColor));
	DetailTagsText->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s"), *FormatTags(SelectedItemId), *FormatOwnership(SelectedItemId))));
	DetailDescriptionText->SetText(Definition->Description);
	DetailStatsText->SetText(FText::FromString(FormatStats(SelectedItemId)));
	DetailCostText->SetText(FText::FromString(FormatCosts(SelectedItemId)));
	bool bAffordable = false;
	if (const AJTSShopTerminalActor* const Terminal = ActiveTerminal.Get())
	{
		bAffordable = true;
		for (const FJTSItemCost& Cost : Definition->ShopCosts)
		{
			bAffordable &= Terminal->GetSharedResourceAmount(Cost.ResourceType) >= Cost.Amount;
		}
	}
	PurchaseButton->SetIsEnabled(bAffordable);
	PurchaseButton->SetBackgroundColor(bAffordable ? FLinearColor(0.06f, 0.54f, 0.42f, 1.0f) : FLinearColor(0.20f, 0.24f, 0.28f, 1.0f));
}

FString UJTSShopWidget::FormatCosts(EJTSItemId ItemId) const
{
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
	if (!IsValid(Definition)) return TEXT("NO PRICE DATA");
	TArray<FString> Pieces;
	for (const FJTSItemCost& Cost : Definition->ShopCosts)
	{
		const TCHAR* Name = Cost.ResourceType == EJTSResourceType::Ore ? TEXT("ORE") : TEXT("ROCK");
		Pieces.Add(FString::Printf(TEXT("%d %s"), Cost.Amount, Name));
	}
	return FString::Printf(TEXT("COST  %s"), *FString::Join(Pieces, TEXT("  +  ")));
}

FString UJTSShopWidget::FormatTags(EJTSItemId ItemId) const
{
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
	if (!IsValid(Definition)) return TEXT("UTILITY");
	TArray<FString> Tags;
	for (const FName Tag : Definition->AffinityTags) Tags.Add(Tag.ToString().ToUpper());
	return FString::Join(Tags, TEXT("  /  "));
}

FString UJTSShopWidget::FormatOwnership(EJTSItemId ItemId) const
{
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
	const AJTSCharacter* const Player = Cast<AJTSCharacter>(GetOwningPlayerPawn());
	if (!IsValid(Definition) || !IsValid(Player))
	{
		return TEXT("STATUS: LINK PENDING");
	}

	if (Definition->IsWearable())
	{
		if (const UJTSPlayerEquipmentComponent* const Wearables = Player->GetEquipmentComponent())
		{
			return Wearables->GetWearable(Definition->WearableSlot).ItemId == ItemId
				? TEXT("STATUS: EQUIPPED")
				: TEXT("STATUS: NOT EQUIPPED");
		}
	}
	else if (const UJTSInventoryComponent* const Inventory = Player->GetInventoryComponent())
	{
		const int32 OwnedCount = Inventory->GetItemCount(ItemId);
		return OwnedCount > 0
			? FString::Printf(TEXT("STATUS: OWNED x%d"), OwnedCount)
			: TEXT("STATUS: NOT OWNED");
	}

	return TEXT("STATUS: NOT OWNED");
}

FString UJTSShopWidget::FormatStats(EJTSItemId ItemId) const
{
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
	if (!IsValid(Definition)) return FString();
	if (Definition->IsWearable())
	{
		return FString::Printf(TEXT("TYPE  WEARABLE\nCAPACITY  +%d INVENTORY SLOTS\nSLOT  BACKPACK"), Definition->InventoryCapacityBonus);
	}
	if (Definition->IsRangedWeapon())
	{
		return FString::Printf(TEXT("TYPE  RANGED WEAPON\nDAMAGE  %.2f\nFIRE INTERVAL  %.2fs\nMINING WORK / SHOT  %.2f\nAMMO  INFINITE PROTOTYPE CELL"), Definition->RangedDamage, Definition->RangedFireInterval, Definition->MiningWork);
	}
	return FString::Printf(TEXT("TYPE  %s\nMELEE DAMAGE  %.1f\nSWING INTERVAL  %.2fs\nMINING WORK / HIT  %.2f"), *GetShopCategoryLabel(Definition->PrimaryCategory == EJTSItemCategory::Mining ? EJTSShopCategory::Mining : EJTSShopCategory::Weapons), Definition->CombatDamage, Definition->MeleeAttackInterval, Definition->MiningWork);
}

void UJTSShopWidget::SelectItem(EJTSItemId ItemId)
{
	if (IsItemVisible(ItemId))
	{
		SelectedItemId = ItemId;
		RefreshCatalog();
		RefreshDetail();
	}
}

void UJTSShopWidget::SelectCategory(EJTSShopCategory Category)
{
	ActiveCategory = Category;
	if (!IsItemVisible(SelectedItemId))
	{
		SelectedItemId = EJTSItemId::None;
		for (const EJTSItemId ItemId : UJTSItemDefinitionLibrary::GetDefaultShopCatalog())
		{
			if (IsItemVisible(ItemId)) { SelectedItemId = ItemId; break; }
		}
	}
	RefreshCatalog();
	RefreshDetail();
}

void UJTSShopWidget::RequestPurchase()
{
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		Controller->ServerRequestShopPurchase(ActiveTerminal.Get(), SelectedItemId);
		StatusText->SetText(FText::FromString(TEXT("Purchase request sent to the expedition authority...")));
	}
}

void UJTSShopWidget::RequestDeposit()
{
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		Controller->ServerRequestDepositShopMaterials(ActiveTerminal.Get());
		StatusText->SetText(FText::FromString(TEXT("Depositing carried Rock and Ore into the shared ship wallet...")));
	}
}

void UJTSShopWidget::HandleAllCategory() { SelectCategory(EJTSShopCategory::All); }
void UJTSShopWidget::HandleWeaponsCategory() { SelectCategory(EJTSShopCategory::Weapons); }
void UJTSShopWidget::HandleMiningCategory() { SelectCategory(EJTSShopCategory::Mining); }
void UJTSShopWidget::HandleUtilityCategory() { SelectCategory(EJTSShopCategory::Utility); }
void UJTSShopWidget::HandleResourcesCategory() { SelectCategory(EJTSShopCategory::Resources); }
void UJTSShopWidget::HandleWearablesCategory() { SelectCategory(EJTSShopCategory::Wearables); }
void UJTSShopWidget::HandlePickaxeSelected() { SelectItem(EJTSItemId::Pickaxe); }
void UJTSShopWidget::HandleKnifeSelected() { SelectItem(EJTSItemId::Knife); }
void UJTSShopWidget::HandlePistolSelected() { SelectItem(EJTSItemId::Pistol); }
void UJTSShopWidget::HandleMachineGunSelected() { SelectItem(EJTSItemId::MachineGun); }
void UJTSShopWidget::HandleBackpackSelected() { SelectItem(EJTSItemId::Backpack); }
void UJTSShopWidget::HandlePurchaseClicked() { RequestPurchase(); }
void UJTSShopWidget::HandleDepositClicked() { RequestDeposit(); }
void UJTSShopWidget::HandleCloseClicked()
{
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer())) Controller->CloseSpaceShop();
}
