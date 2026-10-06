// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSWeaponUpgradePanel.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSWeaponProgression.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Ships/JTSSpacecraftActor.h"

namespace
{
	FSlateFontInfo PanelFont(int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size);
	}

	const TCHAR* ResourceName(EJTSResourceType Type)
	{
		switch (Type)
		{
		case EJTSResourceType::Rock: return TEXT("石头");
		case EJTSResourceType::Ore: return TEXT("铁矿物");
		case EJTSResourceType::Organic: return TEXT("有机物");
		default: return TEXT("资源");
		}
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
				SNew(STextBlock).Text(Label).Font(PanelFont(FontSize))
			];
	}
}

bool UJTSWeaponUpgradePanel::SelectSlot(AJTSSpacecraftActor* Spacecraft, int32 InSlotIndex)
{
	const AJTSPlayerState* const State = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	if (!IsValid(Spacecraft) || !IsValid(State)) return false;
	const FJTSShipLockerSlot LockerSlot = State->GetShipLockerSlot(InSlotIndex);
	if (LockerSlot.bPendingStellarReveal || LockerSlot.StandardItem.IsEmpty()
		|| !FJTSWeaponProgression::IsUpgradeable(LockerSlot.StandardItem.ItemId)) return false;
	ActiveSpacecraft = Spacecraft;
	SlotIndex = InSlotIndex;
	SelectedToken = LockerSlot.SlotToken;
	Draft = GetRecordedPoints(LockerSlot);
	PendingPointRequests = 0;
	StatusMessage.Reset();
	bStatusIsError = false;
	return true;
}

void UJTSWeaponUpgradePanel::ClearSelection()
{
	SlotIndex = INDEX_NONE;
	SelectedToken.Invalidate();
	Draft.Reset();
	PendingPointRequests = 0;
	StatusMessage.Reset();
}

void UJTSWeaponUpgradePanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!HasValidSelection()) return;
	// With nothing in flight the replicated slot wins, so a rejected or external change never leaves a stale draft.
	if (PendingPointRequests <= 0)
	{
		const TArray<uint8> Recorded = GetRecordedPoints(ReadSlot());
		if (Draft != Recorded) Draft = Recorded;
	}
}

void UJTSWeaponUpgradePanel::NotifyUpgradeResult(bool bUpgrade, bool bSucceeded)
{
	if (!bUpgrade) PendingPointRequests = FMath::Max(0, PendingPointRequests - 1);
	if (bSucceeded)
	{
		SetStatus(bUpgrade ? TEXT("机体已升级，获得 4 点技能点，基础伤害提高") : TEXT(""), false);
	}
	else
	{
		SetStatus(bUpgrade ? TEXT("升级失败：材料不足或物品已变化") : TEXT("加点失败：物品已变化或点数超出预算"), true);
	}
}

FJTSShipLockerSlot UJTSWeaponUpgradePanel::ReadSlot() const
{
	const AJTSPlayerState* const State = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	return IsValid(State) ? State->GetShipLockerSlot(SlotIndex) : FJTSShipLockerSlot();
}

bool UJTSWeaponUpgradePanel::HasValidSelection() const
{
	return SlotIndex != INDEX_NONE && IsSlotStillValid(ReadSlot());
}

bool UJTSWeaponUpgradePanel::IsSlotStillValid(const FJTSShipLockerSlot& LockerSlot) const
{
	return !LockerSlot.bPendingStellarReveal && !LockerSlot.StandardItem.IsEmpty()
		&& FJTSWeaponProgression::IsUpgradeable(LockerSlot.StandardItem.ItemId)
		&& SelectedToken.IsValid() && LockerSlot.SlotToken == SelectedToken;
}

TArray<uint8> UJTSWeaponUpgradePanel::GetRecordedPoints(const FJTSShipLockerSlot& LockerSlot) const
{
	TArray<uint8> Points = LockerSlot.StandardItem.WeaponPoints;
	FJTSWeaponProgression::NormalizePoints(Points);
	return Points;
}

