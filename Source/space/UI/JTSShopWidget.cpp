// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSShopWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSStellarLootTable.h"
#include "space/UI/JTSStellarAttachmentDialog.h"
#include "space/UI/JTSWeaponUpgradePanel.h"
#include "space/UI/JTSGameUILayout.h"
#include "space/UI/SJTSStellarLoadoutView.h"
#include "space/Components/JTSStellarLoadoutComponent.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "space/Items/JTSWeaponProgression.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Player/JTSCharacter.h"
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
			Result->SetFont(FCoreStyle::GetDefaultFontStyle(FName(TEXT("Bold")), FMath::Max(16.0f, FontSize)));
			Result->SetColorAndOpacity(FSlateColor(Color));
			Result->SetAutoWrapText(false);
			Result->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
			Result->SetJustification(Justify);
		}
		return Result;
	}

	UTextBlock* MakeButtonLabel(UWidgetTree* Tree, FName Name, const FString& Label, float FontSize)
	{
		UTextBlock* const Result = MakeText(Tree, Name, Label, FMath::Max(18.0f, FontSize), FLinearColor::White, ETextJustify::Center);
		if (Result) Result->SetAutoWrapText(false);
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

	void SetReadableTooltip(UWidgetTree* Tree, UWidget* Owner, const FText& Text)
	{
		if (!Tree || !Owner || (Owner->GetToolTip() && Owner->GetToolTipText().EqualTo(Text))) return;
		Owner->SetToolTipText(Text);
		if (Text.IsEmpty())
		{
			Owner->SetToolTip(nullptr);
			return;
		}
		USizeBox* const Size = Tree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(440.0f);
		UBorder* const Background = MakeBorder(Tree, NAME_None, FLinearColor(0.008f, 0.014f, 0.025f, 0.98f), 16.0f);
		UTextBlock* const Body = MakeText(Tree, NAME_None, Text.ToString(), 18.0f, FLinearColor::White);
		Body->SetAutoWrapText(true);
		Body->SetTextOverflowPolicy(ETextOverflowPolicy::Clip);
		Background->SetContent(Body);
		Size->SetContent(Background);
		Owner->SetToolTip(Size);
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
			return FLinearColor(0.065f, 0.043f, 0.018f, 1.0f);
		}
		if (Definition->IsRangedWeapon())
		{
			return FLinearColor(0.018f, 0.043f, 0.070f, 1.0f);
		}
		return FLinearColor(0.025f, 0.034f, 0.045f, 1.0f);
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

	if (IsValid(AttachmentDialog) && AttachmentDialog->IsDialogOpen() && InKeyEvent.GetKey() == EKeys::Escape)
	{
		AttachmentDialog->CloseDialog();
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
	if (bShopOpen && InKeyEvent.GetKey() == EKeys::Q)
	{
		AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer());
		const AJTSCharacter* const Character = IsValid(Controller) ? Cast<AJTSCharacter>(Controller->GetPawn()) : nullptr;
		const UJTSInventoryComponent* const Inventory = IsValid(Character) ? Character->GetInventoryComponent() : nullptr;
		if (IsValid(Controller) && IsValid(Inventory))
		{
			const int32 SlotIndex = Inventory->GetSelectedQuickbarSlot();
			const FJTSItemInstance Item = Inventory->GetItemAtSlot(SlotIndex);
			if (!Item.IsEmpty()) Controller->ServerRequestInventoryQuantityAction(SlotIndex, Item.StackCount, false);
		}
		return FReply::Handled();
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
	ResetLockerDrag();
	LastDisplayedResourceAmounts.Reset();
	LastRequestedItem = EJTSItemId::None;
	bShowingAbilityPage = false;
	bShowingStellarPage = false;
	bShowingWeaponPage = false;
	bRollAnimating = false;
	bDebugStellarGrantPending = false;
	bRollResultReceived = false;
	RolledStellarItemId = NAME_None;
	RolledLockerSlotIndex = INDEX_NONE;
	SelectedLockerSlot = INDEX_NONE;
	DraggedLockerSlot = INDEX_NONE;
	bLeverDragging = false;
	SetLeverPull(0.0f);
	bAbilityCommitPending = false;
	ResetPendingAbilityAllocation();
	RefreshAccumulator = 0.0f;
	bShopOpen = true;
	if (StatusText != nullptr) StatusText->SetText(FText::GetEmpty());
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RefreshAll();
	return true;
}

void UJTSShopWidget::CloseShop()
{
	ResetLockerDrag();
	if (IsValid(WeaponPanel)) WeaponPanel->ClearSelection();
	if (IsValid(AttachmentDialog)) AttachmentDialog->CloseDialog();
	bShopOpen = false;
	LastRequestedItem = EJTSItemId::None;
	LastDisplayedResourceAmounts.Reset();
	bAbilityCommitPending = false;
	bLeverDragging = false;
	SetLeverPull(0.0f);
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
		SetStatus(TEXT("已放入飞船物品栏"), false);
		break;
	case EJTSShopPurchaseResult::SucceededDropped:
		SetStatus(TEXT("已放入飞船物品栏"), false);
		break;
	case EJTSShopPurchaseResult::InsufficientResources:
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("MISSING %s"), *FormatMissingCosts(LastRequestedItem))));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.46f, 0.34f, 1.0f)));
		break;
	case EJTSShopPurchaseResult::InvalidItem:
		StatusText->SetText(FText::FromString(TEXT("ITEM UNAVAILABLE")));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.46f, 0.34f, 1.0f)));
		break;
	case EJTSShopPurchaseResult::InventoryFull:
		SetStatus(TEXT("物品栏已满，请拖拽物品到废纸篓删除"), true);
		break;
	case EJTSShopPurchaseResult::DeliveryFailed:
	default:
		StatusText->SetText(FText::FromString(TEXT("DELIVERY FAILED / MATERIALS RESTORED")));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.46f, 0.34f, 1.0f)));
		break;
	}

	RefreshAll();
}

void UJTSShopWidget::SetStatus(const FString& Message, bool bError)
{
	if (StatusText == nullptr) return;
	StatusText->SetText(FText::FromString(Message));
	StatusText->SetColorAndOpacity(FSlateColor(bError
		? FLinearColor(1.0f, 0.42f, 0.32f, 1.0f)
		: FLinearColor(0.38f, 1.0f, 0.70f, 1.0f)));
	RefreshPageVisibility();
}

void UJTSShopWidget::NotifyStellarRollResult(EJTSStellarRollResult Result, FName ItemId, int32 SlotIndex)
{
	bRollResultReceived = true;
	RolledStellarItemId = ItemId;
	RolledLockerSlotIndex = SlotIndex;
	if (Result == EJTSStellarRollResult::Succeeded && !ItemId.IsNone())
	{
		if (RollElapsed >= UJTSStellarLootTable::RevealDurationSeconds && CanFinishStellarRoll()) FinishStellarRoll();
	}
	else
	{
		bRollAnimating = false;
		SetLeverPull(0.0f);
		if (StellarRollButton != nullptr) StellarRollButton->SetIsEnabled(true);
		switch (Result)
		{
		case EJTSStellarRollResult::InventoryFull:
			SetStatus(TEXT("物品栏已满，请拖拽物品到废纸篓删除"), true); break;
		case EJTSStellarRollResult::InsufficientResources:
			SetStatus(TEXT("飞船资源不足"), true); break;
		case EJTSStellarRollResult::CoolingDown:
			SetStatus(TEXT("遥感装置冷却中"), true); break;
		default:
			SetStatus(TEXT("星际商店暂不可用"), true); break;
		}
	}
	RefreshShipLocker();
	RefreshStellarReel();
}

void UJTSShopWidget::NotifyDebugStellarItemsResult(bool bAvailable, int32 AddedCount, int32 RequestedCount)
{
	bDebugStellarGrantPending = false;
	if (!bAvailable)
	{
		SetStatus(TEXT("DEBUG 发放不可用，请靠近当前飞船并检查星际物品表"), true);
	}
	else if (AddedCount < RequestedCount)
	{
		SetStatus(FString::Printf(TEXT("DEBUG：已加入 %d / %d 件；商店物品栏空间不足，%d 件未加入"),
			AddedCount, RequestedCount, RequestedCount - AddedCount), true);
	}
	else
	{
		SetStatus(FString::Printf(TEXT("DEBUG：已加入 %d 件核心与配件各一个（不含升级固件）"), AddedCount), false);
	}
	RefreshShipLocker();
	RefreshStellarReel();
}

void UJTSShopWidget::NotifyShipLockerActionResult(bool bSucceeded, bool bTakeAction)
{
	SetStatus(bSucceeded
		? (bTakeAction ? TEXT("已取到角色背包") : TEXT("物品已删除"))
		: (bTakeAction ? TEXT("角色背包已满或物品暂不可使用") : TEXT("删除失败，请重试")),
		!bSucceeded);
	RefreshShipLocker();
}

void UJTSShopWidget::NotifyShipLockerExchangeResult(bool bSucceeded)
{
	SetStatus(bSucceeded ? TEXT("已与角色物品格交换") : TEXT("交换失败，请重试"), !bSucceeded);
	RefreshShipLocker();
}

void UJTSShopWidget::NotifyShipLockerMoveResult(bool bSucceeded)
{
	SetStatus(bSucceeded ? TEXT("已调整飞船物品格") : TEXT("移格失败，物品格可能已变化"), !bSucceeded);
	RefreshShipLocker();
}

void UJTSShopWidget::NotifyStellarCombineResult(bool bSucceeded)
{
	SetStatus(bSucceeded ? TEXT("星际武器组合完成，拖到人物星际栏装备")
		: TEXT("组合失败：核心必须在对应配件的前一格"), !bSucceeded);
	RefreshShipLocker();
}

void UJTSShopWidget::NotifyCarriedItemActionResult(bool bSucceeded, bool bStored)
{
	SetStatus(bSucceeded
		? (bStored ? TEXT("已放入飞船物品格") : TEXT("角色物品已删除"))
		: (bStored ? TEXT("放入失败，请选择空物品格") : TEXT("删除失败，请重试")),
		!bSucceeded);
	RefreshShipLocker();
}

