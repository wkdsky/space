// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSShopWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Ships/JTSSpacecraftActor.h"

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
		if (Parent == nullptr || Child == nullptr)
		{
			return nullptr;
		}

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

	UTextBlock* MakeText(
		UWidgetTree* Tree,
		const FName Name,
		const FString& Text,
		const float FontSize,
		const FLinearColor& Color,
		const ETextJustify::Type Justify = ETextJustify::Left)
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
			Result->SetAutoWrapText(true);
			Result->SetJustification(Justify);
		}
		return Result;
	}

	UBorder* MakeBorder(UWidgetTree* Tree, const FName Name, const FLinearColor& Color, const float Padding = 0.0f)
	{
		if (Tree == nullptr)
		{
			return nullptr;
		}

		UBorder* const Result = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		if (Result != nullptr)
		{
			Result->SetBrushColor(Color);
			Result->SetPadding(FMargin(Padding));
		}
		return Result;
	}

	FString ResourceLabel(const EJTSResourceType ResourceType)
	{
		switch (ResourceType)
		{
		case EJTSResourceType::Rock: return TEXT("ROCK");
		case EJTSResourceType::Ore: return TEXT("ORE");
		case EJTSResourceType::Fuel: return TEXT("FUEL");
		case EJTSResourceType::Water: return TEXT("WATER");
		case EJTSResourceType::Food: return TEXT("FOOD");
		case EJTSResourceType::Organic: return TEXT("ORGANIC");
		default: return TEXT("MATERIAL");
		}
	}

	FString ItemRoleLabel(const UJTSItemDefinition* Definition)
	{
		if (!IsValid(Definition))
		{
			return TEXT("SUPPLY");
		}
		if (Definition->IsRangedWeapon())
		{
			return TEXT("RANGED");
		}
		if (Definition->PrimaryCategory == EJTSItemCategory::Mining)
		{
			return TEXT("MINING");
		}
		return TEXT("FIELD GEAR");
	}

	FLinearColor ItemCardColor(const UJTSItemDefinition* Definition)
	{
		if (Definition->PrimaryCategory == EJTSItemCategory::Mining)
		{
			return FLinearColor(0.16f, 0.115f, 0.055f, 1.0f);
		}
		if (Definition->IsRangedWeapon())
		{
			return FLinearColor(0.055f, 0.11f, 0.17f, 1.0f);
		}
		return FLinearColor(0.09f, 0.105f, 0.12f, 1.0f);
	}
}

TSharedRef<SWidget> UJTSShopWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

FReply UJTSShopWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bShopOpen && InKeyEvent.GetKey() == EKeys::Tab)
	{
		ToggleShopPage();
		return FReply::Handled();
	}

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

bool UJTSShopWidget::OpenForSpacecraft(AJTSSpacecraftActor* Spacecraft)
{
	if (!IsValid(Spacecraft))
	{
		return false;
	}

	BuildWidgetTree();
	SetIsFocusable(true);
	ActiveSpacecraft = Spacecraft;
	LastDisplayedResourceAmounts.Reset();
	LastRequestedItem = EJTSItemId::None;
	bShowingAbilityPage = false;
	bAbilityCommitPending = false;
	ResetPendingAbilityAllocation();
	RefreshAccumulator = 0.0f;
	bShopOpen = true;
	if (StatusText != nullptr) StatusText->SetText(FText::GetEmpty());
	SetVisibility(ESlateVisibility::Visible);
	RefreshAll();
	return true;
}

void UJTSShopWidget::CloseShop()
{
	bShopOpen = false;
	LastRequestedItem = EJTSItemId::None;
	LastDisplayedResourceAmounts.Reset();
	bAbilityCommitPending = false;
	ResetPendingAbilityAllocation();
	ObservedProgressionRevision = INDEX_NONE;
	ActiveSpacecraft.Reset();
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UJTSShopWidget::IsShopOpen() const
{
	return bShopOpen && ActiveSpacecraft.IsValid();
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
		StatusText->SetText(FText::GetEmpty());
		break;
	case EJTSShopPurchaseResult::SucceededDropped:
		StatusText->SetText(FText::FromString(TEXT("PURCHASED / AIRLOCK DROP")));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.38f, 1.0f, 0.70f, 1.0f)));
		break;
	case EJTSShopPurchaseResult::InsufficientResources:
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("MISSING %s"), *FormatMissingCosts(LastRequestedItem))));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.46f, 0.34f, 1.0f)));
		break;
	case EJTSShopPurchaseResult::InvalidItem:
		StatusText->SetText(FText::FromString(TEXT("ITEM UNAVAILABLE")));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.46f, 0.34f, 1.0f)));
		break;
	case EJTSShopPurchaseResult::DeliveryFailed:
	default:
		StatusText->SetText(FText::FromString(TEXT("DELIVERY FAILED / MATERIALS RESTORED")));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.46f, 0.34f, 1.0f)));
		break;
	}

	RefreshAll();
}

void UJTSShopWidget::NotifyAbilityAllocationResult(bool bSucceeded)
{
	bAbilityCommitPending = false;
	if (AbilityStatusText != nullptr)
	{
		AbilityStatusText->SetText(FText::FromString(bSucceeded
			? TEXT("UPGRADES APPLIED")
			: TEXT("COULD NOT APPLY UPGRADES")));
		AbilityStatusText->SetColorAndOpacity(FSlateColor(bSucceeded
			? FLinearColor(0.38f, 1.0f, 0.70f, 1.0f)
			: FLinearColor(1.0f, 0.46f, 0.34f, 1.0f)));
	}
	if (bSucceeded)
	{
		ResetPendingAbilityAllocation();
	}
	RefreshAbilities();
}

void UJTSShopWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bShopOpen)
	{
		return;
	}
	if (!ActiveSpacecraft.IsValid())
	{
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
	if (RefreshAccumulator >= 0.20f)
	{
		RefreshAccumulator = 0.0f;
		RefreshAll();
	}
}

void UJTSShopWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || RootCanvas != nullptr)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShipSupplyRoot"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* const Dimmer = MakeBorder(WidgetTree, TEXT("ShipSupplyDimmer"), FLinearColor(0.003f, 0.008f, 0.018f, 0.84f));
	AddCanvas(RootCanvas, Dimmer, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);

	ShopFrame = MakeBorder(WidgetTree, TEXT("ShipSupplyFrame"), FLinearColor(0.025f, 0.060f, 0.105f, 0.995f));
	AddCanvas(RootCanvas, ShopFrame, FAnchors(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(1160.0f, 610.0f), FVector2D(0.5f, 0.5f));
	UCanvasPanel* const FrameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShipSupplyFrameCanvas"));
	ShopFrame->SetContent(FrameCanvas);

	AddCanvas(FrameCanvas, MakeText(WidgetTree, TEXT("ShipSupplyTitle"), TEXT("SHIP TERMINAL"), 28.0f, FLinearColor(0.92f, 0.97f, 1.0f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(30.0f, 24.0f), FVector2D(440.0f, 42.0f));
	AddCanvas(FrameCanvas, MakeText(WidgetTree, TEXT("ShipTabHint"), TEXT("TAB  SWITCH"), 12.0f, FLinearColor(0.55f, 0.69f, 0.82f, 1.0f), ETextJustify::Right), FAnchors(1.0f, 0.0f), FVector2D(-164.0f, 93.0f), FVector2D(150.0f, 20.0f), FVector2D(1.0f, 0.0f));

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipSupplyClose"));
	CloseButton->SetBackgroundColor(FLinearColor(0.16f, 0.22f, 0.30f, 1.0f));
	CloseButton->SetContent(MakeText(WidgetTree, TEXT("ShipSupplyCloseLabel"), TEXT("CLOSE  [E]"), 13.0f, FLinearColor::White, ETextJustify::Center));
	CloseButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleCloseClicked);
	AddCanvas(FrameCanvas, CloseButton, FAnchors(1.0f, 0.0f), FVector2D(-30.0f, 25.0f), FVector2D(120.0f, 36.0f), FVector2D(1.0f, 0.0f));

	SupplyTabButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipSupplyTab"));
	SupplyTabButton->SetBackgroundColor(FLinearColor(0.08f, 0.29f, 0.43f, 1.0f));
	SupplyTabButton->SetContent(MakeText(WidgetTree, TEXT("ShipSupplyTabLabel"), TEXT("SHOP"), 16.0f, FLinearColor::White, ETextJustify::Center));
	SupplyTabButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleSupplyTabClicked);
	AddCanvas(FrameCanvas, SupplyTabButton, FAnchors(0.0f, 0.0f), FVector2D(30.0f, 84.0f), FVector2D(168.0f, 40.0f));

	AbilityTabButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipAbilityTab"));
	AbilityTabButton->SetBackgroundColor(FLinearColor(0.10f, 0.16f, 0.25f, 1.0f));
	AbilityTabLabel = MakeText(WidgetTree, TEXT("ShipAbilityTabLabel"), TEXT("ABILITIES"), 16.0f, FLinearColor::White, ETextJustify::Center);
	AbilityTabButton->SetContent(AbilityTabLabel);
	AbilityTabButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleAbilityTabClicked);
	AddCanvas(FrameCanvas, AbilityTabButton, FAnchors(0.0f, 0.0f), FVector2D(206.0f, 84.0f), FVector2D(168.0f, 40.0f));

	CatalogPanel = MakeBorder(WidgetTree, TEXT("ShipSupplyGridFrame"), FLinearColor(0.014f, 0.040f, 0.075f, 1.0f));
	AddCanvas(FrameCanvas, CatalogPanel, FAnchors(0.0f, 0.0f), FVector2D(30.0f, 138.0f), FVector2D(1100.0f, 410.0f));
	UCanvasPanel* const CatalogCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShipSupplyCatalogCanvas"));
	CatalogPanel->SetContent(CatalogCanvas);
	AddCanvas(CatalogCanvas, MakeText(WidgetTree, TEXT("ShipResourcesLabel"), TEXT("SHIP RESOURCES"), 12.0f, FLinearColor(0.55f, 0.69f, 0.82f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(20.0f, 19.0f), FVector2D(160.0f, 22.0f));
	WalletText = MakeText(WidgetTree, TEXT("ShipSupplyWallet"), TEXT("ROCK 0   ORE 0"), 17.0f, FLinearColor(0.38f, 1.0f, 0.72f, 1.0f));
	AddCanvas(CatalogCanvas, WalletText, FAnchors(0.0f, 0.0f), FVector2D(184.0f, 14.0f), FVector2D(680.0f, 30.0f));
	DebugResourcesButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipDebugResources"));
	DebugResourcesButton->SetBackgroundColor(FLinearColor(0.13f, 0.22f, 0.27f, 1.0f));
	DebugResourcesButton->SetContent(MakeText(WidgetTree, TEXT("ShipDebugResourcesLabel"), TEXT("DEBUG +100"), 12.0f, FLinearColor::White, ETextJustify::Center));
	DebugResourcesButton->SetToolTipText(FText::FromString(TEXT("Add 100 of every ship resource")));
	DebugResourcesButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleDebugResourcesClicked);
	AddCanvas(CatalogCanvas, DebugResourcesButton, FAnchors(1.0f, 0.0f), FVector2D(-18.0f, 10.0f), FVector2D(198.0f, 34.0f), FVector2D(1.0f, 0.0f));
	UScrollBox* const CatalogScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ShipSupplyCatalogScroll"));
	AddCanvas(CatalogCanvas, CatalogScroll, FAnchors(0.0f, 0.0f), FVector2D(18.0f, 62.0f), FVector2D(1064.0f, 330.0f));
	CatalogGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("ShipSupplyGrid"));
	CatalogScroll->AddChild(CatalogGrid);

	AbilityPanel = MakeBorder(WidgetTree, TEXT("ShipAbilityPanel"), FLinearColor(0.014f, 0.040f, 0.075f, 1.0f));
	AddCanvas(FrameCanvas, AbilityPanel, FAnchors(0.0f, 0.0f), FVector2D(30.0f, 138.0f), FVector2D(1100.0f, 410.0f));
	UCanvasPanel* const AbilityCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShipAbilityCanvas"));
	AbilityPanel->SetContent(AbilityCanvas);
	AbilityProgressText = MakeText(WidgetTree, TEXT("ShipAbilityProgress"), TEXT("LEVEL 1  ·  XP 0 / 100"), 20.0f, FLinearColor(0.55f, 0.88f, 1.0f, 1.0f));
	AddCanvas(AbilityCanvas, AbilityProgressText, FAnchors(0.0f, 0.0f), FVector2D(22.0f, 16.0f), FVector2D(600.0f, 32.0f));
	AbilityPointsText = MakeText(WidgetTree, TEXT("ShipAbilityPoints"), TEXT("POINTS 0"), 20.0f, FLinearColor(0.38f, 1.0f, 0.72f, 1.0f), ETextJustify::Right);
	AddCanvas(AbilityCanvas, AbilityPointsText, FAnchors(1.0f, 0.0f), FVector2D(-22.0f, 16.0f), FVector2D(420.0f, 32.0f), FVector2D(1.0f, 0.0f));

	AbilityDetailTexts.Reset();
	AbilityRankTexts.Reset();
	AbilityDecreaseButtons.Reset();
	AbilityIncreaseButtons.Reset();
	auto AddAbilityCard = [this, AbilityCanvas](const EJTSPlayerAbility Ability, const FString& Title, const FName Name, const float Y)
	{
		UBorder* const Card = MakeBorder(WidgetTree, Name, FLinearColor(0.045f, 0.105f, 0.165f, 1.0f));
		AddCanvas(AbilityCanvas, Card, FAnchors(0.0f, 0.0f), FVector2D(20.0f, Y), FVector2D(1060.0f, 84.0f));
		UCanvasPanel* const CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("%sCanvas"), *Name.ToString()));
		Card->SetContent(CardCanvas);
		AddCanvas(CardCanvas, MakeText(WidgetTree, *FString::Printf(TEXT("%sTitle"), *Name.ToString()), Title, 18.0f, FLinearColor::White), FAnchors(0.0f, 0.0f), FVector2D(24.0f, 12.0f), FVector2D(570.0f, 28.0f));
		UTextBlock* const Detail = MakeText(WidgetTree, *FString::Printf(TEXT("%sDetail"), *Name.ToString()), TEXT(""), 15.0f, FLinearColor(0.68f, 0.79f, 0.90f, 1.0f));
		AddCanvas(CardCanvas, Detail, FAnchors(0.0f, 0.0f), FVector2D(24.0f, 46.0f), FVector2D(650.0f, 25.0f));
		UButton* const Decrease = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("%sDecrease"), *Name.ToString()));
		Decrease->SetBackgroundColor(FLinearColor(0.12f, 0.18f, 0.27f, 1.0f));
		Decrease->SetContent(MakeText(WidgetTree, *FString::Printf(TEXT("%sDecreaseLabel"), *Name.ToString()), TEXT("−"), 18.0f, FLinearColor::White, ETextJustify::Center));
		UTextBlock* const Rank = MakeText(WidgetTree, *FString::Printf(TEXT("%sRank"), *Name.ToString()), TEXT("0 / 5"), 16.0f, FLinearColor(0.55f, 0.90f, 1.0f, 1.0f), ETextJustify::Center);
		AddCanvas(CardCanvas, Rank, FAnchors(0.0f, 0.0f), FVector2D(768.0f, 28.0f), FVector2D(154.0f, 26.0f));
		AddCanvas(CardCanvas, Decrease, FAnchors(0.0f, 0.0f), FVector2D(930.0f, 21.0f), FVector2D(52.0f, 42.0f));
		UButton* const Increase = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("%sIncrease"), *Name.ToString()));
		Increase->SetBackgroundColor(FLinearColor(0.08f, 0.34f, 0.30f, 1.0f));
		Increase->SetContent(MakeText(WidgetTree, *FString::Printf(TEXT("%sIncreaseLabel"), *Name.ToString()), TEXT("+"), 18.0f, FLinearColor::White, ETextJustify::Center));
		AddCanvas(CardCanvas, Increase, FAnchors(0.0f, 0.0f), FVector2D(996.0f, 21.0f), FVector2D(52.0f, 42.0f));

		switch (Ability)
		{
		case EJTSPlayerAbility::InventorySlots:
			Decrease->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleInventorySlotsDecrease);
			Increase->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleInventorySlotsIncrease);
			break;
		case EJTSPlayerAbility::StackLimit:
			Decrease->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleStackLimitDecrease);
			Increase->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleStackLimitIncrease);
			break;
		case EJTSPlayerAbility::RunSpeed:
			Decrease->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleRunSpeedDecrease);
			Increase->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleRunSpeedIncrease);
			break;
		default:
			break;
		}
		AbilityDetailTexts.Add(Detail);
		AbilityRankTexts.Add(Rank);
		AbilityDecreaseButtons.Add(Decrease);
		AbilityIncreaseButtons.Add(Increase);
	};
	AddAbilityCard(EJTSPlayerAbility::InventorySlots, TEXT("INVENTORY SLOTS"), TEXT("ShipAbilityCargo"), 66.0f);
	AddAbilityCard(EJTSPlayerAbility::StackLimit, TEXT("STACK SIZE"), TEXT("ShipAbilityStack"), 162.0f);
	AddAbilityCard(EJTSPlayerAbility::RunSpeed, TEXT("RUN SPEED"), TEXT("ShipAbilitySpeed"), 258.0f);

	ConfirmAbilitiesButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipAbilityConfirm"));
	ConfirmAbilitiesButton->SetBackgroundColor(FLinearColor(0.08f, 0.40f, 0.29f, 1.0f));
	ConfirmAbilitiesButton->SetContent(MakeText(WidgetTree, TEXT("ShipAbilityConfirmLabel"), TEXT("APPLY"), 14.0f, FLinearColor::White, ETextJustify::Center));
	ConfirmAbilitiesButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleConfirmAbilitiesClicked);
	AddCanvas(AbilityCanvas, ConfirmAbilitiesButton, FAnchors(0.0f, 0.0f), FVector2D(864.0f, 354.0f), FVector2D(216.0f, 42.0f));
	ResetAbilitiesButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipAbilityReset"));
	ResetAbilitiesButton->SetBackgroundColor(FLinearColor(0.13f, 0.18f, 0.27f, 1.0f));
	ResetAbilitiesButton->SetContent(MakeText(WidgetTree, TEXT("ShipAbilityResetLabel"), TEXT("UNDO"), 14.0f, FLinearColor::White, ETextJustify::Center));
	ResetAbilitiesButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleResetAbilitiesClicked);
	AddCanvas(AbilityCanvas, ResetAbilitiesButton, FAnchors(0.0f, 0.0f), FVector2D(682.0f, 354.0f), FVector2D(170.0f, 42.0f));
	DebugLevelsButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipDebugLevels"));
	DebugLevelsButton->SetBackgroundColor(FLinearColor(0.13f, 0.22f, 0.27f, 1.0f));
	DebugLevelsButton->SetContent(MakeText(WidgetTree, TEXT("ShipDebugLevelsLabel"), TEXT("DEBUG +10 LVL"), 13.0f, FLinearColor::White, ETextJustify::Center));
	DebugLevelsButton->SetToolTipText(FText::FromString(TEXT("+10 levels and +10 ability points")));
	DebugLevelsButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleDebugLevelsClicked);
	AddCanvas(AbilityCanvas, DebugLevelsButton, FAnchors(0.0f, 0.0f), FVector2D(20.0f, 354.0f), FVector2D(202.0f, 42.0f));
	AbilityStatusText = MakeText(WidgetTree, TEXT("ShipAbilityStatus"), TEXT("1 POINT PER RANK"), 13.0f, FLinearColor(0.72f, 0.82f, 0.92f, 1.0f), ETextJustify::Center);
	AddCanvas(FrameCanvas, AbilityStatusText, FAnchors(0.5f, 1.0f), FVector2D(0.0f, -19.0f), FVector2D(1060.0f, 24.0f), FVector2D(0.5f, 1.0f));

	StatusText = MakeText(WidgetTree, TEXT("ShipSupplyStatus"), TEXT(""), 13.0f, FLinearColor(0.64f, 0.78f, 0.92f, 1.0f), ETextJustify::Center);
	AddCanvas(FrameCanvas, StatusText, FAnchors(0.5f, 1.0f), FVector2D(0.0f, -43.0f), FVector2D(1060.0f, 24.0f), FVector2D(0.5f, 1.0f));

	SetVisibility(ESlateVisibility::Collapsed);
}

