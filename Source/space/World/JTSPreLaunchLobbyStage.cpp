// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSPreLaunchLobbyStage.h"

#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AJTSPreLaunchLobbyStage::AJTSPreLaunchLobbyStage()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* const Cube = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;
	UStaticMesh* const Cylinder = CylinderMesh.Succeeded() ? CylinderMesh.Object : nullptr;

	HangarFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HangarFloor"));
	HangarFloor->SetupAttachment(SceneRoot);
	HangarFloor->SetStaticMesh(Cube);
	HangarFloor->SetRelativeLocation(FVector(0.0f, 0.0f, -75.0f));
	HangarFloor->SetRelativeScale3D(FVector(18.0f, 14.0f, 0.5f));
	HangarFloor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// This actor is spawned by the server, so its dressing must remain movable relative to its root.
	HangarFloor->SetMobility(EComponentMobility::Movable);

	BackWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackWall"));
	BackWall->SetupAttachment(SceneRoot);
	BackWall->SetStaticMesh(Cube);
	BackWall->SetRelativeLocation(FVector(900.0f, 0.0f, 650.0f));
	BackWall->SetRelativeScale3D(FVector(0.5f, 14.0f, 7.0f));
	BackWall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackWall->SetMobility(EComponentMobility::Movable);

	ShipSilhouette = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipSilhouette"));
	ShipSilhouette->SetupAttachment(SceneRoot);
	ShipSilhouette->SetStaticMesh(Cube);
	ShipSilhouette->SetRelativeLocation(FVector(450.0f, 0.0f, 430.0f));
	ShipSilhouette->SetRelativeRotation(FRotator(0.0f, 0.0f, 18.0f));
	ShipSilhouette->SetRelativeScale3D(FVector(5.8f, 2.0f, 1.1f));
	ShipSilhouette->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PresentationOffsets = {
		FVector(-360.0f, -420.0f, 20.0f),
		FVector(-360.0f, -140.0f, 20.0f),
		FVector(-360.0f, 140.0f, 20.0f),
		FVector(-360.0f, 420.0f, 20.0f)
	};
	for (int32 Index = 0; Index < PresentationOffsets.Num(); ++Index)
	{
		UStaticMeshComponent* const Pad = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PresentationPad%d"), Index + 1));
		Pad->SetupAttachment(SceneRoot);
		Pad->SetStaticMesh(Cylinder);
		Pad->SetRelativeLocation(PresentationOffsets[Index] + FVector(0.0f, 0.0f, -60.0f));
		Pad->SetRelativeScale3D(FVector(1.4f, 1.4f, 0.15f));
		Pad->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PresentationPads.Add(Pad);
	}

	KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(SceneRoot);
	KeyLight->SetRelativeLocation(FVector(-300.0f, -700.0f, 850.0f));
	KeyLight->SetLightColor(FLinearColor(0.18f, 0.62f, 1.0f));
	KeyLight->SetIntensity(28000.0f);
	KeyLight->AttenuationRadius = 2200.0f;

	AccentLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("AccentLight"));
	AccentLight->SetupAttachment(SceneRoot);
	AccentLight->SetRelativeLocation(FVector(650.0f, 650.0f, 700.0f));
	AccentLight->SetLightColor(FLinearColor(1.0f, 0.32f, 0.10f));
	AccentLight->SetIntensity(18000.0f);
	AccentLight->AttenuationRadius = 1800.0f;

	LobbyCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("LobbyCamera"));
	LobbyCamera->SetupAttachment(SceneRoot);
	LobbyCamera->SetRelativeLocation(FVector(-2000.0f, -1600.0f, 900.0f));
	LobbyCamera->SetRelativeRotation(FRotator(-14.0f, 36.0f, 0.0f));
	LobbyCamera->FieldOfView = 58.0f;
	LobbyCamera->bAutoActivate = true;
}

FVector AJTSPreLaunchLobbyStage::GetPresentationPosition(int32 SlotIndex) const
{
	if (PresentationOffsets.IsValidIndex(SlotIndex))
	{
		return GetActorTransform().TransformPosition(PresentationOffsets[SlotIndex]);
	}
	return GetActorLocation();
}