void UJTSShopWidget::UpdateCarriedDragPreview(const FVector2D& ScreenPosition,
	const FString& ItemLabel, bool bVisible)
{
	if (!DragGhost || !DragGhostText || !RootCanvas) return;
	if (!bVisible)
	{
		DragGhost->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	const FVector2D Position = RootCanvas->GetCachedGeometry().AbsoluteToLocal(ScreenPosition);
	if (UCanvasPanelSlot* const GhostSlot = Cast<UCanvasPanelSlot>(DragGhost->Slot))
	{
		GhostSlot->SetPosition(Position + FVector2D(12.0f, 12.0f));
	}
	DragGhostText->SetText(FText::FromString(ItemLabel));
	DragGhost->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UJTSShopWidget::HandleStellarItemDrop(const FVector2D& ScreenPosition, int32 StellarIndex, FGuid ExpectedId)
{
	if (!IsShopOpen() || bShowingAbilityPage) return;
	auto* PS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	if (!PS) return;
	for (int32 Index = 0; Index < LockerSlotBorders.Num(); ++Index)
		if (LockerSlotBorders[Index] && LockerSlotBorders[Index]->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			PS->GetStellarLoadout()->ServerExchangeLocker(ActiveSpacecraft.Get(), Index,
				PS->GetShipLockerSlot(Index).SlotToken, StellarIndex, ExpectedId); return;
		}
}

void UJTSShopWidget::HandleCarriedItemDrop(const FVector2D& ScreenPosition,
	int32 CarriedSlotIndex, FGuid ExpectedInstanceId)
{
	if (!IsShopOpen() || bShowingAbilityPage || !ExpectedInstanceId.IsValid()) return;
	AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer());
	if (!IsValid(Controller)) return;
	if (TrashImage && TrashImage->GetCachedGeometry().IsUnderLocation(ScreenPosition))
	{
		Controller->ServerDestroyCarriedItemAtShip(ActiveSpacecraft.Get(), CarriedSlotIndex, ExpectedInstanceId);
		return;
	}
	for (int32 Index = 0; Index < LockerSlotBorders.Num(); ++Index)
	{
		if (LockerSlotBorders[Index] && LockerSlotBorders[Index]->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			const AJTSPlayerState* const State = Controller->GetPlayerState<AJTSPlayerState>();
			if (State && State->GetShipLockerSlot(Index).IsEmpty())
			{
				Controller->ServerStoreCarriedItemInShipLocker(ActiveSpacecraft.Get(),
					CarriedSlotIndex, ExpectedInstanceId, Index);
			}
			else
			{
				SetStatus(TEXT("格子已占用，请拖到空物品格"), true);
			}
			return;
		}
	}
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
	RefreshResponsiveLayout();
	if (bRollAnimating)
	{
		RollElapsed += InDeltaTime;
		SetLeverPull(90.0f * (1.0f - FMath::Clamp(RollElapsed / 0.45f, 0.0f, 1.0f)));
		const float Ease = FMath::Square(FMath::Clamp(RollElapsed / UJTSStellarLootTable::RevealDurationSeconds, 0.0f, 1.0f));
		const float StepPeriod = FMath::Lerp(0.055f, 0.30f, Ease);
		ReelStepAccumulator += InDeltaTime;
		if (ReelStepAccumulator >= StepPeriod)
		{
			ReelStepAccumulator = FMath::Fmod(ReelStepAccumulator, StepPeriod);
			if (const UJTSStellarLootTable* const Table = ActiveSpacecraft->GetStellarLootTable(); IsValid(Table) && !Table->Entries.IsEmpty())
			{
				ReelDisplayIndex = FMath::RandRange(0, Table->Entries.Num() - 1);
			}
			RefreshStellarReel();
		}
		const float Phase = ReelStepAccumulator / StepPeriod;
		if (UCanvasPanelSlot* const ReelSlot = Cast<UCanvasPanelSlot>(ReelPreviousText ? ReelPreviousText->Slot : nullptr)) ReelSlot->SetPosition(FVector2D(18.0f, 40.0f - 78.0f * Phase));
		if (UCanvasPanelSlot* const ReelSlot = Cast<UCanvasPanelSlot>(ReelCurrentText ? ReelCurrentText->Slot : nullptr)) ReelSlot->SetPosition(FVector2D(18.0f, 114.0f - 78.0f * Phase));
		if (UCanvasPanelSlot* const ReelSlot = Cast<UCanvasPanelSlot>(ReelNextText ? ReelNextText->Slot : nullptr)) ReelSlot->SetPosition(FVector2D(18.0f, 192.0f - 78.0f * Phase));
		if (bRollResultReceived && RollElapsed >= UJTSStellarLootTable::RevealDurationSeconds
			&& CanFinishStellarRoll()) FinishStellarRoll();
	}

	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.20f)
	{
		RefreshAccumulator = 0.0f;
		RefreshAll();
	}
}

