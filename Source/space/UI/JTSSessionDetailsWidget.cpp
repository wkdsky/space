// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSSessionDetailsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "HAL/PlatformApplicationMisc.h"

TSharedRef<SWidget> UJTSSessionDetailsWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSSessionDetailsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UJTSSessionDetailsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetJoinCode(CurrentJoinCode);
}

void UJTSSessionDetailsWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass()); WidgetTree->RootWidget = Layout;
	JoinCodeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); Layout->AddChildToVerticalBox(JoinCodeText);
	UButton* CopyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass()); UTextBlock* CopyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); CopyText->SetText(FText::FromString(TEXT("Copy join code"))); CopyButton->AddChild(CopyText); Layout->AddChildToVerticalBox(CopyButton); CopyButton->OnClicked.AddDynamic(this, &UJTSSessionDetailsWidget::CopyJoinCode);
}
void UJTSSessionDetailsWidget::SetJoinCode(const FString& JoinCode)
{
	SetSessionDetails(JoinCode, bCurrentPasswordProtected, CurrentPlayerCount, CurrentMaximumPlayers);
}
void UJTSSessionDetailsWidget::SetSessionDetails(const FString& JoinCode, bool bPasswordProtected, int32 CurrentPlayers, int32 MaximumPlayers)
{
	CurrentJoinCode = JoinCode;
	bCurrentPasswordProtected = bPasswordProtected;
	CurrentPlayerCount = FMath::Max(0, CurrentPlayers);
	CurrentMaximumPlayers = FMath::Clamp(MaximumPlayers, 1, 4);
	if (JoinCodeText != nullptr)
	{
		JoinCodeText->SetText(FText::FromString(FString::Printf(
			TEXT("Join code: %s | Players: %d/%d | %s"),
			*CurrentJoinCode,
			CurrentPlayerCount,
			CurrentMaximumPlayers,
			bCurrentPasswordProtected ? TEXT("Password protected") : TEXT("No password"))));
	}
}
void UJTSSessionDetailsWidget::CopyJoinCode() { if (!CurrentJoinCode.IsEmpty()) FPlatformApplicationMisc::ClipboardCopy(*CurrentJoinCode); }
