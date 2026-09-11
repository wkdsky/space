// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSPlanetArrivalAnchor.h"

#include "Components/SceneComponent.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSpaceWorldManager.h"

AJTSPlanetArrivalAnchor::AJTSPlanetArrivalAnchor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PlayerArrivalPoint = CreateDefaultSubobject<USceneComponent>(TEXT("PlayerArrivalPoint"));
	PlayerArrivalPoint->SetupAttachment(SceneRoot);

	SpacecraftArrivalPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SpacecraftArrivalPoint"));
	SpacecraftArrivalPoint->SetupAttachment(SceneRoot);
	SpacecraftArrivalPoint->SetRelativeLocation(FVector(900.0f, 0.0f, 450.0f));
}

AJTSPlanetAnchor* AJTSPlanetArrivalAnchor::GetPlanetAnchor() const
{
	if (IsValid(PlanetAnchor))
	{
		return PlanetAnchor.Get();
	}

	if (PlanetId.IsNone())
	{
		return nullptr;
	}

	if (AJTSSpaceWorldManager* const SpaceWorldManager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
	{
		return SpaceWorldManager->FindPlanetById(PlanetId);
	}

	return nullptr;
}

bool AJTSPlanetArrivalAnchor::IsArrivalEnabled() const
{
	return bEnabled;
}

int32 AJTSPlanetArrivalAnchor::GetPriority() const
{
	return Priority;
}

FTransform AJTSPlanetArrivalAnchor::GetPlayerArrivalTransform() const
{
	return IsValid(PlayerArrivalPoint) ? PlayerArrivalPoint->GetComponentTransform() : GetActorTransform();
}

FTransform AJTSPlanetArrivalAnchor::GetSpacecraftArrivalTransform() const
{
	return IsValid(SpacecraftArrivalPoint) ? SpacecraftArrivalPoint->GetComponentTransform() : GetActorTransform();
}
