// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "JTSSpaceWorldGameMode.generated.h"

class AJTSCharacter;
class AJTSPlanetLandingManager;
class AJTSSpaceWorldManager;
class AJTSSpacecraftActor;
class APlayerController;

/**
 * Flow-layer entry point for persistent SpaceWorld gameplay.
 *
 * This class selects Blueprint-configured manager/vehicle classes and starts an arrival sequence.
 * It deliberately owns neither surface placement nor LandingSite validation: those responsibilities
 * live in AJTSPlanetLandingManager and AJTSPlanetLandingSite.
 */
UCLASS()
class SPACE_API AJTSSpaceWorldGameMode : public AGameModeBase
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Space World", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSSpaceWorldManager> SpaceWorldManagerClass;

	/** Blueprint-configurable runtime coordinator for arrival, LandingSite registry, and respawn queries. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Space World", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSPlanetLandingManager> PlanetLandingManagerClass;

	/** Default vehicle only; a GameInstance persisted vehicle class still wins at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Arrival", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSSpacecraftActor> SpacecraftClass;

	TWeakObjectPtr<AJTSSpaceWorldManager> SpaceWorldManager;
	TWeakObjectPtr<AJTSPlanetLandingManager> PlanetLandingManager;
	TSet<TWeakObjectPtr<APlayerController>> StartedLandingSequences;
};
