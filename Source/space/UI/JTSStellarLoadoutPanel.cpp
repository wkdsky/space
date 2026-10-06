#include "space/UI/JTSStellarLoadoutPanel.h"
#include "space/Components/JTSStellarLoadoutComponent.h"
#include "space/Items/JTSStellarLootTable.h"
#include "space/Items/JTSStellarProgression.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Player/JTSPlayerController.h"
#include "space/UI/JTSStellarAttachmentDialog.h"
#include "space/UI/SJTSStellarLoadoutView.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SToolTip.h"

UJTSStellarLoadoutComponent* UJTSStellarLoadoutPanel::Loadout() const
{
	const auto* PS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	return PS ? PS->GetStellarLoadout() : nullptr;
}

int32 UJTSStellarLoadoutPanel::GetSlotAtScreenPosition(FVector2D Screen) const
{
	if (!IsVisible()) return INDEX_NONE;
	return SlotView ? SlotView->FindSlot(Screen) : INDEX_NONE;
}

void UJTSStellarLoadoutPanel::OpenDetails(int32 Index)
{
	if (!GetOwningPlayer()) return;
	if (!AttachmentDialog) AttachmentDialog = CreateWidget<UJTSStellarAttachmentDialog>(GetOwningPlayer());
	if (AttachmentDialog && AttachmentDialog->OpenForLoadoutSlot(Index))
		if (auto* PC = Cast<AJTSPlayerController>(GetOwningPlayer())) PC->OpenStellarItemDialog(AttachmentDialog);
}

FReply UJTSStellarLoadoutPanel::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const int32 Index = GetSlotAtScreenPosition(Event.GetScreenSpacePosition());
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton || Index == INDEX_NONE) return Super::NativeOnMouseButtonDown(Geometry, Event);
	const auto* State = Loadout();
	if (!State || State->GetSlot(Index).IsEmpty()) return FReply::Handled();
	DraggedSlot = Index; DraggedId = State->GetSlot(Index).InstanceId;
	PressPosition = Event.GetScreenSpacePosition(); bDragging = false;
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UJTSStellarLoadoutPanel::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (DraggedSlot == INDEX_NONE) return Super::NativeOnMouseMove(Geometry, Event);
	bDragging |= FVector2D::Distance(PressPosition, Event.GetScreenSpacePosition()) >= 8.0f;
	if (auto* PC = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		PC->SetStellarItemDragging(bDragging);
		const auto* State = Loadout();
		const auto Item = State ? State->GetSlot(DraggedSlot) : FJTSItemInstance();
		PC->UpdateShipCarriedDragPreview(Event.GetScreenSpacePosition(), Item.CustomDisplayName.ToString(), bDragging);
	}
	return FReply::Handled();
}

FReply UJTSStellarLoadoutPanel::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (DraggedSlot == INDEX_NONE || Event.GetEffectingButton() != EKeys::LeftMouseButton) return Super::NativeOnMouseButtonUp(Geometry, Event);
	const int32 From = DraggedSlot;
	const FGuid Id = DraggedId;
	const bool bWasDragging = bDragging || FVector2D::Distance(PressPosition, Event.GetScreenSpacePosition()) >= 8.0f;
	ResetDrag();
	if (auto* State = Loadout())
	{
		const int32 To = GetSlotAtScreenPosition(Event.GetScreenSpacePosition());
		if (!bWasDragging && To == From) OpenDetails(From);
		else if (bWasDragging && To != INDEX_NONE) State->ServerMoveSlot(From, Id, To, State->GetSlot(To).InstanceId);
		else if (bWasDragging) if (auto* PC = Cast<AJTSPlayerController>(GetOwningPlayer()))
		{
			PC->DropStellarItemInSpaceShop(Event.GetScreenSpacePosition(), From, Id);
		}
	}
	return FReply::Handled().ReleaseMouseCapture();
}

void UJTSStellarLoadoutPanel::ResetDrag()
{
	if (auto* PC = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		PC->SetStellarItemDragging(false);
		PC->UpdateShipCarriedDragPreview(FVector2D::ZeroVector, FString(), false);
	}
	DraggedSlot = INDEX_NONE; DraggedId.Invalidate(); bDragging = false;
}
void UJTSStellarLoadoutPanel::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{
	Super::NativeOnMouseCaptureLost(Event); ResetDrag();
}
void UJTSStellarLoadoutPanel::NativeDestruct()
{
	if (AttachmentDialog) AttachmentDialog->CloseDialog();
	ResetDrag(); Super::NativeDestruct();
}

TSharedRef<SWidget> UJTSStellarLoadoutPanel::RebuildWidget()
{
	return SAssignNew(SlotView, SJTSStellarLoadoutView).Loadout([this] { return Loadout(); })
		.ToolTip(SNew(SToolTip)
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
		.TextMargin(FMargin(14))
		.Text_Lambda([this]
		{
			const int32 Index = GetSlotAtScreenPosition(FSlateApplication::Get().GetCursorPos());
			const auto* State = Loadout();
			const auto* Table = State ? State->GetLootTable() : nullptr;
			if (!State || Index == INDEX_NONE) return FText::GetEmpty();
			const auto Item = State->GetSlot(Index);
			const FString Role = Index == 0 ? TEXT("备用槽：核心或配件，不参与激活")
				: FJTSStellarLoadoutRules::IsCoreSlot(Index) ? TEXT("核心槽：与正下方配件配对") : TEXT("配件槽：与正上方核心配对");
			FString ProgressionHint;
			const auto* Entry = Table ? Table->FindEntry(Item.StellarItemId) : nullptr;
			if (Entry && Entry->bCore) ProgressionHint = TEXT("点击升级核心");
			else if (Entry && FJTSStellarProgression::FindSkillDefs(Item.StellarItemId))
			{
				ProgressionHint = State->GetWeapons().ContainsByPredicate([Index](const auto& Weapon)
					{ return Weapon.CoreSlot + 1 == Index; }) ? TEXT("点击分配点数") : TEXT("未激活：配对后才能分配点数");
			}
			return FText::FromString(Item.IsEmpty() ? Role : FString::Printf(TEXT("%s\n%s\n%s\n%s"),
				*Item.CustomDisplayName.ToString(), *Role,
				Table ? *Table->GetItemDescription(Item.StellarItemId).ToString() : TEXT(""), *ProgressionHint));
		}));
}

void UJTSStellarLoadoutPanel::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	SlotView.Reset();
}
