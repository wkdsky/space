// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "space/UI/JTSNavigationMapModel.h"

#include "JTSCruiseNavigationWidget.generated.h"

class SJTSCruiseNavigation;
class UJTSSurfaceMapCapture;
class UTextureRenderTarget2D;

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

/** Local presentation of surface/minimap, system and interstellar navigation. */
UCLASS()
class SPACE_API UJTSCruiseNavigationWidget : public UWidget
{
	GENERATED_BODY()

public:
	UJTSCruiseNavigationWidget(const FObjectInitializer& ObjectInitializer);

	void UpdateNavigation(const AJTSSpaceWorldManager* Manager, const AJTSSpacecraftActor* Ship, const AJTSCharacter* Character);
	/** Read-only diagnostics for PIE validation; each widget belongs to its own local player. */
	UFUNCTION(BlueprintPure, Category="Space|Navigation") FString GetNavigationStage() const;
	UFUNCTION(BlueprintPure, Category="Space|Navigation") FString GetNavigationCoordinates() const { return Frame ? Frame->Coordinates : FString(); }
	UFUNCTION(BlueprintPure, Category="Space|Navigation") TArray<FString> GetNavigationContactNames() const;
	UFUNCTION(BlueprintPure, Category="Space|Navigation") float GetNavigationZoom() const;
	UFUNCTION(BlueprintPure, Category="Space|Navigation") UTextureRenderTarget2D* GetSurfaceMapTexture() const;
	UFUNCTION(BlueprintPure, Category="Space|Navigation") FVector GetGroundMapCenter() const { return Frame->Terrain ? Frame->Terrain->Origin : FVector::ZeroVector; }
	UFUNCTION(BlueprintPure, Category="Space|Navigation") FVector2D GetPlayerMapPosition() const { return Frame->PlayerPosition; }
	UFUNCTION(BlueprintPure, Category="Space|Navigation") TArray<FVector> GetNavigationBodyProjections() const;
	UFUNCTION(BlueprintPure, Category="Space|Navigation") bool HasNavigationShipContact() const { return Frame && Frame->bHasShip; }

	static FString FormatAstronomicalRange(float Centimeters);
	static FString FormatNavigationSpeed(float CentimetersPerSecond);
	/** Stable dial radius for a contact's own AU range, independent of other contacts. */
	static float CruiseRangeRadialFraction(float Centimeters);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	void PushPresentation();

	// Keep UObject allocation independent of changes to the native presentation snapshot layout.
	TUniquePtr<FJTSNavigationMapModel> Model;
	TUniquePtr<FJTSNavigationMapFrame> Frame;
	TSharedPtr<SJTSCruiseNavigation> NavigationSlate;
	UPROPERTY(Transient) TObjectPtr<UJTSSurfaceMapCapture> SurfaceCapture;
};
