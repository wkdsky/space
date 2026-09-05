// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSPrototypeHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "space/Components/JTSCarryComponent.h"
#include "space/Core/JTSGameInstance.h"
#include "space/Core/JTSGameMode.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/UI/JTSCircularProgressWidget.h"

namespace
{
	UCanvasPanelSlot* AddCanvasChild(
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
			Slot->SetAlignment(Alignment);
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
		}

		return Slot;
	}

	UTextBlock* MakeTextBlock(
		UWidgetTree* WidgetTree,
		const FName& Name,
		const FString& Text,
		float FontSize,
		const FLinearColor& Color,
		ETextJustify::Type Justification = ETextJustify::Left)
	{
		if (WidgetTree == nullptr)
		{
			return nullptr;
		}

		UTextBlock* const TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		if (TextBlock == nullptr)
		{
			return nullptr;
		}

		TextBlock->SetText(FText::FromString(Text));
		TextBlock->SetFont(FCoreStyle::GetDefaultFontStyle(FName(TEXT("Bold")), FontSize));
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
		TextBlock->SetShadowOffset(FVector2D(2.0f, 2.0f));
		TextBlock->SetJustification(Justification);
		TextBlock->SetAutoWrapText(true);
		return TextBlock;
	}

	UBorder* MakeBorder(
		UWidgetTree* WidgetTree,
		const FName& Name,
		const FLinearColor& Color,
		float Padding = 0.0f)
	{
		if (WidgetTree == nullptr)
		{
			return nullptr;
		}

		UBorder* const Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		if (Border != nullptr)
		{
			Border->SetBrushColor(Color);
			Border->SetPadding(FMargin(Padding));
			Border->SetHorizontalAlignment(HAlign_Fill);
			Border->SetVerticalAlignment(VAlign_Fill);
		}
		return Border;
	}

	void AddVerticalChild(
		UVerticalBox* Parent,
		UWidget* Child,
		const FMargin& Padding,
		EHorizontalAlignment HorizontalAlignment = HAlign_Fill)
	{
		if (Parent == nullptr || Child == nullptr)
		{
			return;
		}

		if (UVerticalBoxSlot* const Slot = Parent->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(Padding);
			Slot->SetHorizontalAlignment(HorizontalAlignment);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
	}

	UButton* MakeButton(UWidgetTree* WidgetTree, const FName& Name, const FString& Label)
	{
		if (WidgetTree == nullptr)
		{
			return nullptr;
		}

		UButton* const Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		if (Button == nullptr)
		{
			return nullptr;
		}

		Button->SetBackgroundColor(FLinearColor(0.08f, 0.35f, 0.58f, 1.0f));
		Button->SetColorAndOpacity(FLinearColor::White);
		UTextBlock* const ButtonLabel = MakeTextBlock(
			WidgetTree,
			*FString::Printf(TEXT("%s_Label"), *Name.ToString()),
			Label,
			24.0f,
			FLinearColor::White,
			ETextJustify::Center);
		if (ButtonLabel != nullptr)
		{
			Button->SetContent(ButtonLabel);
		}
		return Button;
	}

	void ConfigureTransparentProgressBar(UProgressBar* ProgressBar)
	{
		if (ProgressBar == nullptr)
		{
			return;
		}

		FProgressBarStyle Style = FProgressBarStyle::GetDefault();
		Style.BackgroundImage.TintColor = FSlateColor(FLinearColor::Transparent);
		Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
		Style.MarqueeImage.TintColor = FSlateColor(FLinearColor::Transparent);
		Style.EnableFillAnimation = false;
		ProgressBar->SetWidgetStyle(Style);
		ProgressBar->SetBarFillType(EProgressBarFillType::BottomToTop);
		ProgressBar->SetBarFillStyle(EProgressBarFillStyle::Scale);
	}
}

TSharedRef<SWidget> UJTSPrototypeHUDWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSPrototypeHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildWidgetTree();
	BindGameState();
	RefreshAvatarSelection();

	if (BoundGameState.IsValid())
	{
		RefreshPhaseView(BoundGameState->GetGameplayPhase());
	}
	else
	{
		RefreshPhaseView(EJTSGameplayPhase::WaitingToStart);
	}
}

void UJTSPrototypeHUDWidget::NativeDestruct()
{
	if (AJTSGameState* const GameState = BoundGameState.Get())
	{
		GameState->OnGameplayPhaseChanged.RemoveDynamic(this, &UJTSPrototypeHUDWidget::HandleGameplayPhaseChanged);
	}
	BoundGameState.Reset();

	Super::NativeDestruct();
}

void UJTSPrototypeHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!BoundGameState.IsValid())
	{
		BindGameState();
	}

	if (BoundGameState.IsValid() && BoundGameState->IsEarthCollectionActive())
	{
		RefreshGameplayHud();
		RefreshBoardingProgress();

		const float RemainingTime = BoundGameState->GetEarthCollectionRemainingTime();
		if (TimeText != nullptr)
		{
			const float PulseAlpha = RemainingTime <= 5.0f
				? (0.5f + 0.5f * FMath::Sin(static_cast<float>(GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0) * 8.0f))
				: 0.0f;
			const float PulseScale = RemainingTime <= 5.0f ? FMath::Lerp(1.0f, 1.25f, PulseAlpha) : 1.0f;
			TimeText->SetRenderScale(FVector2D(PulseScale, PulseScale));
		}
	}
	else
	{
		SetBoardingProgressVisible(false);
		if (TimeText != nullptr)
		{
			TimeText->SetRenderScale(FVector2D(1.0f, 1.0f));
		}
	}
}

void UJTSPrototypeHUDWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || RootCanvas != nullptr)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	if (RootCanvas == nullptr)
	{
		return;
	}
	WidgetTree->RootWidget = RootCanvas;

	StartMenuLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("StartMenuLayer"));
	SettingsLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SettingsLayer"));
	GameplayLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GameplayLayer"));
	LaunchingLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("LaunchingLayer"));
	ResultLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ResultLayer"));

	AddCanvasChild(RootCanvas, StartMenuLayer, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);
	AddCanvasChild(RootCanvas, SettingsLayer, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);
	AddCanvasChild(RootCanvas, GameplayLayer, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);
	AddCanvasChild(RootCanvas, LaunchingLayer, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);
	AddCanvasChild(RootCanvas, ResultLayer, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);

	if (StartMenuLayer != nullptr)
	{
		UBorder* const Background = MakeBorder(
			WidgetTree,
			TEXT("StartMenuBackground"),
			FLinearColor(0.008f, 0.025f, 0.055f, 0.97f));
		AddCanvasChild(StartMenuLayer, Background, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);

		UBorder* const Card = MakeBorder(
			WidgetTree,
			TEXT("StartMenuCard"),
			FLinearColor(0.025f, 0.10f, 0.17f, 0.96f),
			30.0f);
		AddCanvasChild(StartMenuLayer, Card, FAnchors(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(760.0f, 650.0f), FVector2D(0.5f, 0.5f));

		UVerticalBox* const MenuBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StartMenuBox"));
		if (Card != nullptr && MenuBox != nullptr)
		{
			Card->SetContent(MenuBox);
			AddVerticalChild(MenuBox, MakeTextBlock(WidgetTree, TEXT("StartTitle"), TEXT("JUMP TO SPACE"), 46.0f, FLinearColor(0.65f, 0.90f, 1.0f, 1.0f), ETextJustify::Center), FMargin(0.0f, 12.0f, 0.0f, 8.0f));
			AddVerticalChild(MenuBox, MakeTextBlock(WidgetTree, TEXT("StartLocation"), TEXT("EARTH BASE"), 30.0f, FLinearColor(0.35f, 0.70f, 1.0f, 1.0f), ETextJustify::Center), FMargin(0.0f, 4.0f));
			AddVerticalChild(MenuBox, MakeTextBlock(WidgetTree, TEXT("StartTagline"), TEXT("PACK THE SHIP BEFORE EARTH CATCHES YOU"), 19.0f, FLinearColor::White, ETextJustify::Center), FMargin(0.0f, 16.0f, 0.0f, 20.0f));
			AddVerticalChild(MenuBox, MakeTextBlock(WidgetTree, TEXT("StartRules"), TEXT("60 SECONDS\nTWO HANDS\nONE TERRIBLE PLAN"), 25.0f, FLinearColor(1.0f, 0.78f, 0.28f, 1.0f), ETextJustify::Center), FMargin(0.0f, 6.0f, 0.0f, 28.0f));

			UButton* const StartButton = MakeButton(WidgetTree, TEXT("StartMissionButton"), TEXT("START MISSION"));
			if (StartButton != nullptr)
			{
				StartButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleStartMissionClicked);
			}
			AddVerticalChild(MenuBox, StartButton, FMargin(80.0f, 8.0f), HAlign_Fill);

			SettingsButton = MakeButton(WidgetTree, TEXT("SettingsButton"), TEXT("SETTINGS"));
			if (SettingsButton != nullptr)
			{
				SettingsButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleSettingsClicked);
			}
			AddVerticalChild(MenuBox, SettingsButton, FMargin(80.0f, 8.0f), HAlign_Fill);
		}
	}

	if (SettingsLayer != nullptr)
	{
		UBorder* const Background = MakeBorder(
			WidgetTree,
			TEXT("SettingsBackground"),
			FLinearColor(0.008f, 0.025f, 0.055f, 0.98f));
		AddCanvasChild(SettingsLayer, Background, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);

		UBorder* const Card = MakeBorder(
			WidgetTree,
			TEXT("SettingsCard"),
			FLinearColor(0.025f, 0.10f, 0.17f, 0.98f),
			28.0f);
		AddCanvasChild(SettingsLayer, Card, FAnchors(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(760.0f, 560.0f), FVector2D(0.5f, 0.5f));

		UTextBlock* const SettingsTitle = MakeTextBlock(WidgetTree, TEXT("SettingsTitle"), TEXT("SETTINGS"), 42.0f, FLinearColor(0.65f, 0.90f, 1.0f, 1.0f), ETextJustify::Center);
		AddCanvasChild(SettingsLayer, SettingsTitle, FAnchors(0.5f, 0.5f), FVector2D(0.0f, -220.0f), FVector2D(500.0f, 55.0f), FVector2D(0.5f, 0.5f));
		UTextBlock* const SettingsPrompt = MakeTextBlock(WidgetTree, TEXT("SettingsPrompt"), TEXT("CHOOSE YOUR AVATAR COLOR"), 20.0f, FLinearColor::White, ETextJustify::Center);
		AddCanvasChild(SettingsLayer, SettingsPrompt, FAnchors(0.5f, 0.5f), FVector2D(0.0f, -165.0f), FVector2D(500.0f, 35.0f), FVector2D(0.5f, 0.5f));

		SettingsPreviewBlock = MakeBorder(WidgetTree, TEXT("SettingsPreviewBlock"), FLinearColor(0.10f, 0.45f, 1.0f, 1.0f), 4.0f);
		AddCanvasChild(SettingsLayer, SettingsPreviewBlock, FAnchors(0.5f, 0.5f), FVector2D(0.0f, -95.0f), FVector2D(82.0f, 58.0f), FVector2D(0.5f, 0.5f));
		UTextBlock* const PreviewLabel = MakeTextBlock(WidgetTree, TEXT("SettingsPreviewLabel"), TEXT("CURRENT"), 14.0f, FLinearColor(0.70f, 0.82f, 0.92f, 1.0f), ETextJustify::Center);
		AddCanvasChild(SettingsLayer, PreviewLabel, FAnchors(0.5f, 0.5f), FVector2D(0.0f, -55.0f), FVector2D(160.0f, 25.0f), FVector2D(0.5f, 0.5f));

		const TArray<FLinearColor> AvatarColors = {
			FLinearColor(0.10f, 0.45f, 1.0f, 1.0f),
			FLinearColor(1.0f, 0.34f, 0.06f, 1.0f),
			FLinearColor(0.18f, 0.85f, 0.28f, 1.0f)};
		const TArray<FString> AvatarLabels = {TEXT("BLUE"), TEXT("ORANGE"), TEXT("GREEN")};
		const TArray<FName> AvatarNames = {TEXT("BlueAvatarButton"), TEXT("OrangeAvatarButton"), TEXT("GreenAvatarButton")};
		TArray<UButton*> AvatarButtons;
		AvatarButtons.Reserve(3);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			UButton* const AvatarButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), AvatarNames[Index]);
			if (AvatarButton == nullptr)
			{
				continue;
			}
			AvatarButton->SetBackgroundColor(FLinearColor(0.08f, 0.12f, 0.18f, 1.0f));
			UBorder* const Swatch = MakeBorder(WidgetTree, *FString::Printf(TEXT("%s_Swatch"), *AvatarNames[Index].ToString()), AvatarColors[Index], 8.0f);
			UVerticalBox* const AvatarBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("%s_Box"), *AvatarNames[Index].ToString()));
			if (AvatarBox != nullptr)
			{
				USizeBox* const SwatchSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("%s_Size"), *AvatarNames[Index].ToString()));
				if (SwatchSize != nullptr)
				{
					SwatchSize->SetWidthOverride(110.0f);
					SwatchSize->SetHeightOverride(85.0f);
					SwatchSize->SetContent(Swatch);
				}
				AddVerticalChild(AvatarBox, SwatchSize, FMargin(10.0f, 8.0f, 10.0f, 4.0f), HAlign_Center);
				AddVerticalChild(AvatarBox, MakeTextBlock(WidgetTree, *FString::Printf(TEXT("%s_Label"), *AvatarNames[Index].ToString()), AvatarLabels[Index], 17.0f, FLinearColor::White, ETextJustify::Center), FMargin(0.0f, 3.0f, 0.0f, 8.0f), HAlign_Center);
				AvatarButton->SetContent(AvatarBox);
			}
			AddCanvasChild(SettingsLayer, AvatarButton, FAnchors(0.5f, 0.5f), FVector2D(-220.0f + 220.0f * Index, 65.0f), FVector2D(170.0f, 150.0f), FVector2D(0.5f, 0.5f));
			AvatarButtons.Add(AvatarButton);
		}
		if (AvatarButtons.Num() == 3)
		{
			BlueAvatarButton = AvatarButtons[0];
			OrangeAvatarButton = AvatarButtons[1];
			GreenAvatarButton = AvatarButtons[2];
			BlueAvatarButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleBlueAvatarClicked);
			OrangeAvatarButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleOrangeAvatarClicked);
			GreenAvatarButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleGreenAvatarClicked);
		}

		UButton* const BackButton = MakeButton(WidgetTree, TEXT("SettingsBackButton"), TEXT("BACK"));
		if (BackButton != nullptr)
		{
			BackButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleBackSettingsClicked);
		}
		AddCanvasChild(SettingsLayer, BackButton, FAnchors(0.5f, 0.5f), FVector2D(0.0f, 210.0f), FVector2D(260.0f, 54.0f), FVector2D(0.5f, 0.5f));
	}

	if (GameplayLayer != nullptr)
	{
		TimeText = MakeTextBlock(WidgetTree, TEXT("TimeText"), TEXT("TIME: 60.00"), 34.0f, FLinearColor::White, ETextJustify::Center);
		if (TimeText != nullptr)
		{
			TimeText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		}
		AddCanvasChild(GameplayLayer, TimeText, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 28.0f), FVector2D(280.0f, 58.0f), FVector2D(0.5f, 0.0f));

		UCanvasPanel* const AvatarCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AvatarCanvas"));
		AddCanvasChild(GameplayLayer, AvatarCanvas, FAnchors(0.0f, 0.0f), FVector2D(28.0f, 28.0f), FVector2D(130.0f, 130.0f));
		AvatarBlock = MakeBorder(WidgetTree, TEXT("AvatarBlock"), FLinearColor(0.10f, 0.45f, 1.0f, 1.0f), 5.0f);
		AddCanvasChild(AvatarCanvas, AvatarBlock, FAnchors(0.0f, 0.0f), FVector2D::ZeroVector, FVector2D(106.0f, 106.0f));
		UTextBlock* const AvatarLabel = MakeTextBlock(WidgetTree, TEXT("AvatarLabel"), TEXT("YOU"), 20.0f, FLinearColor::White, ETextJustify::Center);
		AddCanvasChild(AvatarCanvas, AvatarLabel, FAnchors(0.0f, 0.0f), FVector2D(3.0f, 39.0f), FVector2D(100.0f, 30.0f));

		RocketIconCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RocketIconCanvas"));
		AddCanvasChild(AvatarCanvas, RocketIconCanvas, FAnchors(1.0f, 1.0f), FVector2D(-42.0f, -42.0f), FVector2D(48.0f, 48.0f), FVector2D(1.0f, 1.0f));
		RocketBody = MakeBorder(WidgetTree, TEXT("RocketBody"), FLinearColor(0.85f, 0.94f, 1.0f, 1.0f), 2.0f);
		AddCanvasChild(RocketIconCanvas, RocketBody, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 5.0f), FVector2D(17.0f, 29.0f), FVector2D(0.5f, 0.0f));
		RocketFlame = MakeBorder(WidgetTree, TEXT("RocketFlame"), FLinearColor(1.0f, 0.42f, 0.05f, 1.0f), 1.0f);
		AddCanvasChild(RocketIconCanvas, RocketFlame, FAnchors(0.5f, 1.0f), FVector2D(0.0f, -4.0f), FVector2D(9.0f, 13.0f), FVector2D(0.5f, 1.0f));

		UBorder* const LeftPanel = MakeBorder(WidgetTree, TEXT("EarthPanel"), FLinearColor(0.02f, 0.06f, 0.10f, 0.90f), 18.0f);
		AddCanvasChild(GameplayLayer, LeftPanel, FAnchors(0.0f, 0.0f), FVector2D(28.0f, 178.0f), FVector2D(390.0f, 220.0f));
		UVerticalBox* const LeftBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EarthPanelBox"));
		if (LeftPanel != nullptr && LeftBox != nullptr)
		{
			LeftPanel->SetContent(LeftBox);
			AddVerticalChild(LeftBox, MakeTextBlock(WidgetTree, TEXT("EarthHeading"), TEXT("EARTH BASE"), 27.0f, FLinearColor(0.75f, 0.95f, 1.0f, 1.0f)), FMargin(0.0f, 0.0f, 0.0f, 2.0f));
			AddVerticalChild(LeftBox, MakeTextBlock(WidgetTree, TEXT("EarthPhase"), TEXT("COLLECTION PHASE"), 17.0f, FLinearColor(0.45f, 0.85f, 1.0f, 1.0f)), FMargin(0.0f, 0.0f, 0.0f, 8.0f));
			CarryText = MakeTextBlock(WidgetTree, TEXT("CarryText"), TEXT("CARRY: 0 / 2"), 20.0f, FLinearColor::White);
			HoldingText = MakeTextBlock(WidgetTree, TEXT("HoldingText"), TEXT("HOLDING: Empty"), 20.0f, FLinearColor::White);
			AddVerticalChild(LeftBox, CarryText, FMargin(0.0f, 2.0f));
			AddVerticalChild(LeftBox, HoldingText, FMargin(0.0f, 2.0f));
		}

		UBorder* const FuelPanel = MakeBorder(WidgetTree, TEXT("FuelPanel"), FLinearColor(0.02f, 0.03f, 0.07f, 0.82f), 14.0f);
		AddCanvasChild(GameplayLayer, FuelPanel, FAnchors(1.0f, 0.5f), FVector2D(-28.0f, 0.0f), FVector2D(250.0f, 360.0f), FVector2D(1.0f, 0.5f));
		UCanvasPanel* const FuelCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FuelCanvas"));
		if (FuelPanel != nullptr && FuelCanvas != nullptr)
		{
			FuelPanel->SetContent(FuelCanvas);
			AddCanvasChild(FuelCanvas, MakeTextBlock(WidgetTree, TEXT("ShipHeading"), TEXT("SPACESHIP"), 25.0f, FLinearColor(0.95f, 0.85f, 1.0f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(10.0f, 4.0f), FVector2D(220.0f, 34.0f));
			FuelProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("FuelProgressBar"));
			ConfigureTransparentProgressBar(FuelProgressBar);
			AddCanvasChild(FuelCanvas, FuelProgressBar, FAnchors(0.0f, 0.0f), FVector2D(18.0f, 52.0f), FVector2D(58.0f, 250.0f));
			FuelText = MakeTextBlock(WidgetTree, TEXT("FuelText"), TEXT("FUEL: 0"), 21.0f, FLinearColor(1.0f, 0.55f, 0.25f, 1.0f));
			WaterText = MakeTextBlock(WidgetTree, TEXT("WaterText"), TEXT("WATER: 0"), 17.0f, FLinearColor(0.35f, 0.65f, 1.0f, 1.0f));
			FoodText = MakeTextBlock(WidgetTree, TEXT("FoodText"), TEXT("FOOD: 0"), 17.0f, FLinearColor(0.40f, 0.90f, 0.30f, 1.0f));
			MinimumFuelText = MakeTextBlock(WidgetTree, TEXT("MinimumFuelText"), TEXT("TARGET: 3"), 17.0f, FLinearColor(1.0f, 0.45f, 0.45f, 1.0f));
			AddCanvasChild(FuelCanvas, FuelText, FAnchors(0.0f, 0.0f), FVector2D(95.0f, 75.0f), FVector2D(140.0f, 34.0f));
			AddCanvasChild(FuelCanvas, MinimumFuelText, FAnchors(0.0f, 0.0f), FVector2D(95.0f, 115.0f), FVector2D(140.0f, 30.0f));
			AddCanvasChild(FuelCanvas, WaterText, FAnchors(0.0f, 0.0f), FVector2D(95.0f, 165.0f), FVector2D(140.0f, 28.0f));
			AddCanvasChild(FuelCanvas, FoodText, FAnchors(0.0f, 0.0f), FVector2D(95.0f, 202.0f), FVector2D(140.0f, 28.0f));
		}

		BoardingProgressWidget = WidgetTree->ConstructWidget<UJTSCircularProgressWidget>(UJTSCircularProgressWidget::StaticClass(), TEXT("BoardingProgressWidget"));
		if (BoardingProgressWidget != nullptr)
		{
			BoardingProgressWidget->SetProgressColor(FLinearColor(0.20f, 0.80f, 1.0f, 1.0f));
			BoardingProgressWidget->SetBackgroundColor(FLinearColor(0.10f, 0.18f, 0.25f, 0.90f));
		}
		AddCanvasChild(GameplayLayer, BoardingProgressWidget, FAnchors(0.5f, 0.5f), FVector2D(0.0f, -40.0f), FVector2D(220.0f, 220.0f), FVector2D(0.5f, 0.5f));
		BoardingRemainingText = MakeTextBlock(WidgetTree, TEXT("BoardingRemainingText"), TEXT("2.0"), 34.0f, FLinearColor::White, ETextJustify::Center);
		AddCanvasChild(GameplayLayer, BoardingRemainingText, FAnchors(0.5f, 0.5f), FVector2D(0.0f, -42.0f), FVector2D(160.0f, 50.0f), FVector2D(0.5f, 0.5f));
		BoardingLabelText = MakeTextBlock(WidgetTree, TEXT("BoardingLabelText"), TEXT("BOARDING"), 18.0f, FLinearColor(0.70f, 0.90f, 1.0f, 1.0f), ETextJustify::Center);
		AddCanvasChild(GameplayLayer, BoardingLabelText, FAnchors(0.5f, 0.5f), FVector2D(0.0f, 40.0f), FVector2D(180.0f, 30.0f), FVector2D(0.5f, 0.5f));

		UTextBlock* const HelpText = MakeTextBlock(WidgetTree, TEXT("HelpText"), TEXT("E: BOARD / EXIT / INTERACT    WASD: MOVE    SHIFT: RUN"), 18.0f, FLinearColor(0.85f, 0.95f, 1.0f, 1.0f), ETextJustify::Center);
		AddCanvasChild(GameplayLayer, HelpText, FAnchors(0.5f, 1.0f), FVector2D(0.0f, -32.0f), FVector2D(760.0f, 32.0f), FVector2D(0.5f, 1.0f));
	}

	if (LaunchingLayer != nullptr)
	{
		UBorder* const LaunchBackground = MakeBorder(WidgetTree, TEXT("LaunchingBackground"), FLinearColor(0.015f, 0.035f, 0.12f, 0.95f));
		AddCanvasChild(LaunchingLayer, LaunchBackground, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);
		UBorder* const LaunchCard = MakeBorder(WidgetTree, TEXT("LaunchingCard"), FLinearColor(0.06f, 0.10f, 0.22f, 0.96f), 28.0f);
		AddCanvasChild(LaunchingLayer, LaunchCard, FAnchors(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(700.0f, 250.0f), FVector2D(0.5f, 0.5f));
		UVerticalBox* const LaunchBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LaunchingBox"));
		if (LaunchCard != nullptr && LaunchBox != nullptr)
		{
			LaunchCard->SetContent(LaunchBox);
			AddVerticalChild(LaunchBox, MakeTextBlock(WidgetTree, TEXT("LaunchingTitle"), TEXT("LAUNCHING..."), 42.0f, FLinearColor(1.0f, 0.86f, 0.24f, 1.0f), ETextJustify::Center), FMargin(0.0f, 24.0f, 0.0f, 18.0f));
			AddVerticalChild(LaunchBox, MakeTextBlock(WidgetTree, TEXT("LaunchingSubtitle"), TEXT("HOLD ON TO YOUR SPACE HELMET"), 22.0f, FLinearColor::White, ETextJustify::Center), FMargin(0.0f, 8.0f));
		}
	}

	if (ResultLayer != nullptr)
	{
		ResultBackground = MakeBorder(WidgetTree, TEXT("ResultBackground"), FLinearColor(0.28f, 0.015f, 0.02f, 0.97f));
		AddCanvasChild(ResultLayer, ResultBackground, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);
		UBorder* const ResultCard = MakeBorder(WidgetTree, TEXT("ResultCard"), FLinearColor(0.04f, 0.04f, 0.08f, 0.94f), 32.0f);
		AddCanvasChild(ResultLayer, ResultCard, FAnchors(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(780.0f, 570.0f), FVector2D(0.5f, 0.5f));
		UVerticalBox* const ResultBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ResultBox"));
		if (ResultCard != nullptr && ResultBox != nullptr)
		{
			ResultCard->SetContent(ResultBox);
			ResultTitleText = MakeTextBlock(WidgetTree, TEXT("ResultTitle"), TEXT("OH NO!"), 48.0f, FLinearColor::White, ETextJustify::Center);
			ResultSubtitleText = MakeTextBlock(WidgetTree, TEXT("ResultSubtitle"), TEXT("EARTH CAUGHT THE SHIP!"), 27.0f, FLinearColor::White, ETextJustify::Center);
			ResultDetailText = MakeTextBlock(WidgetTree, TEXT("ResultDetail"), TEXT("NOT ENOUGH FUEL TO REACH THE MOON"), 20.0f, FLinearColor::White, ETextJustify::Center);
			AddVerticalChild(ResultBox, ResultTitleText, FMargin(0.0f, 12.0f, 0.0f, 8.0f));
			AddVerticalChild(ResultBox, ResultSubtitleText, FMargin(0.0f, 8.0f));
			AddVerticalChild(ResultBox, ResultDetailText, FMargin(0.0f, 8.0f, 0.0f, 18.0f));
			AddVerticalChild(ResultBox, MakeTextBlock(WidgetTree, TEXT("ResultArt"), TEXT("[  SHIP  ]\n      v\n   ( MOON )"), 28.0f, FLinearColor(0.75f, 0.85f, 1.0f, 1.0f), ETextJustify::Center), FMargin(0.0f, 4.0f, 0.0f, 18.0f));

			UButton* const RestartButton = MakeButton(WidgetTree, TEXT("RestartButton"), TEXT("RESTART"));
			if (RestartButton != nullptr)
			{
				RestartButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleRestartClicked);
			}
			AddVerticalChild(ResultBox, RestartButton, FMargin(80.0f, 5.0f), HAlign_Fill);

			UButton* const QuitButton = MakeButton(WidgetTree, TEXT("QuitButton"), TEXT("QUIT"));
			if (QuitButton != nullptr)
			{
				QuitButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleQuitClicked);
			}
			AddVerticalChild(ResultBox, QuitButton, FMargin(80.0f, 5.0f), HAlign_Fill);
		}
	}

	ApplyLayerVisibility(StartMenuLayer, false);
	ApplyLayerVisibility(SettingsLayer, false);
	ApplyLayerVisibility(GameplayLayer, false);
	ApplyLayerVisibility(LaunchingLayer, false);
	ApplyLayerVisibility(ResultLayer, false);
	SetBoardingProgressVisible(false);
}

