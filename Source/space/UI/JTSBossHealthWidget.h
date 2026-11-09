#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JTSBossHealthWidget.generated.h"
class AJTSPlanetBoss;
class UTextBlock;
class UProgressBar;
class UCanvasPanel;
UCLASS()
class SPACE_API UJTSBossHealthWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetBoss(AJTSPlanetBoss* InBoss) { Boss = InBoss; }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
private:
	TWeakObjectPtr<AJTSPlanetBoss> Boss;
	UPROPERTY() TObjectPtr<UCanvasPanel> Frame;
	UPROPERTY() TObjectPtr<UTextBlock> NameText;
	UPROPERTY() TObjectPtr<UProgressBar> Bar;
	float DeathTime = 0;
};
