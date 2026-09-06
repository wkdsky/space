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
#include "Math/RotationMatrix.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "space/Components/JTSCarryComponent.h"
#include "space/Core/JTSGameInstance.h"
#include "space/Interaction/InteractionComponent.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Modes/JTSEarthGameMode.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Systems/JTSMoonWrapSubsystem.h"
#include "space/UI/JTSCircularProgressWidget.h"
#include "space/World/JTSMoonResourceActor.h"

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
	if (!IsValid(Player) || !BoundGameState.IsValid() || !BoundGameState->IsMoonExploration() || MoonShopPanel == nullptr)
	{
		return false;
	}

	AJTSSpacecraftActor* const Spacecraft = Player->GetNearbySpacecraft();
	if (!IsValid(Spacecraft) || !Spacecraft->IsPawnInBoardingRange(Player))
	{
		return false;
	}

	ShopPlayer = Player;
	ShopSpacecraft = Spacecraft;
	bMoonShopOpen = true;
	bWorkshopEquipmentTab = false;
	ApplyLayerVisibility(MoonShopPanel, true);
	RefreshWorkshopTabs();
	RefreshMoonShop();
	RefreshGameplayHud();
	UE_LOG(
		LogTemp,
		Log,
		TEXT("JumpToSpace Shop Open: Player=%s ShipRock=%d"),
		*Player->GetName(),
		Spacecraft->GetResourceAmount(EJTSResourceType::Rock));
	return true;
}

void UJTSPrototypeHUDWidget::CloseMoonShop()
{
	bMoonShopOpen = false;
	ShopPlayer.Reset();
	ShopSpacecraft.Reset();
	ApplyLayerVisibility(MoonShopPanel, false);
	RefreshGameplayHud();
}

bool UJTSPrototypeHUDWidget::IsMoonShopOpen() const
{
	return bMoonShopOpen;
}

void UJTSPrototypeHUDWidget::OpenGameMenu()
{
	bGameMenuOpen = true;
	ApplyLayerVisibility(PauseMenuLayer, true);
	RefreshGameplayHud();
}