FReply UJTSShopWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bShopOpen && bShowingStellarPage && !bRollAnimating && LeverKnob
		&& InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
		&& LeverKnob->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		bLeverDragging = true;
		LeverGrabY = StellarPanel->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()).Y;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	if (bShopOpen && !bShowingAbilityPage && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const AJTSPlayerState* const State = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
		for (int32 Index = 0; Index < LockerSlotBorders.Num(); ++Index)
		{
			if (LockerSlotBorders[Index] && LockerSlotBorders[Index]->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
			{
				SelectedLockerSlot = Index;
				LockerPressPosition = InMouseEvent.GetScreenSpacePosition();
				const FJTSShipLockerSlot LockerEntry = State ? State->GetShipLockerSlot(Index) : FJTSShipLockerSlot();
				DraggedLockerSlot = LockerEntry.IsEmpty() || LockerEntry.bPendingStellarReveal
					|| (bRollAnimating && Index == RolledLockerSlotIndex) ? INDEX_NONE : Index;
				DraggedLockerToken = LockerEntry.SlotToken;
				RefreshShipLocker();
				return DraggedLockerSlot != INDEX_NONE
					? FReply::Handled().CaptureMouse(TakeWidget()) : FReply::Handled();
			}
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UJTSShopWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bLeverDragging && StellarPanel)
	{
		const float CurrentY = StellarPanel->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()).Y;
		SetLeverPull(CurrentY - LeverGrabY);
		return FReply::Handled();
	}
	if (DraggedLockerSlot != INDEX_NONE && DragGhost && RootCanvas)
	{
		if (auto* Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
		{
			const auto* State = Controller->GetPlayerState<AJTSPlayerState>();
			const auto Entry = State ? State->GetShipLockerSlot(DraggedLockerSlot) : FJTSShipLockerSlot();
			Controller->SetStellarItemDragging(!Entry.StellarItemId.IsNone()
				|| Entry.StandardItem.ItemId == EJTSItemId::StellarText
				|| Entry.StandardItem.ItemId == EJTSItemId::StellarWeapon);
		}
		const FVector2D Position = RootCanvas->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		if (UCanvasPanelSlot* const GhostSlot = Cast<UCanvasPanelSlot>(DragGhost->Slot)) GhostSlot->SetPosition(Position + FVector2D(12.0f, 12.0f));
		if (LockerSlotTexts.IsValidIndex(DraggedLockerSlot) && DragGhostText)
		{
			DragGhostText->SetText(LockerSlotTexts[DraggedLockerSlot]->GetText());
		}
		DragGhost->SetVisibility(ESlateVisibility::HitTestInvisible);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UJTSShopWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bLeverDragging)
	{
		bLeverDragging = false;
		if (LeverPull >= 52.0f) HandleStellarRollClicked();
		if (!bRollAnimating) SetLeverPull(0.0f);
		return FReply::Handled().ReleaseMouseCapture();
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && DraggedLockerSlot != INDEX_NONE)
	{
		// A press that barely moved is a click: open the allocation dialog instead of starting a move.
		if (FVector2D::Distance(InMouseEvent.GetScreenSpacePosition(), LockerPressPosition) < 8.0f
			&& LockerSlotBorders.IsValidIndex(DraggedLockerSlot) && LockerSlotBorders[DraggedLockerSlot]
			&& LockerSlotBorders[DraggedLockerSlot]->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
		{
			const int32 ClickedSlot = DraggedLockerSlot;
			ResetLockerDrag();
			OpenAttachmentDialog(ClickedSlot);
			return FReply::Handled().ReleaseMouseCapture();
		}
		if (TrashImage && TrashImage->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
		{
			if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
			{
				Controller->ServerDeleteShipLockerSlot(ActiveSpacecraft.Get(), DraggedLockerSlot, DraggedLockerToken);
			}
		}
		else if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
		{
			const int32 StellarIndex = Controller->GetStellarSlotAtScreenPosition(InMouseEvent.GetScreenSpacePosition());
			if (StellarIndex != INDEX_NONE)
			{
				if (auto* State = Controller->GetPlayerState<AJTSPlayerState>())
				{
					auto* Loadout = State->GetStellarLoadout();
					Loadout->ServerExchangeLocker(ActiveSpacecraft.Get(), DraggedLockerSlot, DraggedLockerToken,
						StellarIndex, Loadout->GetSlot(StellarIndex).InstanceId);
				}
				ResetLockerDrag();
				return FReply::Handled().ReleaseMouseCapture();
			}
			int32 TargetLockerSlot = INDEX_NONE;
			for (int32 Index = 0; Index < LockerSlotBorders.Num(); ++Index)
			{
				if (LockerSlotBorders[Index]
					&& LockerSlotBorders[Index]->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
				{
					TargetLockerSlot = Index;
					break;
				}
			}
			if (TargetLockerSlot != INDEX_NONE && TargetLockerSlot != DraggedLockerSlot)
			{
				const AJTSPlayerState* const State = Controller->GetPlayerState<AJTSPlayerState>();
				const FJTSShipLockerSlot Target = State
					? State->GetShipLockerSlot(TargetLockerSlot) : FJTSShipLockerSlot();
				Controller->ServerMoveShipLockerSlot(ActiveSpacecraft.Get(), DraggedLockerSlot,
					DraggedLockerToken, TargetLockerSlot, Target.SlotToken);
				SelectedLockerSlot = TargetLockerSlot;
			}
			else if (const int32 CarriedSlotIndex = Controller->GetCarriedQuickbarSlotAtPosition(
				InMouseEvent.GetScreenSpacePosition()); CarriedSlotIndex != INDEX_NONE)
			{
				const AJTSPlayerState* const State = Controller->GetPlayerState<AJTSPlayerState>();
				const FJTSShipLockerSlot LockerEntry = State
					? State->GetShipLockerSlot(DraggedLockerSlot) : FJTSShipLockerSlot();
				const AJTSCharacter* const Character = Cast<AJTSCharacter>(Controller->GetPawn());
				const UJTSInventoryComponent* const Inventory = IsValid(Character)
					? Character->GetInventoryComponent() : nullptr;
				if (!LockerEntry.StellarItemId.IsNone()
					|| LockerEntry.StandardItem.ItemId == EJTSItemId::StellarText
					|| LockerEntry.StandardItem.ItemId == EJTSItemId::StellarWeapon)
				{
					SetStatus(TEXT("星际物品只能拖到人物右侧星际栏"), true);
				}
				else if (!LockerEntry.IsEmpty() && !LockerEntry.bPendingStellarReveal && IsValid(Inventory))
				{
					const FJTSItemInstance CarriedItem = Inventory->GetItemAtSlot(CarriedSlotIndex);
					Controller->ServerExchangeShipLockerWithCarriedSlot(ActiveSpacecraft.Get(),
						DraggedLockerSlot, DraggedLockerToken, CarriedSlotIndex, CarriedItem.InstanceId);
				}
				else
				{
					SetStatus(TEXT("此物品暂时无法移动"), true);
				}
			}
		}
		ResetLockerDrag();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UJTSShopWidget::ResetLockerDrag()
{
	DraggedLockerSlot = INDEX_NONE;
	DraggedLockerToken.Invalidate();
	if (DragGhost) DragGhost->SetVisibility(ESlateVisibility::Collapsed);
	if (auto* Controller = Cast<AJTSPlayerController>(GetOwningPlayer())) Controller->SetStellarItemDragging(false);
}

void UJTSShopWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
	ResetLockerDrag();
	bLeverDragging = false;
	if (!bRollAnimating) SetLeverPull(0.0f);
}

void UJTSShopWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || RootCanvas != nullptr)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShipSupplyRoot"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	UBorder* const Dimmer = MakeBorder(WidgetTree, TEXT("ShipSupplyDimmer"), FLinearColor(0.003f, 0.008f, 0.018f, 0.42f));
	Dimmer->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddCanvas(RootCanvas, Dimmer, FAnchors(0.0f, 0.0f, 1.0f, 0.80f), FVector2D::ZeroVector, FVector2D::ZeroVector);

	ShopFrame = MakeBorder(WidgetTree, TEXT("ShipSupplyFrame"), FLinearColor(0.012f, 0.025f, 0.043f, 0.98f));
	UScaleBox* const ResponsiveScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ShipTerminalResponsiveScale"));
	ResponsiveScale->SetStretch(EStretch::ScaleToFit);
	TerminalLayoutSlot = AddCanvas(RootCanvas, ResponsiveScale, FAnchors(0.0f, 0.0f),
		FVector2D::ZeroVector, FVector2D(1320.0f, 720.0f));
	USizeBox* const DesignSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShipTerminalDesignSize"));
	DesignSize->SetWidthOverride(1320.0f);
	DesignSize->SetHeightOverride(720.0f);
	DesignSize->SetContent(ShopFrame);
	ResponsiveScale->SetContent(DesignSize);
	UCanvasPanel* const FrameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShipSupplyFrameCanvas"));
	ShopFrame->SetContent(FrameCanvas);

	AddCanvas(FrameCanvas, MakeText(WidgetTree, TEXT("ShipSupplyTitle"), TEXT("飞船终端  /  SHIP TERMINAL"), 32.0f, FLinearColor(0.92f, 0.97f, 1.0f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(30.0f, 24.0f), FVector2D(880.0f, 46.0f));
	AddCanvas(FrameCanvas, MakeText(WidgetTree, TEXT("ShipTabHint"), TEXT("TAB  SWITCH"), 12.0f, FLinearColor(0.55f, 0.69f, 0.82f, 1.0f), ETextJustify::Right), FAnchors(1.0f, 0.0f), FVector2D(-164.0f, 93.0f), FVector2D(150.0f, 20.0f), FVector2D(1.0f, 0.0f));

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipSupplyClose"));
	CloseButton->SetBackgroundColor(FLinearColor(0.16f, 0.22f, 0.30f, 1.0f));
	CloseButton->SetContent(MakeButtonLabel(WidgetTree, TEXT("ShipSupplyCloseLabel"), TEXT("关闭 [E]"), 13.0f));
	CloseButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleCloseClicked);
	AddCanvas(FrameCanvas, CloseButton, FAnchors(1.0f, 0.0f), FVector2D(-30.0f, 25.0f), FVector2D(120.0f, 36.0f), FVector2D(1.0f, 0.0f));

	SupplyTabButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipSupplyTab"));
	SupplyTabButton->SetBackgroundColor(FLinearColor(0.08f, 0.29f, 0.43f, 1.0f));
	SupplyTabButton->SetContent(MakeButtonLabel(WidgetTree, TEXT("ShipSupplyTabLabel"), TEXT("普通商店"), 16.0f));
	SupplyTabButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleSupplyTabClicked);
	AddCanvas(FrameCanvas, SupplyTabButton, FAnchors(0.0f, 0.0f), FVector2D(30.0f, 84.0f), FVector2D(168.0f, 40.0f));

	StellarTabButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipStellarTab"));
	StellarTabButton->SetBackgroundColor(FLinearColor(0.10f, 0.16f, 0.25f, 1.0f));
	StellarTabButton->SetContent(MakeButtonLabel(WidgetTree, TEXT("ShipStellarTabLabel"), TEXT("星际商店"), 16.0f));
	StellarTabButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleStellarTabClicked);
	AddCanvas(FrameCanvas, StellarTabButton, FAnchors(0.0f, 0.0f), FVector2D(206.0f, 84.0f), FVector2D(168.0f, 40.0f));

	AbilityTabButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipAbilityTab"));
	AbilityTabButton->SetBackgroundColor(FLinearColor(0.10f, 0.16f, 0.25f, 1.0f));
	AbilityTabLabel = MakeButtonLabel(WidgetTree, TEXT("ShipAbilityTabLabel"), TEXT("能力"), 16.0f);
	AbilityTabButton->SetContent(AbilityTabLabel);
	AbilityTabButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleAbilityTabClicked);
	AddCanvas(FrameCanvas, AbilityTabButton, FAnchors(0.0f, 0.0f), FVector2D(558.0f, 84.0f), FVector2D(168.0f, 40.0f));

	WeaponTabButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipWeaponTab"));
	WeaponTabButton->SetBackgroundColor(FLinearColor(0.10f, 0.16f, 0.25f, 1.0f));
	WeaponTabButton->SetContent(MakeButtonLabel(WidgetTree, TEXT("ShipWeaponTabLabel"), TEXT("武器改装"), 16.0f));
	WeaponTabButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleWeaponTabClicked);
	AddCanvas(FrameCanvas, WeaponTabButton, FAnchors(0.0f, 0.0f), FVector2D(382.0f, 84.0f), FVector2D(168.0f, 40.0f));

	CatalogPanel = MakeBorder(WidgetTree, TEXT("ShipSupplyGridFrame"), FLinearColor(0.014f, 0.040f, 0.075f, 1.0f));
	AddCanvas(FrameCanvas, CatalogPanel, FAnchors(0.0f, 0.0f), FVector2D(30.0f, 138.0f), FVector2D(620.0f, 520.0f));
	UCanvasPanel* const CatalogCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShipSupplyCatalogCanvas"));
	CatalogPanel->SetContent(CatalogCanvas);
	AddCanvas(CatalogCanvas, MakeText(WidgetTree, TEXT("ShipResourcesLabel"), TEXT("SHIP RESOURCES"), 12.0f, FLinearColor(0.55f, 0.69f, 0.82f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(20.0f, 19.0f), FVector2D(160.0f, 22.0f));
	WalletText = MakeText(WidgetTree, TEXT("ShipSupplyWallet"), TEXT("ROCK 0   ORE 0"), 17.0f, FLinearColor(0.38f, 1.0f, 0.72f, 1.0f));
	AddCanvas(CatalogCanvas, WalletText, FAnchors(0.0f, 0.0f), FVector2D(16.0f, 42.0f), FVector2D(585.0f, 28.0f));
	DebugResourcesButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipDebugResources"));
	DebugResourcesButton->SetBackgroundColor(FLinearColor(0.13f, 0.22f, 0.27f, 1.0f));
	DebugResourcesButton->SetContent(MakeButtonLabel(WidgetTree, TEXT("ShipDebugResourcesLabel"), TEXT("DEBUG +100"), 18.0f));
	DebugResourcesButton->SetToolTipText(FText::FromString(TEXT("Add 100 of every ship resource")));
	DebugResourcesButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleDebugResourcesClicked);
	AddCanvas(CatalogCanvas, DebugResourcesButton, FAnchors(1.0f, 0.0f), FVector2D(-18.0f, 10.0f), FVector2D(174.0f, 34.0f), FVector2D(1.0f, 0.0f));
	UScrollBox* const CatalogScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ShipSupplyCatalogScroll"));
	AddCanvas(CatalogCanvas, CatalogScroll, FAnchors(0.0f, 0.0f), FVector2D(14.0f, 88.0f), FVector2D(592.0f, 416.0f));
	CatalogGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("ShipSupplyGrid"));
	CatalogScroll->AddChild(CatalogGrid);

	StellarPanel = MakeBorder(WidgetTree, TEXT("ShipStellarPanel"), FLinearColor(0.014f, 0.040f, 0.075f, 1.0f));
	AddCanvas(FrameCanvas, StellarPanel, FAnchors(0.0f, 0.0f), FVector2D(30.0f, 138.0f), FVector2D(620.0f, 520.0f));
	UCanvasPanel* const StellarCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShipStellarCanvas"));
	StellarPanel->SetContent(StellarCanvas);
	AddCanvas(StellarCanvas, MakeText(WidgetTree, TEXT("ShipStellarTitle"), TEXT("星际联盟遥感"), 23.0f, FLinearColor(0.83f, 0.92f, 1.0f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(28.0f, 25.0f), FVector2D(270.0f, 35.0f));
	DebugStellarItemsButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipDebugStellarItems"));
	DebugStellarItemsButton->SetBackgroundColor(FLinearColor(0.13f, 0.22f, 0.27f, 1.0f));
	DebugStellarItemsButton->SetContent(MakeButtonLabel(WidgetTree, TEXT("ShipDebugStellarItemsLabel"), TEXT("DEBUG 全套物品"), 16.0f));
	DebugStellarItemsButton->SetToolTipText(FText::FromString(TEXT("向商店物品栏的空格加入每种核心、配件各一个；不含升级固件，不消耗资源。")));
	DebugStellarItemsButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleDebugStellarItemsClicked);
	AddCanvas(StellarCanvas, DebugStellarItemsButton, FAnchors(1.0f, 0.0f), FVector2D(-28.0f, 23.0f), FVector2D(220.0f, 36.0f), FVector2D(1.0f, 0.0f));
	AddCanvas(StellarCanvas, MakeText(WidgetTree, TEXT("ShipStellarHint"), TEXT("拉动摇杆，遥感信号将锁定一件随机物品"), 14.0f, FLinearColor(0.55f, 0.69f, 0.82f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(28.0f, 65.0f), FVector2D(520.0f, 26.0f));
	AddCanvas(StellarCanvas, MakeText(WidgetTree, TEXT("ShipStellarCombineHint"), TEXT("组合说明：核心放在前一格，对应配件放在紧邻的后一格。"), 12.0f, FLinearColor(0.66f, 0.81f, 0.90f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(28.0f, 88.0f), FVector2D(552.0f, 20.0f));
	UBorder* const ReelFrame = MakeBorder(WidgetTree, TEXT("ShipStellarReelFrame"), FLinearColor(0.035f, 0.075f, 0.11f, 1.0f));
	AddCanvas(StellarCanvas, ReelFrame, FAnchors(0.0f, 0.0f), FVector2D(28.0f, 112.0f), FVector2D(452.0f, 276.0f));
	UCanvasPanel* const ReelCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShipStellarReelCanvas"));
	ReelCanvas->SetClipping(EWidgetClipping::ClipToBounds);
	ReelFrame->SetContent(ReelCanvas);
	AddCanvas(ReelCanvas, MakeBorder(WidgetTree, TEXT("ShipStellarSelection"), FLinearColor(0.08f, 0.29f, 0.36f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(8.0f, 106.0f), FVector2D(436.0f, 62.0f));
	ReelPreviousText = MakeText(WidgetTree, TEXT("ShipStellarPrevious"), TEXT(""), 16.0f, FLinearColor(0.38f, 0.57f, 0.67f, 1.0f), ETextJustify::Center);
	ReelCurrentText = MakeText(WidgetTree, TEXT("ShipStellarCurrent"), TEXT("等待信号"), 23.0f, FLinearColor(0.86f, 1.0f, 0.94f, 1.0f), ETextJustify::Center);
	ReelNextText = MakeText(WidgetTree, TEXT("ShipStellarNext"), TEXT(""), 16.0f, FLinearColor(0.38f, 0.57f, 0.67f, 1.0f), ETextJustify::Center);
	AddCanvas(ReelCanvas, ReelPreviousText, FAnchors(0.0f, 0.0f), FVector2D(18.0f, 40.0f), FVector2D(416.0f, 45.0f));
	AddCanvas(ReelCanvas, ReelCurrentText, FAnchors(0.0f, 0.0f), FVector2D(18.0f, 114.0f), FVector2D(416.0f, 50.0f));
	AddCanvas(ReelCanvas, ReelNextText, FAnchors(0.0f, 0.0f), FVector2D(18.0f, 192.0f), FVector2D(416.0f, 45.0f));
	LeverStem = MakeBorder(WidgetTree, TEXT("ShipLeverStem"), FLinearColor(0.66f, 0.78f, 0.82f, 1.0f));
	LeverKnob = MakeBorder(WidgetTree, TEXT("ShipLeverKnob"), FLinearColor(1.0f, 0.43f, 0.27f, 1.0f));
	AddCanvas(StellarCanvas, LeverStem, FAnchors(0.0f, 0.0f), FVector2D(540.0f, 190.0f), FVector2D(9.0f, 132.0f));
	AddCanvas(StellarCanvas, LeverKnob, FAnchors(0.0f, 0.0f), FVector2D(521.0f, 160.0f), FVector2D(48.0f, 48.0f));
	StellarRollButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipStellarRoll"));
	StellarRollButton->SetBackgroundColor(FLinearColor(0.08f, 0.40f, 0.34f, 1.0f));
	StellarRollButton->SetContent(MakeButtonLabel(WidgetTree, TEXT("ShipStellarRollLabel"), TEXT("拉动摇杆  /  ROLL"), 17.0f));
	StellarRollButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleStellarRollClicked);
	AddCanvas(StellarCanvas, StellarRollButton, FAnchors(0.0f, 0.0f), FVector2D(28.0f, 411.0f), FVector2D(552.0f, 50.0f));
	ReelCostText = MakeText(WidgetTree, TEXT("ShipStellarCost"), TEXT(""), 13.0f, FLinearColor(0.66f, 0.81f, 0.90f, 1.0f), ETextJustify::Center);
	AddCanvas(StellarCanvas, ReelCostText, FAnchors(0.0f, 0.0f), FVector2D(28.0f, 474.0f), FVector2D(552.0f, 24.0f));

	LockerPanel = MakeBorder(WidgetTree, TEXT("ShipLockerPanel"), FLinearColor(0.014f, 0.040f, 0.075f, 1.0f));
	AddCanvas(FrameCanvas, LockerPanel, FAnchors(0.0f, 0.0f), FVector2D(672.0f, 138.0f), FVector2D(618.0f, 520.0f));
	UCanvasPanel* const LockerCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShipLockerCanvas"));
	LockerPanel->SetContent(LockerCanvas);
	AddCanvas(LockerCanvas, MakeText(WidgetTree, TEXT("ShipLockerTitle"), TEXT("商店物品栏"), 24.0f, FLinearColor(0.84f, 0.93f, 1.0f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(18.0f, 13.0f), FVector2D(350.0f, 36.0f));
	LockerCountText = MakeText(WidgetTree, TEXT("ShipLockerCount"), TEXT("0 / 30"), 18.0f, FLinearColor(0.40f, 1.0f, 0.74f, 1.0f), ETextJustify::Right);
	AddCanvas(LockerCanvas, LockerCountText, FAnchors(0.0f, 0.0f), FVector2D(454.0f, 16.0f), FVector2D(142.0f, 24.0f));
	LockerGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("ShipLockerGrid"));
	LockerGrid->SetSlotPadding(FMargin(3.0f));
	AddCanvas(LockerCanvas, LockerGrid, FAnchors(0.0f, 0.0f), FVector2D(17.0f, 56.0f), FVector2D(584.0f, 378.0f));
	LockerSlotBorders.Reset();
	LockerSlotTexts.Reset();
	LockerSlotBadges.Reset();
	LockerVisualStates.Init(INDEX_NONE, AJTSPlayerState::ShipLockerCapacity);
	for (int32 Index = 0; Index < AJTSPlayerState::ShipLockerCapacity; ++Index)
	{
		USizeBox* const Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("ShipLockerCell_%02d"), Index));
		Cell->SetWidthOverride(110.0f);
		Cell->SetHeightOverride(57.0f);
		UBorder* const Border = MakeBorder(WidgetTree, *FString::Printf(TEXT("ShipLockerSlot_%02d"), Index), FLinearColor(0.055f, 0.105f, 0.15f, 1.0f), 5.0f);
		UTextBlock* const Label = MakeText(WidgetTree, *FString::Printf(TEXT("ShipLockerLabel_%02d"), Index), FString::Printf(TEXT("%02d"), Index + 1), 11.0f, FLinearColor(0.51f, 0.65f, 0.75f, 1.0f), ETextJustify::Center);
		// The corner badge shows how many skill points a normal weapon has absorbed (0 after a reset).
		UTextBlock* const Badge = MakeText(WidgetTree, *FString::Printf(TEXT("ShipLockerBadge_%02d"), Index), TEXT(""), 13.0f, FLinearColor(1.0f, 0.82f, 0.30f, 1.0f), ETextJustify::Right);
		UOverlay* const SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("ShipLockerOverlay_%02d"), Index));
		if (UOverlaySlot* const LabelSlot = SlotOverlay->AddChildToOverlay(Label))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Fill);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UOverlaySlot* const BadgeSlot = SlotOverlay->AddChildToOverlay(Badge))
		{
			BadgeSlot->SetHorizontalAlignment(HAlign_Right);
			BadgeSlot->SetVerticalAlignment(VAlign_Top);
			BadgeSlot->SetPadding(FMargin(0.0f, 0.0f, 3.0f, 0.0f));
		}
		Badge->SetVisibility(ESlateVisibility::HitTestInvisible);
		Border->SetContent(SlotOverlay);
		LockerSlotBadges.Add(Badge);
		Cell->SetContent(Border);
		LockerGrid->AddChildToUniformGrid(Cell, Index / 5, Index % 5);
		LockerSlotBorders.Add(Border);
		LockerSlotTexts.Add(Label);
	}
	TakeLockerItemButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipLockerTake"));
	TakeLockerItemButton->SetBackgroundColor(FLinearColor(0.10f, 0.34f, 0.30f, 1.0f));
	TakeLockerItemButton->SetContent(MakeButtonLabel(WidgetTree, TEXT("ShipLockerTakeLabel"), TEXT("取到角色背包"), 13.0f));
	TakeLockerItemButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleTakeLockerItemClicked);
	AddCanvas(LockerCanvas, TakeLockerItemButton, FAnchors(0.0f, 0.0f), FVector2D(18.0f, 452.0f), FVector2D(182.0f, 48.0f));
	CombineStellarWeaponButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipLockerCombine"));
	CombineStellarWeaponButton->SetBackgroundColor(FLinearColor(0.26f, 0.30f, 0.48f, 1.0f));
	CombineStellarWeaponButton->SetContent(MakeButtonLabel(WidgetTree, TEXT("ShipLockerCombineLabel"), TEXT("组合武器"), 13.0f));
	CombineStellarWeaponButton->SetToolTipText(FText::FromString(TEXT("选择已按规则摆放的核心或配件，点击组合武器")));
	CombineStellarWeaponButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleCombineStellarWeaponClicked);
	AddCanvas(LockerCanvas, CombineStellarWeaponButton, FAnchors(0.0f, 0.0f), FVector2D(207.0f, 452.0f), FVector2D(120.0f, 48.0f));
	AddCanvas(LockerCanvas, MakeText(WidgetTree, TEXT("ShipLockerDeleteHint"), TEXT("拖到废纸篓删除"), 12.0f, FLinearColor(0.75f, 0.62f, 0.56f, 1.0f), ETextJustify::Right), FAnchors(0.0f, 0.0f), FVector2D(333.0f, 466.0f), FVector2D(170.0f, 25.0f));
	TrashImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ShipLockerTrashImage"));
	if (UTexture2D* const TrashTexture = TrashIcon.LoadSynchronous())
	{
		TrashImage->SetBrushFromTexture(TrashTexture);
	}
	AddCanvas(LockerCanvas, TrashImage, FAnchors(0.0f, 0.0f), FVector2D(525.0f, 447.0f), FVector2D(66.0f, 66.0f));
	DragGhost = MakeBorder(WidgetTree, TEXT("ShipLockerDragGhost"), FLinearColor(0.08f, 0.29f, 0.36f, 0.91f), 4.0f);
	DragGhostText = MakeText(WidgetTree, TEXT("ShipLockerDragGhostText"), TEXT(""), 12.0f, FLinearColor::White, ETextJustify::Center);
	DragGhost->SetContent(DragGhostText);
	AddCanvas(RootCanvas, DragGhost, FAnchors(0.0f, 0.0f), FVector2D::ZeroVector, FVector2D(140.0f, 56.0f));
	DragGhost->SetVisibility(ESlateVisibility::Collapsed);

	WeaponPage = MakeBorder(WidgetTree, TEXT("ShipWeaponPage"), FLinearColor(0.014f, 0.040f, 0.075f, 1.0f));
	AddCanvas(FrameCanvas, WeaponPage, FAnchors(0.0f, 0.0f), FVector2D(30.0f, 138.0f), FVector2D(620.0f, 520.0f));
	WeaponPanel = GetOwningPlayer() != nullptr
		? CreateWidget<UJTSWeaponUpgradePanel>(GetOwningPlayer())
		: WidgetTree->ConstructWidget<UJTSWeaponUpgradePanel>(UJTSWeaponUpgradePanel::StaticClass(), TEXT("ShipWeaponPanel"));
	WeaponPage->SetContent(WeaponPanel);
	WeaponPage->SetVisibility(ESlateVisibility::Collapsed);


	AbilityPanel = MakeBorder(WidgetTree, TEXT("ShipAbilityPanel"), FLinearColor(0.014f, 0.040f, 0.075f, 1.0f));
	AddCanvas(FrameCanvas, AbilityPanel, FAnchors(0.0f, 0.0f), FVector2D(30.0f, 138.0f), FVector2D(1260.0f, 520.0f));
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
		AddCanvas(AbilityCanvas, Card, FAnchors(0.0f, 0.0f), FVector2D(20.0f, Y), FVector2D(1060.0f, 70.0f));
		UCanvasPanel* const CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("%sCanvas"), *Name.ToString()));
		Card->SetContent(CardCanvas);
		AddCanvas(CardCanvas, MakeText(WidgetTree, *FString::Printf(TEXT("%sTitle"), *Name.ToString()), Title, 18.0f, FLinearColor::White), FAnchors(0.0f, 0.0f), FVector2D(24.0f, 7.0f), FVector2D(570.0f, 25.0f));
		UTextBlock* const Detail = MakeText(WidgetTree, *FString::Printf(TEXT("%sDetail"), *Name.ToString()), TEXT(""), 15.0f, FLinearColor(0.68f, 0.79f, 0.90f, 1.0f));
		AddCanvas(CardCanvas, Detail, FAnchors(0.0f, 0.0f), FVector2D(24.0f, 37.0f), FVector2D(650.0f, 22.0f));
		UButton* const Decrease = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("%sDecrease"), *Name.ToString()));
		Decrease->SetBackgroundColor(FLinearColor(0.12f, 0.18f, 0.27f, 1.0f));
		Decrease->SetContent(MakeText(WidgetTree, *FString::Printf(TEXT("%sDecreaseLabel"), *Name.ToString()), TEXT("−"), 18.0f, FLinearColor::White, ETextJustify::Center));
		UTextBlock* const Rank = MakeText(WidgetTree, *FString::Printf(TEXT("%sRank"), *Name.ToString()), TEXT("0 / 5"), 16.0f, FLinearColor(0.55f, 0.90f, 1.0f, 1.0f), ETextJustify::Center);
		AddCanvas(CardCanvas, Rank, FAnchors(0.0f, 0.0f), FVector2D(768.0f, 21.0f), FVector2D(154.0f, 26.0f));
		AddCanvas(CardCanvas, Decrease, FAnchors(0.0f, 0.0f), FVector2D(930.0f, 14.0f), FVector2D(52.0f, 42.0f));
		UButton* const Increase = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("%sIncrease"), *Name.ToString()));
		Increase->SetBackgroundColor(FLinearColor(0.08f, 0.34f, 0.30f, 1.0f));
		Increase->SetContent(MakeText(WidgetTree, *FString::Printf(TEXT("%sIncreaseLabel"), *Name.ToString()), TEXT("+"), 18.0f, FLinearColor::White, ETextJustify::Center));
		AddCanvas(CardCanvas, Increase, FAnchors(0.0f, 0.0f), FVector2D(996.0f, 14.0f), FVector2D(52.0f, 42.0f));

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
		case EJTSPlayerAbility::Stamina:
			Decrease->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleStaminaDecrease);
			Increase->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleStaminaIncrease);
			break;
		case EJTSPlayerAbility::CriticalChance:
			Decrease->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleCriticalChanceDecrease);
			Increase->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleCriticalChanceIncrease);
			break;
		default:
			break;
		}
		AbilityDetailTexts.Add(Detail);
		AbilityRankTexts.Add(Rank);
		AbilityDecreaseButtons.Add(Decrease);
		AbilityIncreaseButtons.Add(Increase);
	};
	AddAbilityCard(EJTSPlayerAbility::InventorySlots, TEXT("INVENTORY SLOTS"), TEXT("ShipAbilityCargo"), 52.0f);
	AddAbilityCard(EJTSPlayerAbility::StackLimit, TEXT("STACK SIZE"), TEXT("ShipAbilityStack"), 128.0f);
	AddAbilityCard(EJTSPlayerAbility::RunSpeed, TEXT("RUN SPEED"), TEXT("ShipAbilitySpeed"), 204.0f);
	AddAbilityCard(EJTSPlayerAbility::Stamina, TEXT("STAMINA"), TEXT("ShipAbilityStamina"), 280.0f);
	AddAbilityCard(EJTSPlayerAbility::CriticalChance, TEXT("CRITICAL CHANCE"), TEXT("ShipAbilityCritical"), 356.0f);

	ConfirmAbilitiesButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipAbilityConfirm"));
	ConfirmAbilitiesButton->SetBackgroundColor(FLinearColor(0.08f, 0.40f, 0.29f, 1.0f));
	ConfirmAbilitiesButton->SetContent(MakeText(WidgetTree, TEXT("ShipAbilityConfirmLabel"), TEXT("APPLY"), 14.0f, FLinearColor::White, ETextJustify::Center));
	ConfirmAbilitiesButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleConfirmAbilitiesClicked);
	AddCanvas(AbilityCanvas, ConfirmAbilitiesButton, FAnchors(0.0f, 0.0f), FVector2D(864.0f, 438.0f), FVector2D(216.0f, 42.0f));
	ResetAbilitiesButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipAbilityReset"));
	ResetAbilitiesButton->SetBackgroundColor(FLinearColor(0.13f, 0.18f, 0.27f, 1.0f));
	ResetAbilitiesButton->SetContent(MakeText(WidgetTree, TEXT("ShipAbilityResetLabel"), TEXT("UNDO"), 14.0f, FLinearColor::White, ETextJustify::Center));
	ResetAbilitiesButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleResetAbilitiesClicked);
	AddCanvas(AbilityCanvas, ResetAbilitiesButton, FAnchors(0.0f, 0.0f), FVector2D(682.0f, 438.0f), FVector2D(170.0f, 42.0f));
	DebugLevelsButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShipDebugLevels"));
	DebugLevelsButton->SetBackgroundColor(FLinearColor(0.13f, 0.22f, 0.27f, 1.0f));
	DebugLevelsButton->SetContent(MakeText(WidgetTree, TEXT("ShipDebugLevelsLabel"), TEXT("DEBUG +10 LVL"), 13.0f, FLinearColor::White, ETextJustify::Center));
	DebugLevelsButton->SetToolTipText(FText::FromString(TEXT("+10 levels and +10 ability points")));
	DebugLevelsButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleDebugLevelsClicked);
	AddCanvas(AbilityCanvas, DebugLevelsButton, FAnchors(0.0f, 0.0f), FVector2D(20.0f, 438.0f), FVector2D(202.0f, 42.0f));
	AbilityStatusText = MakeText(WidgetTree, TEXT("ShipAbilityStatus"), TEXT("1 POINT PER RANK"), 13.0f, FLinearColor(0.72f, 0.82f, 0.92f, 1.0f), ETextJustify::Center);
	AddCanvas(FrameCanvas, AbilityStatusText, FAnchors(0.5f, 1.0f), FVector2D(0.0f, -19.0f), FVector2D(1060.0f, 24.0f), FVector2D(0.5f, 1.0f));

	StatusText = MakeText(WidgetTree, TEXT("ShipSupplyStatus"), TEXT(""), 13.0f, FLinearColor(0.64f, 0.78f, 0.92f, 1.0f), ETextJustify::Center);
	AddCanvas(FrameCanvas, StatusText, FAnchors(0.5f, 1.0f), FVector2D(0.0f, -43.0f), FVector2D(1060.0f, 24.0f), FVector2D(0.5f, 1.0f));

	SetVisibility(ESlateVisibility::Collapsed);
}

