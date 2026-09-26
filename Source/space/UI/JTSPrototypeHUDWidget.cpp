// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSPrototypeHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Math/RotationMatrix.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "space/Components/JTSCarryComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSMeleeComponent.h"
#include "space/Components/JTSRangedWeaponComponent.h"
#include "space/Components/JTSSpacecraftFlightMovementComponent.h"
#include "space/Core/JTSGameInstance.h"
#include "space/Interaction/IInteractable.h"
#include "space/Interaction/InteractionComponent.h"
#include "space/Interaction/JTSMeleeTarget.h"
#include "space/Items/JTSResourcePickupActor.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Systems/JTSOnlineSessionSubsystem.h"
#include "space/Systems/JTSVoiceSubsystem.h"
#include "space/UI/JTSCircularProgressWidget.h"
#include "space/World/JTSMoonResourceActor.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "space/World/JTSMoonSurfaceGameplaySettings.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetLandingManager.h"
#include "space/World/JTSSpaceWorldManager.h"

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

}

bool UJTSPrototypeHUDWidget::OpenMoonShop(AJTSCharacter* Player)
{
	// This native HUD keeps the old panel code only so prior widget assets do
	// not lose bindings.  The transaction path is deliberately retired: the
	// physical SpaceWorld terminal owns the formal market.
	static_cast<void>(Player);
	return false;
}

void UJTSPrototypeHUDWidget::CloseMoonShop()
{
	// Retained as a no-op for pre-existing controller calls. Moon workshop sales
	// were replaced by the SpaceWorld shop, so there is no legacy modal to close.
	RefreshGameplayHud();
}

bool UJTSPrototypeHUDWidget::IsMoonShopOpen() const
{
	return false;
}

void UJTSPrototypeHUDWidget::OpenGameMenu()
{
	bGameMenuOpen = true;
	ApplyLayerVisibility(PauseMenuLayer, true);
	ApplyLayerVisibility(GameMenuInfoText, false);
	RefreshGameplayHud();
}

void UJTSPrototypeHUDWidget::CloseGameMenu()
{
	bGameMenuOpen = false;
	ApplyLayerVisibility(PauseMenuLayer, false);
	ApplyLayerVisibility(GameMenuInfoText, false);
	RefreshGameplayHud();
}

bool UJTSPrototypeHUDWidget::IsGameMenuOpen() const
{
	return bGameMenuOpen;
}

TSharedRef<SWidget> UJTSPrototypeHUDWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UJTSPrototypeHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	BuildWidgetTree();
	BindGameState();
	BindPlayerHealth();
	BindSpacecraftResources();
	RefreshAvatarSelection();
	RefreshEarthCollectionDurationText();

	if (BoundGameState.IsValid())
	{
		RefreshPhaseView(BoundGameState->GetGameplayPhase());
	}
	else
	{
		RefreshPhaseView(EJTSGameplayPhase::WaitingToStart);
	}
}

FReply UJTSPrototypeHUDWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bGameMenuOpen && InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(GetOwningPlayer()))
		{
			Controller->CloseGameMenu();
			return FReply::Handled();
		}
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UJTSPrototypeHUDWidget::NativeDestruct()
{
	UnbindPlayerHealth();
	UnbindSpacecraftResources();

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

	const bool bEarthCollectionActive = BoundGameState.IsValid() && BoundGameState->IsEarthCollectionActive();
	const bool bMoonExplorationActive = BoundGameState.IsValid() && BoundGameState->IsMoonExploration();
	const bool bSpaceFlightActive = BoundGameState.IsValid() && BoundGameState->IsSpaceFlight();
	const AJTSSpaceWorldManager* const SpaceWorldManager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
	const bool bSpaceWorldSurfaceActive = IsValid(SpaceWorldManager) && SpaceWorldManager->IsSurfaceGameplayReady();
	if (bEarthCollectionActive || bMoonExplorationActive || bSpaceWorldSurfaceActive)
	{
		if (bSpaceWorldSurfaceActive)
		{
			// SpaceWorld uses a persistent world rather than a GameState phase, so keep the shared
			// gameplay layer available for the nearby-ship prompt and hold-to-board feedback.
			ApplyLayerVisibility(GameplayLayer, true);
		}
		RefreshGameplayHud();
		if (bEarthCollectionActive || bSpaceWorldSurfaceActive)
		{
			RefreshBoardingProgress();
		}
		else
		{
			SetBoardingProgressVisible(false);
		}

		if (bEarthCollectionActive && TimeText != nullptr)
		{
			const float RemainingTime = BoundGameState->GetEarthCollectionRemainingTime();
			const float PulseAlpha = RemainingTime <= 5.0f
				? (0.5f + 0.5f * FMath::Sin(static_cast<float>(GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0) * 8.0f))
				: 0.0f;
			const float PulseScale = RemainingTime <= 5.0f ? FMath::Lerp(1.0f, 1.25f, PulseAlpha) : 1.0f;
			TimeText->SetRenderScale(FVector2D(PulseScale, PulseScale));
		}
	}

	const bool bDrivingSpacecraft = IsValid(Cast<AJTSSpacecraftActor>(
		GetOwningPlayer() != nullptr ? GetOwningPlayer()->GetPawn() : nullptr));
	if (bSpaceFlightActive || bDrivingSpacecraft)
	{
		// Run after character HUD refresh so the active vehicle owns its own instructions and state.
		SetBoardingProgressVisible(false);
		RefreshFlightHud();
	}
	else
	{
		if (!bEarthCollectionActive && !bSpaceWorldSurfaceActive)
		{
			SetBoardingProgressVisible(false);
		}
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
	PauseMenuLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PauseMenuLayer"));

	AddCanvasChild(RootCanvas, StartMenuLayer, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);
	AddCanvasChild(RootCanvas, SettingsLayer, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);
	AddCanvasChild(RootCanvas, GameplayLayer, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);
	AddCanvasChild(RootCanvas, LaunchingLayer, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);
	AddCanvasChild(RootCanvas, ResultLayer, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);
	AddCanvasChild(RootCanvas, PauseMenuLayer, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);

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
			StartRulesText = MakeTextBlock(WidgetTree, TEXT("StartRules"), TEXT("TIME LIMITED\nTWO HANDS\nONE TERRIBLE PLAN"), 25.0f, FLinearColor(1.0f, 0.78f, 0.28f, 1.0f), ETextJustify::Center);
			AddVerticalChild(MenuBox, StartRulesText, FMargin(0.0f, 6.0f, 0.0f, 28.0f));

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
		AddCanvasChild(SettingsLayer, Card, FAnchors(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(920.0f, 560.0f), FVector2D(0.5f, 0.5f));

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
			FLinearColor(0.18f, 0.85f, 0.28f, 1.0f),
			FLinearColor(0.58f, 0.25f, 0.90f, 1.0f)};
		const TArray<FString> AvatarLabels = {TEXT("BLUE"), TEXT("ORANGE"), TEXT("GREEN"), TEXT("PURPLE")};
		const TArray<FName> AvatarNames = {TEXT("BlueAvatarButton"), TEXT("OrangeAvatarButton"), TEXT("GreenAvatarButton"), TEXT("PurpleAvatarButton")};
		TArray<UButton*> AvatarButtons;
		AvatarButtons.Reserve(4);
		for (int32 Index = 0; Index < 4; ++Index)
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
			AddCanvasChild(SettingsLayer, AvatarButton, FAnchors(0.5f, 0.5f), FVector2D(-300.0f + 200.0f * Index, 65.0f), FVector2D(160.0f, 150.0f), FVector2D(0.5f, 0.5f));
			AvatarButtons.Add(AvatarButton);
		}
		if (AvatarButtons.Num() == 4)
		{
			BlueAvatarButton = AvatarButtons[0];
			OrangeAvatarButton = AvatarButtons[1];
			GreenAvatarButton = AvatarButtons[2];
			PurpleAvatarButton = AvatarButtons[3];
			BlueAvatarButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleBlueAvatarClicked);
			OrangeAvatarButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleOrangeAvatarClicked);
			GreenAvatarButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleGreenAvatarClicked);
			PurpleAvatarButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandlePurpleAvatarClicked);
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
		TimeText = MakeTextBlock(WidgetTree, TEXT("TimeText"), TEXT("TIME: 00.00"), 34.0f, FLinearColor::White, ETextJustify::Center);
		if (TimeText != nullptr)
		{
			TimeText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		}
		AddCanvasChild(GameplayLayer, TimeText, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 28.0f), FVector2D(280.0f, 58.0f), FVector2D(0.5f, 0.0f));

		FlightTelemetryText = MakeTextBlock(
			WidgetTree,
			TEXT("FlightTelemetryText"),
			TEXT("SPD 0 m/s\nMOON 0 m"),
			18.0f,
			FLinearColor(0.70f, 0.92f, 1.0f, 1.0f),
			ETextJustify::Right);
		AddCanvasChild(GameplayLayer, FlightTelemetryText, FAnchors(1.0f, 0.0f), FVector2D(-28.0f, 28.0f), FVector2D(300.0f, 120.0f), FVector2D(1.0f, 0.0f));

		PlayerCardPanel = MakeBorder(WidgetTree, TEXT("PlayerCardPanel"), FLinearColor(0.015f, 0.035f, 0.070f, 0.93f), 5.0f);
		AddCanvasChild(GameplayLayer, PlayerCardPanel, FAnchors(0.0f, 0.0f), FVector2D(28.0f, 28.0f), FVector2D(276.0f, 106.0f));
		UCanvasPanel* const PlayerCardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerCardCanvas"));
		if (PlayerCardPanel != nullptr && PlayerCardCanvas != nullptr)
		{
			PlayerCardPanel->SetContent(PlayerCardCanvas);
		}

		AvatarBlock = MakeBorder(WidgetTree, TEXT("AvatarBlock"), FLinearColor(0.10f, 0.45f, 1.0f, 1.0f), 5.0f);
		AddCanvasChild(PlayerCardCanvas, AvatarBlock, FAnchors(0.0f, 0.0f), FVector2D(5.0f, 5.0f), FVector2D(94.0f, 94.0f));
		UTextBlock* const AvatarLabel = MakeTextBlock(WidgetTree, TEXT("AvatarLabel"), TEXT("YOU"), 19.0f, FLinearColor::White, ETextJustify::Center);
		AddCanvasChild(PlayerCardCanvas, AvatarLabel, FAnchors(0.0f, 0.0f), FVector2D(10.0f, 35.0f), FVector2D(84.0f, 26.0f));

		PlayerHealthPanel = MakeBorder(WidgetTree, TEXT("PlayerHealthPanel"), FLinearColor(0.035f, 0.075f, 0.12f, 0.96f), 5.0f);
		AddCanvasChild(PlayerCardCanvas, PlayerHealthPanel, FAnchors(0.0f, 0.0f), FVector2D(104.0f, 5.0f), FVector2D(162.0f, 94.0f));
		UCanvasPanel* const PlayerHealthCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerHealthCanvas"));
		if (PlayerHealthPanel != nullptr && PlayerHealthCanvas != nullptr)
		{
			PlayerHealthPanel->SetContent(PlayerHealthCanvas);
			AddCanvasChild(PlayerHealthCanvas, MakeTextBlock(WidgetTree, TEXT("PlayerHealthLabel"), TEXT("HP"), 15.0f, FLinearColor(0.65f, 0.90f, 1.0f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(7.0f, 15.0f), FVector2D(28.0f, 21.0f));
			PlayerHealthProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PlayerHealthProgressBar"));
			if (PlayerHealthProgressBar != nullptr)
			{
				PlayerHealthProgressBar->SetFillColorAndOpacity(FLinearColor(0.20f, 0.90f, 0.62f, 1.0f));
			}
			AddCanvasChild(PlayerHealthCanvas, PlayerHealthProgressBar, FAnchors(0.0f, 0.0f), FVector2D(39.0f, 18.0f), FVector2D(112.0f, 15.0f));
			PlayerHealthAmountText = MakeTextBlock(WidgetTree, TEXT("PlayerHealthAmountText"), TEXT("10 / 10"), 13.0f, FLinearColor(0.88f, 0.95f, 1.0f, 1.0f), ETextJustify::Center);
			AddCanvasChild(PlayerHealthCanvas, PlayerHealthAmountText, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 48.0f), FVector2D(145.0f, 20.0f), FVector2D(0.5f, 0.0f));
		}

		RocketIconCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RocketIconCanvas"));
		AddCanvasChild(PlayerCardCanvas, RocketIconCanvas, FAnchors(0.0f, 0.0f), FVector2D(59.0f, 59.0f), FVector2D(38.0f, 38.0f));
		RocketBody = MakeBorder(WidgetTree, TEXT("RocketBody"), FLinearColor(0.85f, 0.94f, 1.0f, 1.0f), 2.0f);
		AddCanvasChild(RocketIconCanvas, RocketBody, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 5.0f), FVector2D(17.0f, 29.0f), FVector2D(0.5f, 0.0f));
		RocketFlame = MakeBorder(WidgetTree, TEXT("RocketFlame"), FLinearColor(1.0f, 0.42f, 0.05f, 1.0f), 1.0f);
		AddCanvasChild(RocketIconCanvas, RocketFlame, FAnchors(0.5f, 1.0f), FVector2D(0.0f, -4.0f), FVector2D(9.0f, 13.0f), FVector2D(0.5f, 1.0f));

		InventoryPanel = MakeBorder(WidgetTree, TEXT("InventoryPanel"), FLinearColor(0.015f, 0.035f, 0.070f, 0.93f), 6.0f);
		InventoryPanelSlot = AddCanvasChild(GameplayLayer, InventoryPanel, FAnchors(0.5f, 1.0f), FVector2D(0.0f, -24.0f), FVector2D(720.0f, 98.0f), FVector2D(0.5f, 1.0f));
		UCanvasPanel* const InventoryCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InventoryCanvas"));
		if (InventoryPanel != nullptr && InventoryCanvas != nullptr)
		{
			InventoryPanel->SetContent(InventoryCanvas);
			InventoryTitleText = MakeTextBlock(WidgetTree, TEXT("InventoryTitle"), TEXT("QUICKBAR 1/1"), 14.0f, FLinearColor(0.78f, 0.92f, 1.0f, 1.0f), ETextJustify::Center);
			AddCanvasChild(InventoryCanvas, InventoryTitleText, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 2.0f), FVector2D(680.0f, 20.0f), FVector2D(0.5f, 0.0f));
			for (int32 SlotIndex = 0; SlotIndex < UJTSInventoryComponent::MaximumQuickbarSlots; ++SlotIndex)
		{
			UBorder* const SlotBorder = MakeBorder(
				WidgetTree,
				*FString::Printf(TEXT("InventorySlot%d"), SlotIndex),
				FLinearColor(0.08f, 0.15f, 0.22f, 0.96f),
				3.0f);
			UTextBlock* const SlotText = MakeTextBlock(
				WidgetTree,
				*FString::Printf(TEXT("InventorySlot%dText"), SlotIndex),
				TEXT("EMPTY"),
				15.0f,
				FLinearColor(0.78f, 0.84f, 0.90f, 1.0f),
				ETextJustify::Center);
			if (SlotBorder != nullptr)
			{
				SlotBorder->SetContent(SlotText);
			}
			AddCanvasChild(InventoryCanvas, SlotBorder, FAnchors(0.0f, 0.0f), FVector2D(8.0f + 76.0f * SlotIndex, 25.0f), FVector2D(72.0f, 64.0f));
			InventorySlotBorders.Add(SlotBorder);
			InventorySlotTexts.Add(SlotText);
		}
		}
		InteractionPromptText = MakeTextBlock(WidgetTree, TEXT("InteractionPromptText"), TEXT(""), 17.0f, FLinearColor(0.90f, 0.96f, 1.0f, 1.0f), ETextJustify::Center);
		InteractionPromptSlot = AddCanvasChild(
			GameplayLayer,
			InteractionPromptText,
			FAnchors(0.0f, 0.0f),
			FVector2D::ZeroVector,
			FVector2D(280.0f, 54.0f),
			FVector2D(0.5f, 1.0f));
		CrosshairText = MakeTextBlock(WidgetTree, TEXT("CrosshairText"), TEXT("+"), 30.0f, FLinearColor(0.88f, 0.96f, 1.0f, 0.92f), ETextJustify::Center);
		AddCanvasChild(GameplayLayer, CrosshairText, FAnchors(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(36.0f, 36.0f), FVector2D(0.5f, 0.5f));

		SpacecraftWorldMarker = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SpacecraftWorldMarker"));
		SpacecraftWorldMarkerSlot = AddCanvasChild(
			GameplayLayer,
			SpacecraftWorldMarker,
			FAnchors(0.0f, 0.0f),
			FVector2D::ZeroVector,
			FVector2D(150.0f, 96.0f),
			FVector2D(0.5f, 1.0f));
		if (SpacecraftWorldMarker != nullptr)
		{
			const FLinearColor MarkerColor(0.22f, 0.91f, 0.90f, 1.0f);
			SpacecraftWorldMarkerText = MakeTextBlock(WidgetTree, TEXT("SpacecraftWorldMarkerText"), TEXT("SHIP"), 19.0f, MarkerColor, ETextJustify::Center);
			SpacecraftWorldMarkerDistanceText = MakeTextBlock(WidgetTree, TEXT("SpacecraftWorldMarkerDistanceText"), TEXT("0m"), 14.0f, MarkerColor, ETextJustify::Center);
			SpacecraftWorldMarkerArrowText = MakeTextBlock(WidgetTree, TEXT("SpacecraftWorldMarkerArrowText"), TEXT("▽"), 25.0f, MarkerColor, ETextJustify::Center);
			SpacecraftWorldMarkerDesignationText = MakeTextBlock(WidgetTree, TEXT("SpacecraftWorldMarkerDesignationText"), TEXT("[SPACECRAFT]"), 12.0f, MarkerColor, ETextJustify::Center);
			AddCanvasChild(SpacecraftWorldMarker, SpacecraftWorldMarkerText, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(140.0f, 26.0f), FVector2D(0.5f, 0.0f));
			AddCanvasChild(SpacecraftWorldMarker, SpacecraftWorldMarkerDistanceText, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 25.0f), FVector2D(140.0f, 21.0f), FVector2D(0.5f, 0.0f));
			AddCanvasChild(SpacecraftWorldMarker, SpacecraftWorldMarkerArrowText, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 44.0f), FVector2D(42.0f, 28.0f), FVector2D(0.5f, 0.0f));
			AddCanvasChild(SpacecraftWorldMarker, SpacecraftWorldMarkerDesignationText, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 70.0f), FVector2D(150.0f, 20.0f), FVector2D(0.5f, 0.0f));
		}

		SpacecraftEdgeIndicator = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SpacecraftEdgeIndicator"));
		SpacecraftEdgeIndicatorSlot = AddCanvasChild(
			GameplayLayer,
			SpacecraftEdgeIndicator,
			FAnchors(0.0f, 0.0f),
			FVector2D::ZeroVector,
			FVector2D(116.0f, 62.0f),
			FVector2D(0.5f, 0.5f));
		if (SpacecraftEdgeIndicator != nullptr)
		{
			SpacecraftEdgeArrowText = MakeTextBlock(WidgetTree, TEXT("SpacecraftEdgeArrowText"), TEXT("^"), 26.0f, FLinearColor(0.22f, 0.91f, 0.90f, 1.0f), ETextJustify::Center);
			if (SpacecraftEdgeArrowText != nullptr)
			{
				SpacecraftEdgeArrowText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			}
			AddCanvasChild(SpacecraftEdgeIndicator, SpacecraftEdgeArrowText, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(42.0f, 30.0f), FVector2D(0.5f, 0.0f));
			SpacecraftEdgeDistanceText = MakeTextBlock(WidgetTree, TEXT("SpacecraftEdgeDistanceText"), TEXT("SHIP\n0m"), 15.0f, FLinearColor(0.22f, 0.91f, 0.90f, 1.0f), ETextJustify::Center);
			AddCanvasChild(SpacecraftEdgeIndicator, SpacecraftEdgeDistanceText, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 30.0f), FVector2D(90.0f, 25.0f), FVector2D(0.5f, 0.0f));
		}

		RightSidebarPanel = MakeBorder(WidgetTree, TEXT("RightSidebarPanel"), FLinearColor(0.02f, 0.03f, 0.07f, 0.88f), 6.0f);
		AddCanvasChild(
			GameplayLayer,
			RightSidebarPanel,
			FAnchors(1.0f, 0.0f),
			FVector2D(-28.0f, 90.0f),
			FVector2D(278.0f, 350.0f),
			FVector2D(1.0f, 0.0f));
		UVerticalBox* const RightSidebarBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RightSidebarBox"));
		if (RightSidebarPanel != nullptr && RightSidebarBox != nullptr)
		{
			RightSidebarPanel->SetContent(RightSidebarBox);

			FuelToMoonPanel = MakeBorder(WidgetTree, TEXT("FuelToMoonPanel"), FLinearColor(0.02f, 0.055f, 0.09f, 0.94f), 6.0f);
			UVerticalBox* const FuelToMoonBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FuelToMoonBox"));
			if (FuelToMoonPanel != nullptr && FuelToMoonBox != nullptr)
			{
				FuelToMoonPanel->SetContent(FuelToMoonBox);
				AddVerticalChild(FuelToMoonBox, MakeTextBlock(WidgetTree, TEXT("FuelToMoonHeading"), TEXT("FUEL TO MOON"), 17.0f, FLinearColor(0.72f, 0.91f, 1.0f, 1.0f), ETextJustify::Center), FMargin(0.0f, 0.0f, 0.0f, 4.0f), HAlign_Center);

				USizeBox* const FuelProgressSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FuelProgressSize"));
				if (FuelProgressSize != nullptr)
				{
					FuelProgressSize->SetWidthOverride(244.0f);
					FuelProgressSize->SetHeightOverride(14.0f);
					FuelProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("FuelProgressBar"));
					if (FuelProgressBar != nullptr)
					{
						FuelProgressBar->SetBarFillType(EProgressBarFillType::LeftToRight);
						FuelProgressBar->SetPercent(0.0f);
						FuelProgressBar->SetFillColorAndOpacity(FLinearColor(1.0f, 0.48f, 0.12f, 1.0f));
						FuelProgressSize->SetContent(FuelProgressBar);
					}
				}
				AddVerticalChild(FuelToMoonBox, FuelProgressSize, FMargin(0.0f, 0.0f, 0.0f, 4.0f), HAlign_Center);

				FuelAmountText = MakeTextBlock(WidgetTree, TEXT("FuelAmountText"), TEXT("0 / 0"), 16.0f, FLinearColor::White, ETextJustify::Center);
				AddVerticalChild(FuelToMoonBox, FuelAmountText, FMargin(0.0f, 0.0f, 0.0f, 1.0f), HAlign_Center);
				FuelStatusText = MakeTextBlock(WidgetTree, TEXT("FuelStatusText"), TEXT("NEED FUEL"), 14.0f, FLinearColor(1.0f, 0.48f, 0.12f, 1.0f), ETextJustify::Center);
				AddVerticalChild(FuelToMoonBox, FuelStatusText, FMargin(0.0f, 0.0f), HAlign_Center);
			}
			AddVerticalChild(RightSidebarBox, FuelToMoonPanel, FMargin(0.0f, 0.0f, 0.0f, 8.0f));

			ShipResourcesPanel = MakeBorder(WidgetTree, TEXT("ShipResourcesPanel"), FLinearColor(0.035f, 0.055f, 0.10f, 0.94f), 6.0f);
			UVerticalBox* const ShipResourcesBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShipResourcesBox"));
			if (ShipResourcesPanel != nullptr && ShipResourcesBox != nullptr)
			{
				ShipResourcesPanel->SetContent(ShipResourcesBox);
				AddVerticalChild(ShipResourcesBox, MakeTextBlock(WidgetTree, TEXT("ShipResourcesHeading"), TEXT("SHIP RESOURCES"), 18.0f, FLinearColor(0.95f, 0.85f, 1.0f, 1.0f)), FMargin(0.0f, 0.0f, 0.0f, 5.0f));

				const EJTSResourceType DisplayedResourceTypes[] = {
					EJTSResourceType::Fuel,
					EJTSResourceType::Water,
					EJTSResourceType::Food,
					EJTSResourceType::Rock,
					EJTSResourceType::Ore,
					EJTSResourceType::Organic};
				for (int32 ResourceIndex = 0; ResourceIndex < UE_ARRAY_COUNT(DisplayedResourceTypes); ++ResourceIndex)
				{
					UHorizontalBox* const ResourceRow = WidgetTree->ConstructWidget<UHorizontalBox>(
						UHorizontalBox::StaticClass(),
						*FString::Printf(TEXT("ShipResourceRow%d"), ResourceIndex));
					UTextBlock* const ResourceNameText = MakeTextBlock(
						WidgetTree,
						*FString::Printf(TEXT("ShipResourceName%d"), ResourceIndex),
						ResourceTypeToString(DisplayedResourceTypes[ResourceIndex]).ToUpper(),
						15.0f,
						FLinearColor(0.80f, 0.88f, 0.98f, 1.0f));
					UTextBlock* const ResourceAmountText = MakeTextBlock(
						WidgetTree,
						*FString::Printf(TEXT("ShipResourceAmount%d"), ResourceIndex),
						TEXT("0"),
						15.0f,
						FLinearColor::White,
						ETextJustify::Right);
					USizeBox* const ResourceNameSize = WidgetTree->ConstructWidget<USizeBox>(
						USizeBox::StaticClass(),
						*FString::Printf(TEXT("ShipResourceNameSize%d"), ResourceIndex));
					USizeBox* const ResourceAmountSize = WidgetTree->ConstructWidget<USizeBox>(
						USizeBox::StaticClass(),
						*FString::Printf(TEXT("ShipResourceAmountSize%d"), ResourceIndex));
					if (ResourceNameSize != nullptr)
					{
						ResourceNameSize->SetWidthOverride(176.0f);
						ResourceNameSize->SetContent(ResourceNameText);
					}
					if (ResourceAmountSize != nullptr)
					{
						ResourceAmountSize->SetWidthOverride(72.0f);
						ResourceAmountSize->SetContent(ResourceAmountText);
					}
					if (ResourceRow != nullptr)
					{
						ResourceRow->AddChildToHorizontalBox(ResourceNameSize);
						ResourceRow->AddChildToHorizontalBox(ResourceAmountSize);
						ResourceRow->SetVisibility(ESlateVisibility::Collapsed);
					}
					AddVerticalChild(ShipResourcesBox, ResourceRow, FMargin(0.0f, 1.0f));
					ShipResourceRows.Add(ResourceRow);
					ShipResourceNameTexts.Add(ResourceNameText);
					ShipResourceAmountTexts.Add(ResourceAmountText);
				}
			}
			AddVerticalChild(RightSidebarBox, ShipResourcesPanel, FMargin(0.0f, 0.0f));
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

		GameplayHelpText = MakeTextBlock(WidgetTree, TEXT("HelpText"), TEXT("WASD: MOVE    SHIFT: RUN    LMB: ATTACK    HOLD F: BOARD    V: CAMERA    WHEEL: ZOOM"), 16.0f, FLinearColor(0.85f, 0.95f, 1.0f, 1.0f), ETextJustify::Right);
		AddCanvasChild(GameplayLayer, GameplayHelpText, FAnchors(1.0f, 1.0f), FVector2D(-28.0f, -20.0f), FVector2D(960.0f, 56.0f), FVector2D(1.0f, 1.0f));
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

			RestartButton = MakeButton(WidgetTree, TEXT("RestartButton"), TEXT("RESTART"));
			if (RestartButton != nullptr)
			{
				RestartButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleRestartClicked);
			}
			AddVerticalChild(ResultBox, RestartButton, FMargin(80.0f, 5.0f), HAlign_Fill);

			QuitButton = MakeButton(WidgetTree, TEXT("QuitButton"), TEXT("QUIT"));
			if (QuitButton != nullptr)
			{
				QuitButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleQuitClicked);
			}
			AddVerticalChild(ResultBox, QuitButton, FMargin(80.0f, 5.0f), HAlign_Fill);
		}
	}

	if (PauseMenuLayer != nullptr)
	{
		UBorder* const PauseBackground = MakeBorder(
			WidgetTree,
			TEXT("PauseMenuBackground"),
			FLinearColor(0.005f, 0.012f, 0.026f, 0.82f));
		AddCanvasChild(PauseMenuLayer, PauseBackground, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector, FVector2D::ZeroVector);

		UBorder* const PauseCard = MakeBorder(
			WidgetTree,
			TEXT("PauseMenuCard"),
			FLinearColor(0.025f, 0.075f, 0.12f, 0.98f),
			22.0f);
		AddCanvasChild(PauseMenuLayer, PauseCard, FAnchors(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(500.0f, 470.0f), FVector2D(0.5f, 0.5f));
		UVerticalBox* const PauseMenuBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PauseMenuBox"));
		if (PauseCard != nullptr && PauseMenuBox != nullptr)
		{
			PauseCard->SetContent(PauseMenuBox);
			AddVerticalChild(PauseMenuBox, MakeTextBlock(WidgetTree, TEXT("PauseMenuTitle"), TEXT("GAME MENU"), 30.0f, FLinearColor(0.22f, 0.91f, 0.90f, 1.0f), ETextJustify::Center), FMargin(0.0f, 18.0f, 0.0f, 18.0f));
			ResumeGameButton = MakeButton(WidgetTree, TEXT("ResumeGameButton"), TEXT("RESUME"));
			if (ResumeGameButton != nullptr)
			{
				ResumeGameButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleResumeGameClicked);
			}
			AddVerticalChild(PauseMenuBox, ResumeGameButton, FMargin(34.0f, 5.0f));

			GameMenuSessionDetailsButton = MakeButton(WidgetTree, TEXT("GameMenuSessionDetailsButton"), TEXT("SESSION DETAILS"));
			if (GameMenuSessionDetailsButton != nullptr)
			{
				GameMenuSessionDetailsButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleGameMenuSessionDetailsClicked);
			}
			AddVerticalChild(PauseMenuBox, GameMenuSessionDetailsButton, FMargin(34.0f, 5.0f));

			GameMenuSettingsButton = MakeButton(WidgetTree, TEXT("GameMenuSettingsButton"), TEXT("SETTINGS"));
			if (GameMenuSettingsButton != nullptr)
			{
				GameMenuSettingsButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleGameMenuSettingsClicked);
			}
			AddVerticalChild(PauseMenuBox, GameMenuSettingsButton, FMargin(34.0f, 5.0f));

			GameMenuInfoText = MakeTextBlock(WidgetTree, TEXT("GameMenuInfoText"), TEXT(""), 14.0f, FLinearColor(0.78f, 0.90f, 1.0f), ETextJustify::Center);
			AddVerticalChild(PauseMenuBox, GameMenuInfoText, FMargin(34.0f, 8.0f));

			ReturnToMainMenuButton = MakeButton(WidgetTree, TEXT("ReturnToMainMenuButton"), TEXT("LEAVE EXPEDITION"));
			if (ReturnToMainMenuButton != nullptr)
			{
				ReturnToMainMenuButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleReturnToMainMenuClicked);
			}
			AddVerticalChild(PauseMenuBox, ReturnToMainMenuButton, FMargin(34.0f, 5.0f));

			GameMenuQuitButton = MakeButton(WidgetTree, TEXT("GameMenuQuitButton"), TEXT("QUIT TO DESKTOP"));
			if (GameMenuQuitButton != nullptr)
			{
				GameMenuQuitButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleGameMenuQuitClicked);
			}
			AddVerticalChild(PauseMenuBox, GameMenuQuitButton, FMargin(34.0f, 5.0f, 34.0f, 18.0f));
		}
	}

	ApplyLayerVisibility(StartMenuLayer, false);
	ApplyLayerVisibility(SettingsLayer, false);
	ApplyLayerVisibility(GameplayLayer, false);
	ApplyLayerVisibility(LaunchingLayer, false);
	ApplyLayerVisibility(ResultLayer, false);
	ApplyLayerVisibility(PauseMenuLayer, false);
	ApplyLayerVisibility(FuelToMoonPanel, false);
	ApplyLayerVisibility(InventoryPanel, false);
	ApplyLayerVisibility(InteractionPromptText, false);
	ApplyLayerVisibility(CrosshairText, false);
	SetSpacecraftNavigationVisibility(false, false);
	ApplyLayerVisibility(GameplayHelpText, false);
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