void UJTSPrototypeHUDWidget::BindGameState()
{
	AJTSGameState* const NewGameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
	if (BoundGameState.Get() == NewGameState)
	{
		return;
	}

	if (AJTSGameState* const PreviousGameState = BoundGameState.Get())
	{
		PreviousGameState->OnGameplayPhaseChanged.RemoveDynamic(this, &UJTSPrototypeHUDWidget::HandleGameplayPhaseChanged);
	}

	BoundGameState = NewGameState;
	if (NewGameState != nullptr)
	{
		NewGameState->OnGameplayPhaseChanged.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleGameplayPhaseChanged);
		RefreshPhaseView(NewGameState->GetGameplayPhase());
	}
}

void UJTSPrototypeHUDWidget::RefreshPhaseView(EJTSGameplayPhase NewGameplayPhase)
{
	CachedGameplayPhase = NewGameplayPhase;
	if (NewGameplayPhase != EJTSGameplayPhase::WaitingToStart)
	{
		bSettingsVisible = false;
	}

	ApplyLayerVisibility(StartMenuLayer, NewGameplayPhase == EJTSGameplayPhase::WaitingToStart && !bSettingsVisible);
	ApplyLayerVisibility(SettingsLayer, NewGameplayPhase == EJTSGameplayPhase::WaitingToStart && bSettingsVisible);
	ApplyLayerVisibility(GameplayLayer, NewGameplayPhase == EJTSGameplayPhase::EarthCollection);
	ApplyLayerVisibility(LaunchingLayer, NewGameplayPhase == EJTSGameplayPhase::Launching);
	ApplyLayerVisibility(ResultLayer, NewGameplayPhase == EJTSGameplayPhase::EarthCaptureFailure || NewGameplayPhase == EJTSGameplayPhase::MoonArrivalSuccess);

	if (NewGameplayPhase == EJTSGameplayPhase::EarthCaptureFailure || NewGameplayPhase == EJTSGameplayPhase::MoonArrivalSuccess)
	{
		RefreshResultView(NewGameplayPhase);
	}
	else if (NewGameplayPhase == EJTSGameplayPhase::EarthCollection)
	{
		RefreshGameplayHud();
	}
	else
	{
		SetBoardingProgressVisible(false);
		if (TimeText != nullptr)
		{
			TimeText->SetRenderScale(FVector2D(1.0f, 1.0f));
		}
	}
}

