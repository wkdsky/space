#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "JTSSpaceDebugWidget.generated.h"
class UTextBlock;
class UVerticalBox;
class AJTSSpaceDebugTravelManager;

UCLASS()
class UJTSDebugPlanetButton : public UButton
{
	GENERATED_BODY()
public:
	void Configure(class AJTSPlayerController* Controller, FName InPlanetId, bool bDefault);
private:
	UFUNCTION() void Activate();
	TWeakObjectPtr<class AJTSPlayerController> Player;
	FName PlanetId;
	bool bSetDefault = false;
};

/** Developer panel; destination configuration belongs to the level actor. */
UCLASS()
class SPACE_API UJTSSpaceDebugWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
private:
	void Refresh();
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> Content;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TArray<TObjectPtr<UJTSDebugPlanetButton>> Buttons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> DefaultLabels;
	TWeakObjectPtr<AJTSSpaceDebugTravelManager> Manager;
	float RefreshElapsed = 1.0f;
};
