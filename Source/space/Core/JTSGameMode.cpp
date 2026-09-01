// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSGameMode.h"

#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"

AJTSGameMode::AJTSGameMode()
{
	DefaultPawnClass = AJTSCharacter::StaticClass();
	PlayerControllerClass = AJTSPlayerController::StaticClass();
}