void UJTSPrototypeHUDWidget::RefreshGameplayHud()
{
	if (!BoundGameState.IsValid())
	{
		return;
	}

	const float RemainingTime = BoundGameState->GetEarthCollectionRemainingTime();
	if (TimeText != nullptr)
	{
		TimeText->SetText(FText::FromString(FString::Printf(TEXT("TIME: %s"), *FormatRemainingTime(RemainingTime))));
		TimeText->SetColorAndOpacity(FSlateColor(RemainingTime <= 5.0f ? FLinearColor(1.0f, 0.25f, 0.18f, 1.0f) : FLinearColor::White));
		TimeText->SetFont(FCoreStyle::GetDefaultFontStyle(FName(TEXT("Bold")), 34.0f));
	}

	AJTSCharacter* const PlayerCharacter = FindPlayerCharacter();
	const UJTSCarryComponent* const CarryComponent = PlayerCharacter != nullptr ? PlayerCharacter->GetCarryComponent() : nullptr;
	const int32 CarriedItemCount = CarryComponent != nullptr ? CarryComponent->GetCarriedItemCount() : 0;
	const int32 CarryCapacity = CarryComponent != nullptr ? CarryComponent->GetCarryCapacity() : 0;
	if (CarryText != nullptr)
	{
		CarryText->SetText(FText::FromString(FString::Printf(TEXT("CARRY: %d / %d"), CarriedItemCount, CarryCapacity)));
	}
	if (HoldingText != nullptr)
	{
		FString HoldingString = TEXT("HOLDING: ");
		if (CarryComponent == nullptr || CarryComponent->GetCarriedResources().IsEmpty())
		{
			HoldingString += TEXT("Empty");
		}
		else
		{
			const TArray<EJTSResourceType>& CarriedResources = CarryComponent->GetCarriedResources();
			for (int32 Index = 0; Index < CarriedResources.Num(); ++Index)
			{
				if (Index > 0)
				{
					HoldingString += TEXT(", ");
				}
				HoldingString += ResourceTypeToString(CarriedResources[Index]);
			}
		}
		HoldingText->SetText(FText::FromString(HoldingString));
	}

	AJTSSpacecraftActor* const Spacecraft = FindSpacecraft();
	const int32 FuelCount = Spacecraft != nullptr ? Spacecraft->GetFuelCount() : 0;
	const int32 WaterCount = Spacecraft != nullptr ? Spacecraft->GetWaterCount() : 0;
	const int32 FoodCount = Spacecraft != nullptr ? Spacecraft->GetFoodCount() : 0;
	if (FuelText != nullptr)
	{
		FuelText->SetText(FText::FromString(FString::Printf(TEXT("FUEL: %d"), FuelCount)));
	}
	if (WaterText != nullptr)
	{
		WaterText->SetText(FText::FromString(FString::Printf(TEXT("WATER: %d"), WaterCount)));
	}
	if (FoodText != nullptr)
	{
		FoodText->SetText(FText::FromString(FString::Printf(TEXT("FOOD: %d"), FoodCount)));
	}

	const AJTSGameMode* const GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AJTSGameMode>() : nullptr;
	const float MinimumFuelRequired = GameMode != nullptr ? FMath::Max(0.0f, GameMode->GetMinimumFuelRequired()) : 0.0f;
	if (MinimumFuelText != nullptr)
	{
		MinimumFuelText->SetText(FText::FromString(FString::Printf(TEXT("TARGET: %d"), FMath::CeilToInt(MinimumFuelRequired))));
	}

	const float FuelRatio = MinimumFuelRequired > KINDA_SMALL_NUMBER
		? FMath::Clamp(static_cast<float>(FuelCount) / MinimumFuelRequired, 0.0f, 1.0f)
		: (FuelCount > 0 ? 1.0f : 0.0f);
	if (FuelProgressBar != nullptr)
	{
		FuelProgressBar->SetPercent(FuelRatio);
		FuelProgressBar->SetFillColorAndOpacity(FuelCount >= MinimumFuelRequired
			? FLinearColor(0.15f, 0.85f, 0.35f, 1.0f)
			: FLinearColor(0.95f, 0.24f, 0.10f, 1.0f));
	}

	if (AvatarBlock != nullptr)
	{
		if (const UJTSGameInstance* const GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance<UJTSGameInstance>() : nullptr)
		{
			AvatarBlock->SetBrushColor(GameInstance->GetSelectedAvatarLinearColor());
		}
	}
	if (RocketIconCanvas != nullptr)
	{
		RocketIconCanvas->SetVisibility(PlayerCharacter != nullptr && PlayerCharacter->IsBoarded() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UJTSPrototypeHUDWidget::RefreshBoardingProgress()
{
	AJTSCharacter* const PlayerCharacter = FindPlayerCharacter();
	const bool bShowProgress = BoundGameState.IsValid()
		&& BoundGameState->IsEarthCollectionActive()
		&& PlayerCharacter != nullptr
		&& PlayerCharacter->IsBoardingHoldActive();
	if (!bShowProgress)
	{
		SetBoardingProgressVisible(false);
		return;
	}

	SetBoardingProgressVisible(true);
	const float Progress = PlayerCharacter->GetBoardingHoldProgress();
	if (BoardingProgressWidget != nullptr)
	{
		BoardingProgressWidget->SetProgress(Progress);
	}
	if (BoardingRemainingText != nullptr)
	{
		BoardingRemainingText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), PlayerCharacter->GetBoardingHoldRemainingTime())));
	}
	if (BoardingLabelText != nullptr)
	{
		BoardingLabelText->SetText(FText::FromString(TEXT("BOARDING")));
	}
}

