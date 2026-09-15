// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSPreLaunchLobbyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/SafeZone.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/PlatformApplicationMisc.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Systems/JTSOnlineSessionSubsystem.h"
#include "space/Systems/JTSVoiceSubsystem.h"
#include "space/UI/JTSPlayerCardWidget.h"
#include "space/UI/JTSPlayerCustomizeWidget.h"
#include "space/UI/JTSUITheme.h"

namespace
{
	UCanvasPanelSlot* AddCanvas(UCanvasPanel* Parent, UWidget* Child, const FAnchors& Anchors, const FMargin& Offsets, const FVector2D& Alignment = FVector2D::ZeroVector)
	{
		if (Parent == nullptr || Child == nullptr)
		{
			return nullptr;
		}
		UCanvasPanelSlot* const Slot = Parent->AddChildToCanvas(Child);
		Slot->SetAnchors(Anchors);
		Slot->SetOffsets(Offsets);
		Slot->SetAlignment(Alignment);
		return Slot;
	}

	void AddVertical(UVerticalBox* Parent, UWidget* Child, const FMargin& Padding = FMargin(0.0f, 4.0f), ESlateSizeRule::Type Rule = ESlateSizeRule::Automatic)
	{
		if (Parent != nullptr && Child != nullptr)
		{
			if (UVerticalBoxSlot* const Slot = Parent->AddChildToVerticalBox(Child))
			{
				Slot->SetPadding(Padding);
				Slot->SetSize(FSlateChildSize(Rule));
				Slot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
	}

	void AddHorizontal(UHorizontalBox* Parent, UWidget* Child, const FMargin& Padding = FMargin(4.0f), ESlateSizeRule::Type Rule = ESlateSizeRule::Automatic)
	{
		if (Parent != nullptr && Child != nullptr)
		{
			if (UHorizontalBoxSlot* const Slot = Parent->AddChildToHorizontalBox(Child))
			{
				Slot->SetPadding(Padding);
				Slot->SetSize(FSlateChildSize(Rule));
				Slot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}
}

TSharedRef<SWidget> UJTSPreLaunchLobbyWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSPreLaunchLobbyWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UJTSPreLaunchLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	Refresh();
}

void UJTSPreLaunchLobbyWidget::NativeDestruct()
{
	ClosePlayerCustomization();
	Super::NativeDestruct();
}

void UJTSPreLaunchLobbyWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.20f)
	{
		RefreshAccumulator = 0.0f;
		Refresh();
	}
}

void UJTSPreLaunchLobbyWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	USafeZone* const SafeZone = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("SafeZone"));
	WidgetTree->RootWidget = SafeZone;
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("LobbyCanvas"));
	SafeZone->SetContent(RootCanvas);

	UBorder* const Background = JTSUITheme::MakePanel(WidgetTree, TEXT("Backdrop"), FLinearColor(0.005f, 0.018f, 0.040f, 0.83f), FMargin(0.0f));
	AddCanvas(RootCanvas, Background, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f));

	UBorder* const Header = JTSUITheme::MakePanel(WidgetTree, TEXT("Header"), FLinearColor(0.025f, 0.070f, 0.125f, 0.94f), FMargin(22.0f, 14.0f));
	AddCanvas(RootCanvas, Header, FAnchors(0.035f, 0.035f, 0.965f, 0.165f), FMargin(0.0f));
	UHorizontalBox* const HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderRow"));
	Header->SetContent(HeaderRow);
	UVerticalBox* const TitleBlock = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TitleBlock"));
	AddVertical(TitleBlock, JTSUITheme::MakeText(WidgetTree, TEXT("GameTitle"), TEXT("JUMP TO SPACE"), 27.0f, JTSUITheme::Ink));
	AddVertical(TitleBlock, JTSUITheme::MakeText(WidgetTree, TEXT("LobbyTitle"), TEXT("PRE-LAUNCH LOBBY"), 13.0f, JTSUITheme::Cyan));
	AddHorizontal(HeaderRow, TitleBlock);
	USpacer* const HeaderSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("HeaderSpacer"));
	AddHorizontal(HeaderRow, HeaderSpacer, FMargin(4.0f), ESlateSizeRule::Fill);
	UVerticalBox* const SessionBlock = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SessionBlock"));
	JoinCodeText = JTSUITheme::MakeText(WidgetTree, TEXT("JoinCode"), TEXT("JOIN CODE: ------"), 17.0f, JTSUITheme::Ink, ETextJustify::Right);
	AddVertical(SessionBlock, JoinCodeText);
	PlayerCountText = JTSUITheme::MakeText(WidgetTree, TEXT("PlayerCount"), TEXT("PLAYERS 1 / 4"), 12.0f, JTSUITheme::Muted, ETextJustify::Right);
	AddVertical(SessionBlock, PlayerCountText);
	VisibilityText = JTSUITheme::MakeText(WidgetTree, TEXT("Visibility"), TEXT("PUBLIC"), 12.0f, JTSUITheme::Ready, ETextJustify::Right);
	AddVertical(SessionBlock, VisibilityText);
	AddHorizontal(HeaderRow, SessionBlock);
	UButton* const CopyButton = JTSUITheme::MakeButton(WidgetTree, TEXT("CopyJoinCode"), TEXT("COPY"), JTSUITheme::EButtonTone::Secondary, 13.0f);
	CopyButton->OnClicked.AddDynamic(this, &UJTSPreLaunchLobbyWidget::CopyJoinCode);
	AddHorizontal(HeaderRow, CopyButton, FMargin(12.0f, 4.0f, 0.0f, 4.0f));

	UBorder* const RosterPanel = JTSUITheme::MakePanel(WidgetTree, TEXT("RosterPanel"), FLinearColor(0.018f, 0.050f, 0.092f, 0.84f), FMargin(18.0f));
	AddCanvas(RootCanvas, RosterPanel, FAnchors(0.035f, 0.205f, 0.965f, 0.705f), FMargin(0.0f));
	UUniformGridPanel* const RosterGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("RosterGrid"));
	RosterGrid->SetSlotPadding(FMargin(9.0f));
	RosterPanel->SetContent(RosterGrid);
	for (int32 SlotIndex = 0; SlotIndex < 4; ++SlotIndex)
	{
		UJTSPlayerCardWidget* const Card = WidgetTree->ConstructWidget<UJTSPlayerCardWidget>(ResolvePlayerCardClass(), *FString::Printf(TEXT("PlayerCard%d"), SlotIndex + 1));
		RosterGrid->AddChildToUniformGrid(Card, 0, SlotIndex);
		PlayerCards.Add(Card);
	}

	UBorder* const Footer = JTSUITheme::MakePanel(WidgetTree, TEXT("Footer"), FLinearColor(0.025f, 0.070f, 0.125f, 0.95f), FMargin(18.0f, 12.0f));
	AddCanvas(RootCanvas, Footer, FAnchors(0.035f, 0.745f, 0.965f, 0.955f), FMargin(0.0f));
	UHorizontalBox* const FooterRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FooterRow"));
	Footer->SetContent(FooterRow);
	UVerticalBox* const FooterStatus = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FooterStatus"));
	LobbyStatusText = JTSUITheme::MakeText(WidgetTree, TEXT("LobbyStatus"), TEXT("WAITING FOR CREW"), 16.0f, JTSUITheme::Warning);
	AddVertical(FooterStatus, LobbyStatusText);
	AddVertical(FooterStatus, JTSUITheme::MakeText(WidgetTree, TEXT("LobbyHint"), TEXT("Ready up when your crew and avatar are set."), 12.0f, JTSUITheme::Muted));
	AddHorizontal(FooterRow, FooterStatus, FMargin(4.0f), ESlateSizeRule::Fill);
	MicButton = JTSUITheme::MakeButton(WidgetTree, TEXT("MicButton"), TEXT("MIC"), JTSUITheme::EButtonTone::Secondary, 14.0f);
	MicButtonText = Cast<UTextBlock>(MicButton->GetContent());
	MicButton->OnClicked.AddDynamic(this, &UJTSPreLaunchLobbyWidget::ToggleInputMute);
	AddHorizontal(FooterRow, MicButton);
	UButton* const LeaveButton = JTSUITheme::MakeButton(WidgetTree, TEXT("LeaveButton"), TEXT("LEAVE"), JTSUITheme::EButtonTone::Secondary, 14.0f);
	LeaveButton->OnClicked.AddDynamic(this, &UJTSPreLaunchLobbyWidget::LeaveLobby);
	AddHorizontal(FooterRow, LeaveButton);
	ReadyButton = JTSUITheme::MakeButton(WidgetTree, TEXT("ReadyButton"), TEXT("READY"), JTSUITheme::EButtonTone::Ready, 16.0f);
	ReadyButtonText = Cast<UTextBlock>(ReadyButton->GetContent());
	ReadyButton->OnClicked.AddDynamic(this, &UJTSPreLaunchLobbyWidget::ToggleReady);
	AddHorizontal(FooterRow, ReadyButton, FMargin(12.0f, 4.0f, 4.0f, 4.0f));
	HostOptionsButton = JTSUITheme::MakeButton(WidgetTree, TEXT("HostOptions"), TEXT("HOST OPTIONS"), JTSUITheme::EButtonTone::Secondary, 14.0f);
	HostOptionsButton->OnClicked.AddDynamic(this, &UJTSPreLaunchLobbyWidget::OpenHostOptions);
	AddHorizontal(FooterRow, HostOptionsButton);
	LaunchButton = JTSUITheme::MakeButton(WidgetTree, TEXT("LaunchButton"), TEXT("LAUNCH EXPEDITION"), JTSUITheme::EButtonTone::Primary, 16.0f);
	LaunchButtonText = Cast<UTextBlock>(LaunchButton->GetContent());
	LaunchButton->OnClicked.AddDynamic(this, &UJTSPreLaunchLobbyWidget::LaunchExpedition);
	AddHorizontal(FooterRow, LaunchButton, FMargin(12.0f, 4.0f, 4.0f, 4.0f));

	ModalCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ModalCanvas"));
	AddCanvas(RootCanvas, ModalCanvas, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f));

	ContextPanel = JTSUITheme::MakePanel(WidgetTree, TEXT("ContextPanel"), JTSUITheme::Panel, FMargin(22.0f));
	AddCanvas(ModalCanvas, ContextPanel, FAnchors(0.5f, 0.5f), FMargin(0.0f, 0.0f, 410.0f, 260.0f), FVector2D(0.5f, 0.5f));
	UVerticalBox* const ContextLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContextLayout"));
	ContextPanel->SetContent(ContextLayout);
	AddVertical(ContextLayout, JTSUITheme::MakeText(WidgetTree, TEXT("ContextTitle"), TEXT("PLAYER OPTIONS"), 22.0f, JTSUITheme::Ink, ETextJustify::Center));
	ContextTargetText = JTSUITheme::MakeText(WidgetTree, TEXT("ContextTarget"), TEXT("PLAYER"), 15.0f, JTSUITheme::Muted, ETextJustify::Center);
	AddVertical(ContextLayout, ContextTargetText, FMargin(0.0f, 2.0f, 0.0f, 12.0f));
	UButton* const MuteButton = JTSUITheme::MakeButton(WidgetTree, TEXT("ContextMute"), TEXT("MUTE"), JTSUITheme::EButtonTone::Secondary);
	ContextMuteText = Cast<UTextBlock>(MuteButton->GetContent());
	MuteButton->OnClicked.AddDynamic(this, &UJTSPreLaunchLobbyWidget::ToggleTargetMute);
	AddVertical(ContextLayout, MuteButton);
	UButton* const KickButton = JTSUITheme::MakeButton(WidgetTree, TEXT("ContextKick"), TEXT("KICK PLAYER"), JTSUITheme::EButtonTone::Danger);
	KickButton->OnClicked.AddDynamic(this, &UJTSPreLaunchLobbyWidget::KickTarget);
	AddVertical(ContextLayout, KickButton);
	UButton* const ContextCloseButton = JTSUITheme::MakeButton(WidgetTree, TEXT("ContextClose"), TEXT("CLOSE"), JTSUITheme::EButtonTone::Secondary);
	ContextCloseButton->OnClicked.AddDynamic(this, &UJTSPreLaunchLobbyWidget::CloseContextMenu);
	AddVertical(ContextLayout, ContextCloseButton);
	SetContextPanelVisible(false);

	HostOptionsPanel = JTSUITheme::MakePanel(WidgetTree, TEXT("HostOptionsPanel"), JTSUITheme::Panel, FMargin(22.0f));
	AddCanvas(ModalCanvas, HostOptionsPanel, FAnchors(0.5f, 0.5f), FMargin(0.0f, 0.0f, 450.0f, 270.0f), FVector2D(0.5f, 0.5f));
	UVerticalBox* const HostOptionsLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HostOptionsLayout"));
	HostOptionsPanel->SetContent(HostOptionsLayout);
	AddVertical(HostOptionsLayout, JTSUITheme::MakeText(WidgetTree, TEXT("HostOptionsTitle"), TEXT("LOBBY SETTINGS"), 22.0f, JTSUITheme::Ink, ETextJustify::Center));
	HostOptionsStatusText = JTSUITheme::MakeText(WidgetTree, TEXT("HostOptionsStatus"), TEXT("JOINING IS OPEN"), 14.0f, JTSUITheme::Muted, ETextJustify::Center);
	AddVertical(HostOptionsLayout, HostOptionsStatusText, FMargin(0.0f, 4.0f, 0.0f, 12.0f));
	CloseJoiningButton = JTSUITheme::MakeButton(WidgetTree, TEXT("CloseJoining"), TEXT("CLOSE JOINING"), JTSUITheme::EButtonTone::Danger);
	CloseJoiningButton->OnClicked.AddDynamic(this, &UJTSPreLaunchLobbyWidget::CloseJoining);
	AddVertical(HostOptionsLayout, CloseJoiningButton);
	UButton* const HostOptionsCloseButton = JTSUITheme::MakeButton(WidgetTree, TEXT("HostOptionsClose"), TEXT("DONE"), JTSUITheme::EButtonTone::Secondary);
	HostOptionsCloseButton->OnClicked.AddDynamic(this, &UJTSPreLaunchLobbyWidget::CloseHostOptions);
	AddVertical(HostOptionsLayout, HostOptionsCloseButton);
	SetHostOptionsVisible(false);
}

