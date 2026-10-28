#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "JTSGradientBar.generated.h"

/** Horizontal gradient with evenly spaced stops. Used behind HSB sliders and as panel scrims. */
UCLASS()
class SPACE_API UJTSGradientBar : public UWidget
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance") TArray<FLinearColor> Stops={FLinearColor::Black,FLinearColor::White};
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance") float CornerRadius=0;
 UFUNCTION(BlueprintCallable) void SetStops(const TArray<FLinearColor>& InStops);
 virtual void SynchronizeProperties() override;
 virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
private:
 TSharedPtr<class SJTSGradientBar> Bar;
};
