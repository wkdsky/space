// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSMoonPlanetActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "space/Core/JTSMoonGameMode.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float BasicSphereRadius = 50.0f;
}

AJTSMoonPlanetActor::AJTSMoonPlanetActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PlanetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlanetMesh"));
	PlanetMesh->SetupAttachment(SceneRoot);
	PlanetMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PlanetMesh->SetCollisionResponseToAllChannels(ECR_Block);
	PlanetMesh->SetGenerateOverlapEvents(false);
	PlanetMesh->SetCanEverAffectNavigation(false);
	PlanetMesh->SetSimulatePhysics(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshAsset.Succeeded())
	{
		PlanetMesh->SetStaticMesh(SphereMeshAsset.Object);
	}

	UpdateVisualScale();
}

void AJTSMoonPlanetActor::BeginPlay()
{
	Super::BeginPlay();

	if (GetWorld() != nullptr && GetWorld()->GetAuthGameMode<AJTSMoonGameMode>() != nullptr)
	{
		if (PlanetMesh != nullptr)
		{
			PlanetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		SetActorHiddenInGame(true);
		UE_LOG(LogTemp, Warning, TEXT("Legacy AJTSMoonPlanetActor is ignored in AJTSMoonGameMode. Use AJTSMoonFakeWorldActor and AJTSMoonLoopGroundActor."));
	}
}

float AJTSMoonPlanetActor::GetPlanetRadius() const
{
	return FMath::IsFinite(PlanetRadius) ? FMath::Max(1.0f, PlanetRadius) : 1.0f;
}

FVector AJTSMoonPlanetActor::GetPlanetCenter() const
{
	return GetActorLocation();
}

FVector AJTSMoonPlanetActor::GetGravityDirection(const FVector& WorldLocation) const
{
	const FVector ToPlanetCenter = GetPlanetCenter() - WorldLocation;
	return ToPlanetCenter.IsNearlyZero() ? FVector::DownVector : ToPlanetCenter.GetSafeNormal();
}

void AJTSMoonPlanetActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateVisualScale();
}

void AJTSMoonPlanetActor::UpdateVisualScale()
{
	if (PlanetMesh != nullptr)
	{
		const float UniformScale = GetPlanetRadius() / BasicSphereRadius;
		PlanetMesh->SetRelativeScale3D(FVector(UniformScale));
	}
}
