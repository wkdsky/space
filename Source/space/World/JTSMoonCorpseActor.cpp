#include "space/World/JTSMoonCorpseActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "space/World/JTSPlanetSurfaceAnchor.h"
#include "space/World/JTSSurfacePlacementBounds.h"

AJTSMoonCorpseActor::AJTSMoonCorpseActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
	bReplicates = true;
	SetReplicateMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	CorpseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CorpseMesh"));
	CorpseMesh->SetupAttachment(SceneRoot);
	CorpseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CorpseMesh->SetGenerateOverlapEvents(false);
	CorpseMesh->SetCanEverAffectNavigation(false);
}

bool AJTSMoonCorpseActor::SnapToPlanetSurfaceAnchor(AJTSPlanetSurfaceAnchor* SurfaceAnchor)
{
	if (!HasAuthority() || !IsValid(SurfaceAnchor) || !IsValid(CorpseMesh->GetStaticMesh()))
	{
		return false;
	}

	FTransform SurfaceTransform;
	if (!SurfaceAnchor->GetSurfaceTransform(SurfaceTransform))
	{
		return false;
	}
	const FVector SurfaceUp = SurfaceTransform.GetUnitAxis(EAxis::Z).GetSafeNormal();
	if (SurfaceUp.IsNearlyZero())
	{
		return false;
	}

	SetActorRotation(SurfaceTransform.Rotator(), ETeleportType::TeleportPhysics);
	CorpseMesh->UpdateBounds();
	FJTSSurfaceVisualProjectionBounds VisualBounds;
	if (!JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(
		CorpseMesh, GetActorLocation(), SurfaceUp, VisualBounds))
	{
		return false;
	}
	SetActorLocation(
		SurfaceTransform.GetLocation() + SurfaceUp *
			(VisualBounds.GetRootToLowestSupport() + FMath::Max(0.0f, PlanetSurfaceClearance)),
		false, nullptr, ETeleportType::TeleportPhysics);
	bUsesRealPlanetSurfacePlacement = true;
	return true;
}

bool AJTSMoonCorpseActor::IsUsingRealPlanetSurfacePlacement() const
{
	return bUsesRealPlanetSurfacePlacement;
}

void AJTSMoonCorpseActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSMoonCorpseActor, bUsesRealPlanetSurfacePlacement);
}
