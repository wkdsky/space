// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"

#include "JTSSpaceWorldGameMode.generated.h"

class AJTSPlanetAnchor;
class AJTSCharacter;
class AJTSSpaceWorldManager;
class AJTSSpacecraftActor;
class APlayerController;

/**
 * Persistent SpaceWorld startup rules. It chooses the player spawn and binds that character to
 * the configured real gameplay planet. Planet math, Moon-specific surface gameplay, and flight
 * behavior remain in their own systems.
 */
UCLASS()
class SPACE_API AJTSSpaceWorldGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJTSSpaceWorldGameMode();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

private:
	AJTSSpaceWorldManager* FindOrCreateSpaceWorldManager();
	void TrySpawnInitialSurfaceCharacter();
	void ScheduleInitialSurfaceSpawnRetry(APlayerController* PlayerController, AJTSPlanetAnchor* Planet);
	void LogInitialSurfaceInitializationFailure(APlayerController* PlayerController, AJTSPlanetAnchor* Planet);
	bool SpawnAndSnapCharacter(APlayerController* PlayerController, AJTSPlanetAnchor* Planet);
	bool TrySpawnInitialGroundedSpacecraft(AJTSPlanetAnchor* Planet, const AJTSCharacter* Character);
	bool ResolveInitialSpacecraftLandingTransform(
		AJTSPlanetAnchor* Planet,
		const AJTSCharacter* Character,
		FTransform& OutLandingSurfaceTransform) const;
	AJTSSpacecraftActor* FindExistingGameplaySpacecraft();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Space World", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSSpaceWorldManager> SpaceWorldManagerClass;

	/** Class used for the single persistent spacecraft parked on the authored landing surface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Space World|Surface", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSSpacecraftActor> SpacecraftClass;

	/**
	 * Minimum real-surface separation between the GameMode-spawned player and parked spacecraft.
	 * The authored Planet landing anchor remains the preferred location; this is only a safe fallback
	 * when that anchor resolves too close to the player spawn.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Space World|Surface", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float InitialSpacecraftMinimumPlayerDistance = 900.0f;

	/** Retained for serialized Blueprint defaults until real-mesh landing is implemented with spacecraft code. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Space World|LegacyFlight", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1", DeprecatedProperty, DeprecationMessage = "Assisted landing is deferred to the real-mesh spacecraft phase."))
	float AssistedLandingDuration = 2.0f;

	TWeakObjectPtr<AJTSSpaceWorldManager> SpaceWorldManager;
	FTimerHandle SurfaceSpawnRetryTimerHandle;
	bool bInitialSurfaceCharacterSpawned = false;
	bool bInitialGroundedSpacecraftInitialized = false;
	int32 InitialSurfaceSpawnRetryCount = 0;
	bool bInitialSurfaceSpawnRetryExhausted = false;
	bool bLoggedSurfaceSnapFailure = false;
	bool bLoggedLandingAnchorFailure = false;
	bool bLoggedMultipleSpacecraft = false;
};
