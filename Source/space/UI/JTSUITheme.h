// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

/** Shared native fallback theme. UMG Blueprint children may replace composition without changing semantic state. */
namespace JTSUITheme
{
	inline const FLinearColor Night = FLinearColor(0.012f, 0.024f, 0.055f, 1.0f);
	inline const FLinearColor Panel = FLinearColor(0.035f, 0.075f, 0.125f, 0.94f);
	inline const FLinearColor PanelRaised = FLinearColor(0.055f, 0.115f, 0.185f, 0.96f);
	inline const FLinearColor Ink = FLinearColor(0.90f, 0.96f, 1.0f, 1.0f);
	inline const FLinearColor Muted = FLinearColor(0.56f, 0.68f, 0.80f, 1.0f);
	inline const FLinearColor Cyan = FLinearColor(0.16f, 0.72f, 1.0f, 1.0f);
	inline const FLinearColor Ready = FLinearColor(0.20f, 0.92f, 0.55f, 1.0f);
	inline const FLinearColor Warning = FLinearColor(1.0f, 0.69f, 0.20f, 1.0f);
	inline const FLinearColor Danger = FLinearColor(0.95f, 0.25f, 0.30f, 1.0f);

	enum class EButtonTone : uint8
	{
		Primary,
		Secondary,
		Danger,
		Ready
	};

	inline UTextBlock* MakeText(UWidgetTree* Tree, const FName Name, const FString& Value, float FontSize, const FLinearColor& Color = Ink, ETextJustify::Type Justification = ETextJustify::Left)
	{
		if (Tree == nullptr)
		{
			return nullptr;
		}
		UTextBlock* const Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(Value));
		Text->SetFont(FCoreStyle::GetDefaultFontStyle(FName(TEXT("Bold")), FontSize));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(Justification);
		Text->SetAutoWrapText(true);
		return Text;
	}

	inline UBorder* MakePanel(UWidgetTree* Tree, const FName Name, const FLinearColor& Color = Panel, const FMargin& Padding = FMargin(18.0f))
	{
		if (Tree == nullptr)
		{
			return nullptr;
		}
		UBorder* const Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Border->SetBrushColor(Color);
		Border->SetPadding(Padding);
		return Border;
	}

	inline UButton* MakeButton(UWidgetTree* Tree, const FName Name, const FString& Label, EButtonTone Tone = EButtonTone::Primary, float FontSize = 19.0f)
	{
		if (Tree == nullptr)
		{
			return nullptr;
		}

		FLinearColor Color = Cyan;
		switch (Tone)
		{
		case EButtonTone::Secondary: Color = PanelRaised; break;
		case EButtonTone::Danger: Color = Danger; break;
		case EButtonTone::Ready: Color = Ready; break;
		default: break;
		}
		UButton* const Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetBackgroundColor(Color);
		Button->SetContent(MakeText(Tree, *FString::Printf(TEXT("%sLabel"), *Name.ToString()), Label, FontSize, Ink, ETextJustify::Center));
		return Button;
	}

	inline FString FormatPlaytime(double Seconds)
	{
		const int32 TotalSeconds = FMath::Max(0, FMath::FloorToInt(Seconds));
		return FString::Printf(TEXT("%02dh %02dm"), TotalSeconds / 3600, (TotalSeconds / 60) % 60);
	}

	inline FString FormatUtcTicks(int64 Ticks)
	{
		if (Ticks <= 0)
		{
			return TEXT("Never launched");
		}
		return FDateTime(Ticks).ToString(TEXT("yyyy-MM-dd HH:mm"));
	}
}
