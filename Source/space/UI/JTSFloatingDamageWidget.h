#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JTSFloatingDamageWidget.generated.h"

class UCanvasPanel;
class UTextBlock;

/** Compact, screen-space damage readout. Critical hits get a larger gold number and a separate label. */
UCLASS()
class SPACE_API UJTSFloatingDamageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetDamage(float Damage, bool bCritical);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DamageText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CriticalText;
};