void UJTSPrototypeHUDWidget::RefreshResultView(EJTSGameplayPhase NewGameplayPhase)
{
	const bool bSuccess = NewGameplayPhase == EJTSGameplayPhase::MoonArrivalSuccess;
	EJTSFailureReason FailureReason = EJTSFailureReason::None;
	if (const AJTSGameState* const GameState = BoundGameState.Get())
	{
		FailureReason = GameState->GetFailureReason();
	}

	FString Subtitle = TEXT("EARTH CAUGHT THE SHIP!");
	FString Detail = TEXT("NOT ENOUGH FUEL TO REACH THE MOON");
	if (FailureReason == EJTSFailureReason::NoTimelyBoarding)
	{
		Subtitle = TEXT("YOU MISSED THE LAUNCH WINDOW");
		Detail = TEXT("YOU DID NOT BOARD THE SHIP IN TIME");
	}
	else if (FailureReason == EJTSFailureReason::NoSpacecraft)
	{
		Subtitle = TEXT("NO SHIP WAS READY");
		Detail = TEXT("THE SPACECRAFT COULD NOT BE FOUND");
	}
	else if (FailureReason == EJTSFailureReason::InvalidGameInstance)
	{
		Subtitle = TEXT("MISSION STATE ERROR");
		Detail = TEXT("THE EXPEDITION SUPPLIES COULD NOT BE SAVED");
	}

	if (ResultBackground != nullptr)
	{
		ResultBackground->SetBrushColor(bSuccess
			? FLinearColor(0.01f, 0.025f, 0.10f, 0.97f)
			: FLinearColor(0.28f, 0.015f, 0.02f, 0.97f));
	}
	if (ResultTitleText != nullptr)
	{
		ResultTitleText->SetText(FText::FromString(bSuccess ? TEXT("SUCCESS!") : TEXT("OH NO!")));
		ResultTitleText->SetColorAndOpacity(FSlateColor(bSuccess ? FLinearColor(0.70f, 0.92f, 1.0f, 1.0f) : FLinearColor(1.0f, 0.92f, 0.78f, 1.0f)));
	}
	if (ResultSubtitleText != nullptr)
	{
		ResultSubtitleText->SetText(FText::FromString(bSuccess ? TEXT("THE SHIP REACHED THE MOON!") : *Subtitle));
	}
	if (ResultDetailText != nullptr)
	{
		ResultDetailText->SetText(FText::FromString(bSuccess ? TEXT("WELCOME TO THE MOON") : *Detail));
		ResultDetailText->SetColorAndOpacity(FSlateColor(bSuccess ? FLinearColor(0.75f, 0.85f, 1.0f, 1.0f) : FLinearColor(1.0f, 0.75f, 0.70f, 1.0f)));
	}
}