FText UJTSWeaponUpgradePanel::BuildTitle(const FJTSShipLockerSlot& LockerSlot) const
{
	return FText::FromString(FString::Printf(TEXT("%s  ·  机体 Lv %d / %d"),
		*UJTSItemDefinitionLibrary::GetItemDisplayName(LockerSlot.StandardItem.ItemId).ToString(),
		LockerSlot.StandardItem.WeaponBodyLevel, FJTSWeaponProgression::MaxBodyLevel));
}

FText UJTSWeaponUpgradePanel::BuildBudgetText(const FJTSShipLockerSlot& LockerSlot) const
{
	const int32 Budget = FJTSWeaponProgression::GetPointBudget(LockerSlot.StandardItem.WeaponBodyLevel);
	const int32 Used = FJTSWeaponProgression::SumPoints(Draft);
	const float Multiplier = FJTSWeaponProgression::Resolve(LockerSlot.StandardItem).LevelMultiplier;
	return FText::FromString(FString::Printf(TEXT("技能点 %d / %d · 可继续分配 %d · 机体基础伤害 ×%.2f"),
		Used, Budget, FMath::Max(0, Budget - Used), Multiplier));
}

FText UJTSWeaponUpgradePanel::BuildCostText(const FJTSShipLockerSlot& LockerSlot) const
{
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, LockerSlot.StandardItem.ItemId);
	TArray<FJTSItemCost> Cost;
	if (!FJTSWeaponProgression::GetUpgradeCost(Definition, LockerSlot.StandardItem.WeaponBodyLevel, Cost))
	{
		return FText::FromString(TEXT("机体已满级"));
	}
	FString Text = FString::Printf(TEXT("升到 Lv %d："), LockerSlot.StandardItem.WeaponBodyLevel + 1);
	for (const FJTSItemCost& Entry : Cost)
	{
		const int32 Have = ActiveSpacecraft.IsValid() ? ActiveSpacecraft->GetResourceAmount(Entry.ResourceType) : 0;
		Text += FString::Printf(TEXT(" %s %d/%d"), ResourceName(Entry.ResourceType), Have, Entry.Amount);
	}
	return FText::FromString(Text);
}

bool UJTSWeaponUpgradePanel::CanAffordUpgrade(const FJTSShipLockerSlot& LockerSlot) const
{
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, LockerSlot.StandardItem.ItemId);
	TArray<FJTSItemCost> Cost;
	if (!ActiveSpacecraft.IsValid()
		|| !FJTSWeaponProgression::GetUpgradeCost(Definition, LockerSlot.StandardItem.WeaponBodyLevel, Cost)) return false;
	for (const FJTSItemCost& Entry : Cost)
	{
		if (ActiveSpacecraft->GetResourceAmount(Entry.ResourceType) < Entry.Amount) return false;
	}
	return true;
}

FText UJTSWeaponUpgradePanel::BuildSkillName(int32 SkillIndex, const FJTSShipLockerSlot& LockerSlot) const
{
	const FJTSWeaponSkillDef* const Defs = FJTSWeaponProgression::FindSkillDefs(LockerSlot.StandardItem.ItemId);
	return Defs ? FText::FromString(Defs[SkillIndex].Name) : FText::GetEmpty();
}