void UJTSShopWidget::RefreshAll()
{
	const bool bResourcesChanged = RefreshWallet();
	if (bResourcesChanged || (CatalogGrid != nullptr && CatalogGrid->GetChildrenCount() == 0))
	{
		RefreshCatalog();
	}
	RefreshAbilities();
	RefreshPageVisibility();
}

void UJTSShopWidget::RefreshAbilities()
{
	const AJTSPlayerState* const PlayerState = GetOwningPlayer() != nullptr
		? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>()
		: nullptr;
	if (PlayerState == nullptr)
	{
		return;
	}

	if (ObservedProgressionRevision != PlayerState->GetProgressionRevision())
	{
		PendingAbilityAllocation = FJTSAbilityAllocation();
		ObservedProgressionRevision = PlayerState->GetProgressionRevision();
	}

	const int32 PendingCost = PendingAbilityAllocation.GetTotalPointCost();
	const int32 ExperienceNeeded = PlayerState->GetExperienceRequiredForNextLevel();
	if (AbilityTabLabel != nullptr)
	{
		AbilityTabLabel->SetText(FText::FromString(PlayerState->GetUnspentAbilityPoints() > 0
			? FString::Printf(TEXT("ABILITIES  ·  %d"), PlayerState->GetUnspentAbilityPoints())
			: TEXT("ABILITIES")));
	}
	if (AbilityProgressText != nullptr)
	{
		const FString ExperienceLabel = ExperienceNeeded > 0
			? FString::Printf(TEXT("XP %d / %d"), PlayerState->GetExperienceInCurrentLevel(), ExperienceNeeded)
			: TEXT("LEVEL CAP");
		AbilityProgressText->SetText(FText::FromString(FString::Printf(
			TEXT("LEVEL %d  ·  %s"), PlayerState->GetProgressionLevel(), *ExperienceLabel)));
	}
	if (AbilityPointsText != nullptr)
	{
		AbilityPointsText->SetText(FText::FromString(PendingCost > 0
			? FString::Printf(TEXT("POINTS %d  ·  PENDING %d"), PlayerState->GetUnspentAbilityPoints(), PendingCost)
			: FString::Printf(TEXT("POINTS %d"), PlayerState->GetUnspentAbilityPoints())));
	}

	const TArray<EJTSPlayerAbility, TInlineAllocator<3>> Abilities = {
		EJTSPlayerAbility::InventorySlots,
		EJTSPlayerAbility::StackLimit,
		EJTSPlayerAbility::RunSpeed };
	for (int32 Index = 0; Index < Abilities.Num(); ++Index)
	{
		const EJTSPlayerAbility Ability = Abilities[Index];
		const int32 CurrentRank = PlayerState->GetAbilityRank(Ability);
		const int32 PendingRank = GetPendingAbilityRank(Ability);
		if (AbilityDetailTexts.IsValidIndex(Index) && AbilityDetailTexts[Index] != nullptr)
		{
			AbilityDetailTexts[Index]->SetText(FText::FromString(BuildAbilityDescription(Ability)));
		}
		if (AbilityRankTexts.IsValidIndex(Index) && AbilityRankTexts[Index] != nullptr)
		{
			AbilityRankTexts[Index]->SetText(FText::FromString(CurrentRank == PendingRank
				? FString::Printf(TEXT("%d / %d"), CurrentRank, FJTSPlayerProgressionRules::MaximumAbilityRank)
				: FString::Printf(TEXT("%d → %d / %d"), CurrentRank, PendingRank, FJTSPlayerProgressionRules::MaximumAbilityRank)));
		}
		if (AbilityDecreaseButtons.IsValidIndex(Index) && AbilityDecreaseButtons[Index] != nullptr)
		{
			AbilityDecreaseButtons[Index]->SetIsEnabled(!bAbilityCommitPending && PendingRank > CurrentRank);
		}
		if (AbilityIncreaseButtons.IsValidIndex(Index) && AbilityIncreaseButtons[Index] != nullptr)
		{
			AbilityIncreaseButtons[Index]->SetIsEnabled(
				!bAbilityCommitPending
				&& PendingRank < FJTSPlayerProgressionRules::MaximumAbilityRank
				&& PendingCost < PlayerState->GetUnspentAbilityPoints());
		}
	}

	if (ConfirmAbilitiesButton != nullptr)
	{
		ConfirmAbilitiesButton->SetIsEnabled(!bAbilityCommitPending
			&& PendingCost > 0 && PendingCost <= PlayerState->GetUnspentAbilityPoints());
	}
	if (ResetAbilitiesButton != nullptr)
	{
		ResetAbilitiesButton->SetIsEnabled(!bAbilityCommitPending && PendingCost > 0);
	}
	if (DebugLevelsButton != nullptr)
	{
		DebugLevelsButton->SetIsEnabled(PlayerState->GetProgressionLevel() + 10 <= FJTSPlayerProgressionRules::MaximumLevel);
	}
}

