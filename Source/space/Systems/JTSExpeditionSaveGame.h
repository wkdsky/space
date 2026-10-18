// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "space/Core/JTSExpeditionTypes.h"

#include "JTSExpeditionSaveGame.generated.h"

/** SaveGame envelope for a server-owned expedition snapshot. */
UCLASS()
class SPACE_API UJTSExpeditionSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FJTSExpeditionSnapshot Snapshot;
};

/** Host-only developer entry preference, kept out of the four expedition save slots. */
UCLASS()
class UJTSSpaceDebugSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() FName DefaultPlanetId;
};
