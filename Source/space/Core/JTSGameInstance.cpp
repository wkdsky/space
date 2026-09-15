// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Core/JTSGameInstance.h"

#include "space/Systems/JTSOnlineSessionSubsystem.h"

void UJTSGameInstance::Init()
{
	Super::Init();
	SelectedAvatarColor = EJTSAvatarColor::Blue;
}

void UJTSGameInstance::Shutdown()
{
	// UGameEngine calls this after EndPlay but before it removes local players and deinitializes
	// GameInstanceSubsystems. Stop session callbacks here so none can start travel in that window.
	if (UJTSOnlineSessionSubsystem* const Online = GetSubsystem<UJTSOnlineSessionSubsystem>())
	{
		Online->BeginShutdown();
	}

	Super::Shutdown();
}

EJTSAvatarColor UJTSGameInstance::GetSelectedAvatarColor() const { return SelectedAvatarColor; }
void UJTSGameInstance::SetSelectedAvatarColor(EJTSAvatarColor NewAvatarColor) { SelectedAvatarColor = NewAvatarColor; }

FLinearColor UJTSGameInstance::GetSelectedAvatarLinearColor() const
{
	switch (SelectedAvatarColor)
	{
	case EJTSAvatarColor::Orange: return FLinearColor(1.0f, 0.34f, 0.06f, 1.0f);
	case EJTSAvatarColor::Green: return FLinearColor(0.18f, 0.85f, 0.28f, 1.0f);
	case EJTSAvatarColor::Purple: return FLinearColor(0.58f, 0.25f, 0.90f, 1.0f);
	case EJTSAvatarColor::Blue:
	default: return FLinearColor(0.10f, 0.45f, 1.0f, 1.0f);
	}
}