void UJTSPreLaunchLobbyWidget::Refresh()
{
	const AJTSGameState* const State = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
	AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer());
	AJTSPlayerState* const LocalState = Controller != nullptr ? Controller->GetPlayerState<AJTSPlayerState>() : nullptr;
	if (State == nullptr || LocalState == nullptr)
	{
		return;
	}

	TArray<AJTSPlayerState*> Players;
	for (APlayerState* const RawPlayerState : State->PlayerArray)
	{
		if (AJTSPlayerState* const PlayerState = Cast<AJTSPlayerState>(RawPlayerState))
		{
			Players.Add(PlayerState);
		}
	}
	Players.Sort([](const AJTSPlayerState& Left, const AJTSPlayerState& Right)
	{
		return Left.GetPlayerId() < Right.GetPlayerId();
	});

	const UJTSOnlineSessionSubsystem* const Online = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>() : nullptr;
	const int32 MaximumPlayers = Online != nullptr ? Online->GetCurrentMaximumPlayers() : 4;
	JoinCodeText->SetText(FText::FromString(FString::Printf(TEXT("JOIN CODE: %s"), Online != nullptr ? *Online->GetCurrentJoinCode() : TEXT("------"))));
	PlayerCountText->SetText(FText::FromString(FString::Printf(TEXT("PLAYERS %d / %d"), Players.Num(), MaximumPlayers)));
	const bool bPrivate = Online != nullptr && Online->GetCurrentLobbyVisibility() == EJTSLobbyVisibility::Private;
	VisibilityText->SetText(FText::FromString(bPrivate ? TEXT("PRIVATE") : TEXT("PUBLIC")));
	VisibilityText->SetColorAndOpacity(FSlateColor(bPrivate ? JTSUITheme::Warning : JTSUITheme::Ready));

	for (int32 SlotIndex = 0; SlotIndex < PlayerCards.Num(); ++SlotIndex)
	{
		PlayerCards[SlotIndex]->SetSlot(SlotIndex, Players.IsValidIndex(SlotIndex) ? Players[SlotIndex] : nullptr, Players.IsValidIndex(SlotIndex) && Players[SlotIndex] == LocalState, LocalState->IsExpeditionHost(), this);
	}

	const int32 NotReadyCount = Players.FilterByPredicate([](const AJTSPlayerState* Player)
	{
		return Player == nullptr || !Player->IsReady();
	}).Num();
	const bool bAllReady = !Players.IsEmpty() && NotReadyCount == 0;
	ReadyButtonText->SetText(FText::FromString(LocalState->IsReady() ? TEXT("✓ READY") : TEXT("READY")));
	ReadyButton->SetIsEnabled(State->IsWaitingToStart());
	LobbyStatusText->SetText(FText::FromString(bAllReady
		? TEXT("ALL PLAYERS READY")
		: FString::Printf(TEXT("WAITING FOR %d PLAYER%s"), NotReadyCount, NotReadyCount == 1 ? TEXT("") : TEXT("S"))));
	LobbyStatusText->SetColorAndOpacity(FSlateColor(bAllReady ? JTSUITheme::Ready : JTSUITheme::Warning));
	LaunchButton->SetVisibility(LocalState->IsExpeditionHost() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	LaunchButton->SetIsEnabled(LocalState->IsExpeditionHost() && State->IsWaitingToStart() && bAllReady);
	LaunchButtonText->SetText(FText::FromString(bAllReady ? TEXT("LAUNCH EXPEDITION") : TEXT("LAUNCH LOCKED")));
	HostOptionsButton->SetVisibility(LocalState->IsExpeditionHost() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	HostOptionsStatusText->SetText(FText::FromString(State->IsAcceptingNewPlayers() ? TEXT("JOINING IS OPEN") : TEXT("JOINING IS CLOSED")));
	CloseJoiningButton->SetIsEnabled(State->IsAcceptingNewPlayers());

	UJTSVoiceSubsystem* const Voice = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>() : nullptr;
	const bool bVoiceAvailable = Voice != nullptr && Voice->IsVoiceAvailable();
	MicButtonText->SetText(FText::FromString(!bVoiceAvailable ? TEXT("MIC DISABLED") : (Voice->IsInputMuted() ? TEXT("MIC MUTED") : TEXT("MIC ON"))));
	MicButton->SetIsEnabled(bVoiceAvailable);
	MicButton->SetToolTipText(FText::FromString(bVoiceAvailable ? TEXT("Toggle local microphone mute.") : TEXT("Voice chat unavailable with the current local/LAN provider.")));
	if (ContextPanel->GetVisibility() == ESlateVisibility::Visible && ContextTarget.Get() == nullptr)
	{
		SetContextPanelVisible(false);
	}
}

void UJTSPreLaunchLobbyWidget::OpenPlayerCustomization()
{
	if (ModalCanvas == nullptr || CustomizeWidget != nullptr)
	{
		return;
	}
	CustomizeWidget = CreateWidget<UJTSPlayerCustomizeWidget>(GetOwningPlayer(), ResolvePlayerCustomizeClass());
	if (CustomizeWidget != nullptr)
	{
		CustomizeWidget->SetLobbyOwner(this);
		UCanvasPanelSlot* const CanvasSlot = ModalCanvas->AddChildToCanvas(CustomizeWidget);
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetSize(FVector2D(620.0f, 430.0f));
		CanvasSlot->SetPosition(FVector2D::ZeroVector);
	}
}

void UJTSPreLaunchLobbyWidget::ClosePlayerCustomization()
{
	if (CustomizeWidget != nullptr)
	{
		CustomizeWidget->RemoveFromParent();
		CustomizeWidget = nullptr;
	}
}

void UJTSPreLaunchLobbyWidget::OpenPlayerContextMenu(AJTSPlayerState* TargetPlayerState)
{
	AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer());
	AJTSPlayerState* const LocalState = Controller != nullptr ? Controller->GetPlayerState<AJTSPlayerState>() : nullptr;
	if (TargetPlayerState == nullptr || LocalState == nullptr || !LocalState->IsExpeditionHost() || TargetPlayerState == LocalState || TargetPlayerState->IsExpeditionHost())
	{
		return;
	}
	ContextTarget = TargetPlayerState;
	ContextTargetText->SetText(FText::FromString(TargetPlayerState->GetPlayerName()));
	if (UJTSVoiceSubsystem* const Voice = GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>())
	{
		ContextMuteText->SetText(FText::FromString(Voice->IsPlayerMuted(TargetPlayerState->GetVoiceIdentityString()) ? TEXT("UNMUTE") : TEXT("MUTE")));
	}
	SetContextPanelVisible(true);
}

