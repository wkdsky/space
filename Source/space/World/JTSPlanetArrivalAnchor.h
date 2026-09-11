// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "JTSPlanetArrivalAnchor.generated.h"

class AJTSPlanetAnchor;
class USceneComponent;

/**
 * Optional authored entry configuration for a planet visit.
 *
 * This is intentionally separate from AJTSPlanetLandingSite: it provides first-arrival transforms
 * only and grants no landing permission, respawn ownership, or base semantics.
 */
UCLASS(BlueprintType)
class SPACE_API AJTSPlanetArrivalAnchor : public AActor
{
	GENERATED_BODY()

public:
	AJTSPlanetArrivalAnchor();

	UFUNCTION(BlueprintPure, Category = "Planet|Arrival")
	AJTSPlanetAnchor* GetPlanetAnchor() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Arrival")
	bool IsArrivalEnabled() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Arrival")
	int32 GetPriority() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Arrival")
	FTransform GetPlayerArrivalTransform() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Arrival")
	FTransform GetSpacecraftArrivalTransform() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet|Arrival", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet|Arrival", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> PlayerArrivalPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet|Arrival", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SpacecraftArrivalPoint;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|Arrival", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetAnchor> PlanetAnchor;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|Arrival", meta = (AllowPrivateAccess = "true"))
	FName PlanetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Arrival", meta = (AllowPrivateAccess = "true"))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Arrival", meta = (AllowPrivateAccess = "true"))
	int32 Priority = 0;
};