void UJTSShopWidget::RefreshPageVisibility()
{
	if (CatalogPanel != nullptr)
	{
		CatalogPanel->SetVisibility(bShowingAbilityPage ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (AbilityPanel != nullptr)
	{
		AbilityPanel->SetVisibility(bShowingAbilityPage ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (StatusText != nullptr)
	{
		StatusText->SetVisibility(bShowingAbilityPage || StatusText->GetText().IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (AbilityStatusText != nullptr)
	{
		AbilityStatusText->SetVisibility(bShowingAbilityPage ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (SupplyTabButton != nullptr)
	{
		SupplyTabButton->SetBackgroundColor(bShowingAbilityPage
			? FLinearColor(0.10f, 0.16f, 0.25f, 1.0f)
			: FLinearColor(0.08f, 0.29f, 0.43f, 1.0f));
	}
	if (AbilityTabButton != nullptr)
	{
		AbilityTabButton->SetBackgroundColor(bShowingAbilityPage
			? FLinearColor(0.08f, 0.29f, 0.43f, 1.0f)
			: FLinearColor(0.10f, 0.16f, 0.25f, 1.0f));
	}
}

void UJTSShopWidget::ToggleShopPage()
{
	bShowingAbilityPage = !bShowingAbilityPage;
	RefreshAll();
}

void UJTSShopWidget::ResetPendingAbilityAllocation()
{
	PendingAbilityAllocation = FJTSAbilityAllocation();
	if (const AJTSPlayerState* const PlayerState = GetOwningPlayer() != nullptr
		? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>()
		: nullptr)
	{
		ObservedProgressionRevision = PlayerState->GetProgressionRevision();
	}
}

bool UJTSShopWidget::AdjustPendingAbility(EJTSPlayerAbility Ability, int32 Delta)
{
	const AJTSPlayerState* const PlayerState = GetOwningPlayer() != nullptr
		? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>()
		: nullptr;
	if (PlayerState == nullptr || bAbilityCommitPending || Delta == 0)
	{
		return false;
	}

	int32* PendingRanks = nullptr;
	switch (Ability)
	{
	case EJTSPlayerAbility::InventorySlots: PendingRanks = &PendingAbilityAllocation.InventorySlotRanks; break;
	case EJTSPlayerAbility::StackLimit: PendingRanks = &PendingAbilityAllocation.StackLimitRanks; break;
	case EJTSPlayerAbility::RunSpeed: PendingRanks = &PendingAbilityAllocation.RunSpeedRanks; break;
	default: return false;
	}

	const int32 CurrentRank = PlayerState->GetAbilityRank(Ability);
	const int32 TargetRank = CurrentRank + *PendingRanks + Delta;
	if (TargetRank < CurrentRank || TargetRank > FJTSPlayerProgressionRules::MaximumAbilityRank)
	{
		return false;
	}
	if (Delta > 0 && PendingAbilityAllocation.GetTotalPointCost() >= PlayerState->GetUnspentAbilityPoints())
	{
		return false;
	}

	*PendingRanks += Delta;
	if (AbilityStatusText != nullptr)
	{
		AbilityStatusText->SetText(FText::FromString(TEXT("CHANGES READY  ·  APPLY TO CONFIRM")));
		AbilityStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.82f, 0.92f, 1.0f)));
	}
	RefreshAbilities();
	return true;
}

int32 UJTSShopWidget::GetPendingAbilityRank(EJTSPlayerAbility Ability) const
{
	const AJTSPlayerState* const PlayerState = GetOwningPlayer() != nullptr
		? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>()
		: nullptr;
	if (PlayerState == nullptr)
	{
		return 0;
	}

	int32 PendingRanks = 0;
	switch (Ability)
	{
	case EJTSPlayerAbility::InventorySlots: PendingRanks = PendingAbilityAllocation.InventorySlotRanks; break;
	case EJTSPlayerAbility::StackLimit: PendingRanks = PendingAbilityAllocation.StackLimitRanks; break;
	case EJTSPlayerAbility::RunSpeed: PendingRanks = PendingAbilityAllocation.RunSpeedRanks; break;
	default: break;
	}
	return FMath::Clamp(PlayerState->GetAbilityRank(Ability) + PendingRanks, 0, FJTSPlayerProgressionRules::MaximumAbilityRank);
}

FString UJTSShopWidget::BuildAbilityDescription(EJTSPlayerAbility Ability) const
{
	const AJTSPlayerState* const PlayerState = GetOwningPlayer() != nullptr
		? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>()
		: nullptr;
	const int32 CurrentRank = PlayerState != nullptr ? PlayerState->GetAbilityRank(Ability) : 0;
	const int32 PendingRank = GetPendingAbilityRank(Ability);
	const int32 PreviewRank = PendingRank > CurrentRank
		? PendingRank
		: FMath::Min(CurrentRank + 1, FJTSPlayerProgressionRules::MaximumAbilityRank);
	const bool bMaxRank = CurrentRank >= FJTSPlayerProgressionRules::MaximumAbilityRank;
	switch (Ability)
	{
	case EJTSPlayerAbility::InventorySlots:
		return bMaxRank
			? FString::Printf(TEXT("MAX  ·  +%d bonus slots"), FJTSPlayerProgressionRules::GetInventorySlotBonus(CurrentRank))
			: FString::Printf(TEXT("Bonus slots  +%d → +%d"),
				FJTSPlayerProgressionRules::GetInventorySlotBonus(CurrentRank),
				FJTSPlayerProgressionRules::GetInventorySlotBonus(PreviewRank));
	case EJTSPlayerAbility::StackLimit:
		return bMaxRank
			? FString::Printf(TEXT("MAX  ·  %d items per slot"), FJTSPlayerProgressionRules::GetStackLimit(CurrentRank))
			: FString::Printf(TEXT("Items per slot  %d → %d"),
				FJTSPlayerProgressionRules::GetStackLimit(CurrentRank),
				FJTSPlayerProgressionRules::GetStackLimit(PreviewRank));
	case EJTSPlayerAbility::RunSpeed:
		return bMaxRank
			? FString::Printf(TEXT("MAX  ·  +%d%% speed"), FJTSPlayerProgressionRules::GetRunSpeedBonusPercent(CurrentRank))
			: FString::Printf(TEXT("Speed bonus  +%d%% → +%d%%"),
				FJTSPlayerProgressionRules::GetRunSpeedBonusPercent(CurrentRank),
				FJTSPlayerProgressionRules::GetRunSpeedBonusPercent(PreviewRank));
	default:
		return FString();
	}
}

void UJTSShopWidget::RefreshCatalog()
{
	if (CatalogGrid == nullptr || WidgetTree == nullptr)
	{
		return;
	}

	CatalogGrid->ClearChildren();
	CatalogGrid->SetSlotPadding(FMargin(6.0f));
	int32 CatalogIndex = 0;
	for (const EJTSItemId ItemId : UJTSItemDefinitionLibrary::GetDefaultShopCatalog())
	{
		const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
		if (!IsValid(Definition) || !Definition->IsShopPurchasable())
		{
			continue;
		}

		const bool bAffordable = CanAfford(ItemId);
		UBorder* const Card = MakeBorder(WidgetTree,
			*FString::Printf(TEXT("ShipSupplyCard_%d"), static_cast<int32>(ItemId)), ItemCardColor(Definition));
		Card->SetToolTipText(BuildItemTooltip(ItemId));

		UCanvasPanel* const CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), *FString::Printf(TEXT("ShipSupplyContents_%d"), static_cast<int32>(ItemId)));
		Card->SetContent(CardCanvas);
		AddCanvas(CardCanvas, MakeBorder(WidgetTree,
			*FString::Printf(TEXT("ShipSupplyAccent_%d"), static_cast<int32>(ItemId)),
			Definition->AccentColor.CopyWithNewOpacity(0.95f)),
			FAnchors(0.0f, 0.0f), FVector2D(14.0f, 16.0f), FVector2D(4.0f, 112.0f));
		AddCanvas(CardCanvas, MakeText(WidgetTree,
			*FString::Printf(TEXT("ShipSupplyRole_%d"), static_cast<int32>(ItemId)),
			ItemRoleLabel(Definition), 12.0f, Definition->AccentColor),
			FAnchors(0.0f, 0.0f), FVector2D(30.0f, 17.0f), FVector2D(250.0f, 20.0f));
		AddCanvas(CardCanvas, MakeText(WidgetTree,
			*FString::Printf(TEXT("ShipSupplyName_%d"), static_cast<int32>(ItemId)),
			Definition->DisplayName.ToString(), 19.0f, FLinearColor::White),
			FAnchors(0.0f, 0.0f), FVector2D(30.0f, 47.0f), FVector2D(270.0f, 32.0f));
		AddCanvas(CardCanvas, MakeText(WidgetTree,
			*FString::Printf(TEXT("ShipSupplyCost_%d"), static_cast<int32>(ItemId)),
			FormatCosts(ItemId), 14.0f, FLinearColor(0.72f, 0.82f, 0.90f, 1.0f)),
			FAnchors(0.0f, 0.0f), FVector2D(30.0f, 104.0f), FVector2D(195.0f, 26.0f));
		if (bAffordable)
		{
			UButton* const BuyButton = WidgetTree->ConstructWidget<UButton>(
				UButton::StaticClass(), *FString::Printf(TEXT("ShipSupplyBuy_%d"), static_cast<int32>(ItemId)));
			BuyButton->SetBackgroundColor(FLinearColor(0.10f, 0.34f, 0.30f, 1.0f));
			BuyButton->SetContent(MakeText(WidgetTree,
				*FString::Printf(TEXT("ShipSupplyBuyLabel_%d"), static_cast<int32>(ItemId)),
				TEXT("BUY"), 13.0f, FLinearColor::White, ETextJustify::Center));
			AddCanvas(CardCanvas, BuyButton, FAnchors(1.0f, 0.0f), FVector2D(-16.0f, 98.0f),
				FVector2D(88.0f, 34.0f), FVector2D(1.0f, 0.0f));
			switch (ItemId)
			{
			case EJTSItemId::Pickaxe: BuyButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandlePickaxeBuy); break;
			case EJTSItemId::Knife: BuyButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleKnifeBuy); break;
			case EJTSItemId::Pistol: BuyButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandlePistolBuy); break;
			case EJTSItemId::MachineGun: BuyButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleMachineGunBuy); break;
			case EJTSItemId::Sniper: BuyButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleSniperBuy); break;
			default: break;
			}
		}

		USizeBox* const Cell = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), *FString::Printf(TEXT("ShipSupplyCell_%d"), static_cast<int32>(ItemId)));
		Cell->SetWidthOverride(330.0f);
		Cell->SetHeightOverride(150.0f);
		Cell->SetContent(Card);
		CatalogGrid->AddChildToUniformGrid(Cell, CatalogIndex / 3, CatalogIndex % 3);
		++CatalogIndex;
	}
}

