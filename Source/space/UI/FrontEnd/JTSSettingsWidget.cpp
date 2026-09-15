// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/FrontEnd/JTSSettingsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "space/UI/FrontEnd/JTSFrontEndRootWidget.h"
#include "space/Systems/JTSVoiceSubsystem.h"
#include "space/UI/JTSUITheme.h"

namespace
{
	void AddRow(UVerticalBox* Parent, UWidget* Child, const FMargin& Padding = FMargin(0.0f, 6.0f))
	{
		if (Parent != nullptr && Child != nullptr)
		{
			if (UVerticalBoxSlot* const VerticalSlot = Parent->AddChildToVerticalBox(Child))
			{
				VerticalSlot->SetPadding(Padding);
				VerticalSlot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
	}
}

TSharedRef<SWidget> UJTSSettingsWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UJTSSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshDevices();
}

void UJTSSettingsWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	UBorder* const Panel = JTSUITheme::MakePanel(WidgetTree, TEXT("SettingsPanel"), JTSUITheme::Panel, FMargin(28.0f));
	WidgetTree->RootWidget = Panel;
	UVerticalBox* const Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsLayout"));
	Panel->SetContent(Layout);
	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("Title"), TEXT("SETTINGS"), 30.0f, JTSUITheme::Ink));
	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("Subtitle"), TEXT("VOICE & AUDIO"), 14.0f, JTSUITheme::Cyan), FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("MicrophoneLabel"), TEXT("MICROPHONE"), 14.0f, JTSUITheme::Muted));
	UButton* const MuteButton = JTSUITheme::MakeButton(WidgetTree, TEXT("MuteButton"), TEXT("MICROPHONE"), JTSUITheme::EButtonTone::Secondary, 17.0f);
	MuteButtonText = Cast<UTextBlock>(MuteButton->GetContent());
	MuteButton->OnClicked.AddDynamic(this, &UJTSSettingsWidget::ToggleMute);
	AddRow(Layout, MuteButton);

	AddRow(Layout, JTSUITheme::MakeText(WidgetTree, TEXT("DevicesLabel"), TEXT("DEVICE CHECK"), 14.0f, JTSUITheme::Muted), FMargin(0.0f, 14.0f, 0.0f, 0.0f));
	UButton* const RefreshButton = JTSUITheme::MakeButton(WidgetTree, TEXT("RefreshDevicesButton"), TEXT("REFRESH AUDIO DEVICES"), JTSUITheme::EButtonTone::Secondary, 15.0f);
	RefreshButton->OnClicked.AddDynamic(this, &UJTSSettingsWidget::RefreshDevices);
	AddRow(Layout, RefreshButton);
	UButton* const NextInputButton = JTSUITheme::MakeButton(WidgetTree, TEXT("NextInputButton"), TEXT("SELECT NEXT MICROPHONE"), JTSUITheme::EButtonTone::Secondary, 15.0f);
	NextInputButton->OnClicked.AddDynamic(this, &UJTSSettingsWidget::SelectNextInputDevice);
	AddRow(Layout, NextInputButton);
	UButton* const NextOutputButton = JTSUITheme::MakeButton(WidgetTree, TEXT("NextOutputButton"), TEXT("SELECT NEXT SPEAKER"), JTSUITheme::EButtonTone::Secondary, 15.0f);
	NextOutputButton->OnClicked.AddDynamic(this, &UJTSSettingsWidget::SelectNextOutputDevice);
	AddRow(Layout, NextOutputButton);

	DeviceText = JTSUITheme::MakeText(WidgetTree, TEXT("DeviceStatus"), TEXT("Checking local voice provider..."), 13.0f, JTSUITheme::Muted);
	AddRow(Layout, DeviceText, FMargin(0.0f, 10.0f));
	UButton* const BackButton = JTSUITheme::MakeButton(WidgetTree, TEXT("SettingsBackButton"), TEXT("BACK"), JTSUITheme::EButtonTone::Secondary);
	BackButton->OnClicked.AddDynamic(this, &UJTSSettingsWidget::ReturnToMain);
	AddRow(Layout, BackButton, FMargin(0.0f, 16.0f, 0.0f, 0.0f));
}

void UJTSSettingsWidget::ToggleMute()
{
	if (UJTSVoiceSubsystem* const Voice = GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>())
	{
		Voice->SetInputMuted(!Voice->IsInputMuted());
		bMuted = Voice->IsInputMuted();
	}
	RefreshDevices();
}

void UJTSSettingsWidget::RefreshDevices()
{
	UJTSVoiceSubsystem* const Voice = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>() : nullptr;
	if (Voice != nullptr)
	{
		Voice->InitializeVoice();
		bMuted = Voice->IsInputMuted();
	}
	const bool bVoiceAvailable = Voice != nullptr && Voice->IsVoiceAvailable();

	if (MuteButtonText != nullptr)
	{
		MuteButtonText->SetText(FText::FromString(!bVoiceAvailable
			? TEXT("MICROPHONE DISABLED")
			: (bMuted ? TEXT("MICROPHONE MUTED") : TEXT("MICROPHONE ON"))));
	}
	if (DeviceText == nullptr)
	{
		return;
	}
	if (!bVoiceAvailable)
	{
		DeviceText->SetText(FText::FromString(TEXT("Voice chat unavailable with the current local/LAN provider.\nLobby readiness and expedition gameplay are unaffected.")));
		return;
	}

	DeviceText->SetText(FText::FromString(FString::Printf(
		TEXT("INPUT: %s\nOUTPUT: %s"),
		*FString::Join(Voice->GetInputDeviceNames(), TEXT(", ")),
		*FString::Join(Voice->GetOutputDeviceNames(), TEXT(", ")))));
}

void UJTSSettingsWidget::SelectNextInputDevice()
{
	if (UJTSVoiceSubsystem* const Voice = GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>())
	{
		const TArray<FString> Devices = Voice->GetInputDeviceNames();
		if (!Devices.IsEmpty())
		{
			InputDeviceIndex = (InputDeviceIndex + 1) % Devices.Num();
			Voice->SelectInputDevice(Devices[InputDeviceIndex]);
		}
	}
	RefreshDevices();
}

void UJTSSettingsWidget::SelectNextOutputDevice()
{
	if (UJTSVoiceSubsystem* const Voice = GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>())
	{
		const TArray<FString> Devices = Voice->GetOutputDeviceNames();
		if (!Devices.IsEmpty())
		{
			OutputDeviceIndex = (OutputDeviceIndex + 1) % Devices.Num();
			Voice->SelectOutputDevice(Devices[OutputDeviceIndex]);
		}
	}
	RefreshDevices();
}

void UJTSSettingsWidget::ReturnToMain()
{
	if (UJTSFrontEndRootWidget* const Root = GetTypedOuter<UJTSFrontEndRootWidget>())
	{
		Root->ShowMainMenu();
	}
}
