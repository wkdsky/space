#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "JTSMaterialProcessorWidget.generated.h"

class AJTSSpacecraftActor;
class AJTSPlayerController;
class SJTSMaterialGraph;
class SVerticalBox;

/** Ship-terminal presentation; submits graphs through the owning controller's server RPC. */
UCLASS()
class SPACE_API UJTSMaterialProcessorWidget : public UWidget
{
	GENERATED_BODY()
public:
	void SetContext(AJTSSpacecraftActor* Ship, AJTSPlayerController* Player);
	void NotifyResult(bool bSucceeded, const FString& Message);
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	FString StockText() const;
	FString PreviewText() const;
	bool CanExecute() const;
	FReply Submit(bool bExecute);
	TWeakObjectPtr<AJTSSpacecraftActor> Spacecraft;
	TWeakObjectPtr<AJTSPlayerController> Controller;
	TSharedPtr<SJTSMaterialGraph> GraphCanvas;
	TSharedPtr<SVerticalBox> Palette;
	void RefreshPalette();
	FString ResultMessage;
	bool bPending = false;
	bool bLastSucceeded = true;
};
