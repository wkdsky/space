// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSPlayerCardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Systems/JTSVoiceSubsystem.h"
#include "space/UI/JTSPreLaunchLobbyWidget.h"
#include "space/UI/JTSUITheme.h"

namespace
{
	FString AvatarColorName(EJTSAvatarColor Color)
	{
		switch (Color)
		{
		case EJTSAvatarColor::Orange: return TEXT("ORANGE");
		case EJTSAvatarColor::Green: return TEXT("GREEN");
		case EJTSAvatarColor::Purple: return TEXT("PURPLE");
		default: return TEXT("BLUE");
		}
	}

	void AddVertical(UVerticalBox* Parent, UWidget* Child, const FMargin& Padding = FMargin(0.0f, 4.0f), ESlateSizeRule::Type SizeRule = ESlateSizeRule::Automatic)
	{
		if (Parent != nullptr && Child != nullptr)
		{
			if (UVerticalBoxSlot* const Slot = Parent->AddChildToVerticalBox(Child))
			{
				Slot->SetPadding(Padding);
				Slot->SetSize(FSlateChildSize(SizeRule));
				Slot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
	}
}

TSharedRef<SWidget> UJTSPlayerCardWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSPlayerCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UJTSPlayerCardWidget::SetSlot(int32 InSlotIndex, AJTSPlayerState* InPlayerState, bool bInIsLocalPlayer, bool bInHostCanManage, UJTSPreLaunchLobbyWidget* InLobbyOwner)
{
	SlotIndex = InSlotIndex;
	AssignedPlayerState = InPlayerState;
	bIsLocalPlayer = bInIsLocalPlayer;
	bHostCanManage = bInHostCanManage;
	LobbyOwner = InLobbyOwner;
	Refresh();
}

void UJTSPlayerCardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.20f)
	{
		RefreshAccumulator = 0.0f;
		Refresh();
	}
}

void UJTSPlayerCardWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	CardBorder = JTSUITheme::MakePanel(WidgetTree, TEXT("PlayerCard"), JTSUITheme::PanelRaised, FMargin(16.0f));
	WidgetTree->RootWidget = CardBorder;
	UVerticalBox* const Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CardLayout"));
	CardBorder->SetContent(Layout);

	UHorizontalBox* const Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CardHeader"));
	SlotText = JTSUITheme::MakeText(WidgetTree, TEXT("SlotText"), TEXT("PLAYER 1"), 13.0f, JTSUITheme::Muted);
	HostText = JTSUITheme::MakeText(WidgetTree, TEXT("HostText"), TEXT("HOST"), 12.0f, JTSUITheme::Warning, ETextJustify::Right);
	Header->AddChildToHorizontalBox(SlotText);
	USpacer* const HeaderSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("HeaderSpacer"));
	if (UHorizontalBoxSlot* const SpacerSlot = Header->AddChildToHorizontalBox(HeaderSpacer)) SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Header->AddChildToHorizontalBox(HostText);
	AddVertical(Layout, Header, FMargin(0.0f, 0.0f, 0.0f, 10.0f));

	AvatarPreview = JTSUITheme::MakePanel(WidgetTree, TEXT("AvatarPreview"), FLinearColor(0.10f, 0.45f, 1.0f, 1.0f), FMargin(10.0f));
	AvatarPreview->SetContent(JTSUITheme::MakeText(WidgetTree, TEXT("AvatarGlyph"), TEXT("●"), 52.0f, FLinearColor::White, ETextJustify::Center));
	AddVertical(Layout, AvatarPreview, FMargin(0.0f, 0.0f, 0.0f, 10.0f));

	DisplayNameText = JTSUITheme::MakeText(WidgetTree, TEXT("DisplayName"), TEXT("OPEN SEAT"), 20.0f, JTSUITheme::Ink, ETextJustify::Center);
	AddVertical(Layout, DisplayNameText);
	ColorText = JTSUITheme::MakeText(WidgetTree, TEXT("ColorText"), TEXT("AWAITING CREW"), 12.0f, JTSUITheme::Muted, ETextJustify::Center);
	AddVertical(Layout, ColorText, FMargin(0.0f, 0.0f, 0.0f, 6.0f));

	ReadyPill = JTSUITheme::MakePanel(WidgetTree, TEXT("ReadyPill"), FLinearColor(0.15f, 0.19f, 0.25f, 1.0f), FMargin(7.0f, 4.0f));
	ReadyText = JTSUITheme::MakeText(WidgetTree, TEXT("ReadyText"), TEXT("OPEN"), 13.0f, JTSUITheme::Muted, ETextJustify::Center);
	ReadyPill->SetContent(ReadyText);
	AddVertical(Layout, ReadyPill, FMargin(0.0f, 4.0f));

	UHorizontalBox* const StatusRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("StatusRow"));
	VoiceIconText = JTSUITheme::MakeText(WidgetTree, TEXT("VoiceIcon"), TEXT("MIC"), 12.0f, JTSUITheme::Muted);
	ConnectionText = JTSUITheme::MakeText(WidgetTree, TEXT("Connection"), TEXT("● CONNECTED"), 12.0f, JTSUITheme::Ready, ETextJustify::Right);
	StatusRow->AddChildToHorizontalBox(VoiceIconText);
	USpacer* const StatusSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("StatusSpacer"));
	if (UHorizontalBoxSlot* const SpacerSlot = StatusRow->AddChildToHorizontalBox(StatusSpacer)) SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	StatusRow->AddChildToHorizontalBox(ConnectionText);
	AddVertical(Layout, StatusRow, FMargin(0.0f, 7.0f, 0.0f, 4.0f));

	CustomizeButton = JTSUITheme::MakeButton(WidgetTree, TEXT("CustomizeButton"), TEXT("CUSTOMIZE"), JTSUITheme::EButtonTone::Secondary, 14.0f);
	CustomizeButton->OnClicked.AddDynamic(this, &UJTSPlayerCardWidget::OpenCustomize);
	AddVertical(Layout, CustomizeButton, FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	ContextButton = JTSUITheme::MakeButton(WidgetTree, TEXT("ContextButton"), TEXT("PLAYER OPTIONS"), JTSUITheme::EButtonTone::Secondary, 14.0f);
	ContextButton->OnClicked.AddDynamic(this, &UJTSPlayerCardWidget::OpenContextMenu);
	AddVertical(Layout, ContextButton, FMargin(0.0f, 8.0f, 0.0f, 0.0f));
}

