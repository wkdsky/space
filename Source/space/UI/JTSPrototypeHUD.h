// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "JTSPrototypeHUD.generated.h"

class UJTSPrototypeHUDWidget;

/** HUD host that installs the native C++ UMG presentation for the prototype. */
UCLASS()
class SPACE_API AJTSPrototypeHUD : public AHUD
{
	GENERATED_BODY()

public:
	AJTSPrototypeHUD();

protected:
	virtual void BeginPlay() override;

public:
	virtual void DrawHUD() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UJTSPrototypeHUDWidget> PrototypeWidget;
};
