// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "space/Core/JTSMapPaths.h"

#include "JTSGameplayGameModeBase.generated.h"

class AJTSPlayerController;
class AJTSPlayerState;

/**
 * Shared server-authoritative multiplayer policy for every playable map.
 * Map-specific modes own rules, while this class owns admission and lobby state.
 */
UCLASS(Config = Game)
class SPACE_API AJTSGameplayGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJTSGameplayGameModeBase();

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Called only by the host controller's server RPC. Derived modes begin their own mission rules. */
	virtual bool RequestStartExpedition(AJTSPlayerController* RequestingController);

	/** Host-only lobby controls. They are server rules, never direct widget actions. */
	bool RequestCloseJoining(AJTSPlayerController* RequestingController);
	bool RequestKickPlayer(AJTSPlayerController* RequestingController, AJTSPlayerState* TargetPlayerState);

	bool IsHostController(const AJTSPlayerController* Controller) const;
	bool AreAllActivePlayersReady() const;
	TArray<AJTSPlayerState*> GetActivePlayerStates() const;

	UFUNCTION(BlueprintPure, Category = "Multiplayer")
	int32 GetMaximumPlayers() const;

	UFUNCTION(BlueprintPure, Category = "Multiplayer")
	FString GetExpectedBuildVersion() const { return ExpectedBuildVersion; }

protected:
	void MarkAllPlayersActive();
	void SetLobbyState();
	void SetLobbyAdmission(bool bAcceptingNewPlayers);
	FString GetFrontEndMapPackage() const;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Multiplayer", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaximumPlayers = 4;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Multiplayer")
	FString ExpectedBuildVersion = TEXT("JTS-1");

	UPROPERTY(Config, EditDefaultsOnly, Category = "Multiplayer")
	FString FrontEndMapPath = JTSMapPaths::FrontEnd;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Multiplayer")
	bool bRequireMatchingBuildVersion = true;
};
