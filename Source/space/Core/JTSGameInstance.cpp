// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSGameInstance.h"

namespace
{
	int32 GetResourceAmount(const TMap<EJTSResourceType, int32>& Storage, EJTSResourceType ResourceType)
	{
		const int32* const Amount = Storage.Find(ResourceType);
		return Amount != nullptr ? FMath::Max(0, *Amount) : 0;
	}
}

void UJTSGameInstance::Init()
{
	Super::Init();

	SelectedAvatarColor = EJTSAvatarColor::Blue;
	PersistedSpacecraftStorage.Reset();
	bHasPersistedSpacecraftStorage = false;
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

int32 UJTSGameInstance::GetPersistedSpacecraftResourceAmount(EJTSResourceType ResourceType) const
{
	return GetResourceAmount(PersistedSpacecraftStorage, ResourceType);
}

bool UJTSGameInstance::HasPersistedSpacecraftStorage() const
{
	return bHasPersistedSpacecraftStorage;
}

void UJTSGameInstance::SetPersistedSpacecraftStorage(const TMap<EJTSResourceType, int32>& NewStorage)
{
	TMap<EJTSResourceType, int32> SanitizedStorage;
	for (const TPair<EJTSResourceType, int32>& Resource : NewStorage)
	{
		if (IsSupportedResourceType(Resource.Key) && Resource.Value > 0)
		{
			SanitizedStorage.Add(Resource.Key, Resource.Value);
		}
	}

	PersistedSpacecraftStorage = MoveTemp(SanitizedStorage);
	bHasPersistedSpacecraftStorage = true;
	OnExpeditionSuppliesChanged.Broadcast(
		static_cast<float>(GetPersistedSpacecraftResourceAmount(EJTSResourceType::Food)),
		static_cast<float>(GetPersistedSpacecraftResourceAmount(EJTSResourceType::Water)));
}

const TMap<EJTSResourceType, int32>& UJTSGameInstance::GetPersistedSpacecraftStorage() const
{
	return PersistedSpacecraftStorage;
}

void UJTSGameInstance::ClearPersistedSpacecraftStorage()
{
	const bool bHadStorage = bHasPersistedSpacecraftStorage || !PersistedSpacecraftStorage.IsEmpty();
	PersistedSpacecraftStorage.Reset();
	bHasPersistedSpacecraftStorage = false;

	if (bHadStorage)
	{
		OnExpeditionSuppliesChanged.Broadcast(0.0f, 0.0f);
	}
}

float UJTSGameInstance::GetExpeditionFood() const
{
	return static_cast<float>(GetPersistedSpacecraftResourceAmount(EJTSResourceType::Food));
}

float UJTSGameInstance::GetExpeditionWater() const
{
	return static_cast<float>(GetPersistedSpacecraftResourceAmount(EJTSResourceType::Water));
}

void UJTSGameInstance::SetExpeditionSupplies(float NewFood, float NewWater)
{
	TMap<EJTSResourceType, int32> UpdatedStorage = PersistedSpacecraftStorage;
	UpdatedStorage.Add(EJTSResourceType::Food, FMath::RoundToInt(NormalizeExpeditionResource(NewFood)));
	UpdatedStorage.Add(EJTSResourceType::Water, FMath::RoundToInt(NormalizeExpeditionResource(NewWater)));
	SetPersistedSpacecraftStorage(UpdatedStorage);
}

void UJTSGameInstance::ConsumeExpeditionSupplies(float FoodAmount, float WaterAmount)
{
	SetExpeditionSupplies(
		GetExpeditionFood() - FMath::Max(0.0f, FoodAmount),
		GetExpeditionWater() - FMath::Max(0.0f, WaterAmount));
}

bool UJTSGameInstance::IsFoodDepleted() const
{
	return GetPersistedSpacecraftResourceAmount(EJTSResourceType::Food) <= 0;
}

bool UJTSGameInstance::IsWaterDepleted() const
{
	return GetPersistedSpacecraftResourceAmount(EJTSResourceType::Water) <= 0;
}

float UJTSGameInstance::NormalizeExpeditionResource(float Value)
{
	if (!FMath::IsFinite(Value) || Value <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::RoundToFloat(Value * 10.0f) / 10.0f;
}

bool UJTSGameInstance::IsSupportedResourceType(EJTSResourceType ResourceType)
{
	switch (ResourceType)
	{
	case EJTSResourceType::Fuel:
	case EJTSResourceType::Water:
	case EJTSResourceType::Food:
	case EJTSResourceType::Rock:
	case EJTSResourceType::Ore:
		return true;

	default:
		return false;
	}
}