void UJTSPreLaunchLobbyWidget::SetContextPanelVisible(bool bVisible)
{
	if (ContextPanel != nullptr)
	{
		ContextPanel->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UJTSPreLaunchLobbyWidget::SetHostOptionsVisible(bool bVisible)
{
	if (HostOptionsPanel != nullptr)
	{
		HostOptionsPanel->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UJTSPreLaunchLobbyWidget::ToggleReady()
{
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer())) Controller->RequestSetReady();
}

void UJTSPreLaunchLobbyWidget::LaunchExpedition()
{
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer())) Controller->StartGame();
}

void UJTSPreLaunchLobbyWidget::LeaveLobby()
{
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>()) Online->LeaveExpedition();
}

void UJTSPreLaunchLobbyWidget::CopyJoinCode()
{
	if (const UJTSOnlineSessionSubsystem* const Online = GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>())
	{
		FPlatformApplicationMisc::ClipboardCopy(*Online->GetCurrentJoinCode());
	}
}

void UJTSPreLaunchLobbyWidget::ToggleInputMute()
{
	if (UJTSVoiceSubsystem* const Voice = GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>()) Voice->SetInputMuted(!Voice->IsInputMuted());
}

void UJTSPreLaunchLobbyWidget::OpenHostOptions() { SetHostOptionsVisible(true); }
void UJTSPreLaunchLobbyWidget::CloseHostOptions() { SetHostOptionsVisible(false); }
void UJTSPreLaunchLobbyWidget::CloseJoining() { if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer())) Controller->RequestCloseJoining(); }