void UJTSShopWidget::RefreshAll()
{
	RefreshResponsiveLayout();
	const bool bResourcesChanged = RefreshWallet();
	if (bResourcesChanged || (CatalogGrid != nullptr && CatalogGrid->GetChildrenCount() == 0))
	{
		RefreshCatalog();
	}
	RefreshAbilities();
	RefreshShipLocker();
	RefreshStellarReel();
	RefreshPageVisibility();
}

void UJTSShopWidget::RefreshResponsiveLayout()
{
	if (!TerminalLayoutSlot || !RootCanvas) return;
	FVector2D Viewport = UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(GetOwningPlayer()).GetLocalSize();
	if (Viewport.X <= 1 || Viewport.Y <= 1) Viewport = GetCachedGeometry().GetLocalSize();
	if (Viewport.X <= 1 || Viewport.Y <= 1) return;
	const AJTSCharacter* const Character = GetOwningPlayer() ? Cast<AJTSCharacter>(GetOwningPlayer()->GetPawn()) : nullptr;
	const UJTSInventoryComponent* const Inventory = Character ? Character->GetInventoryComponent() : nullptr;
	const int32 VisibleItems = Inventory ? FMath::Clamp(Inventory->GetInventoryCapacity() - Inventory->GetQuickbarPageStart(),
		1, UJTSInventoryComponent::MaximumQuickbarSlots) : 1;
	const AJTSPlayerState* const State = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	const auto* Loadout = State ? State->GetStellarLoadout() : nullptr;
	const auto Dock = FJTSGameUILayout::InventoryDock(Viewport, VisibleItems,
		SJTSStellarLoadoutView::LayoutSize(Loadout ? Loadout->GetAvailableSlots() : 9),
		SJTSStellarLoadoutView::SlotCenter(0).Y);
	const FBox2D Bounds = FJTSGameUILayout::TerminalBounds(Viewport, Dock.Top);
	TerminalLayoutSlot->SetPosition(Bounds.Min);
	TerminalLayoutSlot->SetSize(Bounds.GetSize());
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
			? FString::Printf(TEXT("能力  ·  %d"), PlayerState->GetUnspentAbilityPoints())
			: TEXT("能力")));
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

	const TArray<EJTSPlayerAbility, TInlineAllocator<5>> Abilities = {
		EJTSPlayerAbility::InventorySlots,
		EJTSPlayerAbility::StackLimit,
		EJTSPlayerAbility::RunSpeed,
		EJTSPlayerAbility::Stamina,
		EJTSPlayerAbility::CriticalChance };
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
	auto SetVisibilityIfChanged = [](UWidget* Widget, ESlateVisibility NewVisibility)
	{
		if (Widget && Widget->GetVisibility() != NewVisibility) Widget->SetVisibility(NewVisibility);
	};
	if (CatalogPanel != nullptr)
	{
		SetVisibilityIfChanged(CatalogPanel, bShowingAbilityPage || bShowingStellarPage || bShowingWeaponPage
			? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (StellarPanel != nullptr)
	{
		SetVisibilityIfChanged(StellarPanel, !bShowingAbilityPage && !bShowingWeaponPage && bShowingStellarPage
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (WeaponPage != nullptr)
	{
		SetVisibilityIfChanged(WeaponPage, bShowingWeaponPage && !bShowingAbilityPage
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (LockerPanel != nullptr)
	{
		SetVisibilityIfChanged(LockerPanel, bShowingAbilityPage
			? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (AbilityPanel != nullptr)
	{
		SetVisibilityIfChanged(AbilityPanel, bShowingAbilityPage
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (StatusText != nullptr)
	{
		SetVisibilityIfChanged(StatusText, bShowingAbilityPage || StatusText->GetText().IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (AbilityStatusText != nullptr)
	{
		SetVisibilityIfChanged(AbilityStatusText, bShowingAbilityPage
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (SupplyTabButton != nullptr)
	{
		SupplyTabButton->SetBackgroundColor(bShowingAbilityPage || bShowingStellarPage || bShowingWeaponPage
			? FLinearColor(0.10f, 0.16f, 0.25f, 1.0f)
			: FLinearColor(0.08f, 0.29f, 0.43f, 1.0f));
	}
	if (StellarTabButton != nullptr)
	{
		StellarTabButton->SetBackgroundColor(!bShowingAbilityPage && !bShowingWeaponPage && bShowingStellarPage
			? FLinearColor(0.08f, 0.29f, 0.43f, 1.0f)
			: FLinearColor(0.10f, 0.16f, 0.25f, 1.0f));
	}
	if (AbilityTabButton != nullptr)
	{
		AbilityTabButton->SetBackgroundColor(bShowingAbilityPage
			? FLinearColor(0.08f, 0.29f, 0.43f, 1.0f)
			: FLinearColor(0.10f, 0.16f, 0.25f, 1.0f));
	}
	if (WeaponTabButton != nullptr)
	{
		WeaponTabButton->SetBackgroundColor(bShowingWeaponPage && !bShowingAbilityPage
			? FLinearColor(0.08f, 0.29f, 0.43f, 1.0f)
			: FLinearColor(0.10f, 0.16f, 0.25f, 1.0f));
	}

}

void UJTSShopWidget::ToggleShopPage()
{
	// Supply -> stellar -> weapon -> ability -> supply.
	if (bShowingAbilityPage) { bShowingAbilityPage = false; }
	else if (bShowingWeaponPage) { bShowingWeaponPage = false; bShowingAbilityPage = true; }
	else if (bShowingStellarPage) { bShowingStellarPage = false; bShowingWeaponPage = true; }
	else bShowingStellarPage = true;
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
	case EJTSPlayerAbility::Stamina: PendingRanks = &PendingAbilityAllocation.StaminaRanks; break;
	case EJTSPlayerAbility::CriticalChance: PendingRanks = &PendingAbilityAllocation.CriticalChanceRanks; break;
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
	case EJTSPlayerAbility::Stamina: PendingRanks = PendingAbilityAllocation.StaminaRanks; break;
	case EJTSPlayerAbility::CriticalChance: PendingRanks = PendingAbilityAllocation.CriticalChanceRanks; break;
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
	case EJTSPlayerAbility::Stamina:
		return bMaxRank
			? FString::Printf(TEXT("MAX  ·  %.0f endurance"), FJTSPlayerProgressionRules::GetMaxStamina(CurrentRank))
			: FString::Printf(TEXT("Run and climb pool  %.0f → %.0f"),
				FJTSPlayerProgressionRules::GetMaxStamina(CurrentRank),
				FJTSPlayerProgressionRules::GetMaxStamina(PreviewRank));
	case EJTSPlayerAbility::CriticalChance:
		return bMaxRank
			? FString::Printf(TEXT("MAX  ·  %d%% crit chance · 1.75x damage"),
				FJTSPlayerProgressionRules::GetCriticalChancePercent(CurrentRank))
			: FString::Printf(TEXT("Weapon crit chance  %d%% → %d%% · weak points always crit"),
				FJTSPlayerProgressionRules::GetCriticalChancePercent(CurrentRank),
				FJTSPlayerProgressionRules::GetCriticalChancePercent(PreviewRank));
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
		SetReadableTooltip(WidgetTree, Card, BuildItemTooltip(ItemId));

		UCanvasPanel* const CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), *FString::Printf(TEXT("ShipSupplyContents_%d"), static_cast<int32>(ItemId)));
		Card->SetContent(CardCanvas);
		AddCanvas(CardCanvas, MakeBorder(WidgetTree,
			*FString::Printf(TEXT("ShipSupplyAccent_%d"), static_cast<int32>(ItemId)),
			Definition->AccentColor.CopyWithNewOpacity(0.95f)),
			FAnchors(0.0f, 0.0f), FVector2D(10.0f, 10.0f), FVector2D(4.0f, 82.0f));
		AddCanvas(CardCanvas, MakeText(WidgetTree,
			*FString::Printf(TEXT("ShipSupplyRole_%d"), static_cast<int32>(ItemId)),
			ItemRoleLabel(Definition), 12.0f, Definition->AccentColor),
			FAnchors(0.0f, 0.0f), FVector2D(24.0f, 9.0f), FVector2D(250.0f, 20.0f));
		AddCanvas(CardCanvas, MakeText(WidgetTree,
			*FString::Printf(TEXT("ShipSupplyName_%d"), static_cast<int32>(ItemId)),
			Definition->DisplayName.ToString(), 24.0f, FLinearColor::White),
			FAnchors(0.0f, 0.0f), FVector2D(24.0f, 34.0f), FVector2D(420.0f, 36.0f));
		AddCanvas(CardCanvas, MakeText(WidgetTree,
			*FString::Printf(TEXT("ShipSupplyCost_%d"), static_cast<int32>(ItemId)),
			FormatCosts(ItemId), 18.0f, FLinearColor(0.72f, 0.82f, 0.90f, 1.0f)),
			FAnchors(0.0f, 0.0f), FVector2D(24.0f, 80.0f), FVector2D(400.0f, 28.0f));
		if (bAffordable)
		{
			UButton* const BuyButton = WidgetTree->ConstructWidget<UButton>(
				UButton::StaticClass(), *FString::Printf(TEXT("ShipSupplyBuy_%d"), static_cast<int32>(ItemId)));
			BuyButton->SetBackgroundColor(FLinearColor(0.10f, 0.34f, 0.30f, 1.0f));
			BuyButton->SetContent(MakeText(WidgetTree,
				*FString::Printf(TEXT("ShipSupplyBuyLabel_%d"), static_cast<int32>(ItemId)),
				TEXT("BUY"), 13.0f, FLinearColor::White, ETextJustify::Center));
			AddCanvas(CardCanvas, BuyButton, FAnchors(1.0f, 0.0f), FVector2D(-14.0f, 34.0f),
				FVector2D(90.0f, 42.0f), FVector2D(1.0f, 0.0f));
			switch (ItemId)
			{
			case EJTSItemId::ShortBlade: BuyButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleShortBladeBuy); break;
			case EJTSItemId::ShockPole: BuyButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleShockPoleBuy); break;
			case EJTSItemId::PowerHammer: BuyButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandlePowerHammerBuy); break;
			case EJTSItemId::RailPistol: BuyButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleRailPistolBuy); break;
			case EJTSItemId::AssaultRifle: BuyButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleAssaultRifleBuy); break;
			case EJTSItemId::Shotgun: BuyButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleShotgunBuy); break;
			case EJTSItemId::RailSniper: BuyButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleRailSniperBuy); break;
			case EJTSItemId::HeavyMachineGun: BuyButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleHeavyMachineGunBuy); break;
			case EJTSItemId::GrenadeLauncher: BuyButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleGrenadeLauncherBuy); break;
			case EJTSItemId::ArcGun: BuyButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleArcGunBuy); break;
			case EJTSItemId::WaistLamp: BuyButton->OnClicked.AddDynamic(this, &UJTSShopWidget::HandleWaistLampBuy); break;
			default: break;
			}
		}

		USizeBox* const Cell = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), *FString::Printf(TEXT("ShipSupplyCell_%d"), static_cast<int32>(ItemId)));
		Cell->SetWidthOverride(572.0f);
		Cell->SetHeightOverride(120.0f);
		Cell->SetContent(Card);
		CatalogGrid->AddChildToUniformGrid(Cell, CatalogIndex, 0);
		++CatalogIndex;
	}
}

