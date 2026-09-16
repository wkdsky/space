// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "space/Modes/JTSGameplayGameModeBase.h"
#include "space/World/JTSPlanetSurfaceGameplay.h"

#include "JTSSpaceWorldGameMode.generated.h"

class AJTSCharacter;
class AJTSPlanetLandingManager;
class AJTSPlanetAnchor;
class AJTSSpaceWorldManager;
class AJTSSpacecraftActor;
class AJTSShopTerminalActor;
class APlayerController;
class AActor;

/**
 * Flow-layer entry point for persistent SpaceWorld gameplay.
 *
 * This class selects Blueprint-configured manager/vehicle classes and starts an arrival sequence.
 * It deliberately owns neither surface placement nor LandingSite validation: those responsibilities
 * live in AJTSPlanetLandingManager and AJTSPlanetLandingSite.
 */
UCLASS()
class SPACE_API AJTSSpaceWorldGameMode : public AJTSGameplayGameModeBase
{
	GENERATED_BODY()

public:
	AJTSSpaceWorldGameMode();

	/** Called by AJTSCharacter when health reaches zero. Only a landed associated spacecraft permits respawn. */
	void HandlePlayerCharacterDeath(AJTSCharacter* Character);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

private:
	AJTSSpaceWorldManager* FindOrCreateSpaceWorldManager();
	AJTSPlanetLandingManager* FindOrCreatePlanetLandingManager();
	void StartInitialLandingSequence(APlayerController* PlayerController);
	void HandleInitialLandingSequenceCompleted(
		APlayerController* PlayerController,
		AJTSPlanetAnchor* Planet,
		AJTSCharacter* Character,
		AJTSSpacecraftActor* Spacecraft);
	const FJTSSurfaceGameplayControllerDefinition* FindSurfaceGameplayDefinition(const AJTSPlanetAnchor* Planet) const;
	AActor* FindOrSpawnSurfaceGameplayController(
		const FJTSSurfaceGameplayControllerDefinition& Definition,
		AJTSPlanetAnchor* Planet);
	AJTSShopTerminalActor* FindOrSpawnShopTerminal(AJTSSpacecraftActor* Spacecraft, AJTSPlanetAnchor* Planet);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Space World", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSSpaceWorldManager> SpaceWorldManagerClass;

	/** Blueprint-configurable runtime coordinator for arrival, LandingSite registry, and respawn queries. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Space World", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSPlanetLandingManager> PlanetLandingManagerClass;

	/** Default vehicle only; the server-owned expedition snapshot can restore the chosen class at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Arrival", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSSpacecraftActor> SpacecraftClass;

	/** Per-planet gameplay classes and Data Assets. Blueprint owns all project-specific selection. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Space World|Surface Gameplay", meta = (AllowPrivateAccess = "true"))
	TArray<FJTSSurfaceGameplayControllerDefinition> SurfaceGameplayControllers;

	/** Blueprint can replace the terminal presentation while C++ owns shared-wallet rules. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Space World|Shop", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSShopTerminalActor> ShopTerminalClass;

	TWeakObjectPtr<AJTSSpaceWorldManager> SpaceWorldManager;
	TWeakObjectPtr<AJTSPlanetLandingManager> PlanetLandingManager;
	TSet<TWeakObjectPtr<APlayerController>> StartedLandingSequences;
	TMap<FName, TWeakObjectPtr<AActor>> ActiveSurfaceGameplayControllers;
	TSet<FName> InitializedSurfacePlanets;
	TWeakObjectPtr<AJTSShopTerminalActor> ShopTerminal;
};