void UJTSPrototypeHUDWidget::CloseGameMenu()
{
	bGameMenuOpen = false;
	ApplyLayerVisibility(PauseMenuLayer, false);
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

	BuildWidgetTree();
	BindGameState();
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
	RefreshWorkshopLayout();

	if (!BoundGameState.IsValid())
	{
		BindGameState();
	}

	const bool bEarthCollectionActive = BoundGameState.IsValid() && BoundGameState->IsEarthCollectionActive();
	const bool bMoonExplorationActive = BoundGameState.IsValid() && BoundGameState->IsMoonExploration();
	if (bEarthCollectionActive || bMoonExplorationActive)
	{
		RefreshGameplayHud();
		if (bMoonShopOpen)
		{
			AJTSCharacter* const ShopCharacter = ShopPlayer.Get();
			AJTSSpacecraftActor* const ShopSpacecraftActor = ShopSpacecraft.Get();
			if (!bMoonExplorationActive
				|| !IsValid(ShopCharacter)
				|| !IsValid(ShopSpacecraftActor)
				|| !ShopSpacecraftActor->IsPawnInBoardingRange(ShopCharacter))
			{
				if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetOwningPlayer()))
				{
					PlayerController->CloseMoonShop();
				}
				else
				{
					CloseMoonShop();
				}
			}
			else
			{
				RefreshMoonShop();
			}
		}
		if (bEarthCollectionActive)
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
		TimeText = MakeTextBlock(WidgetTree, TEXT("TimeText"), TEXT("TIME: 00.00"), 34.0f, FLinearColor::White, ETextJustify::Center);
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

		InventoryPanel = MakeBorder(WidgetTree, TEXT("InventoryPanel"), FLinearColor(0.015f, 0.035f, 0.070f, 0.93f), 6.0f);
		InventoryPanelSlot = AddCanvasChild(GameplayLayer, InventoryPanel, FAnchors(0.5f, 1.0f), FVector2D(0.0f, -24.0f), FVector2D(204.0f, 108.0f), FVector2D(0.5f, 1.0f));
		UCanvasPanel* const InventoryCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InventoryCanvas"));
		if (InventoryPanel != nullptr && InventoryCanvas != nullptr)
		{
			InventoryPanel->SetContent(InventoryCanvas);
			AddCanvasChild(InventoryCanvas, MakeTextBlock(WidgetTree, TEXT("InventoryTitle"), TEXT("INVENTORY"), 17.0f, FLinearColor(0.78f, 0.92f, 1.0f, 1.0f), ETextJustify::Center), FAnchors(0.5f, 0.0f), FVector2D(0.0f, 2.0f), FVector2D(200.0f, 24.0f), FVector2D(0.5f, 0.0f));
		for (int32 SlotIndex = 0; SlotIndex < 7; ++SlotIndex)
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
			AddCanvasChild(InventoryCanvas, SlotBorder, FAnchors(0.0f, 0.0f), FVector2D(8.0f + 94.0f * SlotIndex, 29.0f), FVector2D(90.0f, 70.0f));
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
			FVector2D(150.0f, 78.0f),
			FVector2D(0.5f, 0.5f));
		if (SpacecraftWorldMarker != nullptr)
		{
			const FLinearColor MarkerColor(0.22f, 0.91f, 0.90f, 1.0f);
			SpacecraftWorldMarkerText = MakeTextBlock(WidgetTree, TEXT("SpacecraftWorldMarkerText"), TEXT("SHIP"), 19.0f, MarkerColor, ETextJustify::Center);
			SpacecraftWorldMarkerDistanceText = MakeTextBlock(WidgetTree, TEXT("SpacecraftWorldMarkerDistanceText"), TEXT("0m"), 14.0f, MarkerColor, ETextJustify::Center);
			SpacecraftWorldMarkerArrowText = MakeTextBlock(WidgetTree, TEXT("SpacecraftWorldMarkerArrowText"), TEXT("v"), 25.0f, MarkerColor, ETextJustify::Center);
			AddCanvasChild(SpacecraftWorldMarker, SpacecraftWorldMarkerText, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(140.0f, 26.0f), FVector2D(0.5f, 0.0f));
			AddCanvasChild(SpacecraftWorldMarker, SpacecraftWorldMarkerDistanceText, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 25.0f), FVector2D(140.0f, 21.0f), FVector2D(0.5f, 0.0f));
			AddCanvasChild(SpacecraftWorldMarker, SpacecraftWorldMarkerArrowText, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 44.0f), FVector2D(42.0f, 28.0f), FVector2D(0.5f, 0.0f));
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

		EquipmentPanel = MakeBorder(WidgetTree, TEXT("EquipmentPanel"), FLinearColor(0.040f, 0.055f, 0.10f, 0.94f), 6.0f);
		AddCanvasChild(GameplayLayer, EquipmentPanel, FAnchors(1.0f, 0.0f), FVector2D(-28.0f, 266.0f), FVector2D(210.0f, 180.0f), FVector2D(1.0f, 0.0f));
		UCanvasPanel* const EquipmentCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EquipmentCanvas"));
		if (EquipmentPanel != nullptr && EquipmentCanvas != nullptr)
		{
			EquipmentPanel->SetContent(EquipmentCanvas);
			AddCanvasChild(EquipmentCanvas, MakeTextBlock(WidgetTree, TEXT("EquipmentTitle"), TEXT("EQUIPMENT"), 16.0f, FLinearColor(0.94f, 0.85f, 0.55f, 1.0f), ETextJustify::Center), FAnchors(0.5f, 0.0f), FVector2D(0.0f, 2.0f), FVector2D(190.0f, 23.0f), FVector2D(0.5f, 0.0f));
			for (int32 SlotIndex = 0; SlotIndex < 4; ++SlotIndex)
			{
				const int32 Column = SlotIndex % 2;
				const int32 Row = SlotIndex / 2;
				UBorder* const SlotBorder = MakeBorder(
					WidgetTree,
					*FString::Printf(TEXT("EquipmentSlot%d"), SlotIndex),
					FLinearColor(0.12f, 0.105f, 0.06f, 0.96f),
					3.0f);
				UCanvasPanel* const SlotCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
					UCanvasPanel::StaticClass(),
					*FString::Printf(TEXT("EquipmentSlot%dCanvas"), SlotIndex));
				UTextBlock* const SlotKeyText = MakeTextBlock(
					WidgetTree,
					*FString::Printf(TEXT("EquipmentSlot%dKey"), SlotIndex),
					FString::FromInt(SlotIndex + 1),
					10.0f,
					FLinearColor(0.96f, 0.85f, 0.48f, 1.0f),
					ETextJustify::Center);
				UTextBlock* const SlotText = MakeTextBlock(
					WidgetTree,
					*FString::Printf(TEXT("EquipmentSlot%dText"), SlotIndex),
					TEXT("EMPTY"),
					12.0f,
					FLinearColor(0.84f, 0.80f, 0.68f, 1.0f),
					ETextJustify::Center);
				if (SlotCanvas != nullptr)
				{
					AddCanvasChild(SlotCanvas, SlotKeyText, FAnchors(0.0f, 0.0f), FVector2D(2.0f, 1.0f), FVector2D(16.0f, 14.0f));
					AddCanvasChild(SlotCanvas, SlotText, FAnchors(0.5f, 0.5f), FVector2D(2.0f, 4.0f), FVector2D(80.0f, 46.0f), FVector2D(0.5f, 0.5f));
				}
				if (SlotBorder != nullptr)
				{
					SlotBorder->SetContent(SlotCanvas);
				}
				AddCanvasChild(EquipmentCanvas, SlotBorder, FAnchors(0.0f, 0.0f), FVector2D(10.0f + 97.0f * Column, 29.0f + 72.0f * Row), FVector2D(92.0f, 65.0f));
				UJTSCircularProgressWidget* const HoldProgress = WidgetTree->ConstructWidget<UJTSCircularProgressWidget>(
					UJTSCircularProgressWidget::StaticClass(),
					*FString::Printf(TEXT("EquipmentSlot%dHoldProgress"), SlotIndex));
				if (HoldProgress != nullptr)
				{
					HoldProgress->SetProgressColor(FLinearColor(0.22f, 0.91f, 0.90f, 1.0f));
					HoldProgress->SetBackgroundColor(FLinearColor(0.04f, 0.10f, 0.13f, 0.82f));
					HoldProgress->SetVisibility(ESlateVisibility::Collapsed);
				}
				AddCanvasChild(EquipmentCanvas, HoldProgress, FAnchors(0.0f, 0.0f), FVector2D(23.0f + 97.0f * Column, 34.0f + 72.0f * Row), FVector2D(65.0f, 55.0f));
				EquipmentSlotBorders.Add(SlotBorder);
				EquipmentSlotTexts.Add(SlotText);
				EquipmentSlotKeyTexts.Add(SlotKeyText);
				EquipmentHoldProgressWidgets.Add(HoldProgress);
			}
		}

		UBorder* const ShipResourcesPanel = MakeBorder(WidgetTree, TEXT("ShipResourcesPanel"), FLinearColor(0.02f, 0.03f, 0.07f, 0.88f), 14.0f);
		AddCanvasChild(GameplayLayer, ShipResourcesPanel, FAnchors(0.0f, 0.0f), FVector2D(176.0f, 28.0f), FVector2D(280.0f, 220.0f));
		UVerticalBox* const ShipResourcesBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShipResourcesBox"));
		if (ShipResourcesPanel != nullptr && ShipResourcesBox != nullptr)
		{
			ShipResourcesPanel->SetContent(ShipResourcesBox);
			AddVerticalChild(ShipResourcesBox, MakeTextBlock(WidgetTree, TEXT("ShipResourcesHeading"), TEXT("SHIP RESOURCES"), 22.0f, FLinearColor(0.95f, 0.85f, 1.0f, 1.0f)), FMargin(0.0f, 0.0f, 0.0f, 8.0f));
			ShipResourcesText = MakeTextBlock(WidgetTree, TEXT("ShipResourcesText"), TEXT(""), 19.0f, FLinearColor::White);
			AddVerticalChild(ShipResourcesBox, ShipResourcesText, FMargin(0.0f, 0.0f));
		}

		FuelToMoonPanel = MakeBorder(WidgetTree, TEXT("FuelToMoonPanel"), FLinearColor(0.02f, 0.055f, 0.09f, 0.91f), 14.0f);
		AddCanvasChild(GameplayLayer, FuelToMoonPanel, FAnchors(1.0f, 0.5f), FVector2D(-28.0f, 0.0f), FVector2D(220.0f, 370.0f), FVector2D(1.0f, 0.5f));
		UVerticalBox* const FuelToMoonBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FuelToMoonBox"));
		if (FuelToMoonPanel != nullptr && FuelToMoonBox != nullptr)
		{
			FuelToMoonPanel->SetContent(FuelToMoonBox);
			AddVerticalChild(FuelToMoonBox, MakeTextBlock(WidgetTree, TEXT("FuelToMoonHeading"), TEXT("FUEL TO MOON"), 21.0f, FLinearColor(0.72f, 0.91f, 1.0f, 1.0f), ETextJustify::Center), FMargin(0.0f, 0.0f, 0.0f, 12.0f), HAlign_Center);

			USizeBox* const FuelProgressSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FuelProgressSize"));
			if (FuelProgressSize != nullptr)
			{
				FuelProgressSize->SetWidthOverride(52.0f);
				FuelProgressSize->SetHeightOverride(205.0f);
				FuelProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("FuelProgressBar"));
				if (FuelProgressBar != nullptr)
				{
					FuelProgressBar->SetBarFillType(EProgressBarFillType::BottomToTop);
					FuelProgressBar->SetPercent(0.0f);
					FuelProgressBar->SetFillColorAndOpacity(FLinearColor(1.0f, 0.48f, 0.12f, 1.0f));
					FuelProgressSize->SetContent(FuelProgressBar);
				}
			}
			AddVerticalChild(FuelToMoonBox, FuelProgressSize, FMargin(0.0f, 0.0f, 0.0f, 12.0f), HAlign_Center);

			FuelAmountText = MakeTextBlock(WidgetTree, TEXT("FuelAmountText"), TEXT("0 / 0"), 24.0f, FLinearColor::White, ETextJustify::Center);
			AddVerticalChild(FuelToMoonBox, FuelAmountText, FMargin(0.0f, 0.0f, 0.0f, 5.0f), HAlign_Center);
			FuelStatusText = MakeTextBlock(WidgetTree, TEXT("FuelStatusText"), TEXT("NEED FUEL"), 19.0f, FLinearColor(1.0f, 0.48f, 0.12f, 1.0f), ETextJustify::Center);
			AddVerticalChild(FuelToMoonBox, FuelStatusText, FMargin(0.0f, 0.0f), HAlign_Center);
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

		GameplayHelpText = MakeTextBlock(WidgetTree, TEXT("HelpText"), TEXT("WASD: MOVE    SHIFT: RUN"), 16.0f, FLinearColor(0.85f, 0.95f, 1.0f, 1.0f), ETextJustify::Right);
		AddCanvasChild(GameplayLayer, GameplayHelpText, FAnchors(1.0f, 1.0f), FVector2D(-28.0f, -20.0f), FVector2D(560.0f, 28.0f), FVector2D(1.0f, 1.0f));
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
		AddCanvasChild(PauseMenuLayer, PauseCard, FAnchors(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(420.0f, 330.0f), FVector2D(0.5f, 0.5f));
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

			ReturnToMainMenuButton = MakeButton(WidgetTree, TEXT("ReturnToMainMenuButton"), TEXT("RETURN TO MAIN MENU"));
			if (ReturnToMainMenuButton != nullptr)
			{
				ReturnToMainMenuButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleReturnToMainMenuClicked);
			}
			AddVerticalChild(PauseMenuBox, ReturnToMainMenuButton, FMargin(34.0f, 5.0f));

			GameMenuQuitButton = MakeButton(WidgetTree, TEXT("GameMenuQuitButton"), TEXT("QUIT GAME"));
			if (GameMenuQuitButton != nullptr)
			{
				GameMenuQuitButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleGameMenuQuitClicked);
			}
			AddVerticalChild(PauseMenuBox, GameMenuQuitButton, FMargin(34.0f, 5.0f, 34.0f, 18.0f));
		}
	}

	BuildWorkshopPanel();

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
	ApplyLayerVisibility(EquipmentPanel, false);
	ApplyLayerVisibility(MoonShopPanel, false);
	SetBoardingProgressVisible(false);
}

