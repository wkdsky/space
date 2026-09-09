// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSMoonPlanetActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "space/World/JTSSpaceWorldManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float BasicSphereRadius = 50.0f;
}

AJTSMoonPlanetActor::AJTSMoonPlanetActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

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

	if (!DisableForActiveMoonSurface())
	{
		SetActorTickEnabled(MayReceiveMoonSurfaceController());
	}
}

void AJTSMoonPlanetActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (DisableForActiveMoonSurface() || !MayReceiveMoonSurfaceController())
	{
		SetActorTickEnabled(false);
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

bool AJTSMoonPlanetActor::DisableForActiveMoonSurface()
{
	if (bDisabledForActiveMoonSurface)
	{
		return true;
	}

	if (const AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this);
		IsValid(SurfaceController) && SurfaceController->OwnsSurfaceActor(this))
	{
		if (PlanetMesh != nullptr)
		{
			PlanetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		SetActorHiddenInGame(true);
		bDisabledForActiveMoonSurface = true;
		UE_LOG(LogTemp, Warning, TEXT("Legacy AJTSMoonPlanetActor is ignored on the active Moon surface. Use AJTSMoonWorldActor and AJTSMoonLoopGroundActor."));
		return true;
	}

	return false;
}

bool AJTSMoonPlanetActor::MayReceiveMoonSurfaceController() const
{
	const UWorld* const World = GetWorld();
	return AJTSSpaceWorldManager::FindSpaceWorldManager(this) != nullptr
		|| (World != nullptr && World->GetAuthGameMode<AJTSMoonGameMode>() != nullptr);
}

void AJTSMoonPlanetActor::UpdateVisualScale()
{
	if (PlanetMesh != nullptr)
	{
		const float UniformScale = GetPlanetRadius() / BasicSphereRadius;
		PlanetMesh->SetRelativeScale3D(FVector(UniformScale));
	}
}
