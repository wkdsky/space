// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSPlanetSurfaceAnchor.h"

#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSpaceWorldManager.h"

AJTSPlanetSurfaceAnchor::AJTSPlanetSurfaceAnchor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AJTSPlanetSurfaceAnchor::BeginPlay()
{
	Super::BeginPlay();

	if (bSnapOnBeginPlay)
	{
		SnapToPlanetSurface();
	}
}

AJTSPlanetAnchor* AJTSPlanetSurfaceAnchor::GetPlanetAnchor() const
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

bool AJTSPlanetSurfaceAnchor::GetSurfaceTransform(FTransform& OutSurfaceTransform) const
{
	return BuildSurfaceTransform(OutSurfaceTransform);
}

bool AJTSPlanetSurfaceAnchor::SnapToPlanetSurface()
{
	AJTSPlanetAnchor* const Planet = GetPlanetAnchor();
	if (!IsValid(Planet) || !Planet->HasGameplaySurface())
	{
		return false;
	}

	FTransform SurfaceTransform;
	if (!BuildSurfaceTransform(SurfaceTransform))
	{
		return false;
	}

	const FQuat TargetRotation = bAlignRotationToSurface
		? SurfaceTransform.GetRotation()
		: GetActorQuat();
	SetActorLocationAndRotation(
		SurfaceTransform.GetLocation(),
		TargetRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	DrawPlacementDebug(SurfaceTransform);
	return true;
}

bool AJTSPlanetSurfaceAnchor::BuildSurfaceTransform(FTransform& OutSurfaceTransform) const
{
	AJTSPlanetAnchor* const Planet = GetPlanetAnchor();
	if (!IsValid(Planet) || !Planet->HasGameplaySurface())
	{
		return false;
	}

	FJTSPlanetSurfaceFrame SurfaceFrame;
	if (!Planet->GetSurfaceFrameAt(GetActorLocation(), GetActorForwardVector(), SurfaceFrame))
	{
		return false;
	}

	OutSurfaceTransform = SurfaceFrame.Transform;
	OutSurfaceTransform.SetLocation(
		SurfaceFrame.Location + SurfaceFrame.Up * FMath::Max(0.0f, SurfaceClearance));
	return true;
}

void AJTSPlanetSurfaceAnchor::DrawPlacementDebug(const FTransform& SurfaceTransform) const
{
	if (!bDebugPlanetSurfacePlacement)
	{
		return;
	}

	UWorld* const World = GetWorld();
	AJTSPlanetAnchor* const Planet = GetPlanetAnchor();
	if (World == nullptr || !IsValid(Planet))
	{
		return;
	}

	FJTSPlanetSurfaceHit SurfaceHit;
	if (Planet->TraceToSurface(GetActorLocation(), SurfaceHit))
	{
		DrawDebugLine(World, SurfaceHit.TraceStart, SurfaceHit.TraceEnd, FColor::Cyan, false, 8.0f, 0, 1.0f);
	}

	const FVector Location = SurfaceTransform.GetLocation();
	DrawDebugPoint(World, Location, 18.0f, FColor::Yellow, false, 8.0f);
	DrawDebugDirectionalArrow(World, Location, Location + SurfaceTransform.GetUnitAxis(EAxis::Z) * 180.0f, 28.0f, FColor::Green, false, 8.0f, 0, 2.0f);
	DrawDebugDirectionalArrow(World, Location, Location + SurfaceTransform.GetUnitAxis(EAxis::X) * 180.0f, 28.0f, FColor::Red, false, 8.0f, 0, 2.0f);
	DrawDebugDirectionalArrow(World, Location, Location + SurfaceTransform.GetUnitAxis(EAxis::Y) * 180.0f, 28.0f, FColor::Blue, false, 8.0f, 0, 2.0f);
}
