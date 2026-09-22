// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/World/JTSPlanetSurfaceGameplay.h"

#include "JTSPassivePlanetSurfaceController.generated.h"

class AJTSCharacter;
class AJTSPlanetAnchor;

/**
 * Minimal surface-gameplay coordinator for planets that intentionally have no
 * generated resources, AI, POIs, or planet-specific interactions yet.
 *
 * A Blueprint child selects the stable PlanetId. The SpaceWorld GameMode then
 * uses the normal arrival pipeline, so landing, player input, and radial
 * character gravity remain available without treating the planet as Moon content.
 */
UCLASS(BlueprintType)
class SPACE_API AJTSPassivePlanetSurfaceController : public AActor, public IJTSPlanetSurfaceGameplay
{
	GENERATED_BODY()

public:
	AJTSPassivePlanetSurfaceController();

	UFUNCTION(BlueprintPure, Category = "Planet|Passive Surface")
	FName GetPlanetId() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Passive Surface")
	AJTSPlanetAnchor* GetOwningPlanet() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Passive Surface")
	bool IsSurfaceGameplayInitialized() const;

	virtual bool SupportsPlanet(const AJTSPlanetAnchor* Planet) const override;
	virtual bool InitializeSurfaceGameplay(const FJTSSurfaceGameplayContext& Context) override;
	virtual void RegisterSurfacePlayer(AJTSCharacter* Player) override;
	virtual void ShutdownSurfaceGameplay() override;
	virtual bool IsSurfaceGameplayReady() const override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Set on the Blueprint child selected by the SpaceWorld GameMode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Passive Surface", meta = (AllowPrivateAccess = "true"))
	FName PlanetId = NAME_None;

	UPROPERTY(Transient)
	TObjectPtr<AJTSPlanetAnchor> OwningPlanet;

	TArray<TWeakObjectPtr<AJTSCharacter>> SurfacePlayers;
	bool bSurfaceGameplayInitialized = false;
};