bool UJTSShopWidget::RefreshWallet()
{
	const AJTSSpacecraftActor* const Spacecraft = ActiveSpacecraft.Get();
	if (!IsValid(Spacecraft))
	{
		return false;
	}

	TMap<EJTSResourceType, int32> CurrentResourceAmounts;
	TArray<EJTSResourceType> ResourceOrder;
	for (const EJTSItemId ItemId : UJTSItemDefinitionLibrary::GetDefaultShopCatalog())
	{
		const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
		if (!IsValid(Definition))
		{
			continue;
		}

		for (const FJTSItemCost& Cost : Definition->ShopCosts)
		{
			if (Cost.Amount > 0 && !CurrentResourceAmounts.Contains(Cost.ResourceType))
			{
				ResourceOrder.Add(Cost.ResourceType);
				CurrentResourceAmounts.Add(Cost.ResourceType, Spacecraft->GetResourceAmount(Cost.ResourceType));
			}
		}
	}

	bool bResourcesChanged = CurrentResourceAmounts.Num() != LastDisplayedResourceAmounts.Num();
	if (!bResourcesChanged)
	{
		for (const TPair<EJTSResourceType, int32>& ResourceAmount : CurrentResourceAmounts)
		{
			const int32* const PreviousAmount = LastDisplayedResourceAmounts.Find(ResourceAmount.Key);
			if (PreviousAmount == nullptr || *PreviousAmount != ResourceAmount.Value)
			{
				bResourcesChanged = true;
				break;
			}
		}
	}
	if (WalletText != nullptr)
	{
		TArray<FString> WalletEntries;
		for (const EJTSResourceType ResourceType : ResourceOrder)
		{
			WalletEntries.Add(FString::Printf(TEXT("%s %d"), *ResourceLabel(ResourceType),
				CurrentResourceAmounts.FindRef(ResourceType)));
		}
		WalletText->SetText(FText::FromString(WalletEntries.IsEmpty()
			? TEXT("NO MATERIAL COST")
			: FString::Join(WalletEntries, TEXT("     "))));
	}
	LastDisplayedResourceAmounts = MoveTemp(CurrentResourceAmounts);

	return bResourcesChanged;
}

