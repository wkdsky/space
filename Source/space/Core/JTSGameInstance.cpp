// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSGameInstance.h"

void UJTSGameInstance::Init()
{
	Super::Init();

	SelectedAvatarColor = EJTSAvatarColor::Blue;
	ExpeditionFood = 0.0f;
	ExpeditionWater = 0.0f;
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

float UJTSGameInstance::GetExpeditionFood() const
{
	return ExpeditionFood;
}

float UJTSGameInstance::GetExpeditionWater() const
{
	return ExpeditionWater;
}

void UJTSGameInstance::SetExpeditionSupplies(float NewFood, float NewWater)
{
	const float NormalizedFood = NormalizeExpeditionResource(NewFood);
	const float NormalizedWater = NormalizeExpeditionResource(NewWater);
	const bool bFoodChanged = !FMath::IsNearlyEqual(ExpeditionFood, NormalizedFood, 0.0001f);
	const bool bWaterChanged = !FMath::IsNearlyEqual(ExpeditionWater, NormalizedWater, 0.0001f);

	if (!bFoodChanged && !bWaterChanged)
	{
		return;
	}

	ExpeditionFood = NormalizedFood;
	ExpeditionWater = NormalizedWater;
	OnExpeditionSuppliesChanged.Broadcast(ExpeditionFood, ExpeditionWater);
}

void UJTSGameInstance::ConsumeExpeditionSupplies(float FoodAmount, float WaterAmount)
{
	const float SafeFoodAmount = FMath::Max(0.0f, FoodAmount);
	const float SafeWaterAmount = FMath::Max(0.0f, WaterAmount);
	SetExpeditionSupplies(ExpeditionFood - SafeFoodAmount, ExpeditionWater - SafeWaterAmount);
}

bool UJTSGameInstance::IsFoodDepleted() const
{
	return ExpeditionFood <= 0.0f;
}

bool UJTSGameInstance::IsWaterDepleted() const
{
	return ExpeditionWater <= 0.0f;
}

float UJTSGameInstance::NormalizeExpeditionResource(float Value)
{
	if (!FMath::IsFinite(Value) || Value <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::RoundToFloat(Value * 10.0f) / 10.0f;
}
