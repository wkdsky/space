// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSStellarAttachmentDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "space/Items/JTSStellarLootTable.h"
#include "space/Components/JTSStellarLoadoutComponent.h"
#include "Widgets/Layout/SScaleBox.h"
#include "space/Items/JTSStellarProgression.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Ships/JTSSpacecraftActor.h"

namespace
{
	FSlateFontInfo DialogFont(int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size);
	}

	TSharedRef<SWidget> MakeLabelledButton(const FText& Label, const FLinearColor& Color, int32 FontSize,
		TFunction<FReply()> OnClicked, TFunction<bool()> IsEnabled)
	{
		return SNew(SButton)
			.ButtonColorAndOpacity(Color)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.IsEnabled_Lambda([IsEnabled]() { return IsEnabled(); })
			.OnClicked_Lambda([OnClicked]() { return OnClicked(); })
			[
				SNew(STextBlock).Text(Label).Font(DialogFont(FontSize))
			];
	}
}

bool UJTSStellarAttachmentDialog::OpenForSlot(AJTSSpacecraftActor* Spacecraft, int32 InSlotIndex)
{
	const AJTSPlayerState* const State = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	if (!IsValid(Spacecraft) || !IsValid(State)) return false;
	const FJTSShipLockerSlot LockerSlot = State->GetShipLockerSlot(InSlotIndex);
	bLoadoutContext = false;
	ActiveSpacecraft = Spacecraft;
	SlotIndex = InSlotIndex;
	OpenedToken = LockerSlot.SlotToken;
	if (LockerSlot.IsEmpty() || LockerSlot.bPendingStellarReveal || !LockerSlot.StandardItem.IsEmpty()
		|| !CanInspect(LockerSlot))
	{
		return false;
	}
	Draft = GetRecordedPoints(LockerSlot);
	StatusMessage.Reset();
	bStatusIsError = false;
	bAwaitingAction = false;
	bOpen = true;
	if (!IsInViewport()) AddToViewport(300);
	SetIsFocusable(true);
	SetKeyboardFocus();
	return true;
}

UJTSStellarLoadoutComponent* UJTSStellarAttachmentDialog::Loadout() const
{
	const auto* PS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	return PS ? PS->GetStellarLoadout() : nullptr;
}
const UJTSStellarLootTable* UJTSStellarAttachmentDialog::LootTable() const
{
	return bLoadoutContext ? (Loadout() ? Loadout()->GetLootTable() : nullptr)
		: (ActiveSpacecraft.IsValid() ? ActiveSpacecraft->GetStellarLootTable() : nullptr);
}
bool UJTSStellarAttachmentDialog::IsEditable() const
{
	return !bLoadoutContext || (Loadout() && SlotIndex >= 0 && SlotIndex < Loadout()->GetAvailableSlots());
}
bool UJTSStellarAttachmentDialog::OpenForLoadoutSlot(int32 InSlotIndex)
{
	if (!Loadout()) return false;
	bLoadoutContext = true; SlotIndex = InSlotIndex;
	const auto ItemSlot = ReadSlot(); OpenedToken = ItemSlot.SlotToken;
	if (ItemSlot.IsEmpty() || !CanInspect(ItemSlot)) return false;
	Draft = GetRecordedPoints(ItemSlot); OpenedRevision = Loadout()->GetRevision();
	ObservedActionResponse = Loadout()->GetActionResponseCount();
	StatusMessage = IsEditable() ? TEXT("") : TEXT("封格只读，请先拖至可用格或仓库");
	bStatusIsError = false; bAwaitingAction = false; bOpen = true;
	if (!IsInViewport()) AddToViewport(400);
	SetIsFocusable(true); SetKeyboardFocus(); return true;
}
void UJTSStellarAttachmentDialog::NativeTick(const FGeometry& Geometry, float Delta)
{
	Super::NativeTick(Geometry, Delta);
	if (!bOpen) return;
	const auto ItemSlot = ReadSlot();
	if (!IsSlotStillValid(ItemSlot) || !CanInspect(ItemSlot))
	{
		CloseDialog();
		return;
	}
	if (!bLoadoutContext || !Loadout()) return;
	if (OpenedRevision != Loadout()->GetRevision() || ObservedActionResponse != Loadout()->GetActionResponseCount())
	{
		const bool bResponse = ObservedActionResponse != Loadout()->GetActionResponseCount();
		if (bResponse) bAwaitingAction = false;
		Draft = GetRecordedPoints(ReadSlot()); OpenedRevision = Loadout()->GetRevision();
		ObservedActionResponse = Loadout()->GetActionResponseCount();
		SetStatus(IsEditable() ? (bResponse ? Loadout()->GetLastActionMessage() : TEXT("装备已更新，草案已刷新"))
			: TEXT("封格只读，请先拖至可用格或仓库"), false);
	}
}