void UJTSPlayerCardWidget::Refresh()
{
	if (SlotText == nullptr)
	{
		return;
	}

	SlotText->SetText(FText::FromString(FString::Printf(TEXT("PLAYER %d"), SlotIndex + 1)));
	const AJTSPlayerState* const State = AssignedPlayerState.Get();
	const bool bOccupied = State != nullptr;
	if (!bOccupied)
	{
		DisplayNameText->SetText(FText::FromString(TEXT("OPEN SEAT")));
		ColorText->SetText(FText::FromString(TEXT("AWAITING CREW")));
		HostText->SetVisibility(ESlateVisibility::Collapsed);
		ReadyText->SetText(FText::FromString(TEXT("OPEN")));
		ReadyPill->SetBrushColor(FLinearColor(0.15f, 0.19f, 0.25f, 1.0f));
		AvatarPreview->SetBrushColor(FLinearColor(0.08f, 0.12f, 0.18f, 1.0f));
		VoiceIconText->SetVisibility(ESlateVisibility::Collapsed);
		ConnectionText->SetVisibility(ESlateVisibility::Collapsed);
		CustomizeButton->SetVisibility(ESlateVisibility::Collapsed);
		ContextButton->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FLinearColor AvatarColor = State->GetAvatarLinearColor();
	DisplayNameText->SetText(FText::FromString(State->GetPlayerName()));
	ColorText->SetText(FText::FromString(AvatarColorName(State->GetAvatarColor())));
	AvatarPreview->SetBrushColor(AvatarColor);
	HostText->SetVisibility(State->IsExpeditionHost() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ReadyText->SetText(FText::FromString(State->IsReady() ? TEXT("✓ READY") : TEXT("NOT READY")));
	ReadyPill->SetBrushColor(State->IsReady() ? FLinearColor(0.05f, 0.35f, 0.18f, 1.0f) : FLinearColor(0.25f, 0.18f, 0.08f, 1.0f));

	const UJTSVoiceSubsystem* const Voice = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>() : nullptr;
	FString VoiceIcon = TEXT("MIC");
	FString VoiceTooltip = TEXT("Microphone available.");
	FLinearColor VoiceColor = JTSUITheme::Muted;
	if (Voice == nullptr || !Voice->IsVoiceAvailable())
	{
		VoiceIcon = TEXT("MIC OFF");
		VoiceTooltip = TEXT("Voice chat unavailable with the current local/LAN provider.");
		VoiceColor = JTSUITheme::Muted;
	}
	else if (Voice->IsPlayerMuted(State->GetVoiceIdentityString()))
	{
		VoiceIcon = TEXT("MUTED");
		VoiceTooltip = TEXT("Locally muted.");
		VoiceColor = JTSUITheme::Danger;
	}
	else if (Voice->IsPlayerTalking(State->GetVoiceIdentityString()))
	{
		VoiceIcon = TEXT("SPEAKING");
		VoiceTooltip = TEXT("Speaking.");
		VoiceColor = JTSUITheme::Ready;
	}
	VoiceIconText->SetText(FText::FromString(VoiceIcon));
	VoiceIconText->SetColorAndOpacity(FSlateColor(VoiceColor));
	VoiceIconText->SetToolTipText(FText::FromString(VoiceTooltip));
	VoiceIconText->SetVisibility(ESlateVisibility::Visible);
	ConnectionText->SetText(FText::FromString(FString::Printf(TEXT("● %d ms"), FMath::RoundToInt(State->GetPingInMilliseconds()))));
	ConnectionText->SetVisibility(ESlateVisibility::Visible);

	CustomizeButton->SetVisibility(bIsLocalPlayer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ContextButton->SetVisibility(bHostCanManage && !bIsLocalPlayer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UJTSPlayerCardWidget::OpenCustomize()
{
	if (UJTSPreLaunchLobbyWidget* const Owner = LobbyOwner.Get())
	{
		Owner->OpenPlayerCustomization();
	}
}

void UJTSPlayerCardWidget::OpenContextMenu()
{
	if (UJTSPreLaunchLobbyWidget* const Owner = LobbyOwner.Get())
	{
		Owner->OpenPlayerContextMenu(AssignedPlayerState.Get());
	}
}
