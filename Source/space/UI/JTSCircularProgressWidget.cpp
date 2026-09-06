// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSCircularProgressWidget.h"

#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

class SJTSCircularProgress : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SJTSCircularProgress)
		: _Progress(0.0f)
		, _ProgressColor(FLinearColor::White)
		, _BackgroundColor(FLinearColor::Gray)
		, _RingThickness(12.0f)
	{
	}
		SLATE_ARGUMENT(float, Progress)
		SLATE_ARGUMENT(FLinearColor, ProgressColor)
		SLATE_ARGUMENT(FLinearColor, BackgroundColor)
		SLATE_ARGUMENT(float, RingThickness)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Progress = FMath::Clamp(InArgs._Progress, 0.0f, 1.0f);
		ProgressColor = InArgs._ProgressColor;
		BackgroundColor = InArgs._BackgroundColor;
		RingThickness = FMath::Max(1.0f, InArgs._RingThickness);
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		return FVector2D(180.0f, 180.0f);
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
		const FVector2D Center = LocalSize * 0.5f;
		const float Radius = FMath::Max(0.0f, FMath::Min(LocalSize.X, LocalSize.Y) * 0.5f - RingThickness * 0.5f);
		const int32 CircleSegments = 32;
		const float FullCircle = 2.0f * PI;
		const FLinearColor WidgetTint = InWidgetStyle.GetColorAndOpacityTint();

		TArray<FVector2D> BackgroundPoints;
		BackgroundPoints.Reserve(CircleSegments + 1);
		for (int32 SegmentIndex = 0; SegmentIndex <= CircleSegments; ++SegmentIndex)
		{
			const float Angle = -PI * 0.5f + FullCircle * static_cast<float>(SegmentIndex) / static_cast<float>(CircleSegments);
			BackgroundPoints.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}

		FLinearColor DrawBackgroundColor = BackgroundColor;
		DrawBackgroundColor.A *= WidgetTint.A;
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			BackgroundPoints,
			ESlateDrawEffect::None,
			DrawBackgroundColor,
			true,
			RingThickness);

		if (Progress > KINDA_SMALL_NUMBER)
		{
			const int32 ProgressSegments = FMath::Max(2, FMath::CeilToInt(static_cast<float>(CircleSegments) * Progress));
			TArray<FVector2D> ProgressPoints;
			ProgressPoints.Reserve(ProgressSegments + 1);
			for (int32 SegmentIndex = 0; SegmentIndex <= ProgressSegments; ++SegmentIndex)
			{
				const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(ProgressSegments);
				const float Angle = -PI * 0.5f + FullCircle * Progress * Alpha;
				ProgressPoints.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
			}

			FLinearColor DrawProgressColor = ProgressColor;
			DrawProgressColor.A *= WidgetTint.A;
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(),
				ProgressPoints,
				ESlateDrawEffect::None,
				DrawProgressColor,
				true,
				RingThickness);
		}

		return LayerId + 2;
	}

	void SetProgress(float NewProgress)
	{
		Progress = FMath::Clamp(NewProgress, 0.0f, 1.0f);
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	void SetProgressColor(FLinearColor NewColor)
	{
		ProgressColor = NewColor;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	void SetBackgroundColor(FLinearColor NewColor)
	{
		BackgroundColor = NewColor;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

private:
	float Progress = 0.0f;
	FLinearColor ProgressColor = FLinearColor::White;
	FLinearColor BackgroundColor = FLinearColor::Gray;
	float RingThickness = 12.0f;
};

UJTSCircularProgressWidget::UJTSCircularProgressWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

float UJTSCircularProgressWidget::GetProgress() const
{
	return Progress;
}

void UJTSCircularProgressWidget::SetProgress(float NewProgress)
{
	Progress = FMath::Clamp(NewProgress, 0.0f, 1.0f);
	if (MyCircularProgress.IsValid())
	{
		MyCircularProgress->SetProgress(Progress);
	}
}

void UJTSCircularProgressWidget::SetProgressColor(FLinearColor NewColor)
{
	ProgressColor = NewColor;
	if (MyCircularProgress.IsValid())
	{
		MyCircularProgress->SetProgressColor(ProgressColor);
	}
}

void UJTSCircularProgressWidget::SetBackgroundColor(FLinearColor NewColor)
{
	BackgroundColor = NewColor;
	if (MyCircularProgress.IsValid())
	{
		MyCircularProgress->SetBackgroundColor(BackgroundColor);
	}
}

TSharedRef<SWidget> UJTSCircularProgressWidget::RebuildWidget()
{
	MyCircularProgress = SNew(SJTSCircularProgress)
		.Progress(Progress)
		.ProgressColor(ProgressColor)
		.BackgroundColor(BackgroundColor)
		.RingThickness(RingThickness);
	return MyCircularProgress.ToSharedRef();
}

void UJTSCircularProgressWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (MyCircularProgress.IsValid())
	{
		MyCircularProgress->SetProgress(Progress);
		MyCircularProgress->SetProgressColor(ProgressColor);
		MyCircularProgress->SetBackgroundColor(BackgroundColor);
	}
}

void UJTSCircularProgressWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyCircularProgress.Reset();
}