void UJTSStellarAttachmentDialog::CloseDialog()
{
	if (!bOpen) return;
	bOpen = false;
	bAwaitingAction = false;
	RemoveFromParent();
	OnDialogClosed.ExecuteIfBound();
}

FReply UJTSStellarAttachmentDialog::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Esc closes the dialog first; the shop behind it stays open.
	if (bOpen && InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseDialog();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UJTSStellarAttachmentDialog::NotifyProgressionResult(bool bUpgrade, bool bSucceeded)
{
	if (!bOpen) return;
	bAwaitingAction = false;
	Draft = GetRecordedPoints(ReadSlot());
	if (bSucceeded)
	{
		SetStatus(bUpgrade ? TEXT("已永久增加 1 点") : TEXT("已应用"), false);
	}
	else
	{
		SetStatus(bUpgrade ? TEXT("升级失败：固件不足或物品已变化") : TEXT("应用失败：物品已变化或点数超出核心预算"), true);
	}
}

FJTSShipLockerSlot UJTSStellarAttachmentDialog::ReadSlot() const
{
	const AJTSPlayerState* const State = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	if (bLoadoutContext && Loadout())
	{
		const auto Item = Loadout()->GetSlot(SlotIndex);
		FJTSShipLockerSlot Result;
		if (Item.IsEmpty()) return Result;
		Result.StellarItemId = Item.StellarItemId; Result.SlotToken = Item.InstanceId;
		Result.StellarCoreLevel = Item.StellarCoreLevel; Result.StellarPoints = Item.StellarPoints;
		for (const auto& Weapon : Loadout()->GetWeapons()) if (Weapon.CoreSlot + 1 == SlotIndex)
		{
			Result.StellarCoreId = Weapon.CoreId;
			Result.StellarCoreLevel = Loadout()->GetSlot(Weapon.CoreSlot).StellarCoreLevel;
		}
		return Result;
	}
	return IsValid(State) ? State->GetShipLockerSlot(SlotIndex) : FJTSShipLockerSlot();
}

bool UJTSStellarAttachmentDialog::IsSlotStillValid(const FJTSShipLockerSlot& LockerSlot) const
{
	return !LockerSlot.IsEmpty() && !LockerSlot.bPendingStellarReveal && LockerSlot.StandardItem.IsEmpty()
		&& OpenedToken.IsValid() && LockerSlot.SlotToken == OpenedToken;
}

bool UJTSStellarAttachmentDialog::HasAttachment(const FJTSShipLockerSlot& LockerSlot) const
{
	const UJTSStellarLootTable* const Table = LootTable();
	const FJTSStellarLootEntry* const LootEntry = IsValid(Table) ? Table->FindEntry(LockerSlot.StellarItemId) : nullptr;
	return LootEntry && !LootEntry->bCore && FJTSStellarProgression::FindSkillDefs(LockerSlot.StellarItemId) != nullptr;
}

bool UJTSStellarAttachmentDialog::HasCore(const FJTSShipLockerSlot& LockerSlot) const
{
	const UJTSStellarLootTable* const Table = LootTable();
	if (!IsValid(Table)) return false;
	const FJTSStellarLootEntry* const LootEntry = Table->FindEntry(LockerSlot.StellarItemId);
	return LootEntry && LootEntry->bCore;
}

bool UJTSStellarAttachmentDialog::CanInspect(const FJTSShipLockerSlot& ItemSlot) const
{
	return HasCore(ItemSlot) || (HasAttachment(ItemSlot) && IsMatchedWeapon(ItemSlot));
}

bool UJTSStellarAttachmentDialog::IsMatchedWeapon(const FJTSShipLockerSlot& LockerSlot) const
{
	const UJTSStellarLootTable* const Table = LootTable();
	return IsValid(Table) && !LockerSlot.StellarCoreId.IsNone() && Table->CanCombine(LockerSlot.StellarCoreId, LockerSlot.StellarItemId);
}

int32 UJTSStellarAttachmentDialog::GetBudget(const FJTSShipLockerSlot& LockerSlot) const
{
	return IsMatchedWeapon(LockerSlot) ? FJTSStellarProgression::GetCoreBudget(LockerSlot.StellarCoreLevel) : 0;
}

TArray<uint8> UJTSStellarAttachmentDialog::GetRecordedPoints(const FJTSShipLockerSlot& LockerSlot) const
{
	TArray<uint8> Points = LockerSlot.StellarPoints;
	FJTSStellarProgression::NormalizePoints(Points);
	return Points;
}

bool UJTSStellarAttachmentDialog::IsDraftDirty(const FJTSShipLockerSlot& LockerSlot) const
{
	return Draft != GetRecordedPoints(LockerSlot);
}

FText UJTSStellarAttachmentDialog::BuildTitle(const FJTSShipLockerSlot& LockerSlot) const
{
	const UJTSStellarLootTable* const Table = LootTable();
	if (!IsValid(Table)) return FText::GetEmpty();
	if (!LockerSlot.StellarCoreId.IsNone())
	{
		return FText::FromString(FString::Printf(TEXT("%s  ·  配件加点"),
			*Table->GetWeaponDisplayName(LockerSlot.StellarCoreId, LockerSlot.StellarItemId).ToString()));
	}
	const FJTSStellarLootEntry* const LootEntry = Table->FindEntry(LockerSlot.StellarItemId);
	const FString Name = LootEntry ? LootEntry->DisplayName.ToString() : LockerSlot.StellarItemId.ToString();
	return FText::FromString(FString::Printf(TEXT("%s  ·  %s"), *Name, HasCore(LockerSlot) ? TEXT("核心加点") : TEXT("配件加点")));
}

FText UJTSStellarAttachmentDialog::BuildDescription(const FJTSShipLockerSlot& LockerSlot) const
{
	if (!IsSlotStillValid(LockerSlot)) return FText::GetEmpty();
	const UJTSStellarLootTable* const Table = LootTable();
	return IsValid(Table) ? Table->GetItemDescription(LockerSlot.StellarItemId, LockerSlot.StellarCoreId) : FText::GetEmpty();
}

FText UJTSStellarAttachmentDialog::BuildBudgetText(const FJTSShipLockerSlot& LockerSlot) const
{
	if (!HasAttachment(LockerSlot)) return FText::GetEmpty();
	if (!IsMatchedWeapon(LockerSlot))
	{
		return FText::GetEmpty();
	}
	const int32 Budget = GetBudget(LockerSlot);
	const int32 Recorded = FJTSStellarProgression::SumPoints(Draft);
	const int32 Remaining = FMath::Max(0, Budget - Recorded);
	FString Text = FString::Printf(TEXT("核心预算 %d · 已记录 %d · 有效 %d · 可继续分配 %d"),
		Budget, Recorded, FMath::Min(Recorded, Budget), Remaining);
	if (Recorded > Budget) Text += TEXT("  （记录超出预算，按比例生效，加号禁用）");
	return FText::FromString(Text);
}

FText UJTSStellarAttachmentDialog::BuildSkillName(int32 SkillIndex, const FJTSShipLockerSlot& LockerSlot) const
{
	const FJTSStellarSkillDef* const Defs = FJTSStellarProgression::FindSkillDefs(LockerSlot.StellarItemId);
	return Defs ? FText::FromString(Defs[SkillIndex].Name) : FText::GetEmpty();
}

FText UJTSStellarAttachmentDialog::BuildSkillEffect(int32 SkillIndex, const FJTSShipLockerSlot& LockerSlot) const
{
	const FJTSStellarSkillDef* const Defs = FJTSStellarProgression::FindSkillDefs(LockerSlot.StellarItemId);
	if (!Defs) return FText::GetEmpty();
	TArray<double> Effective;
	FJTSStellarProgression::ComputeEffectiveLevels(Draft, GetBudget(LockerSlot), Effective);
	FString Text = FJTSStellarProgression::FormatSkill(Defs[SkillIndex], Effective[SkillIndex]);
	const double ToNext = FJTSStellarProgression::PointsToNextStep(Defs[SkillIndex], Effective[SkillIndex]);
	if (ToNext > 0.0) Text += FString::Printf(TEXT("  （再 %.1f 有效点提升一档）"), ToNext);
	return FText::FromString(Text);
}

FText UJTSStellarAttachmentDialog::BuildSkillLevels(int32 SkillIndex, const FJTSShipLockerSlot& LockerSlot) const
{
	TArray<double> Effective;
	FJTSStellarProgression::ComputeEffectiveLevels(Draft, GetBudget(LockerSlot), Effective);
	return FText::FromString(FString::Printf(TEXT("记录 %d / 有效 %.1f"), Draft.IsValidIndex(SkillIndex) ? Draft[SkillIndex] : 0, Effective[SkillIndex]));
}

int32 UJTSStellarAttachmentDialog::GetAvailableFirmwareUnits() const
{
	const AJTSPlayerState* const State = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	const UJTSStellarLootTable* const Table = LootTable();
	if (!IsValid(State) || !IsValid(Table)) return 0;
	if (bLoadoutContext) return Loadout() ? Loadout()->GetAvailableFirmwareUnits() : 0;
	int32 Units = State->GetStellarFirmwareUnits();
	for (int32 Index = 0; Index < State->GetShipLockerSlots().Num(); ++Index)
	{
		const FJTSShipLockerSlot& Other = State->GetShipLockerSlots()[Index];
		if (Index == SlotIndex || Other.bPendingStellarReveal || !Other.StandardItem.IsEmpty()
			|| !Other.StellarCoreId.IsNone()) continue;
		if (const FJTSStellarLootEntry* const LootEntry = Table->FindEntry(Other.StellarItemId)) Units += LootEntry->FirmwareUnits;
	}
	return Units;
}

FText UJTSStellarAttachmentDialog::BuildCoreText(const FJTSShipLockerSlot& LockerSlot) const
{
	if (!HasCore(LockerSlot)) return FText::GetEmpty();
	const int32 Points = FJTSStellarProgression::GetCoreBudget(LockerSlot.StellarCoreLevel);
	if (LockerSlot.StellarCoreLevel >= FJTSStellarProgression::MaxCoreLevel)
	{
		return FText::FromString(FString::Printf(TEXT("核心点数  %d / 60\n已满级"), Points));
	}
	return FText::FromString(FString::Printf(TEXT("核心点数  %d / 60\n升级材料（固件）  拥有 %d / 需要 %d"),
		Points, GetAvailableFirmwareUnits(), FJTSStellarProgression::GetUpgradeUnitCost(LockerSlot.StellarCoreLevel)));
}

bool UJTSStellarAttachmentDialog::CanUpgradeCore(const FJTSShipLockerSlot& ItemSlot) const
{
	return bOpen && !bAwaitingAction && IsEditable() && IsSlotStillValid(ItemSlot) && HasCore(ItemSlot)
		&& ItemSlot.StellarCoreLevel < FJTSStellarProgression::MaxCoreLevel
		&& GetAvailableFirmwareUnits() >= FJTSStellarProgression::GetUpgradeUnitCost(ItemSlot.StellarCoreLevel);
}

bool UJTSStellarAttachmentDialog::CanChangeSkill(int32 SkillIndex, int32 Delta, const FJTSShipLockerSlot& LockerSlot) const
{
	if (!bOpen || bAwaitingAction || !IsEditable() || !IsSlotStillValid(LockerSlot)
		|| !IsMatchedWeapon(LockerSlot) || !Draft.IsValidIndex(SkillIndex)) return false;
	if (Delta < 0) return Draft[SkillIndex] > 0;
	return IsMatchedWeapon(LockerSlot) && Draft[SkillIndex] < FJTSStellarProgression::MaxPointsPerSkill
		&& FJTSStellarProgression::SumPoints(Draft) < GetBudget(LockerSlot);
}

void UJTSStellarAttachmentDialog::ChangeSkill(int32 SkillIndex, int32 Delta)
{
	if (!CanChangeSkill(SkillIndex, Delta, ReadSlot())) return;
	Draft[SkillIndex] = static_cast<uint8>(Draft[SkillIndex] + Delta);
	SetStatus(TEXT("尚未应用"), false);
}

void UJTSStellarAttachmentDialog::ResetDraft()
{
	if (bAwaitingAction || !IsEditable() || !IsSlotStillValid(ReadSlot()) || !IsMatchedWeapon(ReadSlot())) return;
	Draft.Init(0, FJTSStellarProgression::SkillCount);
	SetStatus(TEXT("尚未应用"), false);
}

void UJTSStellarAttachmentDialog::ApplyDraft()
{
	const FJTSShipLockerSlot LockerSlot = ReadSlot();
	AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer());
	if (bAwaitingAction || !IsEditable() || !IsSlotStillValid(LockerSlot) || !IsMatchedWeapon(LockerSlot)
		|| !IsDraftDirty(LockerSlot) || FJTSStellarProgression::SumPoints(Draft) > GetBudget(LockerSlot)
		|| !IsValid(Controller) || (!bLoadoutContext && !ActiveSpacecraft.IsValid())) return;
	bAwaitingAction = true;
	SetStatus(TEXT("正在保存…"), false);
	if (bLoadoutContext && Loadout())
	{
		Loadout()->ServerApplyPoints(SlotIndex, OpenedToken, OpenedRevision, Draft);
		return;
	}
	Controller->ServerApplyStellarPoints(ActiveSpacecraft.Get(), SlotIndex, OpenedToken, Draft);
}