void UJTSPrototypeHUDWidget::BuildWorkshopPanel()
{
	if (WidgetTree == nullptr || RootCanvas == nullptr || MoonShopPanel != nullptr)
	{
		return;
	}

	MoonShopPanel = MakeBorder(WidgetTree, TEXT("MoonShopPanel"), FLinearColor(0.018f, 0.035f, 0.075f, 0.985f), 10.0f);
	MoonShopPanelSlot = AddCanvasChild(RootCanvas, MoonShopPanel, FAnchors(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(800.0f, 460.0f), FVector2D(0.5f, 0.5f));
	UCanvasPanel* const WorkshopCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WorkshopCanvas"));
	if (MoonShopPanel == nullptr || WorkshopCanvas == nullptr)
	{
		return;
	}
	MoonShopPanel->SetContent(WorkshopCanvas);

	AddCanvasChild(
		WorkshopCanvas,
		MakeTextBlock(WidgetTree, TEXT("MoonShopTitle"), TEXT("SHIP WORKSHOP"), 30.0f, FLinearColor(0.72f, 0.91f, 1.0f, 1.0f), ETextJustify::Center),
		FAnchors(0.5f, 0.0f), FVector2D(0.0f, 12.0f), FVector2D(520.0f, 42.0f), FVector2D(0.5f, 0.0f));

	ShopToolsTabButton = MakeButton(WidgetTree, TEXT("WorkshopToolsTab"), TEXT("TOOLS"));
	if (ShopToolsTabButton != nullptr)
	{
		ShopToolsTabButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleWorkshopToolsTabClicked);
	}
	ShopToolsTabSlot = AddCanvasChild(WorkshopCanvas, ShopToolsTabButton, FAnchors(0.5f, 0.0f), FVector2D(-82.0f, 56.0f), FVector2D(150.0f, 42.0f), FVector2D(0.5f, 0.0f));

	ShopEquipmentTabButton = MakeButton(WidgetTree, TEXT("WorkshopEquipmentTab"), TEXT("EQUIPMENT"));
	if (ShopEquipmentTabButton != nullptr)
	{
		ShopEquipmentTabButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleWorkshopEquipmentTabClicked);
	}
	ShopEquipmentTabSlot = AddCanvasChild(WorkshopCanvas, ShopEquipmentTabButton, FAnchors(0.5f, 0.0f), FVector2D(82.0f, 56.0f), FVector2D(150.0f, 42.0f), FVector2D(0.5f, 0.0f));

	ShopPickaxeCard = BuildWorkshopItemCard(
		WorkshopCanvas,
		TEXT("WorkshopPickaxeCard"),
		TEXT("PICKAXE"),
		TEXT("TOOL"),
		TEXT("MINE LARGE ROCKS AND ORE DEPOSITS"),
		ShopPickaxeCostText,
		ShopPickaxeBuyButton);
	ShopPickaxeCardSlot = ShopPickaxeCard != nullptr ? Cast<UCanvasPanelSlot>(ShopPickaxeCard->Slot) : nullptr;
	if (ShopPickaxeBuyButton != nullptr)
	{
		ShopPickaxeBuyButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleBuyPickaxeClicked);
	}

	ShopBackpackCard = BuildWorkshopItemCard(
		WorkshopCanvas,
		TEXT("WorkshopBackpackCard"),
		TEXT("BACKPACK"),
		TEXT("EQUIPMENT"),
		TEXT("+5 INVENTORY SLOTS"),
		ShopBackpackCostText,
		ShopBackpackBuyButton);
	ShopBackpackCardSlot = ShopBackpackCard != nullptr ? Cast<UCanvasPanelSlot>(ShopBackpackCard->Slot) : nullptr;
	if (ShopBackpackBuyButton != nullptr)
	{
		ShopBackpackBuyButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleBuyBackpackClicked);
	}

	ShopCloseButton = MakeButton(WidgetTree, TEXT("MoonShopCloseButton"), TEXT("CLOSE"));
	if (ShopCloseButton != nullptr)
	{
		ShopCloseButton->OnClicked.AddDynamic(this, &UJTSPrototypeHUDWidget::HandleCloseMoonShopClicked);
	}
	ShopCloseButtonSlot = AddCanvasChild(WorkshopCanvas, ShopCloseButton, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 398.0f), FVector2D(220.0f, 40.0f), FVector2D(0.5f, 0.0f));

	RefreshWorkshopLayout();
	RefreshWorkshopTabs();
}

