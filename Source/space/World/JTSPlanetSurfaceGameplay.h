// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "UObject/Interface.h"

#include "JTSPlanetSurfaceGameplay.generated.h"

class AJTSCharacter;
class AJTSPlanetAnchor;
class AJTSSpacecraftActor;

/** Runtime inputs supplied by the flow layer after a planet arrival has completed. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSSurfaceGameplayContext
{
	GENERATED_BODY()

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Planet|Surface Gameplay")
	TObjectPtr<AJTSPlanetAnchor> Planet = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Planet|Surface Gameplay")
	TObjectPtr<AJTSCharacter> Player = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Planet|Surface Gameplay")
	TObjectPtr<AJTSSpacecraftActor> Spacecraft = nullptr;

	/** Asset/configuration selected by the GameMode Blueprint; it must not contain level Actor references. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Planet|Surface Gameplay")
	TObjectPtr<UPrimaryDataAsset> GameplayData = nullptr;

	bool HasRequiredRuntimeActors() const
	{
		return Planet.Get() != nullptr && Player.Get() != nullptr && Spacecraft.Get() != nullptr;
	}
};

/** A Blueprint-configured mapping from a stable PlanetId to one runtime surface gameplay controller. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSSurfaceGameplayControllerDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Surface Gameplay")
	FName PlanetId = NAME_None;

	/** Actor class must implement IJTSPlanetSurfaceGameplay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Surface Gameplay")
	TSubclassOf<AActor> ControllerClass;

	/** Soft configuration reference keeps art and project balance in Blueprint/Data Asset rather than C++. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Surface Gameplay")
	TSoftObjectPtr<UPrimaryDataAsset> GameplayData;
};

/**
 * Contract between SpaceWorld flow and a planet-specific surface gameplay controller.
 *
 * The interface intentionally has no streaming, gravity, or planet-registry responsibilities.
 */
UINTERFACE(MinimalAPI)
class UJTSPlanetSurfaceGameplay : public UInterface
{
	GENERATED_BODY()
};

class SPACE_API IJTSPlanetSurfaceGameplay
{
	GENERATED_BODY()

public:
	virtual bool SupportsPlanet(const AJTSPlanetAnchor* Planet) const = 0;
	virtual bool InitializeSurfaceGameplay(const FJTSSurfaceGameplayContext& Context) = 0;
	virtual void ShutdownSurfaceGameplay() = 0;
	virtual bool IsSurfaceGameplayReady() const = 0;
};
