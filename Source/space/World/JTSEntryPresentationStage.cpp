// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSEntryPresentationStage.h"

#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AJTSEntryPresentationStage::AJTSEntryPresentationStage()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SceneRoot->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* const Cube = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;
	UStaticMesh* const Sphere = SphereMesh.Succeeded() ? SphereMesh.Object : nullptr;
	UStaticMesh* const Cylinder = CylinderMesh.Succeeded() ? CylinderMesh.Object : nullptr;
	// Keep the world presentation on the free right-hand side of the front-end composition.
	const FVector PresentationOffset(-520.0f, 1020.0f, 0.0f);

	LaunchDeck = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaunchDeck"));
	LaunchDeck->SetupAttachment(SceneRoot);
	LaunchDeck->SetStaticMesh(Cube);
	LaunchDeck->SetRelativeLocation(FVector(380.0f, -20.0f, -100.0f) + PresentationOffset);
	LaunchDeck->SetRelativeScale3D(FVector(7.0f, 5.5f, 0.45f));
	LaunchDeck->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LaunchDeck->SetMobility(EComponentMobility::Movable);

	Planet = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Planet"));
	Planet->SetupAttachment(SceneRoot);
	Planet->SetStaticMesh(Sphere);
	Planet->SetRelativeLocation(FVector(880.0f, 680.0f, 670.0f) + PresentationOffset);
	Planet->SetRelativeScale3D(FVector(4.5f));
	Planet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Planet->SetMobility(EComponentMobility::Movable);

	OrbitBand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrbitBand"));
	OrbitBand->SetupAttachment(SceneRoot);
	OrbitBand->SetStaticMesh(Cylinder);
	OrbitBand->SetRelativeLocation(FVector(880.0f, 680.0f, 670.0f) + PresentationOffset);
	OrbitBand->SetRelativeRotation(FRotator(72.0f, 25.0f, 0.0f));
	OrbitBand->SetRelativeScale3D(FVector(5.7f, 5.7f, 0.06f));
	OrbitBand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OrbitBand->SetMobility(EComponentMobility::Movable);

	SpacecraftHull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpacecraftHull"));
	SpacecraftHull->SetupAttachment(SceneRoot);
	SpacecraftHull->SetStaticMesh(Cube);
	SpacecraftHull->SetRelativeLocation(FVector(450.0f, -130.0f, 310.0f) + PresentationOffset);
	SpacecraftHull->SetRelativeRotation(FRotator(0.0f, 15.0f, 18.0f));
	SpacecraftHull->SetRelativeScale3D(FVector(4.3f, 1.5f, 0.9f));
	SpacecraftHull->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpacecraftHull->SetMobility(EComponentMobility::Movable);

	SpacecraftNose = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpacecraftNose"));
	SpacecraftNose->SetupAttachment(SceneRoot);
	SpacecraftNose->SetStaticMesh(Cylinder);
	SpacecraftNose->SetRelativeLocation(FVector(1000.0f, 35.0f, 495.0f) + PresentationOffset);
	SpacecraftNose->SetRelativeRotation(FRotator(0.0f, 105.0f, 0.0f));
	SpacecraftNose->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.7f));
	SpacecraftNose->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpacecraftNose->SetMobility(EComponentMobility::Movable);

	Beacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
	Beacon->SetupAttachment(SceneRoot);
	Beacon->SetStaticMesh(Cylinder);
	Beacon->SetRelativeLocation(FVector(-280.0f, -620.0f, 140.0f) + PresentationOffset);
	Beacon->SetRelativeScale3D(FVector(0.7f, 0.7f, 3.5f));
	Beacon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beacon->SetMobility(EComponentMobility::Movable);

	KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(SceneRoot);
	KeyLight->SetRelativeLocation(FVector(220.0f, -850.0f, 860.0f) + PresentationOffset);
	KeyLight->SetLightColor(FLinearColor(0.12f, 0.67f, 1.0f));
	KeyLight->SetIntensity(72000.0f);
	KeyLight->SetAttenuationRadius(3300.0f);

	AccentLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("AccentLight"));
	AccentLight->SetupAttachment(SceneRoot);
	AccentLight->SetRelativeLocation(FVector(1240.0f, 560.0f, 650.0f) + PresentationOffset);
	AccentLight->SetLightColor(FLinearColor(1.0f, 0.30f, 0.08f));
	AccentLight->SetIntensity(54000.0f);
	AccentLight->SetAttenuationRadius(2700.0f);

	PresentationCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PresentationCamera"));
	PresentationCamera->SetupAttachment(SceneRoot);
	PresentationCamera->SetRelativeLocation(FVector(-2180.0f, -1260.0f, 920.0f));
	PresentationCamera->SetRelativeRotation(FRotator(-11.0f, 26.0f, 0.0f));
	PresentationCamera->FieldOfView = 59.0f;
	PresentationCamera->bAutoActivate = true;
}