void UJTSPrototypeHUDWidget::RefreshAvatarSelection()
{
	EJTSAvatarColor SelectedColor = EJTSAvatarColor::Blue;
	if (const UJTSGameInstance* const GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance<UJTSGameInstance>() : nullptr)
	{
		SelectedColor = GameInstance->GetSelectedAvatarColor();
		if (SettingsPreviewBlock != nullptr)
		{
			SettingsPreviewBlock->SetBrushColor(GameInstance->GetSelectedAvatarLinearColor());
		}
	}

	const FLinearColor SelectedButtonColor(0.92f, 0.96f, 1.0f, 1.0f);
	const FLinearColor UnselectedButtonColor(0.08f, 0.12f, 0.18f, 1.0f);
	if (BlueAvatarButton != nullptr)
	{
		BlueAvatarButton->SetBackgroundColor(SelectedColor == EJTSAvatarColor::Blue ? SelectedButtonColor : UnselectedButtonColor);
	}
	if (OrangeAvatarButton != nullptr)
	{
		OrangeAvatarButton->SetBackgroundColor(SelectedColor == EJTSAvatarColor::Orange ? SelectedButtonColor : UnselectedButtonColor);
	}
	if (GreenAvatarButton != nullptr)
	{
		GreenAvatarButton->SetBackgroundColor(SelectedColor == EJTSAvatarColor::Green ? SelectedButtonColor : UnselectedButtonColor);
	}
}

