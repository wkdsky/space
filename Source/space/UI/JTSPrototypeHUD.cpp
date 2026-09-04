// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/UI/JTSPrototypeHUD.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "space/UI/JTSPrototypeHUDWidget.h"

AJTSPrototypeHUD::AJTSPrototypeHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AJTSPrototypeHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = GetOwningPlayerController();
	if (PlayerController == nullptr && GetWorld() != nullptr)
	{
		PlayerController = GetWorld()->GetFirstPlayerController();
	}
	if (PlayerController == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Jump to Space HUD could not create its native widget because the owning PlayerController is unavailable."));
		return;
	}

	PrototypeWidget = CreateWidget<UJTSPrototypeHUDWidget>(PlayerController, UJTSPrototypeHUDWidget::StaticClass());
	if (PrototypeWidget != nullptr)
	{
		PrototypeWidget->AddToViewport(100);
	}
}

void AJTSPrototypeHUD::DrawHUD()
{
	Super::DrawHUD();
}
