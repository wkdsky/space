#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "space/World/JTSCelestialNavigationTypes.h"
#include "JTSCelestialMarkerWidget.generated.h"

class APlayerController;
class SJTSCelestialMarkers;

USTRUCT(BlueprintType)
struct SPACE_API FJTSCelestialScreenMarker
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FText DisplayName;
	UPROPERTY(BlueprintReadOnly) FVector2D Min = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector2D Max = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly) float ScreenDiameterFraction = 0.0f;
	UPROPERTY(BlueprintReadOnly) int32 PaletteIndex = 2;
};

/** Local-player presentation only; no targeting, travel, or replicated gameplay state. */
UCLASS()
class SPACE_API UJTSCelestialMarkerWidget : public UWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Navigation")
	TArray<FJTSCelestialScreenMarker> Markers;
	void UpdateMarkers(APlayerController* Viewer, const TArray<FJTSCelestialNavigationBody>& Bodies, bool bCruise);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
	TSharedPtr<SJTSCelestialMarkers> MarkerSlate;
};
