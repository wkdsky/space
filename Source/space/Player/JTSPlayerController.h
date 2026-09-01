// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "JTSPlayerController.generated.h"

/**
 * Native player controller for third-person Jump to Space gameplay.
 */
UCLASS()
class SPACE_API AJTSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AJTSPlayerController();

protected:
	virtual void BeginPlayingState() override;
};
