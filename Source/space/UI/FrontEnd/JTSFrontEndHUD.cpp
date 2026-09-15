// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/FrontEnd/JTSFrontEndHUD.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "space/UI/FrontEnd/JTSFrontEndRootWidget.h"
#include "TimerManager.h"

void AJTSFrontEndHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!EnsureRootWidget())
	{
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				RootWidgetRetryTimer,
				this,
				&AJTSFrontEndHUD::RetryRootWidgetCreation,
				0.1f,
				true);
		}
	}
}

void AJTSFrontEndHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RootWidgetRetryTimer);
	}

	if (RootWidget != nullptr)
	{
		RootWidget->RemoveFromParent();
		RootWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

bool AJTSFrontEndHUD::EnsureRootWidget()
{
	APlayerController* const Controller = GetOwningPlayerController();
	if (Controller == nullptr || !Controller->IsLocalController())
	{
		return false;
	}

	if (RootWidget == nullptr)
	{
		const TSubclassOf<UJTSFrontEndRootWidget> WidgetClass = FrontEndRootWidgetClass.IsNull()
			? UJTSFrontEndRootWidget::StaticClass()
			: FrontEndRootWidgetClass.LoadSynchronous();
		RootWidget = CreateWidget<UJTSFrontEndRootWidget>(Controller, WidgetClass != nullptr ? WidgetClass : TSubclassOf<UJTSFrontEndRootWidget>(UJTSFrontEndRootWidget::StaticClass()));
		if (RootWidget == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Jump to Space Front End could not create its root widget for local controller %s."), *GetNameSafe(Controller));
			return false;
		}
	}

	if (!RootWidget->IsInViewport())
	{
		if (!RootWidget->AddToPlayerScreen(100))
		{
			// This fallback is only for unusual viewport implementations; the widget is still owned by this local player.
			RootWidget->AddToViewport(100);
		}
	}
	RootWidget->SetVisibility(ESlateVisibility::Visible);
	ApplyFrontEndInput(Controller);
	UE_LOG(LogTemp, Log, TEXT("Jump to Space Front End root %s is visible for local controller %s."), *GetNameSafe(RootWidget->GetClass()), *GetNameSafe(Controller));
	return true;
}

void AJTSFrontEndHUD::RetryRootWidgetCreation()
{
	++RootWidgetCreationAttempts;
	if (EnsureRootWidget() || RootWidgetCreationAttempts >= 30)
	{
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RootWidgetRetryTimer);
		}

		if (RootWidget == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Jump to Space Front End gave up waiting for a local PlayerController after %d attempts."), RootWidgetCreationAttempts);
		}
	}
}

void AJTSFrontEndHUD::ApplyFrontEndInput(APlayerController* Controller) const
{
	if (Controller == nullptr || RootWidget == nullptr)
	{
		return;
	}

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetWidgetToFocus(RootWidget->TakeWidget());
	Controller->SetInputMode(InputMode);
	Controller->bShowMouseCursor = true;
	Controller->bEnableClickEvents = true;
	Controller->bEnableMouseOverEvents = true;
	Controller->SetIgnoreMoveInput(true);
	Controller->SetIgnoreLookInput(true);
}