void UJTSPrototypeHUDWidget::ApplyLayerVisibility(UWidget* Layer, bool bVisible)
{
	if (Layer != nullptr)
	{
		Layer->SetVisibility(bVisible
			? (Layer == GameplayLayer ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Visible)
			: ESlateVisibility::Collapsed);
	}
}

void UJTSPrototypeHUDWidget::SetBoardingProgressVisible(bool bVisible)
{
	const ESlateVisibility BoardingVisibility = bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
	if (BoardingProgressWidget != nullptr)
	{
		BoardingProgressWidget->SetVisibility(BoardingVisibility);
		if (!bVisible)
		{
			BoardingProgressWidget->SetProgress(0.0f);
		}
	}
	if (BoardingRemainingText != nullptr)
	{
		BoardingRemainingText->SetVisibility(BoardingVisibility);
		if (!bVisible)
		{
			BoardingRemainingText->SetText(FText::FromString(TEXT("0.0")));
		}
	}
	if (BoardingLabelText != nullptr)
	{
		BoardingLabelText->SetVisibility(BoardingVisibility);
	}
}

void UJTSPrototypeHUDWidget::HandleGameplayPhaseChanged(EJTSGameplayPhase NewGameplayPhase)
{
	RefreshPhaseView(NewGameplayPhase);
}

