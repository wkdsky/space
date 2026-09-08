#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "JTSHealthBarWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UProgressBar;
class SWidget;

/** Small native-only world health bar used by Moon Ants without requiring a Widget Blueprint asset. */
UCLASS()
class SPACE_API UJTSHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetHealth(float CurrentHealth, float MaxHealth);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildWidgetTree();
	void RefreshPresentation();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Background;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthProgressBar;

	float CachedCurrentHealth = 1.0f;
	float CachedMaxHealth = 1.0f;
};