FText UJTSWeaponUpgradePanel::BuildSkillEffect(int32 SkillIndex, const FJTSShipLockerSlot& LockerSlot) const
{
	const FJTSWeaponSkillDef* const Defs = FJTSWeaponProgression::FindSkillDefs(LockerSlot.StandardItem.ItemId);
	if (!Defs || !Draft.IsValidIndex(SkillIndex)) return FText::GetEmpty();
	const FJTSWeaponSkillDef& Def = Defs[SkillIndex];
	FString Text = Draft[SkillIndex] > 0 ? FJTSWeaponProgression::FormatSkill(Def, Draft[SkillIndex]) : FString(TEXT("未加点"));
	const int32 ToNext = FJTSWeaponProgression::PointsToNextStep(Def, Draft[SkillIndex]);
	if (ToNext > 0 && Draft[SkillIndex] < FJTSWeaponProgression::MaxPointsPerSkill)
	{
		Text += FString::Printf(TEXT("（再 %d 点升档）"), ToNext);
	}
	if (!FJTSWeaponProgression::IsStatImplemented(LockerSlot.StandardItem.ItemId, Def.Stat)) Text += TEXT(" [尚未生效]");
	return FText::FromString(Text);
}

bool UJTSWeaponUpgradePanel::CanChangeSkill(int32 SkillIndex, int32 Delta, const FJTSShipLockerSlot& LockerSlot) const
{
	if (!IsSlotStillValid(LockerSlot) || !Draft.IsValidIndex(SkillIndex)) return false;
	if (Delta < 0) return Draft[SkillIndex] > 0;
	return Draft[SkillIndex] < FJTSWeaponProgression::MaxPointsPerSkill
		&& FJTSWeaponProgression::SumPoints(Draft) < FJTSWeaponProgression::GetPointBudget(LockerSlot.StandardItem.WeaponBodyLevel);
}

void UJTSWeaponUpgradePanel::ChangeSkill(int32 SkillIndex, int32 Delta)
{
	if (!CanChangeSkill(SkillIndex, Delta, ReadSlot())) return;
	Draft[SkillIndex] = static_cast<uint8>(Draft[SkillIndex] + Delta);
	SubmitDraft();
}

void UJTSWeaponUpgradePanel::ResetPoints()
{
	if (!HasValidSelection()) return;
	Draft.Init(0, FJTSWeaponProgression::SkillCount);
	SubmitDraft();
}

void UJTSWeaponUpgradePanel::SubmitDraft()
{
	AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer());
	if (!IsValid(Controller) || !ActiveSpacecraft.IsValid() || !SelectedToken.IsValid()) return;
	++PendingPointRequests;
	SetStatus(TEXT(""), false);
	Controller->ServerApplyWeaponPoints(ActiveSpacecraft.Get(), SlotIndex, SelectedToken, Draft);
}

void UJTSWeaponUpgradePanel::UpgradeBody()
{
	AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer());
	if (!HasValidSelection() || !IsValid(Controller) || !ActiveSpacecraft.IsValid()) return;
	SetStatus(TEXT("正在确认…"), false);
	Controller->ServerUpgradeWeaponBody(ActiveSpacecraft.Get(), SlotIndex, SelectedToken);
}

void UJTSWeaponUpgradePanel::SetStatus(const FString& Message, bool bError)
{
	StatusMessage = Message;
	bStatusIsError = bError;
}