void UJTSPreLaunchLobbyWidget::ToggleTargetMute()
{
	if (AJTSPlayerState* const Target = ContextTarget.Get())
	{
		if (UJTSVoiceSubsystem* const Voice = GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>())
		{
			Voice->SetPlayerMuted(Target->GetVoiceIdentityString(), !Voice->IsPlayerMuted(Target->GetVoiceIdentityString()));
			ContextMuteText->SetText(FText::FromString(Voice->IsPlayerMuted(Target->GetVoiceIdentityString()) ? TEXT("UNMUTE") : TEXT("MUTE")));
		}
	}
}

void UJTSPreLaunchLobbyWidget::KickTarget()
{
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		Controller->RequestKickPlayer(ContextTarget.Get());
	}
	CloseContextMenu();
}

void UJTSPreLaunchLobbyWidget::CloseContextMenu()
{
	ContextTarget.Reset();
	SetContextPanelVisible(false);
}

TSubclassOf<UJTSPlayerCardWidget> UJTSPreLaunchLobbyWidget::ResolvePlayerCardClass() const
{
	const TSubclassOf<UJTSPlayerCardWidget> LoadedClass = PlayerCardWidgetClass.LoadSynchronous();
	if (LoadedClass != nullptr)
	{
		return LoadedClass;
	}
	return UJTSPlayerCardWidget::StaticClass();
}

TSubclassOf<UJTSPlayerCustomizeWidget> UJTSPreLaunchLobbyWidget::ResolvePlayerCustomizeClass() const
{
	const TSubclassOf<UJTSPlayerCustomizeWidget> LoadedClass = PlayerCustomizeWidgetClass.LoadSynchronous();
	if (LoadedClass != nullptr)
	{
		return LoadedClass;
	}
	return UJTSPlayerCustomizeWidget::StaticClass();
}