void UJTSPrototypeHUDWidget::HandleStartMissionClicked()
{
	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		PlayerController->StartGame();
	}
}

void UJTSPrototypeHUDWidget::HandleSettingsClicked()
{
	if (CachedGameplayPhase != EJTSGameplayPhase::WaitingToStart)
	{
		return;
	}

	bSettingsVisible = true;
	ApplyLayerVisibility(StartMenuLayer, false);
	ApplyLayerVisibility(SettingsLayer, true);
	RefreshAvatarSelection();
}

void UJTSPrototypeHUDWidget::HandleBackSettingsClicked()
{
	bSettingsVisible = false;
	ApplyLayerVisibility(SettingsLayer, false);
	ApplyLayerVisibility(StartMenuLayer, CachedGameplayPhase == EJTSGameplayPhase::WaitingToStart);
}

void UJTSPrototypeHUDWidget::HandleBlueAvatarClicked()
{
	if (UJTSGameInstance* const GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance<UJTSGameInstance>() : nullptr)
	{
		GameInstance->SetSelectedAvatarColor(EJTSAvatarColor::Blue);
	}
	RefreshAvatarSelection();
	RefreshGameplayHud();
}

void UJTSPrototypeHUDWidget::HandleOrangeAvatarClicked()
{
	if (UJTSGameInstance* const GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance<UJTSGameInstance>() : nullptr)
	{
		GameInstance->SetSelectedAvatarColor(EJTSAvatarColor::Orange);
	}
	RefreshAvatarSelection();
	RefreshGameplayHud();
}

void UJTSPrototypeHUDWidget::HandleGreenAvatarClicked()
{
	if (UJTSGameInstance* const GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance<UJTSGameInstance>() : nullptr)
	{
		GameInstance->SetSelectedAvatarColor(EJTSAvatarColor::Green);
	}
	RefreshAvatarSelection();
	RefreshGameplayHud();
}

void UJTSPrototypeHUDWidget::HandleRestartClicked()
{
	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		PlayerController->RestartCurrentLevel();
	}
}

void UJTSPrototypeHUDWidget::HandleQuitClicked()
{
	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		PlayerController->QuitGame();
	}
}

AJTSSpacecraftActor* UJTSPrototypeHUDWidget::FindSpacecraft() const
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<AJTSSpacecraftActor> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}

	return nullptr;
}

AJTSCharacter* UJTSPrototypeHUDWidget::FindPlayerCharacter() const
{
	APlayerController* const PlayerController = GetOwningPlayer();
	return PlayerController != nullptr ? Cast<AJTSCharacter>(PlayerController->GetPawn()) : nullptr;
}

FString UJTSPrototypeHUDWidget::ResourceTypeToString(EJTSResourceType ResourceType)
{
	switch (ResourceType)
	{
	case EJTSResourceType::Fuel:
		return TEXT("Fuel");

	case EJTSResourceType::Water:
		return TEXT("Water");

	case EJTSResourceType::Food:
		return TEXT("Food");

	default:
		return TEXT("Unknown");
	}
}

FString UJTSPrototypeHUDWidget::FormatRemainingTime(float RemainingSeconds)
{
	const int32 RemainingCentiseconds = FMath::Max(0, FMath::CeilToInt(FMath::Max(0.0f, RemainingSeconds) * 100.0f));
	const int32 WholeSeconds = RemainingCentiseconds / 100;
	const int32 Centiseconds = RemainingCentiseconds % 100;
	return FString::Printf(TEXT("%02d.%02d"), WholeSeconds, Centiseconds);
}
