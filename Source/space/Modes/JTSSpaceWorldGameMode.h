// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"

#include "JTSSpaceWorldGameMode.generated.h"

class AJTSSpaceWorldManager;
class AJTSMoonSurfaceController;
class AJTSSpacecraftActor;
class APlayerController;

/**
 * Persistent-world ruleset. It owns the one player and persistent spacecraft instance;
 * a loaded surface controller owns Moon-only runtime initialization and placement data.
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
	void HandleInitialSurfaceLevelReady(AJTSMoonSurfaceController* SurfaceController);
	void TryCompleteInitialSurfaceArrival();
	void PollSurfaceGameplayReady();
	AJTSSpacecraftActor* FindPersistentSpacecraft() const;
	AJTSSpacecraftActor* CreateOrAdoptSurfaceSpacecraft(AJTSMoonSurfaceController* SurfaceController);
	bool SpawnOrMovePlayer(AJTSMoonSurfaceController* SurfaceController);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Space World", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSSpaceWorldManager> SpaceWorldManagerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Space World|Arrival", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSSpacecraftActor> SpacecraftClass;

	TWeakObjectPtr<AJTSSpaceWorldManager> SpaceWorldManager;
	TWeakObjectPtr<AJTSMoonSurfaceController> PendingSurfaceController;
	TWeakObjectPtr<AJTSSpacecraftActor> PersistentSpacecraft;
	FTimerHandle ArrivalRetryTimerHandle;
	bool bPersistentActorsPlaced = false;
};
