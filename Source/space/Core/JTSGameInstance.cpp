// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSGameInstance.h"

void UJTSGameInstance::Init()
{
	Super::Init();

	SelectedAvatarColor = EJTSAvatarColor::Blue;
	UE_LOG(LogTemp, Log, TEXT("Jump to Space GameInstance initialized."));
}

EJTSAvatarColor UJTSGameInstance::GetSelectedAvatarColor() const
{
	return SelectedAvatarColor;
}

void UJTSGameInstance::SetSelectedAvatarColor(EJTSAvatarColor NewAvatarColor)
{
	SelectedAvatarColor = NewAvatarColor;
}

FLinearColor UJTSGameInstance::GetSelectedAvatarLinearColor() const
{
	switch (SelectedAvatarColor)
	{
	case EJTSAvatarColor::Orange:
		return FLinearColor(1.0f, 0.34f, 0.06f, 1.0f);

	case EJTSAvatarColor::Green:
		return FLinearColor(0.18f, 0.85f, 0.28f, 1.0f);

	case EJTSAvatarColor::Blue:
	default:
		return FLinearColor(0.10f, 0.45f, 1.0f, 1.0f);
	}
}
