// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSPassivePlanetSurfaceController.h"

#include "space/Player/JTSCharacter.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSPlanetAnchor.h"

AJTSPassivePlanetSurfaceController::AJTSPassivePlanetSurfaceController()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
}

FName AJTSPassivePlanetSurfaceController::GetPlanetId() const
{
	return PlanetId;
}

AJTSPlanetAnchor* AJTSPassivePlanetSurfaceController::GetOwningPlanet() const
{
	return OwningPlanet.Get();
}

bool AJTSPassivePlanetSurfaceController::IsSurfaceGameplayInitialized() const
{
	return bSurfaceGameplayInitialized;
}

bool AJTSPassivePlanetSurfaceController::SupportsPlanet(const AJTSPlanetAnchor* Planet) const
{
	return IsValid(Planet)
		&& !PlanetId.IsNone()
		&& PlanetId == Planet->GetPlanetId();
}

bool AJTSPassivePlanetSurfaceController::InitializeSurfaceGameplay(const FJTSSurfaceGameplayContext& Context)
{
	if (!HasAuthority()
		|| !Context.HasRequiredRuntimeActors()
		|| !SupportsPlanet(Context.Planet)
		|| !Context.Planet->HasGameplaySurface())
	{
		UE_LOG(LogTemp, Error,
			TEXT("Passive surface controller %s rejected its arrival context: Planet=%s Player=%s Spacecraft=%s."),
			*GetName(),
			*GetNameSafe(Context.Planet),
			*GetNameSafe(Context.Player),
			*GetNameSafe(Context.Spacecraft.Get()));
		return false;
	}

	OwningPlanet = Context.Planet;
	bSurfaceGameplayInitialized = true;
	for (AJTSCharacter* const Player : Context.Players)
	{
		RegisterSurfacePlayer(Player);
	}
	RegisterSurfacePlayer(Context.Player);

	UE_LOG(LogTemp, Log, TEXT("Passive surface gameplay ready: Planet=%s Controller=%s."),
		*PlanetId.ToString(), *GetName());
	return true;
}

void AJTSPassivePlanetSurfaceController::RegisterSurfacePlayer(AJTSCharacter* Player)
{
	if (!IsValid(Player))
	{
		return;
	}

	SurfacePlayers.RemoveAll([](const TWeakObjectPtr<AJTSCharacter>& Candidate)
	{
		return !Candidate.IsValid();
	});
	SurfacePlayers.AddUnique(Player);
}

void AJTSPassivePlanetSurfaceController::ShutdownSurfaceGameplay()
{
	SurfacePlayers.Reset();
	OwningPlanet = nullptr;
	bSurfaceGameplayInitialized = false;
}

bool AJTSPassivePlanetSurfaceController::IsSurfaceGameplayReady() const
{
	return bSurfaceGameplayInitialized && IsValid(OwningPlanet);
}

void AJTSPassivePlanetSurfaceController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownSurfaceGameplay();
	Super::EndPlay(EndPlayReason);
}