FString UJTSShopWidget::GetStellarItemLabel(FName ItemId) const
{
	const UJTSStellarLootTable* const Table = ActiveSpacecraft.IsValid() ? ActiveSpacecraft->GetStellarLootTable() : nullptr;
	if (const FJTSStellarLootEntry* const Entry = IsValid(Table) ? Table->FindEntry(ItemId) : nullptr)
	{
		return Entry->DisplayName.ToString();
	}
	return ItemId.IsNone() ? TEXT("等待信号") : ItemId.ToString();
}

int32 UJTSShopWidget::FindSelectedStellarCoreSlot() const
{
	const AJTSPlayerState* const State = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	const UJTSStellarLootTable* const Table = ActiveSpacecraft.IsValid()
		? ActiveSpacecraft->GetStellarLootTable() : nullptr;
	if (!IsValid(State) || !IsValid(Table)) return INDEX_NONE;
	for (const int32 CoreIndex : { SelectedLockerSlot, SelectedLockerSlot - 1 })
	{
		if (CoreIndex < 0 || CoreIndex + 1 >= AJTSPlayerState::ShipLockerCapacity) continue;
		const FJTSShipLockerSlot Core = State->GetShipLockerSlot(CoreIndex);
		const FJTSShipLockerSlot Attachment = State->GetShipLockerSlot(CoreIndex + 1);
		if (!Core.IsEmpty() && !Attachment.IsEmpty()
			&& !Core.bPendingStellarReveal && !Attachment.bPendingStellarReveal
			&& Core.StandardItem.IsEmpty() && Attachment.StandardItem.IsEmpty()
			&& Core.StellarCoreId.IsNone() && Attachment.StellarCoreId.IsNone()
			&& Table->CanCombine(Core.StellarItemId, Attachment.StellarItemId))
		{
			return CoreIndex;
		}
	}
	return INDEX_NONE;
}

