// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "JTSFrontEndHUD.generated.h"

class UJTSFrontEndRootWidget;

/** HUD responsible solely for creating native front-end UI on the local client. */
UCLASS(Config = Game)
class SPACE_API AJTSFrontEndHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool EnsureRootWidget();
	void RetryRootWidgetCreation();
	void ApplyFrontEndInput(class APlayerController* Controller) const;

	UPROPERTY(Transient)
	TObjectPtr<UJTSFrontEndRootWidget> RootWidget;

	/** Project-owned UMG composition point. Native root remains a safe fallback if this asset is unavailable. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UJTSFrontEndRootWidget> FrontEndRootWidgetClass;

	FTimerHandle RootWidgetRetryTimer;
	int32 RootWidgetCreationAttempts = 0;
};
