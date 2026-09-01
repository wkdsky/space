// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"

#include "JTSGameInstance.generated.h"

/**
 * Native cross-level root for Jump to Space runtime state.
 */
UCLASS()
class SPACE_API UJTSGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
};
