// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"

#include "JTSCircularProgressWidget.generated.h"

class SJTSCircularProgress;

UCLASS()
class SPACE_API UJTSCircularProgressWidget : public UWidget
{
	GENERATED_BODY()

public:
	UJTSCircularProgressWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Progress")
	float GetProgress() const;

	UFUNCTION(BlueprintCallable, Category = "Progress")
	void SetProgress(float NewProgress);

	UFUNCTION(BlueprintCallable, Category = "Progress")
	void SetProgressColor(FLinearColor NewColor);

	UFUNCTION(BlueprintCallable, Category = "Progress")
	void SetBackgroundColor(FLinearColor NewColor);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progress", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float Progress = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (AllowPrivateAccess = "true"))
	FLinearColor ProgressColor = FLinearColor(0.20f, 0.80f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (AllowPrivateAccess = "true"))
	FLinearColor BackgroundColor = FLinearColor(0.10f, 0.18f, 0.25f, 0.90f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float RingThickness = 12.0f;

	TSharedPtr<SJTSCircularProgress> MyCircularProgress;
};