bool UJTSShopWidget::CanAfford(EJTSItemId ItemId) const
{
	const AJTSSpacecraftActor* const Spacecraft = ActiveSpacecraft.Get();
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
	if (!IsValid(Spacecraft) || !IsValid(Definition) || !Definition->IsShopPurchasable())
	{
		return false;
	}

	for (const FJTSItemCost& Cost : Definition->ShopCosts)
	{
		if (Cost.Amount > 0 && Spacecraft->GetResourceAmount(Cost.ResourceType) < Cost.Amount)
		{
			return false;
		}
	}
	return true;
}

FString UJTSShopWidget::FormatCosts(EJTSItemId ItemId) const
{
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
	if (!IsValid(Definition))
	{
		return TEXT("NO COST");
	}

	TArray<FString> Costs;
	for (const FJTSItemCost& Cost : Definition->ShopCosts)
	{
		if (Cost.Amount > 0)
		{
			Costs.Add(FString::Printf(TEXT("%s %d"), *ResourceLabel(Cost.ResourceType), Cost.Amount));
		}
	}
	return FString::Join(Costs, TEXT("   ·   "));
}

FString UJTSShopWidget::FormatMissingCosts(EJTSItemId ItemId) const
{
	const AJTSSpacecraftActor* const Spacecraft = ActiveSpacecraft.Get();
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
	if (!IsValid(Spacecraft) || !IsValid(Definition))
	{
		return TEXT("LINK");
	}

	TArray<FString> Missing;
	for (const FJTSItemCost& Cost : Definition->ShopCosts)
	{
		const int32 Shortfall = FMath::Max(0, Cost.Amount - Spacecraft->GetResourceAmount(Cost.ResourceType));
		if (Shortfall > 0)
		{
			Missing.Add(FString::Printf(TEXT("%s +%d"), *ResourceLabel(Cost.ResourceType), Shortfall));
		}
	}
	return Missing.IsEmpty() ? TEXT("NONE") : FString::Join(Missing, TEXT("  /  "));
}