void UJTSStellarAttachmentDialog::UpgradeCore()
{
	AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer());
	if (!CanUpgradeCore(ReadSlot()) || !IsValid(Controller) || (!bLoadoutContext && !ActiveSpacecraft.IsValid())) return;
	bAwaitingAction = true;
	SetStatus(TEXT("正在加点…"), false);
	if (bLoadoutContext && Loadout())
	{
		Loadout()->ServerUpgradeCore(SlotIndex, OpenedToken, OpenedRevision);
		return;
	}
	Controller->ServerUpgradeStellarCore(ActiveSpacecraft.Get(), SlotIndex, OpenedToken);
}

void UJTSStellarAttachmentDialog::SetStatus(const FString& Message, bool bError)
{
	StatusMessage = Message;
	bStatusIsError = bError;
}

TSharedRef<SWidget> UJTSStellarAttachmentDialog::RebuildWidget()
{
	const FLinearColor Panel(0.03f, 0.07f, 0.11f, 0.98f);
	const FLinearColor RowColor(0.05f, 0.11f, 0.17f, 1.0f);

	TSharedRef<SScrollBox> Rows = SNew(SScrollBox)
		.Visibility_Lambda([this]() { return HasAttachment(ReadSlot()) ? EVisibility::Visible : EVisibility::Collapsed; });
	for (int32 Skill = 0; Skill < FJTSStellarProgression::SkillCount; ++Skill)
	{
		Rows->AddSlot().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(RowColor)
			.Padding(6.0f)
			.Visibility_Lambda([this]()
			{
				return HasAttachment(ReadSlot()) ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(DialogFont(18))
						.Text_Lambda([this, Skill]() { return BuildSkillName(Skill, ReadSlot()); })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Font(DialogFont(14)).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.68f, 0.82f, 0.94f))
						.Text_Lambda([this, Skill]() { return BuildSkillEffect(Skill, ReadSlot()); })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(170.0f)
					[
						SNew(STextBlock).Font(DialogFont(15)).ColorAndOpacity(FLinearColor(0.55f, 0.90f, 1.0f))
						.Justification(ETextJustify::Center)
						.Text_Lambda([this, Skill]() { return BuildSkillLevels(Skill, ReadSlot()); })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(52.0f).HeightOverride(36.0f)
					[
						MakeLabelledButton(FText::FromString(TEXT("−")), FLinearColor(0.12f, 0.18f, 0.27f), 18,
							[this, Skill]() { ChangeSkill(Skill, -1); return FReply::Handled(); },
							[this, Skill]() { return CanChangeSkill(Skill, -1, ReadSlot()); })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(52.0f).HeightOverride(36.0f)
					[
						MakeLabelledButton(FText::FromString(TEXT("+")), FLinearColor(0.08f, 0.34f, 0.30f), 18,
							[this, Skill]() { ChangeSkill(Skill, 1); return FReply::Handled(); },
							[this, Skill]() { return CanChangeSkill(Skill, 1, ReadSlot()); })
					]
				]
			]
		];
	}

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			// Full-screen scrim: swallows clicks so the shop behind cannot be driven while the dialog is open.
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.62f))
			.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
			.OnMouseButtonUp_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		]
		+ SOverlay::Slot().Padding(20)
		[
			SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
			[
			SNew(SBox)
			.WidthOverride_Lambda([this]() { return FOptionalSize(HasCore(ReadSlot()) ? 480.0f : 900.0f); })
			.HeightOverride_Lambda([this]() { return FOptionalSize(HasCore(ReadSlot()) ? 280.0f : 660.0f); })
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(Panel)
				.Padding(20.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(DialogFont(22)).AutoWrapText(true)
						.Text_Lambda([this]()
						{
							const FJTSShipLockerSlot LockerSlot = ReadSlot();
							return IsSlotStillValid(LockerSlot) ? BuildTitle(LockerSlot) : FText::FromString(TEXT("物品已变化"));
						})
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Font(DialogFont(14)).ColorAndOpacity(FLinearColor(0.68f, 0.82f, 0.94f))
						.AutoWrapText(true)
						.Text_Lambda([this]() { return BuildDescription(ReadSlot()); })
						.Visibility_Lambda([this]()
						{
							return HasCore(ReadSlot()) || BuildDescription(ReadSlot()).IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
						})
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 12.0f)
					[
						SNew(STextBlock).Font(DialogFont(15)).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.72f, 0.88f, 1.0f))
						.Text_Lambda([this]() { return BuildBudgetText(ReadSlot()); })
						.Visibility_Lambda([this]() { return HasAttachment(ReadSlot()) ? EVisibility::Visible : EVisibility::Collapsed; })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
					[
						SNew(STextBlock).Font(DialogFont(18)).AutoWrapText(true)
						.ColorAndOpacity(FLinearColor(1.0f, 0.86f, 0.5f))
						.Text_Lambda([this]() { return BuildCoreText(ReadSlot()); })
						.Visibility_Lambda([this]() { return HasCore(ReadSlot()) ? EVisibility::Visible : EVisibility::Collapsed; })
					]
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SNew(SBox)[Rows]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
					[
						SNew(STextBlock).Font(DialogFont(14)).AutoWrapText(true)
						.ColorAndOpacity_Lambda([this]()
						{
							return bStatusIsError ? FLinearColor(1.0f, 0.46f, 0.34f) : FLinearColor(0.55f, 0.95f, 0.70f);
						})
						.Text_Lambda([this]()
						{
							const FJTSShipLockerSlot LockerSlot = ReadSlot();
							if (!IsSlotStillValid(LockerSlot)) return FText::FromString(TEXT("该物品已移动、合成或被删除，请关闭后重新点击"));
							if (StatusMessage.IsEmpty() && IsDraftDirty(LockerSlot)) return FText::FromString(TEXT("尚未应用"));
							return FText::FromString(StatusMessage);
						})
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNew(SBox).HeightOverride(44.0f)
							.Visibility_Lambda([this]() { return HasCore(ReadSlot()) ? EVisibility::Visible : EVisibility::Collapsed; })
							[
								MakeLabelledButton(FText::FromString(TEXT("加 1 点")), FLinearColor(0.35f, 0.24f, 0.06f), 18,
									[this]() { UpgradeCore(); return FReply::Handled(); },
									[this]() { return CanUpgradeCore(ReadSlot()); })
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNew(SBox).HeightOverride(44.0f)
							.Visibility_Lambda([this]() { return HasAttachment(ReadSlot()) ? EVisibility::Visible : EVisibility::Collapsed; })
							[
								MakeLabelledButton(FText::FromString(TEXT("重置分配")), FLinearColor(0.16f, 0.22f, 0.30f), 18,
									[this]() { ResetDraft(); return FReply::Handled(); },
									[this]() { return bOpen && !bAwaitingAction && IsEditable() && IsMatchedWeapon(ReadSlot()) && IsSlotStillValid(ReadSlot()); })
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(12.0f, 0.0f)
						[
							SNew(SBox).HeightOverride(44.0f)
							.Visibility_Lambda([this]() { return HasAttachment(ReadSlot()) ? EVisibility::Visible : EVisibility::Collapsed; })
							[
								MakeLabelledButton(FText::FromString(TEXT("应用")), FLinearColor(0.08f, 0.34f, 0.30f), 18,
									[this]() { ApplyDraft(); return FReply::Handled(); },
									[this]()
									{
										const FJTSShipLockerSlot LockerSlot = ReadSlot();
										return bOpen && !bAwaitingAction && IsEditable() && IsMatchedWeapon(LockerSlot) && IsSlotStillValid(LockerSlot) && IsDraftDirty(LockerSlot)
											&& FJTSStellarProgression::SumPoints(Draft) <= GetBudget(LockerSlot);
									})
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(12.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SBox).HeightOverride(44.0f)
							[
								MakeLabelledButton(FText::FromString(TEXT("关闭 [Esc]")), FLinearColor(0.20f, 0.12f, 0.12f), 18,
									[this]() { CloseDialog(); return FReply::Handled(); },
									[]() { return true; })
							]
						]
					]
				]
			]
			]
		];
}
