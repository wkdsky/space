// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSLobbyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Systems/JTSOnlineSessionSubsystem.h"
#include "space/Systems/JTSVoiceSubsystem.h"
#include "space/UI/JTSSessionDetailsWidget.h"

namespace
{
	const TCHAR* AvatarColorLabel(EJTSAvatarColor Color)
	{
		switch (Color)
		{
		case EJTSAvatarColor::Orange: return TEXT("Orange");
		case EJTSAvatarColor::Green: return TEXT("Green");
		case EJTSAvatarColor::Purple: return TEXT("Purple");
		default: return TEXT("Blue");
		}
	}
}

TSharedRef<SWidget> UJTSLobbyWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSLobbyWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UJTSLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

void UJTSLobbyWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass()); WidgetTree->RootWidget = Layout;
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); Title->SetText(FText::FromString(TEXT("Expedition Lobby"))); Layout->AddChildToVerticalBox(Title);
	SessionDetails = WidgetTree->ConstructWidget<UJTSSessionDetailsWidget>(UJTSSessionDetailsWidget::StaticClass()); Layout->AddChildToVerticalBox(SessionDetails);
	PlayerListText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); Layout->AddChildToVerticalBox(PlayerListText);
	ReadyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass()); UTextBlock* ReadyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); ReadyText->SetText(FText::FromString(TEXT("Ready / Unready"))); ReadyButton->AddChild(ReadyText); Layout->AddChildToVerticalBox(ReadyButton); ReadyButton->OnClicked.AddDynamic(this, &UJTSLobbyWidget::ToggleReady);
	AvatarButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass()); UTextBlock* AvatarText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); AvatarText->SetText(FText::FromString(TEXT("Choose next avatar color"))); AvatarButton->AddChild(AvatarText); Layout->AddChildToVerticalBox(AvatarButton); AvatarButton->OnClicked.AddDynamic(this, &UJTSLobbyWidget::CycleAvatarColor);
	StartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass()); UTextBlock* StartText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); StartText->SetText(FText::FromString(TEXT("Host: Start expedition"))); StartButton->AddChild(StartText); Layout->AddChildToVerticalBox(StartButton); StartButton->OnClicked.AddDynamic(this, &UJTSLobbyWidget::RequestStart);
	CloseJoiningButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass()); UTextBlock* CloseJoiningText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); CloseJoiningText->SetText(FText::FromString(TEXT("Host: Close joining"))); CloseJoiningButton->AddChild(CloseJoiningText); Layout->AddChildToVerticalBox(CloseJoiningButton); CloseJoiningButton->OnClicked.AddDynamic(this, &UJTSLobbyWidget::CloseJoining);
	KickButtons.Reserve(4);
	MuteButtons.Reserve(4);
	for (int32 SlotIndex = 0; SlotIndex < 4; ++SlotIndex)
	{
		UButton* const KickButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock* const KickText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		KickText->SetText(FText::FromString(FString::Printf(TEXT("Host: Kick slot %d"), SlotIndex + 1)));
		KickButton->AddChild(KickText);
		Layout->AddChildToVerticalBox(KickButton);
		KickButtons.Add(KickButton);
		switch (SlotIndex)
		{
		case 0: KickButton->OnClicked.AddDynamic(this, &UJTSLobbyWidget::KickSlotOne); break;
		case 1: KickButton->OnClicked.AddDynamic(this, &UJTSLobbyWidget::KickSlotTwo); break;
		case 2: KickButton->OnClicked.AddDynamic(this, &UJTSLobbyWidget::KickSlotThree); break;
		default: KickButton->OnClicked.AddDynamic(this, &UJTSLobbyWidget::KickSlotFour); break;
		}

		UButton* const MuteButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock* const MuteText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		MuteText->SetText(FText::FromString(FString::Printf(TEXT("Mute / unmute slot %d"), SlotIndex + 1)));
		MuteButton->AddChild(MuteText);
		Layout->AddChildToVerticalBox(MuteButton);
		MuteButtons.Add(MuteButton);
		switch (SlotIndex)
		{
		case 0: MuteButton->OnClicked.AddDynamic(this, &UJTSLobbyWidget::MuteSlotOne); break;
		case 1: MuteButton->OnClicked.AddDynamic(this, &UJTSLobbyWidget::MuteSlotTwo); break;
		case 2: MuteButton->OnClicked.AddDynamic(this, &UJTSLobbyWidget::MuteSlotThree); break;
		default: MuteButton->OnClicked.AddDynamic(this, &UJTSLobbyWidget::MuteSlotFour); break;
		}
	}
	UButton* LeaveButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass()); UTextBlock* LeaveText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); LeaveText->SetText(FText::FromString(TEXT("Leave"))); LeaveButton->AddChild(LeaveText); Layout->AddChildToVerticalBox(LeaveButton); LeaveButton->OnClicked.AddDynamic(this, &UJTSLobbyWidget::Leave);
	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); Layout->AddChildToVerticalBox(StatusText);
	Refresh();
}

void UJTSLobbyWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.25f) { RefreshAccumulator = 0.0f; Refresh(); }
}
void UJTSLobbyWidget::ToggleReady() { if (AJTSPlayerController* PC = Cast<AJTSPlayerController>(GetOwningPlayer())) PC->RequestSetReady(); }
void UJTSLobbyWidget::CycleAvatarColor()
{
	if (AJTSPlayerController* const PC = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		if (const AJTSPlayerState* const State = PC->GetPlayerState<AJTSPlayerState>())
		{
			const uint8 NextColor = (static_cast<uint8>(State->GetAvatarColor()) + 1) % 4;
			PC->RequestAvatarColor(static_cast<EJTSAvatarColor>(NextColor));
		}
	}
}
void UJTSLobbyWidget::RequestStart() { if (AJTSPlayerController* PC = Cast<AJTSPlayerController>(GetOwningPlayer())) PC->StartGame(); }
void UJTSLobbyWidget::CloseJoining() { if (AJTSPlayerController* PC = Cast<AJTSPlayerController>(GetOwningPlayer())) PC->RequestCloseJoining(); }
void UJTSLobbyWidget::Leave() { if (UJTSOnlineSessionSubsystem* Online = GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>()) Online->LeaveExpedition(); }
void UJTSLobbyWidget::KickSlotOne() { KickSlot(0); }
void UJTSLobbyWidget::KickSlotTwo() { KickSlot(1); }
void UJTSLobbyWidget::KickSlotThree() { KickSlot(2); }
void UJTSLobbyWidget::KickSlotFour() { KickSlot(3); }
void UJTSLobbyWidget::MuteSlotOne() { MuteSlot(0); }
void UJTSLobbyWidget::MuteSlotTwo() { MuteSlot(1); }
void UJTSLobbyWidget::MuteSlotThree() { MuteSlot(2); }
void UJTSLobbyWidget::MuteSlotFour() { MuteSlot(3); }
void UJTSLobbyWidget::KickSlot(int32 SlotIndex)
{
	if (AJTSPlayerController* const PC = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		if (SlotPlayers.IsValidIndex(SlotIndex)) PC->RequestKickPlayer(SlotPlayers[SlotIndex].Get());
	}
}
void UJTSLobbyWidget::MuteSlot(int32 SlotIndex)
{
	if (!SlotPlayers.IsValidIndex(SlotIndex)) return;
	if (UJTSVoiceSubsystem* const Voice = GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>())
	{
		if (const AJTSPlayerState* const Target = SlotPlayers[SlotIndex].Get())
		{
			const FString VoiceIdentity = Target->GetVoiceIdentityString();
			Voice->SetPlayerMuted(VoiceIdentity, !Voice->IsPlayerMuted(VoiceIdentity));
		}
	}
}
void UJTSLobbyWidget::Refresh()
{
	const AJTSGameState* State = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
	AJTSPlayerState* LocalState = GetOwningPlayer() != nullptr ? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr;
	if (State == nullptr || LocalState == nullptr) return;
	TArray<const AJTSPlayerState*> Players;
	const UJTSVoiceSubsystem* const Voice = GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>();
	for (APlayerState* Player : State->PlayerArray)
	{
		if (const AJTSPlayerState* JTS = Cast<AJTSPlayerState>(Player)) Players.Add(JTS);
	}
	Players.Sort([](const AJTSPlayerState& Left, const AJTSPlayerState& Right) { return Left.GetPlayerId() < Right.GetPlayerId(); });
	SlotPlayers.SetNum(4);
	for (int32 SlotIndex = 0; SlotIndex < SlotPlayers.Num(); ++SlotIndex)
	{
		SlotPlayers[SlotIndex].Reset();
		if (Players.IsValidIndex(SlotIndex)) SlotPlayers[SlotIndex] = const_cast<AJTSPlayerState*>(Players[SlotIndex]);
	}
	FString PlayerSlots;
	for (int32 SlotIndex = 0; SlotIndex < 4; ++SlotIndex)
	{
		if (Players.IsValidIndex(SlotIndex))
		{
			const AJTSPlayerState* JTS = Players[SlotIndex];
			const TCHAR* VoiceStatus = Voice == nullptr || !Voice->IsVoiceAvailable()
				? TEXT("Voice unavailable")
				: (Voice->IsPlayerTalking(JTS->GetVoiceIdentityString()) ? TEXT("Talking") : TEXT("Mic ready"));
			PlayerSlots += FString::Printf(TEXT("Slot %d: %s%s - %s (%s, %s, Connected)%s\n"), SlotIndex + 1, JTS->IsExpeditionHost() ? TEXT("Host ") : TEXT(""), *JTS->GetPlayerName(), JTS->IsReady() ? TEXT("Ready") : TEXT("Not ready"), AvatarColorLabel(JTS->GetAvatarColor()), VoiceStatus, JTS->IsBoarded() ? TEXT(" - Boarded") : TEXT(""));
		}
		else
		{
			PlayerSlots += FString::Printf(TEXT("Slot %d: Open\n"), SlotIndex + 1);
		}
	}
	PlayerListText->SetText(FText::FromString(PlayerSlots));
	StartButton->SetVisibility(LocalState->IsExpeditionHost() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	StartButton->SetIsEnabled(LocalState->IsExpeditionHost() && State->IsWaitingToStart() && Players.Num() > 0 && !Players.ContainsByPredicate([](const AJTSPlayerState* Player) { return Player == nullptr || !Player->IsReady(); }));
	CloseJoiningButton->SetVisibility(LocalState->IsExpeditionHost() && State->IsWaitingToStart() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	CloseJoiningButton->SetIsEnabled(State->IsAcceptingNewPlayers());
	for (int32 SlotIndex = 0; SlotIndex < KickButtons.Num(); ++SlotIndex)
	{
		const AJTSPlayerState* const Target = SlotPlayers.IsValidIndex(SlotIndex) ? SlotPlayers[SlotIndex].Get() : nullptr;
		KickButtons[SlotIndex]->SetVisibility(LocalState->IsExpeditionHost() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		KickButtons[SlotIndex]->SetIsEnabled(State->IsWaitingToStart() && Target != nullptr && !Target->IsExpeditionHost());
	}
	for (int32 SlotIndex = 0; SlotIndex < MuteButtons.Num(); ++SlotIndex)
	{
		const AJTSPlayerState* const Target = SlotPlayers.IsValidIndex(SlotIndex) ? SlotPlayers[SlotIndex].Get() : nullptr;
		MuteButtons[SlotIndex]->SetIsEnabled(Target != nullptr && Target != LocalState && Voice != nullptr && Voice->IsVoiceAvailable());
	}
	StatusText->SetText(FText::FromString(State->IsWaitingToStart()
		? (State->IsAcceptingNewPlayers() ? TEXT("Waiting for all players to ready. Joining is open.") : TEXT("Waiting for all players to ready. Joining is closed."))
		: TEXT("Expedition in progress.")));
	if (SessionDetails != nullptr)
	{
		if (const UJTSOnlineSessionSubsystem* const Online = GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>())
		{
			SessionDetails->SetSessionDetails(
				Online->GetCurrentJoinCode(),
				Online->IsCurrentSessionPasswordProtected(),
				Players.Num(),
				Online->GetCurrentMaximumPlayers());
		}
	}
}
