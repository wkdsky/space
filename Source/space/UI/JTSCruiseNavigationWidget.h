// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"

#include "JTSCruiseNavigationWidget.generated.h"

class SJTSCruiseNavigation;

/** One body drawn on the local-space cruise dial. */
USTRUCT()
struct FJTSCruiseNavigationContact
{
	GENERATED_BODY()

	UPROPERTY()
	FString DisplayName;

	/** 0 forward, positive right, degrees. */
	UPROPERTY()
	float BearingDegrees = 0.0f;

	/** Signed elevation relative to the ship's forward plane. Positive is above the nose. */
	UPROPERTY()
	float ElevationDegrees = 0.0f;

	UPROPERTY()
	float RangeCentimeters = 0.0f;

	/** 0 Moon ice, 1 MarII rust, 2 other gold. */
	UPROPERTY()
	int32 PaletteIndex = 2;
};

/**
 * Local-space cruise dial for free flight.
 *
 * The ship stays fixed at the bottom of the disc, pointing up. Registered planets slide with the
 * hull's heading. A body outside the forward cone, or above or below the nose, pins to the rim
 * with a direction chevron. Inside 500 m of a planet the same frame switches to that body's
 * surface distance.
 */
UCLASS()
class SPACE_API UJTSCruiseNavigationWidget : public UWidget
{
	GENERATED_BODY()

public:
	UJTSCruiseNavigationWidget(const FObjectInitializer& ObjectInitializer);

	void SetCruisePresentation(
		const TArray<FJTSCruiseNavigationContact>& Contacts,
		const FString& LeftCaption,
		const FString& RightCaption,
		bool bSurfaceMode,
		float SurfaceFraction);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	void PushPresentation();

	TArray<FJTSCruiseNavigationContact> Contacts;
	FString LeftCaption;
	FString RightCaption;
	bool bSurfaceMode = false;
	float SurfaceFraction = 0.0f;
	TSharedPtr<SJTSCruiseNavigation> NavigationSlate;
};