TSharedRef<SWidget> UJTSWeaponUpgradePanel::RebuildWidget()
{
	const FLinearColor RowColor(0.045f, 0.105f, 0.165f, 1.0f);

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	for (int32 Skill = 0; Skill < FJTSWeaponProgression::SkillCount; ++Skill)
	{
		Rows->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(RowColor)
			.Padding(FMargin(12.0f, 6.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(112.0f)
					[
						SNew(STextBlock).Font(PanelFont(17))
						.Text_Lambda([this, Skill]() { return BuildSkillName(Skill, ReadSlot()); })
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(PanelFont(14)).ColorAndOpacity(FLinearColor(0.68f, 0.82f, 0.94f))
					.AutoWrapText(true)
					.Text_Lambda([this, Skill]() { return BuildSkillEffect(Skill, ReadSlot()); })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(58.0f)
					[
						SNew(STextBlock).Font(PanelFont(18)).ColorAndOpacity(FLinearColor(0.55f, 0.90f, 1.0f))
						.Justification(ETextJustify::Center)
						.Text_Lambda([this, Skill]()
						{
							return FText::FromString(FString::Printf(TEXT("%d / %d"),
								Draft.IsValidIndex(Skill) ? Draft[Skill] : 0, FJTSWeaponProgression::MaxPointsPerSkill));
						})
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(44.0f).HeightOverride(36.0f)
					[
						MakeLabelledButton(FText::FromString(TEXT("−")), FLinearColor(0.12f, 0.18f, 0.27f), 20,
							[this, Skill]() { ChangeSkill(Skill, -1); return FReply::Handled(); },
							[this, Skill]() { return CanChangeSkill(Skill, -1, ReadSlot()); })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(44.0f).HeightOverride(36.0f)
					[
						MakeLabelledButton(FText::FromString(TEXT("+")), FLinearColor(0.08f, 0.34f, 0.30f), 20,
							[this, Skill]() { ChangeSkill(Skill, 1); return FReply::Handled(); },
							[this, Skill]() { return CanChangeSkill(Skill, 1, ReadSlot()); })
					]
				]
			]
		];
	}

	return SNew(SOverlay)
		// Prompt shown until a weapon has been picked from the locker on the right.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Font(PanelFont(22)).ColorAndOpacity(FLinearColor(0.55f, 0.69f, 0.82f))
			.Justification(ETextJustify::Center)
			.Visibility_Lambda([this]() { return HasValidSelection() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			.Text(FText::FromString(TEXT("点击右侧物品栏中的任意武器进行改装")))
		]
		+ SOverlay::Slot().Padding(18.0f)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([this]() { return HasValidSelection() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(PanelFont(24))
				.Text_Lambda([this]()
				{
					const FJTSShipLockerSlot LockerSlot = ReadSlot();
					return IsSlotStillValid(LockerSlot) ? BuildTitle(LockerSlot) : FText::GetEmpty();
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 6.0f)
			[
				SNew(STextBlock).Font(PanelFont(16)).ColorAndOpacity(FLinearColor(0.72f, 0.88f, 1.0f))
				.Text_Lambda([this]()
				{
					const FJTSShipLockerSlot LockerSlot = ReadSlot();
					return IsSlotStillValid(LockerSlot) ? BuildBudgetText(LockerSlot) : FText::GetEmpty();
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(PanelFont(15)).ColorAndOpacity(FLinearColor(1.0f, 0.86f, 0.5f))
					.Text_Lambda([this]()
					{
						const FJTSShipLockerSlot LockerSlot = ReadSlot();
						return IsSlotStillValid(LockerSlot) ? BuildCostText(LockerSlot) : FText::GetEmpty();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(130.0f).HeightOverride(38.0f)
					[
						MakeLabelledButton(FText::FromString(TEXT("升级机体")), FLinearColor(0.35f, 0.24f, 0.06f), 17,
							[this]() { UpgradeBody(); return FReply::Handled(); },
							[this]()
							{
								const FJTSShipLockerSlot LockerSlot = ReadSlot();
								return IsSlotStillValid(LockerSlot)
									&& LockerSlot.StandardItem.WeaponBodyLevel < FJTSWeaponProgression::MaxBodyLevel
									&& CanAffordUpgrade(LockerSlot);
							})
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Rows
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
			[
				SNew(STextBlock).Font(PanelFont(15))
				.ColorAndOpacity_Lambda([this]()
				{
					return bStatusIsError ? FLinearColor(1.0f, 0.46f, 0.34f) : FLinearColor(0.55f, 0.95f, 0.70f);
				})
				.Text_Lambda([this]() { return FText::FromString(StatusMessage); })
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HeightOverride(40.0f)
				[
					MakeLabelledButton(FText::FromString(TEXT("重置加点（免费，数字归零）")), FLinearColor(0.16f, 0.22f, 0.30f), 17,
						[this]() { ResetPoints(); return FReply::Handled(); },
						[this]() { return HasValidSelection() && FJTSWeaponProgression::SumPoints(Draft) > 0; })
				]
			]
		];
}
