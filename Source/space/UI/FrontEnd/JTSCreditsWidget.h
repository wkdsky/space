// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "JTSCreditsWidget.generated.h"

/** Small front-end credits page kept separate from gameplay/UI state. */
UCLASS()
class SPACE_API UJTSCreditsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;

private:
	void BuildWidgetTree();
	UFUNCTION() void Back();
};