FString UJTSShopWidget::FormatStellarCosts() const
{
	const UJTSStellarLootTable* const Table = ActiveSpacecraft.IsValid() ? ActiveSpacecraft->GetStellarLootTable() : nullptr;
	if (!IsValid(Table)) return TEXT("遥感奖池未配置");
	TArray<FString> Parts;
	for (const FJTSItemCost& Cost : Table->SpinCosts)
	{
		if (Cost.Amount > 0) Parts.Add(FString::Printf(TEXT("%s %d"), *ResourceLabel(Cost.ResourceType), Cost.Amount));
	}
	return Parts.IsEmpty() ? TEXT("本次遥感免费") : FString::Printf(TEXT("每次消耗  %s"), *FString::Join(Parts, TEXT("  /  ")));
}

void UJTSShopWidget::RefreshStellarReel()
{
	RefreshStellarOddsTooltip();
	const UJTSStellarLootTable* const Table = ActiveSpacecraft.IsValid() ? ActiveSpacecraft->GetStellarLootTable() : nullptr;
	if (ReelCostText) ReelCostText->SetText(FText::FromString(FormatStellarCosts()));
	if (StellarRollButton) StellarRollButton->SetIsEnabled(IsValid(Table) && !Table->Entries.IsEmpty() && !bRollAnimating);
	if (DebugStellarItemsButton) DebugStellarItemsButton->SetIsEnabled(IsValid(Table) && !Table->Entries.IsEmpty() && !bDebugStellarGrantPending);
	if (!IsValid(Table) || Table->Entries.IsEmpty())
	{
		if (ReelCurrentText) ReelCurrentText->SetText(FText::FromString(TEXT("奖池待配置")));
		return;
	}
	if (!bRollAnimating && !RolledStellarItemId.IsNone())
	{
		if (ReelPreviousText) ReelPreviousText->SetText(FText::GetEmpty());
		if (ReelCurrentText) ReelCurrentText->SetText(FText::FromString(GetStellarItemLabel(RolledStellarItemId)));
		if (ReelNextText) ReelNextText->SetText(FText::GetEmpty());
		return;
	}
	const int32 Count = Table->Entries.Num();
	const int32 Index = FMath::Clamp(ReelDisplayIndex, 0, Count - 1);
	if (ReelPreviousText) ReelPreviousText->SetText(Table->Entries[(Index + Count - 1) % Count].DisplayName);
	if (ReelCurrentText) ReelCurrentText->SetText(Table->Entries[Index].DisplayName);
	if (ReelNextText) ReelNextText->SetText(Table->Entries[(Index + 1) % Count].DisplayName);
}