UBorder* UJTSPrototypeHUDWidget::BuildWorkshopItemCard(
	UCanvasPanel* Parent,
	const FName& CardName,
	const FString& ItemName,
	const FString& ItemCategory,
	const FString& Description,
	TObjectPtr<UTextBlock>& OutCostText,
	TObjectPtr<UButton>& OutBuyButton)
{
	OutCostText = nullptr;
	OutBuyButton = nullptr;
	UBorder* const Card = MakeBorder(WidgetTree, CardName, FLinearColor(0.035f, 0.075f, 0.12f, 0.98f), 10.0f);
	AddCanvasChild(Parent, Card, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 110.0f), FVector2D(700.0f, 240.0f), FVector2D(0.5f, 0.0f));
	UCanvasPanel* const CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("%sCanvas"), *CardName.ToString()));
	if (Card == nullptr || CardCanvas == nullptr)
	{
		return Card;
	}
	Card->SetContent(CardCanvas);

	UBorder* const IconArea = MakeBorder(WidgetTree, *FString::Printf(TEXT("%sIcon"), *CardName.ToString()), FLinearColor(0.10f, 0.17f, 0.24f, 1.0f), 5.0f);
	if (IconArea != nullptr)
	{
		IconArea->SetContent(MakeTextBlock(WidgetTree, *FString::Printf(TEXT("%sIconLabel"), *CardName.ToString()), TEXT("+"), 42.0f, FLinearColor(0.78f, 0.91f, 1.0f, 1.0f), ETextJustify::Center));
	}
	AddCanvasChild(CardCanvas, IconArea, FAnchors(0.0f, 0.0f), FVector2D(12.0f, 14.0f), FVector2D(112.0f, 142.0f));
	AddCanvasChild(CardCanvas, MakeTextBlock(WidgetTree, *FString::Printf(TEXT("%sName"), *CardName.ToString()), ItemName, 24.0f, FLinearColor(0.96f, 0.88f, 0.55f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(140.0f, 16.0f), FVector2D(300.0f, 32.0f));
	AddCanvasChild(CardCanvas, MakeTextBlock(WidgetTree, *FString::Printf(TEXT("%sCategory"), *CardName.ToString()), ItemCategory, 15.0f, FLinearColor(0.52f, 0.75f, 0.94f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(140.0f, 48.0f), FVector2D(300.0f, 24.0f));
	AddCanvasChild(CardCanvas, MakeTextBlock(WidgetTree, *FString::Printf(TEXT("%sDescription"), *CardName.ToString()), Description, 16.0f, FLinearColor(0.88f, 0.92f, 0.96f, 1.0f)), FAnchors(0.0f, 0.0f), FVector2D(140.0f, 76.0f), FVector2D(340.0f, 38.0f));
	OutCostText = MakeTextBlock(WidgetTree, *FString::Printf(TEXT("%sCost"), *CardName.ToString()), TEXT("COST"), 16.0f, FLinearColor::White);
	AddCanvasChild(CardCanvas, OutCostText, FAnchors(0.0f, 0.0f), FVector2D(140.0f, 128.0f), FVector2D(270.0f, 48.0f));
	OutBuyButton = MakeButton(WidgetTree, *FString::Printf(TEXT("%sBuy"), *CardName.ToString()), TEXT("BUY"));
	AddCanvasChild(CardCanvas, OutBuyButton, FAnchors(1.0f, 1.0f), FVector2D(-14.0f, -14.0f), FVector2D(150.0f, 42.0f), FVector2D(1.0f, 1.0f));
	return Card;
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
	const bool bEarthCollection = NewGameplayPhase == EJTSGameplayPhase::EarthCollection;
	const bool bMoonExploration = NewGameplayPhase == EJTSGameplayPhase::MoonExploration;
	if (!bMoonExploration && bMoonShopOpen)
	{
		CloseMoonShop();
	}
	if (NewGameplayPhase != EJTSGameplayPhase::WaitingToStart)
	{
		bSettingsVisible = false;
	}

	ApplyLayerVisibility(StartMenuLayer, NewGameplayPhase == EJTSGameplayPhase::WaitingToStart && !bSettingsVisible);
	ApplyLayerVisibility(SettingsLayer, NewGameplayPhase == EJTSGameplayPhase::WaitingToStart && bSettingsVisible);
	ApplyLayerVisibility(GameplayLayer, bEarthCollection || bMoonExploration);
	ApplyLayerVisibility(LaunchingLayer, NewGameplayPhase == EJTSGameplayPhase::Launching);
	ApplyLayerVisibility(ResultLayer, NewGameplayPhase == EJTSGameplayPhase::EarthCaptureFailure || NewGameplayPhase == EJTSGameplayPhase::MoonArrivalSuccess);
	ApplyLayerVisibility(FuelToMoonPanel, bEarthCollection);
	ApplyLayerVisibility(TimeText, bEarthCollection);
	ApplyLayerVisibility(InventoryPanel, bEarthCollection || bMoonExploration);
	ApplyLayerVisibility(InteractionPromptText, (bEarthCollection || bMoonExploration) && !bMoonShopOpen && !bGameMenuOpen);
	ApplyLayerVisibility(CrosshairText, (bEarthCollection || bMoonExploration) && !bMoonShopOpen && !bGameMenuOpen);
	ApplyLayerVisibility(GameplayHelpText, (bEarthCollection || bMoonExploration) && !bMoonShopOpen && !bGameMenuOpen);
	if (!bMoonExploration || bMoonShopOpen || bGameMenuOpen)
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
	else if (bEarthCollection || bMoonExploration)
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

	const bool bEarthCollection = BoundGameState->IsEarthCollectionActive();
	if (bEarthCollection && TimeText != nullptr)
	{
		const float RemainingTime = BoundGameState->GetEarthCollectionRemainingTime();
		TimeText->SetText(FText::FromString(FString::Printf(TEXT("TIME: %s"), *FormatRemainingTime(RemainingTime))));
		TimeText->SetColorAndOpacity(FSlateColor(RemainingTime <= 5.0f ? FLinearColor(1.0f, 0.25f, 0.18f, 1.0f) : FLinearColor::White));
		TimeText->SetFont(FCoreStyle::GetDefaultFontStyle(FName(TEXT("Bold")), 34.0f));
	}

	AJTSCharacter* const PlayerCharacter = FindPlayerCharacter();
	RefreshInventorySlots();
	RefreshEquipmentSlots();
	RefreshInteractionPrompt();
	const bool bShowGameplayAiming = (BoundGameState->IsEarthCollectionActive() || BoundGameState->IsMoonExploration())
		&& !bMoonShopOpen
		&& !bGameMenuOpen
		&& PlayerCharacter != nullptr
		&& !PlayerCharacter->IsBoarded();
	ApplyLayerVisibility(CrosshairText, bShowGameplayAiming);
	ApplyLayerVisibility(GameplayHelpText, bShowGameplayAiming);

	AJTSSpacecraftActor* const Spacecraft = FindSpacecraft();
	RefreshSpacecraftNavigation(Spacecraft);
	if (ShipResourcesText != nullptr)
	{
		const EJTSResourceType DisplayedResourceTypes[] = {
			EJTSResourceType::Fuel,
			EJTSResourceType::Water,
			EJTSResourceType::Food,
			EJTSResourceType::Rock,
			EJTSResourceType::Ore};

		FString ShipResourceLines;
		for (const EJTSResourceType ResourceType : DisplayedResourceTypes)
		{
			const int32 ResourceAmount = Spacecraft != nullptr ? Spacecraft->GetResourceAmount(ResourceType) : 0;
			if (ResourceAmount <= 0)
			{
				continue;
			}

			if (!ShipResourceLines.IsEmpty())
			{
				ShipResourceLines += TEXT("\n");
			}
			ShipResourceLines += FString::Printf(TEXT("%s %d"), *ResourceTypeToString(ResourceType), ResourceAmount);
		}

		ShipResourcesText->SetText(FText::FromString(ShipResourceLines));
	}

	RefreshFuelToMoonHud();

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

void UJTSPrototypeHUDWidget::RefreshEarthCollectionDurationText()
{
	if (StartRulesText == nullptr)
	{
		return;
	}

	const AJTSEarthGameMode* const EarthGameMode = GetWorld() != nullptr
		? GetWorld()->GetAuthGameMode<AJTSEarthGameMode>()
		: nullptr;
	if (!IsValid(EarthGameMode))
	{
		StartRulesText->SetText(FText::FromString(TEXT("TIME LIMITED\nTWO HANDS\nONE TERRIBLE PLAN")));
		return;
	}

	const int32 DisplayedDuration = FMath::Max(0, FMath::RoundToInt(EarthGameMode->GetEarthCollectionDuration()));
	StartRulesText->SetText(FText::FromString(FString::Printf(
		TEXT("%d SECONDS\nTWO HANDS\nONE TERRIBLE PLAN"),
		DisplayedDuration)));
}

void UJTSPrototypeHUDWidget::RefreshFuelToMoonHud()
{
	const bool bEarthCollectionActive = BoundGameState.IsValid() && BoundGameState->IsEarthCollectionActive();
	const AJTSEarthGameMode* const EarthGameMode = bEarthCollectionActive && GetWorld() != nullptr
		? GetWorld()->GetAuthGameMode<AJTSEarthGameMode>()
		: nullptr;
	const bool bShowFuelPanel = bEarthCollectionActive && IsValid(EarthGameMode);
	ApplyLayerVisibility(FuelToMoonPanel, bShowFuelPanel);
	if (!bShowFuelPanel)
	{
		return;
	}

	const AJTSSpacecraftActor* const Spacecraft = FindSpacecraft();
	const int32 CurrentFuel = IsValid(Spacecraft) ? Spacecraft->GetFuelCount() : 0;
	const float RequiredFuel = EarthGameMode->GetMinimumFuelRequired();
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

	const UJTSCarryComponent* CarryComponent = nullptr;
	int32 EffectiveCapacity = 2;
	int32 BaseCapacity = EffectiveCapacity;
	if (const AJTSCharacter* const PlayerCharacter = FindPlayerCharacter())
	{
		CarryComponent = PlayerCharacter->GetCarryComponent();
		if (IsValid(CarryComponent))
		{
			EffectiveCapacity = CarryComponent->GetCarryCapacity();
			BaseCapacity = CarryComponent->GetBaseCapacity();
		}
	}
	EffectiveCapacity = FMath::Clamp(EffectiveCapacity, 1, InventorySlotTexts.Num());
	BaseCapacity = FMath::Clamp(BaseCapacity, 0, EffectiveCapacity);

	int32 ViewportWidth = 1280;
	int32 ViewportHeight = 720;
	if (APlayerController* const PlayerController = GetOwningPlayer())
	{
		PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	}
	const int32 SlotsPerRow = EffectiveCapacity > 4 ? 4 : EffectiveCapacity;
	const int32 RowCount = FMath::CeilToInt(static_cast<float>(EffectiveCapacity) / static_cast<float>(SlotsPerRow));
	const float PanelWidth = FMath::Min(
		FMath::Max(180.0f, static_cast<float>(ViewportWidth) * 0.72f),
		static_cast<float>(SlotsPerRow) * 104.0f + 16.0f);
	const float PanelHeight = 30.0f + static_cast<float>(RowCount) * 70.0f + 8.0f;
	const float SlotWidth = (PanelWidth - 16.0f - static_cast<float>(SlotsPerRow - 1) * 4.0f) / static_cast<float>(SlotsPerRow);
	const TArray<EJTSResourceType>* const CarriedItems = IsValid(CarryComponent) ? &CarryComponent->GetCarriedItems() : nullptr;
	if (InventoryPanelSlot != nullptr)
	{
		InventoryPanelSlot->SetSize(FVector2D(PanelWidth, PanelHeight));
	}

	for (int32 SlotIndex = 0; SlotIndex < InventorySlotTexts.Num(); ++SlotIndex)
	{
		const bool bSlotVisible = SlotIndex < EffectiveCapacity;
		const bool bBackpackBonusSlot = SlotIndex >= BaseCapacity;
		if (InventorySlotBorders.IsValidIndex(SlotIndex) && InventorySlotBorders[SlotIndex] != nullptr)
		{
			InventorySlotBorders[SlotIndex]->SetVisibility(bSlotVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			InventorySlotBorders[SlotIndex]->SetBrushColor(bBackpackBonusSlot
				? FLinearColor(0.045f, 0.095f, 0.135f, 0.96f)
				: FLinearColor(0.08f, 0.15f, 0.22f, 0.96f));
			InventorySlotBorders[SlotIndex]->SetPadding(bBackpackBonusSlot ? 2.0f : 3.0f);
			if (UCanvasPanelSlot* const LayoutSlot = Cast<UCanvasPanelSlot>(InventorySlotBorders[SlotIndex]->Slot))
			{
				const int32 RowIndex = SlotIndex / SlotsPerRow;
				const int32 ColumnIndex = SlotIndex % SlotsPerRow;
				LayoutSlot->SetPosition(FVector2D(8.0f + static_cast<float>(ColumnIndex) * (SlotWidth + 4.0f), 29.0f + static_cast<float>(RowIndex) * 70.0f));
				LayoutSlot->SetSize(FVector2D(SlotWidth, 64.0f));
			}
		}

		if (UTextBlock* const SlotText = InventorySlotTexts[SlotIndex])
		{
			const FString SlotLabel = CarriedItems != nullptr && CarriedItems->IsValidIndex(SlotIndex)
				? ResourceTypeToString((*CarriedItems)[SlotIndex]).ToUpper()
				: TEXT("EMPTY");
			SlotText->SetText(FText::FromString(SlotLabel));
			SlotText->SetFont(FCoreStyle::GetDefaultFontStyle(FName(TEXT("Bold")), SlotWidth < 76.0f ? 11.0f : 14.0f));
			SlotText->SetColorAndOpacity(FSlateColor(bBackpackBonusSlot
				? FLinearColor(0.66f, 0.80f, 0.87f, 1.0f)
				: FLinearColor(0.78f, 0.84f, 0.90f, 1.0f)));
		}
	}
}

void UJTSPrototypeHUDWidget::RefreshEquipmentSlots()
{
	const bool bMoonExploration = BoundGameState.IsValid() && BoundGameState->IsMoonExploration();
	const AJTSCharacter* const PlayerCharacter = FindPlayerCharacter();
	const UJTSPlayerEquipmentComponent* const EquipmentComponent = PlayerCharacter != nullptr
		? PlayerCharacter->GetEquipmentComponent()
		: nullptr;
	const bool bShowEquipment = bMoonExploration && IsValid(EquipmentComponent);
	ApplyLayerVisibility(EquipmentPanel, bShowEquipment);
	const int32 SelectedSlotIndex = bShowEquipment ? EquipmentComponent->GetSelectedEquipmentSlotIndex() : INDEX_NONE;
	const int32 HeldSlotIndex = PlayerCharacter != nullptr ? PlayerCharacter->GetEquipmentHoldSlotIndex() : INDEX_NONE;
	const float HoldProgress = PlayerCharacter != nullptr ? PlayerCharacter->GetEquipmentHoldProgress() : 0.0f;

	for (int32 SlotIndex = 0; SlotIndex < EquipmentSlotTexts.Num(); ++SlotIndex)
	{
		const bool bSelected = bShowEquipment && SlotIndex == SelectedSlotIndex;
		if (EquipmentSlotBorders.IsValidIndex(SlotIndex) && EquipmentSlotBorders[SlotIndex] != nullptr)
		{
			EquipmentSlotBorders[SlotIndex]->SetBrushColor(bSelected
				? FLinearColor(0.12f, 0.42f, 0.43f, 0.98f)
				: FLinearColor(0.12f, 0.105f, 0.06f, 0.96f));
			EquipmentSlotBorders[SlotIndex]->SetPadding(bSelected ? 2.0f : 3.0f);
		}
		if (UTextBlock* const SlotText = EquipmentSlotTexts[SlotIndex])
		{
			const EJTSEquipmentType EquipmentType = bShowEquipment
				? EquipmentComponent->GetEquipmentSlot(SlotIndex)
				: EJTSEquipmentType::None;
			SlotText->SetText(FText::FromString(EquipmentTypeToString(EquipmentType)));
			SlotText->SetColorAndOpacity(FSlateColor(bSelected
				? FLinearColor(0.77f, 1.0f, 0.97f, 1.0f)
				: FLinearColor(0.84f, 0.80f, 0.68f, 1.0f)));
		}
		if (EquipmentSlotKeyTexts.IsValidIndex(SlotIndex) && EquipmentSlotKeyTexts[SlotIndex] != nullptr)
		{
			EquipmentSlotKeyTexts[SlotIndex]->SetText(FText::AsNumber(SlotIndex + 1));
		}
		if (EquipmentHoldProgressWidgets.IsValidIndex(SlotIndex) && EquipmentHoldProgressWidgets[SlotIndex] != nullptr)
		{
			const bool bShowHoldProgress = bShowEquipment && SlotIndex == HeldSlotIndex && HoldProgress > 0.0f;
			EquipmentHoldProgressWidgets[SlotIndex]->SetProgress(HoldProgress);
			EquipmentHoldProgressWidgets[SlotIndex]->SetVisibility(bShowHoldProgress
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
		}
	}
}

void UJTSPrototypeHUDWidget::RefreshInteractionPrompt()
{
	FText PromptText;
	FText TargetName;
	FVector PromptAnchor = FVector::ZeroVector;
	bool bHasPromptAnchor = false;
	if (!bMoonShopOpen && !bGameMenuOpen)
	{
		if (const AJTSCharacter* const PlayerCharacter = FindPlayerCharacter())
		{
			if (const UInteractionComponent* const InteractionComponent = PlayerCharacter->FindComponentByClass<UInteractionComponent>())
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
		}
	}

	APlayerController* const PlayerController = GetOwningPlayer();
	FVector2D PromptScreenPosition = FVector2D::ZeroVector;
	const bool bProjected = bHasPromptAnchor
		&& IsValid(PlayerController)
		&& PlayerController->ProjectWorldLocationToScreen(PromptAnchor, PromptScreenPosition, true);
	const bool bShowPrompt = !PromptText.IsEmpty() && !TargetName.IsEmpty() && bProjected;
	if (InteractionPromptText != nullptr)
	{
		InteractionPromptText->SetText(bShowPrompt
			? FText::FromString(FString::Printf(TEXT("%s\n%s"), *TargetName.ToString(), *PromptText.ToString()))
			: FText::GetEmpty());
		InteractionPromptText->SetVisibility(bShowPrompt ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (bShowPrompt && InteractionPromptSlot != nullptr)
	{
		InteractionPromptSlot->SetPosition(PromptScreenPosition);
	}
}

void UJTSPrototypeHUDWidget::RefreshMoonShop()
{
	if (!bMoonShopOpen)
	{
		return;
	}

	AJTSMoonGameMode* const MoonGameMode = GetWorld() != nullptr
		? GetWorld()->GetAuthGameMode<AJTSMoonGameMode>()
		: nullptr;
	AJTSCharacter* const PlayerCharacter = ShopPlayer.Get();
	AJTSSpacecraftActor* const Spacecraft = ShopSpacecraft.Get();
	if (!IsValid(MoonGameMode) || !IsValid(PlayerCharacter) || !IsValid(Spacecraft))
	{
		if (ShopPickaxeBuyButton != nullptr)
		{
			ShopPickaxeBuyButton->SetBackgroundColor(FLinearColor(0.17f, 0.19f, 0.22f, 1.0f));
			ShopPickaxeBuyButton->SetIsEnabled(false);
		}
		if (ShopBackpackBuyButton != nullptr)
		{
			ShopBackpackBuyButton->SetBackgroundColor(FLinearColor(0.17f, 0.19f, 0.22f, 1.0f));
			ShopBackpackBuyButton->SetIsEnabled(false);
		}
		return;
	}

	const int32 ShipRock = Spacecraft->GetResourceAmount(EJTSResourceType::Rock);
	const int32 ShipOre = Spacecraft->GetResourceAmount(EJTSResourceType::Ore);

	const UJTSPlayerEquipmentComponent* const EquipmentComponent = PlayerCharacter->GetEquipmentComponent();
	auto RefreshItemCard = [EquipmentComponent, ShipRock, ShipOre](
		int32 RockCost,
		int32 OreCost,
		UTextBlock* CostText,
		UButton* BuyButton)
	{
		if (CostText != nullptr)
		{
			const FString Cost = OreCost > 0
				? FString::Printf(TEXT("COST\nROCK x%d    ORE x%d"), RockCost, OreCost)
				: FString::Printf(TEXT("COST\nROCK x%d"), RockCost);
			CostText->SetText(FText::FromString(Cost));
		}

		const bool bHasResources = ShipRock >= RockCost && ShipOre >= OreCost;
		const bool bCanBuy = IsValid(EquipmentComponent) && bHasResources;
		if (BuyButton != nullptr)
		{
			BuyButton->SetBackgroundColor(bCanBuy
				? FLinearColor(0.08f, 0.35f, 0.58f, 1.0f)
				: FLinearColor(0.17f, 0.19f, 0.22f, 1.0f));
			BuyButton->SetIsEnabled(bCanBuy);
		}
	};

	RefreshItemCard(
		MoonGameMode->GetPickaxeRockCost(),
		0,
		ShopPickaxeCostText,
		ShopPickaxeBuyButton);
	RefreshItemCard(
		MoonGameMode->GetBackpackRockCost(),
		MoonGameMode->GetBackpackOreCost(),
		ShopBackpackCostText,
		ShopBackpackBuyButton);
	RefreshWorkshopTabs();
}

void UJTSPrototypeHUDWidget::RefreshWorkshopTabs()
{
	if (ShopToolsTabButton != nullptr)
	{
		ShopToolsTabButton->SetBackgroundColor(bWorkshopEquipmentTab
			? FLinearColor(0.055f, 0.11f, 0.16f, 1.0f)
			: FLinearColor(0.12f, 0.42f, 0.64f, 1.0f));
	}
	if (ShopEquipmentTabButton != nullptr)
	{
		ShopEquipmentTabButton->SetBackgroundColor(bWorkshopEquipmentTab
			? FLinearColor(0.12f, 0.42f, 0.64f, 1.0f)
			: FLinearColor(0.055f, 0.11f, 0.16f, 1.0f));
	}
	if (ShopPickaxeCard != nullptr)
	{
		ShopPickaxeCard->SetVisibility(bWorkshopEquipmentTab ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (ShopBackpackCard != nullptr)
	{
		ShopBackpackCard->SetVisibility(bWorkshopEquipmentTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UJTSPrototypeHUDWidget::RefreshWorkshopLayout()
{
	if (MoonShopPanelSlot == nullptr)
	{
		return;
	}

	int32 ViewportWidth = 1280;
	int32 ViewportHeight = 720;
	if (APlayerController* const PlayerController = GetOwningPlayer())
	{
		PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	}
	ViewportWidth = FMath::Max(1, ViewportWidth);
	ViewportHeight = FMath::Max(1, ViewportHeight);

	const FIntPoint ViewportSize(ViewportWidth, ViewportHeight);
	if (CachedWorkshopViewportSize == ViewportSize)
	{
		return;
	}
	CachedWorkshopViewportSize = ViewportSize;

	const float CardHeight = FMath::Clamp(static_cast<float>(ViewportHeight) * 0.33f, 220.0f, 250.0f);
	const float CardTop = 106.0f;
	const float CloseTop = CardTop + CardHeight + 22.0f;
	const float PanelWidth = FMath::Clamp(static_cast<float>(ViewportWidth) * 0.70f, 620.0f, 860.0f);
	const float PanelHeight = CloseTop + 60.0f;
	const float TabWidth = FMath::Clamp(PanelWidth * 0.22f, 120.0f, 160.0f);
	const float TabOffset = TabWidth * 0.5f + 10.0f;

	MoonShopPanelSlot->SetSize(FVector2D(PanelWidth, PanelHeight));
	if (ShopToolsTabSlot != nullptr)
	{
		ShopToolsTabSlot->SetPosition(FVector2D(-TabOffset, 56.0f));
		ShopToolsTabSlot->SetSize(FVector2D(TabWidth, 40.0f));
	}
	if (ShopEquipmentTabSlot != nullptr)
	{
		ShopEquipmentTabSlot->SetPosition(FVector2D(TabOffset, 56.0f));
		ShopEquipmentTabSlot->SetSize(FVector2D(TabWidth, 40.0f));
	}

	auto LayoutItemCard = [CardTop, CardHeight, PanelWidth](UCanvasPanelSlot* CardSlot)
	{
		if (CardSlot != nullptr)
		{
			CardSlot->SetPosition(FVector2D(0.0f, CardTop));
			CardSlot->SetSize(FVector2D(PanelWidth - 44.0f, CardHeight));
		}
	};
	LayoutItemCard(ShopPickaxeCardSlot);
	LayoutItemCard(ShopBackpackCardSlot);

	if (ShopCloseButtonSlot != nullptr)
	{
		ShopCloseButtonSlot->SetPosition(FVector2D(0.0f, CloseTop));
		ShopCloseButtonSlot->SetSize(FVector2D(FMath::Clamp(PanelWidth * 0.30f, 160.0f, 220.0f), 40.0f));
	}
}

void UJTSPrototypeHUDWidget::RefreshSpacecraftNavigation(AJTSSpacecraftActor* Spacecraft)
{
	const AJTSMoonGameMode* const MoonGameMode = GetWorld() != nullptr
		? GetWorld()->GetAuthGameMode<AJTSMoonGameMode>()
		: nullptr;
	AJTSCharacter* const PlayerCharacter = FindPlayerCharacter();
	APlayerController* const PlayerController = GetOwningPlayer();
	if (!IsValid(MoonGameMode)
		|| !IsValid(Spacecraft)
		|| !IsValid(PlayerCharacter)
		|| !IsValid(PlayerController)
		|| bMoonShopOpen
		|| bGameMenuOpen
		|| PlayerCharacter->IsBoarded()
		|| (GetWorld() != nullptr && GetWorld()->IsPaused()))
	{
		SetSpacecraftNavigationVisibility(false, false);
		return;
	}

	UJTSMoonWrapSubsystem* const WrapSubsystem = GetWorld()->GetSubsystem<UJTSMoonWrapSubsystem>();
	if (!IsValid(WrapSubsystem) || !WrapSubsystem->IsConfiguredForMoon())
	{
		SetSpacecraftNavigationVisibility(false, false);
		return;
	}

	const FVector PlayerLocation = PlayerCharacter->GetActorLocation();
	const FVector ShipLocation = Spacecraft->GetActorLocation();
	const FVector2D PlayerLogicalPosition = WrapSubsystem->GetLogicalPositionFromWorld(PlayerLocation);
	const FVector2D ShipLogicalPosition = WrapSubsystem->GetLogicalPositionFromWorld(ShipLocation);
	const FVector2D WrappedDelta = WrapSubsystem->ShortestWrappedDelta2D(PlayerLogicalPosition, ShipLogicalPosition);
	const float HorizontalDistance = WrappedDelta.Size();
	if (HorizontalDistance < MoonGameMode->GetSpacecraftMarkerShowDistance())
	{
		SetSpacecraftNavigationVisibility(false, false);
		return;
	}

	const FVector NavigationAnchor = Spacecraft->GetNavigationMarkerWorldLocation();
	const FVector DistanceDelta(WrappedDelta.X, WrappedDelta.Y, ShipLocation.Z - PlayerLocation.Z);
	const int32 DistanceMeters = FMath::Max(0, FMath::RoundToInt(DistanceDelta.Size() / 100.0f));

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth <= 0 || ViewportHeight <= 0)
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
	const bool bProjected = PlayerController->ProjectWorldLocationToScreen(NavigationAnchor, ProjectedLocation, true);
	const float BaseSafeInset = MoonGameMode->GetSpacecraftMarkerScreenSafeMargin();
	const float HysteresisInset = bSpacecraftWasOnScreen ? -12.0f : 12.0f;
	const float SafeInset = FMath::Max(16.0f, BaseSafeInset + HysteresisInset);
	const bool bOnScreen = bProjected
		&& ForwardDot > 0.0f
		&& ProjectedLocation.X >= SafeInset
		&& ProjectedLocation.X <= static_cast<float>(ViewportWidth) - SafeInset
		&& ProjectedLocation.Y >= SafeInset
		&& ProjectedLocation.Y <= static_cast<float>(ViewportHeight) - SafeInset;
	if (bOnScreen)
	{
		const FVector2D MarkerPosition = ProjectedLocation + FVector2D(0.0f, -4.0f);
		if (SpacecraftWorldMarkerSlot != nullptr)
		{
			SpacecraftWorldMarkerSlot->SetPosition(MarkerPosition);
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

	const FVector2D ViewportCenter(static_cast<float>(ViewportWidth) * 0.5f, static_cast<float>(ViewportHeight) * 0.5f);
	const FVector WrappedDirectionWorld(WrappedDelta.X, WrappedDelta.Y, NavigationAnchor.Z - PlayerLocation.Z);
	const FVector SafeWrappedDirection = WrappedDirectionWorld.GetSafeNormal();
	FVector2D EdgeDirection(
		FVector::DotProduct(SafeWrappedDirection, CameraMatrix.GetUnitAxis(EAxis::Y)),
		-FVector::DotProduct(SafeWrappedDirection, CameraMatrix.GetUnitAxis(EAxis::Z)));
	const float WrappedForwardDot = FVector::DotProduct(SafeWrappedDirection, CameraMatrix.GetUnitAxis(EAxis::X));
	if (WrappedForwardDot <= 0.0f)
	{
		EdgeDirection.Y += FMath::Max(0.35f, -WrappedForwardDot);
	}
	if (EdgeDirection.IsNearlyZero())
	{
		EdgeDirection = FVector2D(0.0f, 1.0f);
	}
	EdgeDirection.Normalize();

	const float SafeMargin = MoonGameMode->GetSpacecraftMarkerScreenSafeMargin();
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

void UJTSPrototypeHUDWidget::HandleWorkshopToolsTabClicked()
{
	bWorkshopEquipmentTab = false;
	RefreshWorkshopTabs();
}

void UJTSPrototypeHUDWidget::HandleWorkshopEquipmentTabClicked()
{
	bWorkshopEquipmentTab = true;
	RefreshWorkshopTabs();
}

void UJTSPrototypeHUDWidget::HandleBuyPickaxeClicked()
{
	AJTSMoonGameMode* const MoonGameMode = GetWorld() != nullptr
		? GetWorld()->GetAuthGameMode<AJTSMoonGameMode>()
		: nullptr;
	if (IsValid(MoonGameMode))
	{
		MoonGameMode->TryCraftPickaxe(ShopPlayer.Get(), ShopSpacecraft.Get());
	}

	RefreshMoonShop();
	RefreshGameplayHud();
}

void UJTSPrototypeHUDWidget::HandleBuyBackpackClicked()
{
	AJTSMoonGameMode* const MoonGameMode = GetWorld() != nullptr
		? GetWorld()->GetAuthGameMode<AJTSMoonGameMode>()
		: nullptr;
	if (IsValid(MoonGameMode))
	{
		MoonGameMode->TryCraftBackpack(ShopPlayer.Get(), ShopSpacecraft.Get());
	}

	RefreshMoonShop();
	RefreshGameplayHud();
}

void UJTSPrototypeHUDWidget::HandleCloseMoonShopClicked()
{
	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		PlayerController->CloseMoonShop();
	}
	else
	{
		CloseMoonShop();
	}
}

void UJTSPrototypeHUDWidget::HandleResumeGameClicked()
{
	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetOwningPlayer()))
	{
		PlayerController->CloseGameMenu();
	}
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
	if (AJTSMoonGameMode* const MoonGameMode = World->GetAuthGameMode<AJTSMoonGameMode>())
	{
		if (AJTSSpacecraftActor* const MoonSpacecraft = MoonGameMode->GetSpacecraft())
		{
			CachedSpacecraft = MoonSpacecraft;
			return MoonSpacecraft;
		}
	}

	for (TActorIterator<AJTSSpacecraftActor> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			CachedSpacecraft = *It;
			return CachedSpacecraft.Get();
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

	default:
		return TEXT("Unknown");
	}
}

FString UJTSPrototypeHUDWidget::EquipmentTypeToString(EJTSEquipmentType EquipmentType)
{
	switch (EquipmentType)
	{
	case EJTSEquipmentType::Pickaxe:
		return TEXT("PICKAXE");

	case EJTSEquipmentType::Backpack:
		return TEXT("BACKPACK");

	case EJTSEquipmentType::None:
	default:
		return TEXT("EMPTY");
	}
}

FString UJTSPrototypeHUDWidget::FormatRemainingTime(float RemainingSeconds)
{
	const int32 RemainingCentiseconds = FMath::Max(0, FMath::CeilToInt(FMath::Max(0.0f, RemainingSeconds) * 100.0f));
	const int32 WholeSeconds = RemainingCentiseconds / 100;
	const int32 Centiseconds = RemainingCentiseconds % 100;
	return FString::Printf(TEXT("%02d.%02d"), WholeSeconds, Centiseconds);
}