FText UJTSShopWidget::BuildItemTooltip(EJTSItemId ItemId) const
{
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
	if (!IsValid(Definition))
	{
		return FText::GetEmpty();
	}

	const FString Availability = CanAfford(ItemId)
		? TEXT("READY")
		: FString::Printf(TEXT("MISSING %s"), *FormatMissingCosts(ItemId));
	return FText::FromString(FString::Printf(
		TEXT("%s\n\n%s\n\n%s\n%s"),
		*Definition->DisplayName.ToString(),
		*Definition->Description.ToString(),
		*FormatCosts(ItemId),
		*Availability));
}

void UJTSShopWidget::RequestPurchase(EJTSItemId ItemId)
{
	if (!CanAfford(ItemId)) return;
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		if (ActiveSpacecraft.IsValid())
		{
			LastRequestedItem = ItemId;
			Controller->ServerRequestShopPurchase(ActiveSpacecraft.Get(), ItemId);
		}
	}
}

void UJTSShopWidget::HandlePickaxeBuy() { RequestPurchase(EJTSItemId::Pickaxe); }
void UJTSShopWidget::HandleKnifeBuy() { RequestPurchase(EJTSItemId::Knife); }
void UJTSShopWidget::HandlePistolBuy() { RequestPurchase(EJTSItemId::Pistol); }
void UJTSShopWidget::HandleMachineGunBuy() { RequestPurchase(EJTSItemId::MachineGun); }
void UJTSShopWidget::HandleSniperBuy() { RequestPurchase(EJTSItemId::Sniper); }
void UJTSShopWidget::HandleDebugResourcesClicked()
{
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		if (ActiveSpacecraft.IsValid()) Controller->ServerRequestShopDebugResources(ActiveSpacecraft.Get());
	}
}
void UJTSShopWidget::HandleDebugLevelsClicked()
{
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		if (ActiveSpacecraft.IsValid()) Controller->ServerRequestDebugAbilityLevels(ActiveSpacecraft.Get());
	}
}
void UJTSShopWidget::HandleSupplyTabClicked() { bShowingAbilityPage = false; RefreshAll(); }
void UJTSShopWidget::HandleAbilityTabClicked() { bShowingAbilityPage = true; RefreshAll(); }
void UJTSShopWidget::HandleInventorySlotsDecrease() { AdjustPendingAbility(EJTSPlayerAbility::InventorySlots, -1); }
void UJTSShopWidget::HandleInventorySlotsIncrease() { AdjustPendingAbility(EJTSPlayerAbility::InventorySlots, 1); }
void UJTSShopWidget::HandleStackLimitDecrease() { AdjustPendingAbility(EJTSPlayerAbility::StackLimit, -1); }
void UJTSShopWidget::HandleStackLimitIncrease() { AdjustPendingAbility(EJTSPlayerAbility::StackLimit, 1); }
void UJTSShopWidget::HandleRunSpeedDecrease() { AdjustPendingAbility(EJTSPlayerAbility::RunSpeed, -1); }
void UJTSShopWidget::HandleRunSpeedIncrease() { AdjustPendingAbility(EJTSPlayerAbility::RunSpeed, 1); }

void UJTSShopWidget::HandleConfirmAbilitiesClicked()
{
	if (bAbilityCommitPending || PendingAbilityAllocation.GetTotalPointCost() <= 0)
	{
		return;
	}
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		bAbilityCommitPending = true;
		Controller->ServerCommitAbilityAllocation(PendingAbilityAllocation);
		RefreshAbilities();
	}
}

void UJTSShopWidget::HandleResetAbilitiesClicked()
{
	if (bAbilityCommitPending)
	{
		return;
	}
	ResetPendingAbilityAllocation();
	if (AbilityStatusText != nullptr)
	{
		AbilityStatusText->SetText(FText::FromString(TEXT("CHANGES UNDONE")));
		AbilityStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.82f, 0.92f, 1.0f)));
	}
	RefreshAbilities();
}

void UJTSShopWidget::HandleCloseClicked()
{
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		Controller->CloseSpaceShop();
	}
}