void UJTSShopWidget::RefreshStellarOddsTooltip()
{
	FString Tooltip;
	if (!bRollAnimating)
	{
		const UJTSStellarLootTable* const Table = ActiveSpacecraft.IsValid()
			? ActiveSpacecraft->GetStellarLootTable() : nullptr;
		const AJTSPlayerState* const State = GetOwningPlayer()
			? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
		TArray<double> Probabilities;
		if (IsValid(Table) && IsValid(State)
			&& Table->GetRollProbabilities(State->GetDiscoveredStellarCoreIds(), Probabilities))
		{
			Tooltip = FString::Printf(TEXT("个人动态奖池（已爆过核心 %d 种，下次抽奖概率）"),
				Table->CountDiscoveredCoreKinds(State->GetDiscoveredStellarCoreIds()));
			for (int32 Index = 0; Index < Table->Entries.Num(); ++Index)
			{
				const FJTSStellarLootEntry& Entry = Table->Entries[Index];
				const double Percent = Probabilities[Index] * 100.0;
				const FString Chance = Percent >= 0.01
					? FString::Printf(TEXT("%.2f%%"), Percent)
					: FString::Printf(TEXT("%.4f%%"), Percent);
				const FString Name = Entry.DisplayName.IsEmpty()
					? Entry.ItemId.ToString() : Entry.DisplayName.ToString();
				Tooltip += FString::Printf(TEXT("\n%s  %s"), *Name, *Chance);
			}
		}
		else
		{
			Tooltip = TEXT("遥感奖池暂不可用");
		}
	}
	auto SetTooltipIfChanged = [&Tooltip](UWidget* Widget)
	{
		if (Widget && Widget->GetToolTipText().ToString() != Tooltip)
		{
			Widget->SetToolTipText(FText::FromString(Tooltip));
		}
	};
	SetTooltipIfChanged(LeverKnob);
	SetTooltipIfChanged(LeverStem);
	SetTooltipIfChanged(StellarRollButton);
}

void UJTSShopWidget::SetLeverPull(float Distance)
{
	LeverPull = FMath::Clamp(Distance, 0.0f, 90.0f);
	if (UCanvasPanelSlot* const StemSlot = Cast<UCanvasPanelSlot>(LeverStem ? LeverStem->Slot : nullptr))
	{
		StemSlot->SetPosition(FVector2D(540.0f, 190.0f + LeverPull));
		StemSlot->SetSize(FVector2D(9.0f, 132.0f - LeverPull));
	}
	if (UCanvasPanelSlot* const KnobSlot = Cast<UCanvasPanelSlot>(LeverKnob ? LeverKnob->Slot : nullptr))
	{
		KnobSlot->SetPosition(FVector2D(521.0f, 160.0f + LeverPull));
	}
}

bool UJTSShopWidget::CanFinishStellarRoll() const
{
	const AJTSPlayerState* const State = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	if (!IsValid(State) || RolledLockerSlotIndex < 0 || RolledStellarItemId.IsNone()) return false;
	const FJTSShipLockerSlot LockerEntry = State->GetShipLockerSlot(RolledLockerSlotIndex);
	return !LockerEntry.bPendingStellarReveal && LockerEntry.StellarItemId == RolledStellarItemId;
}

void UJTSShopWidget::FinishStellarRoll()
{
	bRollAnimating = false;
	SetLeverPull(0.0f);
	if (ReelPreviousText)
	{
		if (UCanvasPanelSlot* const ReelSlot = Cast<UCanvasPanelSlot>(ReelPreviousText->Slot)) ReelSlot->SetPosition(FVector2D(18.0f, 40.0f));
	}
	if (ReelCurrentText)
	{
		if (UCanvasPanelSlot* const ReelSlot = Cast<UCanvasPanelSlot>(ReelCurrentText->Slot)) ReelSlot->SetPosition(FVector2D(18.0f, 114.0f));
	}
	if (ReelNextText)
	{
		if (UCanvasPanelSlot* const ReelSlot = Cast<UCanvasPanelSlot>(ReelNextText->Slot)) ReelSlot->SetPosition(FVector2D(18.0f, 192.0f));
	}
	RefreshStellarReel();
	SetStatus(FString::Printf(TEXT("获得：%s · 已放入飞船物品栏"), *GetStellarItemLabel(RolledStellarItemId)), false);
	RefreshShipLocker();
	RolledLockerSlotIndex = INDEX_NONE;
}