void UJTSPrototypeHUDWidget::BindPlayerHealth()
{
	AJTSCharacter* const PlayerCharacter = FindPlayerCharacter();
	UJTSHealthComponent* const NewHealthComponent = IsValid(PlayerCharacter)
		? PlayerCharacter->GetHealthComponent()
		: nullptr;
	if (BoundPlayerHealthComponent.Get() == NewHealthComponent)
	{
		if (IsValid(NewHealthComponent))
		{
			RefreshPlayerHealth(NewHealthComponent->GetHealth(), NewHealthComponent->GetMaxHealth());
		}
		return;
	}

	UnbindPlayerHealth();
	BoundPlayerHealthComponent = NewHealthComponent;
	if (IsValid(NewHealthComponent))
	{
		NewHealthComponent->OnHealthChanged.AddDynamic(this, &UJTSPrototypeHUDWidget::HandlePlayerHealthChanged);
		RefreshPlayerHealth(NewHealthComponent->GetHealth(), NewHealthComponent->GetMaxHealth());
	}
	else
	{
		RefreshPlayerHealth(0.0f, 0.0f);
	}
}

void UJTSPrototypeHUDWidget::UnbindPlayerHealth()
{
	if (UJTSHealthComponent* const PreviousHealthComponent = BoundPlayerHealthComponent.Get())
	{
		PreviousHealthComponent->OnHealthChanged.RemoveDynamic(this, &UJTSPrototypeHUDWidget::HandlePlayerHealthChanged);
	}
	BoundPlayerHealthComponent.Reset();
}

