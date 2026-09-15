// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/FrontEnd/JTSHostExpeditionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"
#include "space/UI/FrontEnd/JTSFrontEndRootWidget.h"
#include "space/Systems/JTSOnlineSessionSubsystem.h"

namespace
{
	UTextBlock* MakeText(UWidgetTree* WidgetTree, const FName& Name, const FString& Text, const float FontSize)
	{
		if (WidgetTree == nullptr)
		{
			return nullptr;
		}
		UTextBlock* const TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		if (TextBlock != nullptr)
		{
			TextBlock->SetText(FText::FromString(Text));
			TextBlock->SetFont(FCoreStyle::GetDefaultFontStyle(FName(TEXT("Bold")), FontSize));
			TextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			TextBlock->SetAutoWrapText(true);
		}
		return TextBlock;
	}

	UButton* MakeButton(UWidgetTree* WidgetTree, const FName& Name, const FString& Label)
	{
		if (WidgetTree == nullptr)
		{
			return nullptr;
		}
		UButton* const Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		if (Button != nullptr)
		{
			Button->SetBackgroundColor(FLinearColor(0.08f, 0.35f, 0.58f, 1.0f));
			Button->SetContent(MakeText(WidgetTree, *FString::Printf(TEXT("%sLabel"), *Name.ToString()), Label, 20.0f));
		}
		return Button;
	}

	void AddRow(UVerticalBox* Layout, UWidget* Child, const FMargin& Padding = FMargin(24.0f, 7.0f))
	{
		if (Layout != nullptr && Child != nullptr)
		{
			if (UVerticalBoxSlot* const Slot = Layout->AddChildToVerticalBox(Child))
			{
				Slot->SetPadding(Padding);
				Slot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
	}
}

TSharedRef<SWidget> UJTSHostExpeditionWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSHostExpeditionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UJTSHostExpeditionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>() : nullptr)
	{
		Online->OnSessionOperationFinished.RemoveDynamic(this, &UJTSHostExpeditionWidget::HandleOperationFinished);
		Online->OnSessionOperationFinished.AddDynamic(this, &UJTSHostExpeditionWidget::HandleOperationFinished);
		if (StatusText != nullptr)
		{
			StatusText->SetText(FText::FromString(Online->GetProviderStatus()));
		}
	}
}

void UJTSHostExpeditionWidget::NativeDestruct()
{
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>() : nullptr)
	{
		Online->OnSessionOperationFinished.RemoveDynamic(this, &UJTSHostExpeditionWidget::HandleOperationFinished);
	}
	Super::NativeDestruct();
}

void UJTSHostExpeditionWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	UVerticalBox* const Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HostExpeditionLayout"));
	WidgetTree->RootWidget = Layout;
	AddRow(Layout, MakeText(WidgetTree, TEXT("HostTitle"), TEXT("HOST EXPEDITION"), 24.0f), FMargin(0.0f, 4.0f, 0.0f, 14.0f));
	AddRow(Layout, MakeText(WidgetTree, TEXT("HostDescription"), TEXT("Create a 1-4 player expedition. NULL/LAN is supported for local development."), 14.0f), FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	JoinCodeBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("JoinCodeBox"));
	JoinCodeBox->SetHintText(FText::FromString(TEXT("Join code (optional)")));
	AddRow(Layout, JoinCodeBox);
	PasswordBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("PasswordBox"));
	PasswordBox->SetHintText(FText::FromString(TEXT("Password (optional)")));
	PasswordBox->SetIsPassword(true);
	AddRow(Layout, PasswordBox);
	PlayerCountBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("PlayerCountBox"));
	PlayerCountBox->SetHintText(FText::FromString(TEXT("Players (1-4)")));
	PlayerCountBox->SetText(FText::FromString(TEXT("4")));
	AddRow(Layout, PlayerCountBox);

	CreateButton = MakeButton(WidgetTree, TEXT("CreateExpeditionButton"), TEXT("Create Expedition"));
	AddRow(Layout, CreateButton, FMargin(24.0f, 14.0f, 24.0f, 7.0f));
	CreateButton->OnClicked.AddDynamic(this, &UJTSHostExpeditionWidget::CreateExpedition);
	UButton* const BackButton = MakeButton(WidgetTree, TEXT("HostBackButton"), TEXT("Back"));
	AddRow(Layout, BackButton);
	BackButton->OnClicked.AddDynamic(this, &UJTSHostExpeditionWidget::ReturnToMain);
	StatusText = MakeText(WidgetTree, TEXT("HostStatusText"), TEXT("Preparing host settings..."), 14.0f);
	AddRow(Layout, StatusText, FMargin(12.0f, 12.0f));
}

void UJTSHostExpeditionWidget::CreateExpedition()
{
	if (UJTSOnlineSessionSubsystem* const Online = GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>())
	{
		const int32 RequestedPlayers = PlayerCountBox != nullptr ? FCString::Atoi(*PlayerCountBox->GetText().ToString()) : 4;
		const FString JoinCode = JoinCodeBox != nullptr ? JoinCodeBox->GetText().ToString() : FString();
		const FString Password = PasswordBox != nullptr ? PasswordBox->GetText().ToString() : FString();
		Online->CreateExpedition(JoinCode, Password, RequestedPlayers);
	}
}

void UJTSHostExpeditionWidget::ReturnToMain()
{
	if (UJTSFrontEndRootWidget* const Root = GetTypedOuter<UJTSFrontEndRootWidget>()) Root->ShowMainMenu();
}

void UJTSHostExpeditionWidget::HandleOperationFinished(bool bSucceeded, const FString& Message)
{
	if (StatusText != nullptr) StatusText->SetText(FText::FromString(Message));
}
