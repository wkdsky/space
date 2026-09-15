// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "JTSMainMenuGameMode.generated.h"

/** Map rule for the native front end. It never creates a playable pawn. */
UCLASS()
class SPACE_API AJTSMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJTSMainMenuGameMode();

	virtual void BeginPlay() override;

private:
	void EnsureEntryPresentation();

	UPROPERTY(Transient)
	TObjectPtr<class AJTSEntryPresentationStage> EntryPresentation;
};
