// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSShopWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Player/JTSPlayerController.h"
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

	void AddVertical(UVerticalBox* Parent, UWidget* Child, const FMargin Padding)
	{
		if (Parent == nullptr || Child == nullptr)
		{
			return;
		}

		if (UVerticalBoxSlot* const Slot = Parent->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(Padding);
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
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
		if (Definition->IsWearable())
		{
			return TEXT("WEARABLE");
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
	RefreshAccumulator = 0.0f;
	bShopOpen = true;
	SetVisibility(ESlateVisibility::Visible);
	RefreshAll();
	return true;
}

void UJTSShopWidget::CloseShop()
{
	bShopOpen = false;
	LastRequestedItem = EJTSItemId::None;
	LastDisplayedResourceAmounts.Reset();
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
		StatusText->SetText(FText::FromString(TEXT("PURCHASED")));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.38f, 1.0f, 0.70f, 1.0f)));
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

	UBorder* const Dimmer = MakeBorder(WidgetTree, TEXT("ShipSupplyDimmer"), FLinearColor(0.003f, 0.008f, 0.018f, 0.91f));
	AddCanvas(RootCanvas, Dimmer, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);

	ShopFrame = MakeBorder(WidgetTree, TEXT("ShipSupplyFrame"), FLinearColor(0.025f, 0.060f, 0.105f, 0.995f), 18.0f);
	AddCanvas(RootCanvas, ShopFrame, FAnchors(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(1160.0f, 600.0f), FVector2D(0.5f, 0.5f));
	UCanvasPanel* const FrameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShipSupplyFrameCanvas"));
	ShopFrame->SetContent(FrameCanvas);

	AddCanvas(FrameCanvas, MakeText(WidgetTree, TEXT("ShipSupplyTitle"), TEXT("SHIP SUPPLY"), 31.0f, FLinearColor(0.92f, 0.97f, 1.0f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(24.0f, 28.0f), FVector2D(450.0f, 48.0f));
	AddCanvas(FrameCanvas, MakeText(WidgetTree, TEXT("ShipSupplyTransfer"), TEXT("AUTO-TRANSFER"), 12.0f, FLinearColor(0.50f, 0.64f, 0.78f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(27.0f, 76.0f), FVector2D(180.0f, 20.0f));

	WalletText = MakeText(WidgetTree, TEXT("ShipSupplyWallet"), TEXT("ROCK 0   ORE 0"), 18.0f, FLinearColor(0.38f, 1.0f, 0.72f, 1.0f), ETextJustify::Right);
	AddCanvas(FrameCanvas, WalletText, FAnchors(1.0f, 0.0f), FVector2D(-76.0f, 39.0f), FVector2D(310.0f, 40.0f), FVector2D(1.0f, 0.0f));

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipSupplyClose"));
	CloseButton->SetBackgroundColor(FLinearColor(0.16f, 0.22f, 0.30f, 1.0f));
	CloseButton->SetContent(MakeText(WidgetTree, TEXT("ShipSupplyCloseLabel"), TEXT("CLOSE [E]"), 13.0f, FLinearColor::White, ETextJustify::Center));
	CloseButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleCloseClicked);
	AddCanvas(FrameCanvas, CloseButton, FAnchors(1.0f, 0.0f), FVector2D(-24.0f, 23.0f), FVector2D(112.0f, 34.0f), FVector2D(1.0f, 0.0f));

	UBorder* const CatalogPanel = MakeBorder(WidgetTree, TEXT("ShipSupplyGridFrame"), FLinearColor(0.014f, 0.040f, 0.075f, 1.0f), 12.0f);
	AddCanvas(FrameCanvas, CatalogPanel, FAnchors(0.0f, 0.0f), FVector2D(24.0f, 118.0f), FVector2D(1112.0f, 378.0f));
	CatalogWrap = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("ShipSupplyGrid"));
	CatalogWrap->SetInnerSlotPadding(FVector2D(10.0f, 10.0f));
	CatalogPanel->SetContent(CatalogWrap);

	StatusText = MakeText(WidgetTree, TEXT("ShipSupplyStatus"), TEXT("SELECT A SUPPLY"), 13.0f, FLinearColor(0.64f, 0.78f, 0.92f, 1.0f), ETextJustify::Center);
	AddCanvas(FrameCanvas, StatusText, FAnchors(0.5f, 1.0f), FVector2D(0.0f, -42.0f), FVector2D(720.0f, 22.0f), FVector2D(0.5f, 1.0f));

	SetVisibility(ESlateVisibility::Collapsed);
}

void UJTSShopWidget::RefreshAll()
{
	const bool bResourcesChanged = RefreshWallet();
	if (bResourcesChanged || (CatalogWrap != nullptr && CatalogWrap->GetChildrenCount() == 0))
	{
		RefreshCatalog();
	}
}

void UJTSShopWidget::RefreshCatalog()
{
	if (CatalogWrap == nullptr || WidgetTree == nullptr)
	{
		return;
	}

	CatalogWrap->ClearChildren();
	for (const EJTSItemId ItemId : UJTSItemDefinitionLibrary::GetDefaultShopCatalog())
	{
		const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
		if (!IsValid(Definition) || !Definition->IsShopPurchasable())
		{
			continue;
		}

		const bool bAffordable = CanAfford(ItemId);
		const FLinearColor CardColor = bAffordable
			? FLinearColor(0.055f, 0.125f, 0.195f, 1.0f)
			: FLinearColor(0.175f, 0.060f, 0.070f, 1.0f);
		UButton* const Card = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), *FString::Printf(TEXT("ShipSupplyCard_%d"), static_cast<int32>(ItemId)));
		Card->SetBackgroundColor(CardColor);
		Card->SetToolTipText(BuildItemTooltip(ItemId));

		UVerticalBox* const Contents = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), *FString::Printf(TEXT("ShipSupplyContents_%d"), static_cast<int32>(ItemId)));
		UBorder* const Accent = MakeBorder(
			WidgetTree,
			*FString::Printf(TEXT("ShipSupplyAccent_%d"), static_cast<int32>(ItemId)),
			Definition->AccentColor.CopyWithNewOpacity(0.86f),
			3.0f);
		Accent->SetContent(MakeText(
			WidgetTree,
			*FString::Printf(TEXT("ShipSupplyRole_%d"), static_cast<int32>(ItemId)),
			ItemRoleLabel(Definition),
			13.0f,
			FLinearColor(0.015f, 0.030f, 0.055f, 1.0f),
			ETextJustify::Center));
		USizeBox* const AccentSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), *FString::Printf(TEXT("ShipSupplyAccentSize_%d"), static_cast<int32>(ItemId)));
		AccentSize->SetHeightOverride(65.0f);
		AccentSize->SetContent(Accent);
		AddVertical(Contents, AccentSize, FMargin(5.0f, 5.0f, 5.0f, 10.0f));
		AddVertical(Contents, MakeText(WidgetTree, *FString::Printf(TEXT("ShipSupplyName_%d"), static_cast<int32>(ItemId)), Definition->DisplayName.ToString(), 17.0f, FLinearColor::White, ETextJustify::Center), FMargin(6.0f, 0.0f, 6.0f, 6.0f));
		AddVertical(Contents, MakeText(WidgetTree, *FString::Printf(TEXT("ShipSupplyCost_%d"), static_cast<int32>(ItemId)), FormatCosts(ItemId), 13.0f, FLinearColor(0.42f, 1.0f, 0.74f, 1.0f), ETextJustify::Center), FMargin(6.0f, 0.0f, 6.0f, 5.0f));
		AddVertical(Contents, MakeText(WidgetTree, *FString::Printf(TEXT("ShipSupplyAvailability_%d"), static_cast<int32>(ItemId)), bAffordable ? TEXT("BUY") : FString::Printf(TEXT("NEED %s"), *FormatMissingCosts(ItemId)), 12.0f, bAffordable ? FLinearColor(0.75f, 0.88f, 1.0f, 1.0f) : FLinearColor(1.0f, 0.48f, 0.38f, 1.0f), ETextJustify::Center), FMargin(6.0f, 10.0f, 6.0f, 4.0f));
		Card->SetContent(Contents);

		switch (ItemId)
		{
		case EJTSItemId::Pickaxe: Card->OnClicked.AddDynamic(this, &UJTSShopWidget::HandlePickaxeBuy); break;
		case EJTSItemId::Knife: Card->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleKnifeBuy); break;
		case EJTSItemId::Pistol: Card->OnClicked.AddDynamic(this, &UJTSShopWidget::HandlePistolBuy); break;
		case EJTSItemId::MachineGun: Card->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleMachineGunBuy); break;
		case EJTSItemId::Backpack: Card->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleBackpackBuy); break;
		default: break;
		}

		USizeBox* const Cell = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), *FString::Printf(TEXT("ShipSupplyCell_%d"), static_cast<int32>(ItemId)));
		Cell->SetWidthOverride(202.0f);
		Cell->SetHeightOverride(300.0f);
		Cell->SetContent(Card);
		if (UWrapBoxSlot* const WrapSlot = CatalogWrap->AddChildToWrapBox(Cell))
		{
			WrapSlot->SetPadding(FMargin(4.0f));
		}
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
	CurrentResourceAmounts.Add(EJTSResourceType::Rock, Spacecraft->GetResourceAmount(EJTSResourceType::Rock));
	CurrentResourceAmounts.Add(EJTSResourceType::Ore, Spacecraft->GetResourceAmount(EJTSResourceType::Ore));
	for (const EJTSItemId ItemId : UJTSItemDefinitionLibrary::GetDefaultShopCatalog())
	{
		const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
		if (!IsValid(Definition))
		{
			continue;
		}

		for (const FJTSItemCost& Cost : Definition->ShopCosts)
		{
			if (Cost.Amount > 0)
			{
				CurrentResourceAmounts.FindOrAdd(Cost.ResourceType) = Spacecraft->GetResourceAmount(Cost.ResourceType);
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
	LastDisplayedResourceAmounts = MoveTemp(CurrentResourceAmounts);

	if (WalletText != nullptr)
	{
		WalletText->SetText(FText::FromString(FString::Printf(
			TEXT("ROCK %d   ORE %d"),
			Spacecraft->GetResourceAmount(EJTSResourceType::Rock),
			Spacecraft->GetResourceAmount(EJTSResourceType::Ore))));
	}

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
	return FString::Join(Costs, TEXT("  /  "));
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
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		if (ActiveSpacecraft.IsValid())
		{
			LastRequestedItem = ItemId;
			Controller->ServerRequestShopPurchase(ActiveSpacecraft.Get(), ItemId);
			if (StatusText != nullptr)
			{
				StatusText->SetText(FText::FromString(TEXT("PURCHASING...")));
				StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.64f, 0.78f, 0.92f, 1.0f)));
			}
		}
	}
}

void UJTSShopWidget::HandlePickaxeBuy() { RequestPurchase(EJTSItemId::Pickaxe); }
void UJTSShopWidget::HandleKnifeBuy() { RequestPurchase(EJTSItemId::Knife); }
void UJTSShopWidget::HandlePistolBuy() { RequestPurchase(EJTSItemId::Pistol); }
void UJTSShopWidget::HandleMachineGunBuy() { RequestPurchase(EJTSItemId::MachineGun); }
void UJTSShopWidget::HandleBackpackBuy() { RequestPurchase(EJTSItemId::Backpack); }

void UJTSShopWidget::HandleCloseClicked()
{
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		Controller->CloseSpaceShop();
	}
}