void UJTSPrototypeHUDWidget::BindSpacecraftResources()
{
	AJTSSpacecraftActor* const NewSpacecraft = FindSpacecraft();
	if (BoundSpacecraftResources.Get() == NewSpacecraft)
	{
		return;
	}

	UnbindSpacecraftResources();
	BoundSpacecraftResources = NewSpacecraft;
	if (IsValid(NewSpacecraft))
	{
		NewSpacecraft->OnShipResourcesChanged.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleShipResourcesChanged);
	}

	RefreshShipResourcesSidebar();
}

void UJTSPrototypeHUDWidget::UnbindSpacecraftResources()
{
	if (AJTSSpacecraftActor* const PreviousSpacecraft = BoundSpacecraftResources.Get())
	{
		PreviousSpacecraft->OnShipResourcesChanged.RemoveDynamic(this, &UJTSPrototypeHUDWidget::HandleShipResourcesChanged);
	}
	BoundSpacecraftResources.Reset();
}

void UJTSPrototypeHUDWidget::RefreshShipResourcesSidebar()
{
	const EJTSResourceType DisplayedResourceTypes[] = {
		EJTSResourceType::Fuel,
		EJTSResourceType::Water,
		EJTSResourceType::Food,
		EJTSResourceType::Rock,
		EJTSResourceType::Ore,
		EJTSResourceType::Organic};
	const AJTSSpacecraftActor* const Spacecraft = BoundSpacecraftResources.IsValid()
		? BoundSpacecraftResources.Get()
		: FindSpacecraft();

	for (int32 ResourceIndex = 0; ResourceIndex < UE_ARRAY_COUNT(DisplayedResourceTypes); ++ResourceIndex)
	{
		const int32 ResourceAmount = IsValid(Spacecraft)
			? Spacecraft->GetResourceAmount(DisplayedResourceTypes[ResourceIndex])
			: 0;
		if (ShipResourceRows.IsValidIndex(ResourceIndex) && ShipResourceRows[ResourceIndex] != nullptr)
		{
			ShipResourceRows[ResourceIndex]->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		if (ShipResourceNameTexts.IsValidIndex(ResourceIndex) && ShipResourceNameTexts[ResourceIndex] != nullptr)
		{
			ShipResourceNameTexts[ResourceIndex]->SetText(FText::FromString(ResourceTypeToString(DisplayedResourceTypes[ResourceIndex]).ToUpper()));
		}
		if (ShipResourceAmountTexts.IsValidIndex(ResourceIndex) && ShipResourceAmountTexts[ResourceIndex] != nullptr)
		{
			ShipResourceAmountTexts[ResourceIndex]->SetText(FText::AsNumber(ResourceAmount));
		}
	}
}

void UJTSPrototypeHUDWidget::RefreshPlayerHealth(float CurrentHealth, float MaxHealth)
{
	const bool bHasPlayerHealth = BoundPlayerHealthComponent.IsValid();
	if (PlayerHealthPanel != nullptr)
	{
		PlayerHealthPanel->SetVisibility(bHasPlayerHealth ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	const float HealthPercent = MaxHealth > KINDA_SMALL_NUMBER
		? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f)
		: 0.0f;
	if (PlayerHealthProgressBar != nullptr)
	{
		PlayerHealthProgressBar->SetPercent(HealthPercent);
		PlayerHealthProgressBar->SetFillColorAndOpacity(HealthPercent <= 0.30f
			? FLinearColor(1.0f, 0.30f, 0.18f, 1.0f)
			: FLinearColor(0.20f, 0.90f, 0.62f, 1.0f));
	}
	if (PlayerHealthAmountText != nullptr)
	{
		PlayerHealthAmountText->SetText(FText::FromString(FString::Printf(
			TEXT("%d / %d"),
			FMath::RoundToInt(FMath::Max(0.0f, CurrentHealth)),
			FMath::RoundToInt(FMath::Max(0.0f, MaxHealth)))));
	}
}

void UJTSPrototypeHUDWidget::HandlePlayerHealthChanged(float CurrentHealth, float MaxHealth)
{
	RefreshPlayerHealth(CurrentHealth, MaxHealth);
}

void UJTSPrototypeHUDWidget::HandleShipResourcesChanged(int32 FuelCount, int32 WaterCount, int32 FoodCount)
{
	(void)FuelCount;
	(void)WaterCount;
	(void)FoodCount;
	RefreshShipResourcesSidebar();
	RefreshFuelToMoonHud();
}

void UJTSPrototypeHUDWidget::RefreshPhaseView(EJTSGameplayPhase NewGameplayPhase)
{
	CachedGameplayPhase = NewGameplayPhase;
	const bool bEarthCollection = NewGameplayPhase == EJTSGameplayPhase::EarthCollection;
	const bool bMoonExploration = NewGameplayPhase == EJTSGameplayPhase::MoonExploration;
	const bool bSpaceFlight = NewGameplayPhase == EJTSGameplayPhase::SpaceFlight;
	const AJTSSpaceWorldManager* const SpaceWorldManager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
	const bool bSpaceWorldSurface = IsValid(SpaceWorldManager) && SpaceWorldManager->IsSurfaceGameplayReady();
	const bool bGameplaySurface = bEarthCollection || bMoonExploration || bSpaceWorldSurface;
	if (bGameplaySurface)
	{
		BindPlayerHealth();
	}
	if (NewGameplayPhase != EJTSGameplayPhase::WaitingToStart)
	{
		bSettingsVisible = false;
	}

	// Expedition lobby ownership moved to UJTSLobbyWidget. The old single-player start/settings
	// layers stay disabled so they cannot compete with replicated lobby state.
	ApplyLayerVisibility(StartMenuLayer, false);
	ApplyLayerVisibility(SettingsLayer, false);
	ApplyLayerVisibility(GameplayLayer, bGameplaySurface || bSpaceFlight);
	ApplyLayerVisibility(LaunchingLayer, NewGameplayPhase == EJTSGameplayPhase::Launching);
	ApplyLayerVisibility(ResultLayer, NewGameplayPhase == EJTSGameplayPhase::EarthCaptureFailure || NewGameplayPhase == EJTSGameplayPhase::MoonArrivalSuccess);
	ApplyLayerVisibility(FuelToMoonPanel, bEarthCollection);
	ApplyLayerVisibility(TimeText, bEarthCollection);
	ApplyLayerVisibility(FlightTelemetryText, bSpaceFlight);
	ApplyLayerVisibility(PlayerCardPanel, bGameplaySurface);
	ApplyLayerVisibility(InventoryPanel, bGameplaySurface);
	ApplyLayerVisibility(RightSidebarPanel, bGameplaySurface);
	if (RightSidebarPanel != nullptr)
	{
		if (UCanvasPanelSlot* const SidebarSlot = Cast<UCanvasPanelSlot>(RightSidebarPanel->Slot))
		{
			SidebarSlot->SetSize(FVector2D(278.0f, bEarthCollection ? 350.0f : 244.0f));
		}
	}
	ApplyLayerVisibility(ShipResourcesPanel, bGameplaySurface);
	ApplyLayerVisibility(InteractionPromptText, bGameplaySurface && !bGameMenuOpen);
	ApplyLayerVisibility(CrosshairText, false);
	ApplyLayerVisibility(GameplayHelpText, bSpaceFlight && !bGameMenuOpen);
	if (!bGameplaySurface)
	{
		SetBoardingProgressVisible(false);
	}
	if (!bMoonExploration || bGameMenuOpen)
	{
		SetSpacecraftNavigationVisibility(false, false);
	}
	if (NewGameplayPhase == EJTSGameplayPhase::WaitingToStart)
	{
		RefreshEarthCollectionDurationText();
	}

	if (NewGameplayPhase == EJTSGameplayPhase::EarthCaptureFailure || NewGameplayPhase == EJTSGameplayPhase::MoonArrivalSuccess)
	{
		RefreshResultView(NewGameplayPhase);
	}
	else if (bGameplaySurface)
	{
		RefreshGameplayHud();
	}
	else if (bSpaceFlight)
	{
		RefreshFlightHud();
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

void UJTSPrototypeHUDWidget::RefreshFlightHud()
{
	// A driver possesses the spacecraft rather than their character, so the normal gameplay HUD
	// refresh does not run for the person who needs the landed-state exit prompt.
	RefreshInteractionPrompt();

	if (FlightTelemetryText == nullptr)
	{
		return;
	}

	AJTSSpacecraftActor* const Spacecraft = Cast<AJTSSpacecraftActor>(
		GetOwningPlayer() != nullptr ? GetOwningPlayer()->GetPawn() : nullptr);
	const bool bLegacySpaceFlight = BoundGameState.IsValid() && BoundGameState->IsSpaceFlight();
	if (!IsValid(Spacecraft))
	{
		if (!bLegacySpaceFlight)
		{
			ApplyLayerVisibility(FlightTelemetryText, false);
			return;
		}

		ApplyLayerVisibility(FlightTelemetryText, true);
		FlightTelemetryText->SetText(FText::FromString(TEXT("FLIGHT SYSTEM\nACQUIRING SPACECRAFT")));
		return;
	}

	ApplyLayerVisibility(FlightTelemetryText, true);
	ApplyLayerVisibility(GameplayHelpText, true);
	// FlightPlanet is explicitly cleared once a craft leaves a planet. In that state the HUD must
	// describe free flight instead of quietly reusing the previous world's CurrentPlanet as a Moon
	// altitude reference.
	const AJTSSpaceWorldManager* const SpaceWorldManager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
	if (IsValid(SpaceWorldManager) && SpaceWorldManager->IsInterplanetaryCruiseActive())
	{
		const AJTSPlanetAnchor* const Destination = SpaceWorldManager->GetCruiseDestinationPlanet();
		const AJTSPlanetAnchor* const Origin = SpaceWorldManager->GetCruiseOriginPlanet();
		const double RemainingKilometers = FMath::Max(0.0, static_cast<double>(SpaceWorldManager->GetCruiseRemainingKilometers()));
		const double SpeedKilometersPerSecond = FMath::Abs(static_cast<double>(SpaceWorldManager->GetCruiseSpeedKilometersPerSecond()));
		const int32 RemainingDisplay = RemainingKilometers >= 1000000.0
			? FMath::RoundToInt(RemainingKilometers / 1000000.0)
			: FMath::Max(1, FMath::RoundToInt(RemainingKilometers));
		const TCHAR* const RemainingUnit = RemainingKilometers >= 1000000.0 ? TEXT("M km") : TEXT("km");
		const int32 SpeedDisplay = FMath::Max(0, FMath::RoundToInt(SpeedKilometersPerSecond));
		const int32 EtaSeconds = SpeedKilometersPerSecond > 0.05
			? FMath::CeilToInt(RemainingKilometers / SpeedKilometersPerSecond)
			: 0;
		const FString DestinationName = IsValid(Destination) ? Destination->GetPlanetId().ToString() : TEXT("TARGET");
		const FString OriginName = IsValid(Origin) ? Origin->GetPlanetId().ToString() : TEXT("ORIGIN");
		FlightTelemetryText->SetText(FText::FromString(FString::Printf(
			TEXT("%s  →  %s\n%s %d %s\nSPD %d km/s    ETA %s"),
			*OriginName.ToUpper(),
			*DestinationName.ToUpper(),
			SpeedKilometersPerSecond > 0.05 ? TEXT("RANGE") : TEXT("HOLD"),
			RemainingDisplay,
			RemainingUnit,
			SpeedDisplay,
			EtaSeconds > 0 ? *FormatRemainingTime(static_cast<float>(EtaSeconds)) : TEXT("--"))));
		FlightTelemetryText->SetColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.92f, 1.0f, 1.0f)));
		return;
	}

	const AJTSPlanetAnchor* const Planet = Spacecraft->GetFlightPlanet();
	if (!IsValid(Planet))
	{
		FlightTelemetryText->SetText(FText::FromString(TEXT("DEEP SPACE\nFREE FLIGHT")));
		return;
	}

	const int32 SpeedMetersPerSecond = FMath::Max(0, FMath::RoundToInt(Spacecraft->GetCurrentSpeed() / 100.0f));
	float SurfaceAltitude = 0.0f;
	const UJTSSpacecraftFlightMovementComponent* const FlightMovement = Spacecraft->GetFlightMovementComponent();
	if ((FlightMovement == nullptr || !FlightMovement->GetResolvedSurfaceAltitude(Planet, SurfaceAltitude))
		&& !Planet->GetAltitudeAboveSurface(Spacecraft->GetActorLocation(), SurfaceAltitude))
	{
		SurfaceAltitude = Planet->GetApproximateAltitude(Spacecraft->GetActorLocation());
	}
	const int32 AltitudeMeters = FMath::Max(0, FMath::RoundToInt(SurfaceAltitude / 100.0f));
	const EJTSSpacecraftFlightState FlightState = Spacecraft->GetFlightState();
	const AJTSPlanetLandingManager* const LandingManager = AJTSPlanetLandingManager::FindPlanetLandingManager(this);
	const bool bLandingAvailable = FlightState == EJTSSpacecraftFlightState::Flying
		&& IsValid(LandingManager)
		&& LandingManager->IsLandingAvailable(Spacecraft);
	const float LandingZoneDistance = IsValid(LandingManager)
		? LandingManager->GetLandingDistance(const_cast<AJTSPlanetAnchor*>(Planet), Spacecraft->GetActorLocation())
		: -1.0f;
	FString StateLine;
	FLinearColor StateColor(0.70f, 0.92f, 1.0f, 1.0f);
	switch (FlightState)
	{
	case EJTSSpacecraftFlightState::LandingAssist:
		switch (Spacecraft->GetLandingAssistPhase())
		{
		case EJTSSpacecraftLandingAssistPhase::Aligning:
			StateLine = TEXT("LANDING ASSIST\nALIGNING");
			break;
		case EJTSSpacecraftLandingAssistPhase::Descending:
			StateLine = TEXT("LANDING ASSIST\nCONTROLLED DESCENT");
			break;
		case EJTSSpacecraftLandingAssistPhase::Touchdown:
			StateLine = TEXT("LANDING ASSIST\nTOUCHDOWN");
			break;
		case EJTSSpacecraftLandingAssistPhase::None:
		default:
			StateLine = TEXT("LANDING ASSIST\nSTABILIZING");
			break;
		}
		StateColor = FLinearColor(1.0f, 0.84f, 0.32f, 1.0f);
		break;

	case EJTSSpacecraftFlightState::Landed:
		StateLine = TEXT("LANDED\nSURFACE LOCK ACTIVE");
		StateColor = FLinearColor(0.35f, 1.0f, 0.68f, 1.0f);
		break;

	case EJTSSpacecraftFlightState::LandingRequest:
		StateLine = TEXT("CHECKING LANDING ZONE");
		StateColor = FLinearColor(1.0f, 0.84f, 0.32f, 1.0f);
		break;

	case EJTSSpacecraftFlightState::Flying:
	default:
		if (bLandingAvailable)
		{
			StateLine = TEXT("HOLD CTRL TO AUTO-LAND");
		}
		else if (LandingZoneDistance >= 0.0f)
		{
			StateLine = FString::Printf(TEXT("LANDING ZONE %dm"), FMath::RoundToInt(LandingZoneDistance / 100.0f));
		}
		else
		{
			StateLine = TEXT("CANNOT LAND HERE");
		}
		StateColor = bLandingAvailable
			? FLinearColor(0.35f, 1.0f, 0.68f, 1.0f)
			: FLinearColor(1.0f, 0.48f, 0.38f, 1.0f);
		break;
	}

	FlightTelemetryText->SetText(FText::FromString(FString::Printf(
		TEXT("SPD %d m/s\nALT %d m\n%s"),
		SpeedMetersPerSecond,
		AltitudeMeters,
		*StateLine)));
	FlightTelemetryText->SetColorAndOpacity(FSlateColor(StateColor));
	if (GameplayHelpText != nullptr)
	{
		FString HelpText;
		if (FlightState == EJTSSpacecraftFlightState::Landed)
		{
			HelpText = TEXT("W/S FORWARD/REVERSE    A/D STRAFE    MOUSE LOOK    WHEEL DISTANCE\nSPACE TAKE OFF    SHIFT BOOST    C BRAKE    F DISEMBARK");
		}
		else if (FlightState == EJTSSpacecraftFlightState::LandingAssist)
		{
			HelpText = TEXT("AUTO-LANDING IN PROGRESS\nSPACE ABORT");
		}
		else
		{
			HelpText = TEXT("W/S FLY/REVERSE    A/D STRAFE    MOUSE LOOK    W + LOOK UP TO DEPART\nSPACE/CTRL UP/DOWN    LOW-ALT DIVE ASSIST    HOLD CTRL OVER PAD TO LAND");
		}
		GameplayHelpText->SetText(FText::FromString(HelpText));
	}
}

void UJTSPrototypeHUDWidget::RefreshGameplayHud()
{
	const AJTSSpaceWorldManager* const SpaceWorldManager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
	const bool bSpaceWorldSurfaceActive = IsValid(SpaceWorldManager) && SpaceWorldManager->IsSurfaceGameplayReady();
	if (!BoundGameState.IsValid() && !bSpaceWorldSurfaceActive)
	{
		return;
	}

	const bool bEarthCollection = BoundGameState.IsValid() && BoundGameState->IsEarthCollectionActive();
	const bool bMoonExploration = BoundGameState.IsValid() && BoundGameState->IsMoonExploration();
	if (bEarthCollection && TimeText != nullptr)
	{
		const float RemainingTime = BoundGameState->GetEarthCollectionRemainingTime();
		TimeText->SetText(FText::FromString(FString::Printf(TEXT("TIME: %s"), *FormatRemainingTime(RemainingTime))));
		TimeText->SetColorAndOpacity(FSlateColor(RemainingTime <= 5.0f ? FLinearColor(1.0f, 0.25f, 0.18f, 1.0f) : FLinearColor::White));
		TimeText->SetFont(FCoreStyle::GetDefaultFontStyle(FName(TEXT("Bold")), 34.0f));
	}

	AJTSCharacter* const PlayerCharacter = FindPlayerCharacter();
	RefreshInventorySlots();
	RefreshInteractionPrompt();
	const UJTSRangedWeaponComponent* const CrosshairRanged = PlayerCharacter != nullptr
		? PlayerCharacter->FindComponentByClass<UJTSRangedWeaponComponent>() : nullptr;
	const bool bShowGameplayAiming = (bEarthCollection || bMoonExploration || bSpaceWorldSurfaceActive)
		&& !bGameMenuOpen
		&& PlayerCharacter != nullptr
		&& !PlayerCharacter->IsBoarded()
		&& IsValid(CrosshairRanged)
		&& CrosshairRanged->IsAiming();
	ApplyLayerVisibility(CrosshairText, bShowGameplayAiming);
	if (CrosshairText != nullptr)
	{
		const UJTSRangedWeaponComponent* const Ranged = PlayerCharacter != nullptr
			? PlayerCharacter->FindComponentByClass<UJTSRangedWeaponComponent>() : nullptr;
		const bool bEquippedRanged = IsValid(Ranged) && Ranged->HasActiveRangedWeapon();
		const float ReticleKick = bEquippedRanged ? Ranged->GetReticleKickAlpha() : 0.0f;
		CrosshairText->SetRenderScale(FVector2D(1.0f + 0.22f * ReticleKick));
		CrosshairText->SetColorAndOpacity(FSlateColor(bEquippedRanged && Ranged->HasRecentConfirmedHit()
			? FLinearColor(1.0f, 0.56f, 0.24f) : FLinearColor::White));
	}
	// The quickbar and contextual interaction prompt already teach surface actions. A second
	// persistent help line competes with the quickbar at narrower viewport sizes.
	ApplyLayerVisibility(GameplayHelpText, false);

	AJTSSpacecraftActor* const Spacecraft = FindSpacecraft();
	BindSpacecraftResources();
	RefreshSpacecraftNavigation(Spacecraft);

	RefreshFuelToMoonHud();

	if (AvatarBlock != nullptr)
	{
		if (const AJTSPlayerState* const PlayerState = GetOwningPlayer() != nullptr ? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr)
		{
			AvatarBlock->SetBrushColor(PlayerState->GetAvatarLinearColor());
		}
		else if (const UJTSGameInstance* const GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance<UJTSGameInstance>() : nullptr)
		{
			AvatarBlock->SetBrushColor(GameInstance->GetSelectedAvatarLinearColor());
		}
	}
	if (RocketIconCanvas != nullptr)
	{
		RocketIconCanvas->SetVisibility(PlayerCharacter != nullptr && PlayerCharacter->IsBoarded() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UJTSPrototypeHUDWidget::RefreshEarthCollectionDurationText()
{
	if (StartRulesText == nullptr)
	{
		return;
	}

	const AJTSGameState* const GameState = BoundGameState.Get();
	if (!IsValid(GameState) || !GameState->IsEarthCollectionActive())
	{
		StartRulesText->SetText(FText::FromString(TEXT("TIME LIMITED\nTWO HANDS\nONE TERRIBLE PLAN")));
		return;
	}

	const int32 DisplayedDuration = FMath::Max(0, FMath::CeilToInt(GameState->GetEarthCollectionRemainingTime()));
	StartRulesText->SetText(FText::FromString(FString::Printf(
		TEXT("%d SECONDS\nTWO HANDS\nONE TERRIBLE PLAN"),
		DisplayedDuration)));
}

void UJTSPrototypeHUDWidget::RefreshFuelToMoonHud()
{
	const bool bEarthCollectionActive = BoundGameState.IsValid() && BoundGameState->IsEarthCollectionActive();
	const float RequiredFuel = BoundGameState.IsValid()
		? BoundGameState->GetEarthLaunchFuelRequirement()
		: 0.0f;
	const bool bShowFuelPanel = bEarthCollectionActive;
	ApplyLayerVisibility(FuelToMoonPanel, bShowFuelPanel);
	if (!bShowFuelPanel)
	{
		return;
	}

	const AJTSSpacecraftActor* const Spacecraft = FindSpacecraft();
	const int32 CurrentFuel = IsValid(Spacecraft) ? Spacecraft->GetFuelCount() : 0;
	const float Progress = RequiredFuel > 0.0f
		? FMath::Clamp(static_cast<float>(CurrentFuel) / RequiredFuel, 0.0f, 1.0f)
		: 1.0f;
	const bool bHasEnoughFuel = static_cast<float>(CurrentFuel) >= RequiredFuel;
	const FLinearColor StatusColor = bHasEnoughFuel
		? FLinearColor(0.18f, 0.90f, 0.63f, 1.0f)
		: FLinearColor(1.0f, 0.48f, 0.12f, 1.0f);
	const float RoundedRequiredFuel = FMath::RoundToFloat(RequiredFuel);
	const FString RequiredFuelText = FMath::IsNearlyEqual(RequiredFuel, RoundedRequiredFuel)
		? FString::FromInt(FMath::RoundToInt(RequiredFuel))
		: FString::SanitizeFloat(RequiredFuel);

	if (FuelProgressBar != nullptr)
	{
		FuelProgressBar->SetPercent(Progress);
		FuelProgressBar->SetFillColorAndOpacity(StatusColor);
	}
	if (FuelAmountText != nullptr)
	{
		FuelAmountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %s"), CurrentFuel, *RequiredFuelText)));
	}
	if (FuelStatusText != nullptr)
	{
		FuelStatusText->SetText(FText::FromString(bHasEnoughFuel ? TEXT("READY") : TEXT("NEED FUEL")));
		FuelStatusText->SetColorAndOpacity(FSlateColor(StatusColor));
	}
}

void UJTSPrototypeHUDWidget::RefreshInventorySlots()
{
	if (InventorySlotTexts.IsEmpty())
	{
		return;
	}

	const AJTSCharacter* const PlayerCharacter = FindPlayerCharacter();
	const UJTSInventoryComponent* const Inventory = PlayerCharacter != nullptr
		? PlayerCharacter->GetInventoryComponent()
		: nullptr;
	const int32 InventoryCapacity = FMath::Max(1, IsValid(Inventory) ? Inventory->GetInventoryCapacity() : 1);
	const int32 PageCount = IsValid(Inventory)
		? Inventory->GetQuickbarPageCount()
		: 1;
	const int32 PageIndex = IsValid(Inventory)
		? Inventory->GetQuickbarPageIndex()
		: 0;
	const int32 PageStart = IsValid(Inventory)
		? Inventory->GetQuickbarPageStart()
		: 0;
	const int32 VisibleSlotCount = FMath::Clamp(
		InventoryCapacity - PageStart,
		0,
		UJTSInventoryComponent::MaximumQuickbarSlots);
	const int32 SelectedSlotIndex = IsValid(Inventory)
		? Inventory->GetSelectedQuickbarSlot()
		: INDEX_NONE;
	const TArray<FJTSItemInstance>* const Items = IsValid(Inventory)
		? &Inventory->GetItemSlots()
		: nullptr;

	int32 ViewportWidth = 1280;
	int32 ViewportHeight = 720;
	if (APlayerController* const PlayerController = GetOwningPlayer())
	{
		PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	}
	const float PanelWidth = FMath::Clamp(static_cast<float>(ViewportWidth) * 0.78f, 360.0f, 920.0f);
	const float SlotGap = 4.0f;
	const float SlotWidth = (PanelWidth - 16.0f
		- static_cast<float>(UJTSInventoryComponent::MaximumQuickbarSlots - 1) * SlotGap)
		/ static_cast<float>(UJTSInventoryComponent::MaximumQuickbarSlots);
	const float SlotHeight = 62.0f;
	const float SlotRowWidth = static_cast<float>(VisibleSlotCount) * SlotWidth
		+ static_cast<float>(FMath::Max(0, VisibleSlotCount - 1)) * SlotGap;
	const float FirstSlotX = (PanelWidth - SlotRowWidth) * 0.5f;

	if (InventoryPanelSlot != nullptr)
	{
		InventoryPanelSlot->SetSize(FVector2D(PanelWidth, 94.0f));
	}
	if (InventoryTitleText != nullptr)
	{
		InventoryTitleText->SetText(FText::FromString(FString::Printf(
			TEXT("QUICKBAR %d/%d  ·  UP/DOWN PAGE  ·  G DROP  ·  HOLD G DESTROY"),
			PageIndex + 1,
			PageCount)));
	}

	for (int32 VisualSlotIndex = 0; VisualSlotIndex < InventorySlotTexts.Num(); ++VisualSlotIndex)
	{
		const bool bSlotVisible = VisualSlotIndex < VisibleSlotCount;
		const int32 InventorySlotIndex = PageStart + VisualSlotIndex;
		const bool bSelected = bSlotVisible && InventorySlotIndex == SelectedSlotIndex;
		if (InventorySlotBorders.IsValidIndex(VisualSlotIndex) && InventorySlotBorders[VisualSlotIndex] != nullptr)
		{
			InventorySlotBorders[VisualSlotIndex]->SetVisibility(bSlotVisible
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
			InventorySlotBorders[VisualSlotIndex]->SetBrushColor(bSelected
				? FLinearColor(0.12f, 0.56f, 0.67f, 0.99f)
				: FLinearColor(0.08f, 0.15f, 0.22f, 0.96f));
			InventorySlotBorders[VisualSlotIndex]->SetPadding(bSelected ? 1.5f : 3.0f);
			if (UCanvasPanelSlot* const LayoutSlot = Cast<UCanvasPanelSlot>(InventorySlotBorders[VisualSlotIndex]->Slot))
			{
				LayoutSlot->SetPosition(FVector2D(
					FirstSlotX + static_cast<float>(VisualSlotIndex) * (SlotWidth + SlotGap),
					25.0f));
				LayoutSlot->SetSize(FVector2D(SlotWidth, SlotHeight));
			}
		}

		if (InventorySlotTexts.IsValidIndex(VisualSlotIndex) && InventorySlotTexts[VisualSlotIndex] != nullptr)
		{
			const FJTSItemInstance Item = bSlotVisible && Items != nullptr && Items->IsValidIndex(InventorySlotIndex)
				? (*Items)[InventorySlotIndex]
				: FJTSItemInstance();
			FString SlotLabel = Item.IsEmpty()
				? TEXT("EMPTY")
				: UJTSItemDefinitionLibrary::GetItemDisplayName(Item.ItemId).ToString().ToUpper();
			if (!Item.IsEmpty() && Item.StackCount > 1)
			{
				SlotLabel += FString::Printf(TEXT(" x%d"), Item.StackCount);
			}
			InventorySlotTexts[VisualSlotIndex]->SetText(FText::FromString(
				FString::Printf(TEXT("[%d] %s"), VisualSlotIndex + 1, *SlotLabel)));
			InventorySlotTexts[VisualSlotIndex]->SetFont(FCoreStyle::GetDefaultFontStyle(
				FName(TEXT("Bold")),
				SlotWidth < 64.0f ? 10.0f : 12.0f));
			InventorySlotTexts[VisualSlotIndex]->SetColorAndOpacity(FSlateColor(bSelected
				? FLinearColor(0.88f, 1.0f, 1.0f, 1.0f)
				: FLinearColor(0.78f, 0.84f, 0.90f, 1.0f)));
		}
	}
}

bool UJTSPrototypeHUDWidget::ProjectWorldToViewportWidget(
	const FVector& WorldLocation,
	FVector2D& OutWidgetPosition) const
{
	OutWidgetPosition = FVector2D::ZeroVector;
	// ProjectWorldLocationToWidgetPosition returns coordinates in the owning viewport widget's
	// Slate-local space. These can be written directly to our RootCanvas slots; converting them
	// a second time with a viewport or DPI offset caused the shared prompt/marker displacement.
	APlayerController* const PlayerController = GetOwningPlayer();
	return IsValid(PlayerController)
		&& UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			PlayerController,
			WorldLocation,
			OutWidgetPosition,
			true);
}

FVector2D UJTSPrototypeHUDWidget::GetViewportWidgetLocalSize() const
{
	// bPlayerViewportRelative=true above is paired with PlayerScreen geometry here.  Keeping
	// projection and screen bounds in this one Slate-local space avoids mixing viewport-absolute
	// pixels with canvas coordinates (and remains correct if a player viewport has an offset).
	APlayerController* const PlayerController = GetOwningPlayer();
	FGeometry PlayerScreenGeometry = IsValid(PlayerController)
		? UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(PlayerController)
		: UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	FVector2D LocalSize = PlayerScreenGeometry.GetLocalSize();
	if (LocalSize.X <= KINDA_SMALL_NUMBER || LocalSize.Y <= KINDA_SMALL_NUMBER)
	{
		LocalSize = GetCachedGeometry().GetLocalSize();
	}
	return FVector2D(FMath::Max(1.0f, LocalSize.X), FMath::Max(1.0f, LocalSize.Y));
}

void UJTSPrototypeHUDWidget::RefreshInteractionPrompt()
{
	FText PromptText;
	FText TargetName;
	FVector PromptAnchor = FVector::ZeroVector;
	bool bHasPromptAnchor = false;
	bool bUseCenteredPrompt = false;
	const AJTSPlayerController* const OwningController = Cast<AJTSPlayerController>(GetOwningPlayer());
	if (!bGameMenuOpen && (!IsValid(OwningController) || !OwningController->IsSpaceShopOpen()))
	{
		AJTSSpacecraftActor* const DrivenSpacecraft = Cast<AJTSSpacecraftActor>(
			GetOwningPlayer() != nullptr ? GetOwningPlayer()->GetPawn() : nullptr);
		if (IsValid(DrivenSpacecraft) && DrivenSpacecraft->IsLanded())
		{
			TargetName = FText::FromString(TEXT("SPACECRAFT LANDED"));
			PromptText = FText::FromString(TEXT("[F] DISEMBARK"));
			bUseCenteredPrompt = true;
		}
		else if (AJTSCharacter* const PlayerCharacter = FindPlayerCharacter())
		{
			if (AJTSSpacecraftActor* const BoardedSpacecraft = PlayerCharacter->GetBoardedSpacecraft();
				IsValid(BoardedSpacecraft) && BoardedSpacecraft->CanDisembarkPlayer(PlayerCharacter))
			{
				TargetName = FText::FromString(TEXT("SPACECRAFT LANDED"));
				PromptText = FText::FromString(TEXT("[F] DISEMBARK"));
				bUseCenteredPrompt = true;
			}
			else if (!PlayerCharacter->IsBoarded())
			{
				// Combat owns its own context prompt, but shares the same world-to-widget projection path
				// as normal interaction targets. This keeps LMB feedback separate from [E] interactions.
				if (const UJTSMeleeComponent* const MeleeComponent = PlayerCharacter->FindComponentByClass<UJTSMeleeComponent>())
				{
					if (AActor* const MeleeTarget = MeleeComponent->GetCurrentMeleeTarget())
					{
						PromptText = IJTSMeleeTarget::Execute_GetMeleeTargetPrompt(MeleeTarget, PlayerCharacter);
						TargetName = IJTSMeleeTarget::Execute_GetMeleeTargetDisplayName(MeleeTarget);
						PromptAnchor = IJTSMeleeTarget::Execute_GetMeleeTargetAnchorWorldLocation(MeleeTarget);
						bHasPromptAnchor = !PromptText.IsEmpty() && !TargetName.IsEmpty();
					}
				}

				if (!bHasPromptAnchor)
				{
					const UInteractionComponent* const InteractionComponent = PlayerCharacter->FindComponentByClass<UInteractionComponent>();
					if (InteractionComponent != nullptr)
					{
						if (AActor* const Target = InteractionComponent->GetCurrentInteractable())
						{
							PromptText = InteractionComponent->GetCurrentInteractionPrompt();
							if (const AJTSWorldPickupActor* const Pickup = Cast<AJTSWorldPickupActor>(Target))
							{
								TargetName = Pickup->GetItemDisplayName();
								PromptAnchor = Pickup->GetInteractionAnchorWorldLocation();
								bHasPromptAnchor = true;
							}
							else if (const AJTSResourcePickupActor* const ResourcePickup = Cast<AJTSResourcePickupActor>(Target))
							{
								TargetName = ResourcePickup->GetInteractionDisplayName();
								PromptAnchor = ResourcePickup->GetInteractionAnchorWorldLocation();
								bHasPromptAnchor = true;
							}
							else if (const AJTSMoonResourceActor* const Resource = Cast<AJTSMoonResourceActor>(Target))
							{
								TargetName = Resource->GetInteractionDisplayName();
								PromptAnchor = Resource->GetInteractionAnchorWorldLocation();
								bHasPromptAnchor = true;
							}
							else if (const AJTSSpacecraftActor* const Spacecraft = Cast<AJTSSpacecraftActor>(Target))
							{
								TargetName = FText::FromString(TEXT("SPACECRAFT"));
								PromptAnchor = Spacecraft->GetNavigationMarkerWorldLocation();
								bHasPromptAnchor = true;
							}
						}
					}

					// The ship's pawn-only boarding trigger is authoritative for nearby-ship state. It is
					// intentionally independent from generic object-overlap discovery, but must still pass
					// the shared camera-cone and Visibility policy before receiving a prompt.
					if (!bHasPromptAnchor && InteractionComponent != nullptr)
					{
						if (AJTSSpacecraftActor* const NearbySpacecraft = PlayerCharacter->GetNearbySpacecraft();
							IsValid(NearbySpacecraft) && InteractionComponent->IsInteractableInView(NearbySpacecraft))
						{
							PromptText = IInteractable::Execute_GetInteractionPrompt(NearbySpacecraft, PlayerCharacter);
							TargetName = FText::FromString(TEXT("SPACECRAFT"));
							PromptAnchor = NearbySpacecraft->GetNavigationMarkerWorldLocation();
							bHasPromptAnchor = !PromptText.IsEmpty();
						}
					}
				}
			}
		}
	}

	FVector2D PromptScreenPosition = FVector2D::ZeroVector;
	const bool bProjected = bHasPromptAnchor && ProjectWorldToViewportWidget(PromptAnchor, PromptScreenPosition);
	const bool bShowPrompt = !PromptText.IsEmpty() && !TargetName.IsEmpty() && (bUseCenteredPrompt || bProjected);
	if (InteractionPromptText != nullptr)
	{
		InteractionPromptText->SetText(bShowPrompt
			? FText::FromString(FString::Printf(TEXT("%s\n%s"), *TargetName.ToString(), *PromptText.ToString()))
			: FText::GetEmpty());
		InteractionPromptText->SetVisibility(bShowPrompt ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (InteractionPromptSlot != nullptr)
	{
		if (bUseCenteredPrompt)
		{
			InteractionPromptSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			InteractionPromptSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			InteractionPromptSlot->SetPosition(FVector2D(0.0f, 76.0f));
			InteractionPromptSlot->SetSize(FVector2D(360.0f, 64.0f));
		}
		else
		{
			InteractionPromptSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			InteractionPromptSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			InteractionPromptSlot->SetSize(FVector2D(280.0f, 54.0f));
			if (bShowPrompt)
			{
				InteractionPromptSlot->SetPosition(PromptScreenPosition);
			}
		}
	}
}

void UJTSPrototypeHUDWidget::RefreshSpacecraftNavigation(AJTSSpacecraftActor* Spacecraft)
{
	const AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this);
	const IJTSMoonSurfaceGameplaySettings* const MoonSettings = IsValid(SurfaceController)
		? SurfaceController->GetMoonSettings()
		: nullptr;
	AJTSCharacter* const PlayerCharacter = FindPlayerCharacter();
	APlayerController* const PlayerController = GetOwningPlayer();
	if (!IsValid(SurfaceController)
		|| !SurfaceController->IsSurfaceGameplayInitialized()
		|| MoonSettings == nullptr
		|| !IsValid(Spacecraft)
		|| !SurfaceController->OwnsSurfaceActor(Spacecraft)
		|| !IsValid(PlayerCharacter)
		|| !SurfaceController->OwnsSurfaceActor(PlayerCharacter)
		|| !IsValid(PlayerController)
		|| bGameMenuOpen
		|| PlayerCharacter->IsBoarded()
		|| (GetWorld() != nullptr && GetWorld()->IsPaused()))
	{
		SetSpacecraftNavigationVisibility(false, false);
		return;
	}

	AJTSPlanetAnchor* const Planet = SurfaceController->GetOwningPlanet();
	if (!IsValid(Planet))
	{
		SetSpacecraftNavigationVisibility(false, false);
		return;
	}

	const FVector PlayerLocation = PlayerCharacter->GetActorLocation();
	const FVector ShipLocation = Spacecraft->GetActorLocation();
	const float SurfaceDistance = Planet->ApproximateSurfaceArcDistance(PlayerLocation, ShipLocation);
	if (SurfaceDistance < MoonSettings->GetSpacecraftMarkerShowDistance())
	{
		SetSpacecraftNavigationVisibility(false, false);
		return;
	}

	const FVector NavigationAnchor = Spacecraft->GetNavigationMarkerWorldLocation();
	const int32 DistanceMeters = FMath::Max(0, FMath::RoundToInt(SurfaceDistance / 100.0f));

	const FVector2D ViewportSize = GetViewportWidgetLocalSize();
	if (ViewportSize.X <= 1.0f || ViewportSize.Y <= 1.0f)
	{
		SetSpacecraftNavigationVisibility(false, false);
		return;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);
	const FRotationMatrix CameraMatrix(CameraRotation);
	const FVector ToNavigationAnchor = NavigationAnchor - CameraLocation;
	const float ForwardDot = FVector::DotProduct(ToNavigationAnchor.GetSafeNormal(), CameraMatrix.GetUnitAxis(EAxis::X));
	FVector2D ProjectedLocation;
	const bool bProjected = ProjectWorldToViewportWidget(NavigationAnchor, ProjectedLocation);
	const float BaseSafeInset = MoonSettings->GetSpacecraftMarkerScreenSafeMargin();
	const float HysteresisInset = bSpacecraftWasOnScreen ? -12.0f : 12.0f;
	const float SafeInset = FMath::Max(16.0f, BaseSafeInset + HysteresisInset);
	const bool bOnScreen = bProjected
		&& ForwardDot > 0.0f
		&& ProjectedLocation.X >= SafeInset
		&& ProjectedLocation.X <= ViewportSize.X - SafeInset
		&& ProjectedLocation.Y >= SafeInset
		&& ProjectedLocation.Y <= ViewportSize.Y - SafeInset;
	if (bOnScreen)
	{
		if (SpacecraftWorldMarkerSlot != nullptr)
		{
			SpacecraftWorldMarkerSlot->SetPosition(ProjectedLocation);
		}
		if (SpacecraftWorldMarkerText != nullptr)
		{
			SpacecraftWorldMarkerText->SetText(FText::FromString(TEXT("SHIP")));
		}
		if (SpacecraftWorldMarkerDistanceText != nullptr)
		{
			SpacecraftWorldMarkerDistanceText->SetText(FText::FromString(FString::Printf(TEXT("%dm"), DistanceMeters)));
		}
		bSpacecraftWasOnScreen = true;
		SetSpacecraftNavigationVisibility(true, false);
		return;
	}

	const FVector2D ViewportCenter = ViewportSize * 0.5f;
	const FVector SafeNavigationDirection = (NavigationAnchor - PlayerLocation).GetSafeNormal();
	FVector2D EdgeDirection(
		FVector::DotProduct(SafeNavigationDirection, CameraMatrix.GetUnitAxis(EAxis::Y)),
		-FVector::DotProduct(SafeNavigationDirection, CameraMatrix.GetUnitAxis(EAxis::Z)));
	const float NavigationForwardDot = FVector::DotProduct(SafeNavigationDirection, CameraMatrix.GetUnitAxis(EAxis::X));
	if (NavigationForwardDot <= 0.0f)
	{
		EdgeDirection.Y += FMath::Max(0.35f, -NavigationForwardDot);
	}
	if (EdgeDirection.IsNearlyZero())
	{
		EdgeDirection = FVector2D(0.0f, 1.0f);
	}
	EdgeDirection.Normalize();

	const float SafeMargin = MoonSettings->GetSpacecraftMarkerScreenSafeMargin();
	const FVector2D AvailableHalfExtent(
		FMath::Max(1.0f, ViewportCenter.X - SafeMargin - 58.0f),
		FMath::Max(1.0f, ViewportCenter.Y - SafeMargin - 181.0f));
	const float EdgeScale = 1.0f / FMath::Max(
		FMath::Abs(EdgeDirection.X) / AvailableHalfExtent.X,
		FMath::Abs(EdgeDirection.Y) / AvailableHalfExtent.Y);
	const FVector2D EdgePosition = ViewportCenter + EdgeDirection * EdgeScale;
	if (SpacecraftEdgeIndicatorSlot != nullptr)
	{
		SpacecraftEdgeIndicatorSlot->SetPosition(EdgePosition);
	}
	if (SpacecraftEdgeArrowText != nullptr)
	{
		SpacecraftEdgeArrowText->SetRenderTransformAngle(
			FMath::RadiansToDegrees(FMath::Atan2(EdgeDirection.Y, EdgeDirection.X)) + 90.0f);
	}
	if (SpacecraftEdgeDistanceText != nullptr)
	{
		SpacecraftEdgeDistanceText->SetText(FText::FromString(FString::Printf(TEXT("SHIP\n%dm"), DistanceMeters)));
	}
	bSpacecraftWasOnScreen = false;
	SetSpacecraftNavigationVisibility(false, true);
}

void UJTSPrototypeHUDWidget::SetSpacecraftNavigationVisibility(bool bShowWorldMarker, bool bShowEdgeIndicator)
{
	if (!bShowWorldMarker && !bShowEdgeIndicator)
	{
		bSpacecraftWasOnScreen = false;
	}
	if (SpacecraftWorldMarker != nullptr)
	{
		SpacecraftWorldMarker->SetVisibility(bShowWorldMarker
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (SpacecraftEdgeIndicator != nullptr)
	{
		SpacecraftEdgeIndicator->SetVisibility(bShowEdgeIndicator
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

void UJTSPrototypeHUDWidget::RefreshBoardingProgress()
{
	AJTSCharacter* const PlayerCharacter = FindPlayerCharacter();
	const AJTSSpaceWorldManager* const SpaceWorldManager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
	const bool bSpaceWorldSurfaceActive = IsValid(SpaceWorldManager) && SpaceWorldManager->IsSurfaceGameplayReady();
	const bool bEarthCollectionActive = BoundGameState.IsValid() && BoundGameState->IsEarthCollectionActive();
	const bool bShowProgress = (bEarthCollectionActive || bSpaceWorldSurfaceActive)
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
		ResultSubtitleText->SetText(FText::FromString(bSuccess ? TEXT("COURSE SET FOR THE MOON") : *Subtitle));
	}
	if (ResultDetailText != nullptr)
	{
		ResultDetailText->SetText(FText::FromString(bSuccess ? TEXT("ARRIVING...") : *Detail));
		ResultDetailText->SetColorAndOpacity(FSlateColor(bSuccess ? FLinearColor(0.75f, 0.85f, 1.0f, 1.0f) : FLinearColor(1.0f, 0.75f, 0.70f, 1.0f)));
	}

	const ESlateVisibility ResultButtonVisibility = bSuccess ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
	if (RestartButton != nullptr)
	{
		RestartButton->SetIsEnabled(!bSuccess);
		RestartButton->SetVisibility(ResultButtonVisibility);
	}
	if (QuitButton != nullptr)
	{
		QuitButton->SetIsEnabled(!bSuccess);
		QuitButton->SetVisibility(ResultButtonVisibility);
	}
}

void UJTSPrototypeHUDWidget::RefreshAvatarSelection()
{
	EJTSAvatarColor SelectedColor = EJTSAvatarColor::Blue;
	FLinearColor PreviewColor = FLinearColor(0.10f, 0.45f, 1.0f, 1.0f);
	if (const AJTSPlayerState* const PlayerState = GetOwningPlayer() != nullptr ? GetOwningPlayer()->GetPlayerState<AJTSPlayerState>() : nullptr)
	{
		SelectedColor = PlayerState->GetAvatarColor();
		PreviewColor = PlayerState->GetAvatarLinearColor();
	}
	else if (const UJTSGameInstance* const GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance<UJTSGameInstance>() : nullptr)
	{
		SelectedColor = GameInstance->GetSelectedAvatarColor();
		PreviewColor = GameInstance->GetSelectedAvatarLinearColor();
	}
	if (SettingsPreviewBlock != nullptr)
	{
		SettingsPreviewBlock->SetBrushColor(PreviewColor);
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
	if (PurpleAvatarButton != nullptr)
	{
		PurpleAvatarButton->SetBackgroundColor(SelectedColor == EJTSAvatarColor::Purple ? SelectedButtonColor : UnselectedButtonColor);
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
	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		PlayerController->RequestAvatarColor(EJTSAvatarColor::Blue);
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
	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		PlayerController->RequestAvatarColor(EJTSAvatarColor::Orange);
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
	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		PlayerController->RequestAvatarColor(EJTSAvatarColor::Green);
	}
	RefreshAvatarSelection();
	RefreshGameplayHud();
}

void UJTSPrototypeHUDWidget::HandlePurpleAvatarClicked()
{
	if (UJTSGameInstance* const GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance<UJTSGameInstance>() : nullptr)
	{
		GameInstance->SetSelectedAvatarColor(EJTSAvatarColor::Purple);
	}
	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		PlayerController->RequestAvatarColor(EJTSAvatarColor::Purple);
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

void UJTSPrototypeHUDWidget::HandleResumeGameClicked()
{
	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		PlayerController->CloseGameMenu();
	}
}

void UJTSPrototypeHUDWidget::HandleGameMenuSessionDetailsClicked()
{
	if (GameMenuInfoText == nullptr)
	{
		return;
	}

	const UJTSOnlineSessionSubsystem* const Online = GetGameInstance() != nullptr
		? GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>()
		: nullptr;
	const AJTSGameState* const GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
	const int32 PlayerCount = GameState != nullptr ? GameState->PlayerArray.Num() : 0;
	GameMenuInfoText->SetText(FText::FromString(FString::Printf(
		TEXT("Join Code: %s\\nPlayers: %d/%d\\n%s"),
		Online != nullptr ? *Online->GetCurrentJoinCode() : TEXT("Unavailable"),
		PlayerCount,
		Online != nullptr ? Online->GetCurrentMaximumPlayers() : 4,
		Online != nullptr && Online->IsCurrentSessionPasswordProtected() ? TEXT("Password protected") : TEXT("No password"))));
	ApplyLayerVisibility(GameMenuInfoText, true);
}

void UJTSPrototypeHUDWidget::HandleGameMenuSettingsClicked()
{
	if (GameMenuInfoText == nullptr)
	{
		return;
	}

	UJTSVoiceSubsystem* const Voice = GetGameInstance() != nullptr
		? GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>()
		: nullptr;
	if (Voice != nullptr)
	{
		Voice->InitializeVoice();
	}
	GameMenuInfoText->SetText(FText::FromString(Voice != nullptr && Voice->IsVoiceAvailable()
		? TEXT("Voice is available. Device selection and input/output levels are available from Front End Settings.")
		: TEXT("Voice is unavailable or connecting. Device selection is available from Front End Settings.")));
	ApplyLayerVisibility(GameMenuInfoText, true);
}

void UJTSPrototypeHUDWidget::HandleReturnToMainMenuClicked()
{
	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		PlayerController->ReturnToMainMenu();
	}
}

void UJTSPrototypeHUDWidget::HandleGameMenuQuitClicked()
{
	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		PlayerController->QuitGame();
	}
}

AJTSSpacecraftActor* UJTSPrototypeHUDWidget::FindSpacecraft() const
{
	if (CachedSpacecraft.IsValid())
	{
		return CachedSpacecraft.Get();
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}
	if (AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this))
	{
		if (AJTSSpacecraftActor* const MoonSpacecraft = SurfaceController->GetSpacecraft())
		{
			CachedSpacecraft = MoonSpacecraft;
			return MoonSpacecraft;
		}
		return nullptr;
	}

	if (World->PersistentLevel != nullptr)
	{
		for (AActor* const Actor : World->PersistentLevel->Actors)
		{
			if (AJTSSpacecraftActor* const Spacecraft = Cast<AJTSSpacecraftActor>(Actor); IsValid(Spacecraft))
			{
				CachedSpacecraft = Spacecraft;
				return CachedSpacecraft.Get();
			}
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

	case EJTSResourceType::Rock:
		return TEXT("Rock");

	case EJTSResourceType::Ore:
		return TEXT("Ore");

	case EJTSResourceType::Organic:
		return TEXT("Organic");

	case EJTSResourceType::MoonAntCorpse:
		return TEXT("MoonAnt Corpse");

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
