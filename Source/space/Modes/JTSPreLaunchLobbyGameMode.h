// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "space/Modes/JTSGameplayGameModeBase.h"

#include "JTSPreLaunchLobbyGameMode.generated.h"

class AJTSPreLaunchLobbyStage;

/**
 * Server-authoritative staging rules. This map is deliberately separate from every gameplay chapter:
 * it accepts players, replicates ready/customisation state, then travels everyone into the actual run.
 */
UCLASS()
class SPACE_API AJTSPreLaunchLobbyGameMode : public AJTSGameplayGameModeBase
{
	GENERATED_BODY()

public:
	AJTSPreLaunchLobbyGameMode();

	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual bool RequestStartExpedition(AJTSPlayerController* RequestingController) override;

private:
	void EnsureLobbyStage();
	void ArrangeLobbyPlayers();
	FString ResolveLaunchDestination();

	UPROPERTY(Transient)
	TObjectPtr<AJTSPreLaunchLobbyStage> LobbyStage;
};