void UJTSShopWidget::RefreshShipLocker()
{
	const AJTSPlayerState* const State = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	if (!State) return;
	const UJTSStellarLootTable* const Table = ActiveSpacecraft.IsValid() ? ActiveSpacecraft->GetStellarLootTable() : nullptr;
	int32 Used = 0;
	for (int32 Index = 0; Index < AJTSPlayerState::ShipLockerCapacity && LockerSlotTexts.IsValidIndex(Index) && LockerSlotBorders.IsValidIndex(Index); ++Index)
	{
		const FJTSShipLockerSlot LockerEntry = State->GetShipLockerSlot(Index);
		const bool bConcealed = LockerEntry.bPendingStellarReveal
			|| (bRollAnimating && Index == RolledLockerSlotIndex);
		const bool bEmpty = LockerEntry.IsEmpty();
		if (!bEmpty) ++Used;
		FString Label = FString::Printf(TEXT("%02d"), Index + 1);
		if (!bConcealed && !LockerEntry.StandardItem.IsEmpty())
		{
			Label = UJTSItemDefinitionLibrary::GetItemDisplayName(LockerEntry.StandardItem.ItemId).ToString();
		}
		else if (!bConcealed && !LockerEntry.StellarItemId.IsNone())
		{
			Label = LockerEntry.StellarCoreId.IsNone()
				? GetStellarItemLabel(LockerEntry.StellarItemId)
				: FString::Printf(TEXT("%s·%s"),
					*GetStellarItemLabel(LockerEntry.StellarCoreId),
					*GetStellarItemLabel(LockerEntry.StellarItemId));
		}
		if (LockerSlotTexts[Index]->GetText().ToString() != Label)
		{
			LockerSlotTexts[Index]->SetText(FText::FromString(Label));
		}
		const int32 VisualState = bConcealed ? 3 : Index == SelectedLockerSlot ? 2 : bEmpty ? 0 : 1;
		if (!LockerVisualStates.IsValidIndex(Index) || LockerVisualStates[Index] != VisualState)
		{
			LockerSlotTexts[Index]->SetColorAndOpacity(FSlateColor((bEmpty || bConcealed)
				? FLinearColor(0.62f, 0.72f, 0.80f, 1.0f)
				: FLinearColor(0.91f, 0.96f, 1.0f, 1.0f)));
			LockerSlotBorders[Index]->SetBrushColor(bConcealed
				? FLinearColor(0.13f, 0.15f, 0.14f, 1.0f)
				: Index == SelectedLockerSlot ? FLinearColor(0.10f, 0.34f, 0.38f, 1.0f)
					: bEmpty ? FLinearColor(0.018f, 0.038f, 0.060f, 1.0f)
						: FLinearColor(0.08f, 0.19f, 0.23f, 1.0f));
			if (LockerVisualStates.IsValidIndex(Index)) LockerVisualStates[Index] = VisualState;
		}
		FString Tooltip = bConcealed ? TEXT("遥感扫描中，格子暂时锁定") : Label;
		if (!bConcealed && !LockerEntry.StellarItemId.IsNone() && IsValid(Table))
		{
			const FString Description = Table->GetItemDescription(LockerEntry.StellarItemId, LockerEntry.StellarCoreId).ToString();
			if (!Description.IsEmpty())
			{
				Tooltip = FString::Printf(TEXT("%s\n\n%s"), *Label, *Description);
			}
		}
		if (LockerSlotBorders[Index]->GetToolTipText().ToString() != Tooltip)
		{
			SetReadableTooltip(WidgetTree, LockerSlotBorders[Index], FText::FromString(Tooltip));
		}
		if (LockerSlotBadges.IsValidIndex(Index) && LockerSlotBadges[Index])
		{
			const bool bUpgradeable = !bConcealed && !LockerEntry.StandardItem.IsEmpty()
				&& FJTSWeaponProgression::IsUpgradeable(LockerEntry.StandardItem.ItemId);
			const FString BadgeText = bUpgradeable
				? FString::FromInt(FJTSWeaponProgression::SumPoints(LockerEntry.StandardItem.WeaponPoints)) : FString();
			if (LockerSlotBadges[Index]->GetText().ToString() != BadgeText)
			{
				LockerSlotBadges[Index]->SetText(FText::FromString(BadgeText));
			}
		}
	}
	const FString CountLabel = FString::Printf(TEXT("%d / 30"), Used);
	if (LockerCountText && LockerCountText->GetText().ToString() != CountLabel)
	{
		LockerCountText->SetText(FText::FromString(CountLabel));
	}
	if (TakeLockerItemButton)
	{
		const FJTSShipLockerSlot Selected = State->GetShipLockerSlot(SelectedLockerSlot);
		const AJTSCharacter* const Character = GetOwningPlayer() ? Cast<AJTSCharacter>(GetOwningPlayer()->GetPawn()) : nullptr;
		const UJTSInventoryComponent* const Inventory = IsValid(Character) ? Character->GetInventoryComponent() : nullptr;
		const bool bStellar = !Selected.StellarItemId.IsNone()
			|| Selected.StandardItem.ItemId == EJTSItemId::StellarText
			|| Selected.StandardItem.ItemId == EJTSItemId::StellarWeapon;
		TakeLockerItemButton->SetToolTipText(bStellar
			? FText::FromString(TEXT("星际核心和配件请拖到人物右侧星际栏")) : FText::GetEmpty());
		TakeLockerItemButton->SetIsEnabled(!bStellar && !Selected.StandardItem.IsEmpty() && !Selected.bPendingStellarReveal
			&& !(bRollAnimating && SelectedLockerSlot == RolledLockerSlotIndex)
			&& IsValid(Inventory) && Inventory->CanAddItem(Selected.StandardItem.ItemId, Selected.StandardItem.StackCount));
	}
	if (CombineStellarWeaponButton)
	{
		CombineStellarWeaponButton->SetIsEnabled(FindSelectedStellarCoreSlot() != INDEX_NONE);
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
	if (const UJTSStellarLootTable* const Table = Spacecraft->GetStellarLootTable())
	{
		for (const FJTSItemCost& Cost : Table->SpinCosts)
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
	const AJTSPlayerState* const State = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	if (State && !State->HasFreeShipLockerSlot())
	{
		SetStatus(TEXT("物品栏已满，请拖拽物品到废纸篓删除"), true);
		return;
	}
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

void UJTSShopWidget::HandleShortBladeBuy() { RequestPurchase(EJTSItemId::ShortBlade); }
void UJTSShopWidget::HandleShockPoleBuy() { RequestPurchase(EJTSItemId::ShockPole); }
void UJTSShopWidget::HandlePowerHammerBuy() { RequestPurchase(EJTSItemId::PowerHammer); }
void UJTSShopWidget::HandleRailPistolBuy() { RequestPurchase(EJTSItemId::RailPistol); }
void UJTSShopWidget::HandleAssaultRifleBuy() { RequestPurchase(EJTSItemId::AssaultRifle); }
void UJTSShopWidget::HandleShotgunBuy() { RequestPurchase(EJTSItemId::Shotgun); }
void UJTSShopWidget::HandleRailSniperBuy() { RequestPurchase(EJTSItemId::RailSniper); }
void UJTSShopWidget::HandleHeavyMachineGunBuy() { RequestPurchase(EJTSItemId::HeavyMachineGun); }
void UJTSShopWidget::HandleGrenadeLauncherBuy() { RequestPurchase(EJTSItemId::GrenadeLauncher); }
void UJTSShopWidget::HandleArcGunBuy() { RequestPurchase(EJTSItemId::ArcGun); }
void UJTSShopWidget::HandleWaistLampBuy() { RequestPurchase(EJTSItemId::WaistLamp); }
void UJTSShopWidget::HandleDebugResourcesClicked()
{
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		if (ActiveSpacecraft.IsValid()) Controller->ServerRequestShopDebugResources(ActiveSpacecraft.Get());
	}
}
void UJTSShopWidget::HandleDebugStellarItemsClicked()
{
	AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer());
	if (!IsValid(Controller) || !ActiveSpacecraft.IsValid() || bDebugStellarGrantPending) return;
	bDebugStellarGrantPending = true;
	RefreshStellarReel();
	Controller->ServerRequestDebugStellarItems(ActiveSpacecraft.Get());
}

void UJTSShopWidget::HandleDebugLevelsClicked()
{
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		if (ActiveSpacecraft.IsValid()) Controller->ServerRequestDebugAbilityLevels(ActiveSpacecraft.Get());
	}
}
void UJTSShopWidget::HandleSupplyTabClicked() { bShowingAbilityPage = false; bShowingStellarPage = false; bShowingWeaponPage = false; RefreshAll(); }
void UJTSShopWidget::HandleStellarTabClicked() { bShowingAbilityPage = false; bShowingStellarPage = true; bShowingWeaponPage = false; RefreshAll(); }
void UJTSShopWidget::HandleWeaponTabClicked() { bShowingAbilityPage = false; bShowingStellarPage = false; bShowingWeaponPage = true; RefreshAll(); }
void UJTSShopWidget::HandleAbilityTabClicked() { bShowingAbilityPage = true; bShowingStellarPage = false; bShowingWeaponPage = false; RefreshAll(); }

void UJTSShopWidget::HandleStellarRollClicked()
{
	if (bRollAnimating || !ActiveSpacecraft.IsValid()) return;
	const AJTSPlayerState* const State = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	if (State && !State->HasFreeShipLockerSlot())
	{
		SetStatus(TEXT("物品栏已满，请拖拽物品到废纸篓删除"), true);
		return;
	}
	bRollAnimating = true;
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().CloseToolTip();
	SetLeverPull(90.0f);
	bRollResultReceived = false;
	RolledStellarItemId = NAME_None;
	RolledLockerSlotIndex = INDEX_NONE;
	RollElapsed = 0.0f;
	ReelStepAccumulator = 0.0f;
	SetStatus(TEXT("遥感扫描中…"), false);
	RefreshStellarReel();
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		Controller->ServerRequestStellarRoll(ActiveSpacecraft.Get(), FGuid::NewGuid());
	}
}

void UJTSShopWidget::HandleTakeLockerItemClicked()
{
	const AJTSPlayerState* const State = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	if (!State || SelectedLockerSlot == INDEX_NONE || !ActiveSpacecraft.IsValid()) return;
	const FJTSShipLockerSlot LockerEntry = State->GetShipLockerSlot(SelectedLockerSlot);
	if (LockerEntry.IsEmpty() || LockerEntry.bPendingStellarReveal) return;
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		Controller->ServerTakeShipLockerSlot(ActiveSpacecraft.Get(), SelectedLockerSlot, LockerEntry.SlotToken);
	}
}

void UJTSShopWidget::HandleCombineStellarWeaponClicked()
{
	const int32 CoreSlotIndex = FindSelectedStellarCoreSlot();
	const AJTSPlayerState* const State = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	if (CoreSlotIndex == INDEX_NONE || !IsValid(State) || !ActiveSpacecraft.IsValid()) return;
	const FJTSShipLockerSlot Core = State->GetShipLockerSlot(CoreSlotIndex);
	const FJTSShipLockerSlot Attachment = State->GetShipLockerSlot(CoreSlotIndex + 1);
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		Controller->ServerCombineStellarSlots(ActiveSpacecraft.Get(), CoreSlotIndex,
			Core.SlotToken, Attachment.SlotToken);
	}
}
void UJTSShopWidget::HandleInventorySlotsDecrease() { AdjustPendingAbility(EJTSPlayerAbility::InventorySlots, -1); }
void UJTSShopWidget::HandleInventorySlotsIncrease() { AdjustPendingAbility(EJTSPlayerAbility::InventorySlots, 1); }
void UJTSShopWidget::HandleStackLimitDecrease() { AdjustPendingAbility(EJTSPlayerAbility::StackLimit, -1); }
void UJTSShopWidget::HandleStackLimitIncrease() { AdjustPendingAbility(EJTSPlayerAbility::StackLimit, 1); }
void UJTSShopWidget::HandleRunSpeedDecrease() { AdjustPendingAbility(EJTSPlayerAbility::RunSpeed, -1); }
void UJTSShopWidget::HandleRunSpeedIncrease() { AdjustPendingAbility(EJTSPlayerAbility::RunSpeed, 1); }
void UJTSShopWidget::HandleStaminaDecrease() { AdjustPendingAbility(EJTSPlayerAbility::Stamina, -1); }
void UJTSShopWidget::HandleStaminaIncrease() { AdjustPendingAbility(EJTSPlayerAbility::Stamina, 1); }
void UJTSShopWidget::HandleCriticalChanceDecrease() { AdjustPendingAbility(EJTSPlayerAbility::CriticalChance, -1); }
void UJTSShopWidget::HandleCriticalChanceIncrease() { AdjustPendingAbility(EJTSPlayerAbility::CriticalChance, 1); }

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

void UJTSShopWidget::OpenAttachmentDialog(int32 LockerSlotIndex)
{
	if (!ActiveSpacecraft.IsValid() || !GetOwningPlayer()) return;

	if (bShowingWeaponPage)
	{
		// On the weapon page a click picks the weapon to modify; the left panel shows its skills.
		if (!IsValid(WeaponPanel) || !WeaponPanel->SelectSlot(ActiveSpacecraft.Get(), LockerSlotIndex))
		{
			SetStatus(TEXT("这里只能改装普通武器"), true);
		}
		return;
	}
	if (!IsValid(AttachmentDialog))
	{
		AttachmentDialog = CreateWidget<UJTSStellarAttachmentDialog>(GetOwningPlayer());
		if (IsValid(AttachmentDialog))
		{
			AttachmentDialog->OnDialogClosed.BindUObject(this, &UJTSShopWidget::HandleAttachmentDialogClosed);
		}
	}
	if (IsValid(AttachmentDialog) && !AttachmentDialog->OpenForSlot(ActiveSpacecraft.Get(), LockerSlotIndex))
	{
		SetStatus(TEXT("核心可直接升级；配件激活后才能加点。普通武器请到“武器改装”页"), true);
	}
}

void UJTSShopWidget::HandleAttachmentDialogClosed()
{
	SetKeyboardFocus();
}

void UJTSShopWidget::NotifyWeaponUpgradeResult(bool bUpgrade, bool bSucceeded)
{
	if (IsValid(WeaponPanel)) WeaponPanel->NotifyUpgradeResult(bUpgrade, bSucceeded);
}

void UJTSShopWidget::NotifyStellarProgressionResult(bool bUpgrade, bool bSucceeded)
{
	if (IsValid(AttachmentDialog)) AttachmentDialog->NotifyProgressionResult(bUpgrade, bSucceeded);
	if (bUpgrade)
	{
		SetStatus(bSucceeded ? TEXT("核心升级成功") : TEXT("升级失败，请检查固件数量、核心等级或物品是否已变化"), !bSucceeded);
		RefreshShipLocker();
	}
}
