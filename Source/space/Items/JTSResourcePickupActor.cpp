// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Items/JTSResourcePickupActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "space/Components/JTSCarryComponent.h"
#include "UObject/ConstructorHelpers.h"

AJTSResourcePickupActor::AJTSResourcePickupActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ResourceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ResourceMesh"));
	ResourceMesh->SetupAttachment(SceneRoot);
	ResourceMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ResourceMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ResourceMesh->SetGenerateOverlapEvents(true);
	ResourceMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		ResourceMesh->SetStaticMesh(CubeMeshAsset.Object);
	}
}

bool AJTSResourcePickupActor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	if (!IsValid(InteractingPawn))
	{
		return false;
	}

	const UJTSCarryComponent* CarryComponent = InteractingPawn->FindComponentByClass<UJTSCarryComponent>();
	return IsValid(CarryComponent) && CarryComponent->CanCarryResource(ResourceType);
}

FText AJTSResourcePickupActor::GetInteractionPrompt_Implementation(APawn* InteractingPawn) const
{
	switch (ResourceType)
	{
	case EJTSResourceType::Fuel:
		return FText::FromString(TEXT("Pick up Fuel"));

	case EJTSResourceType::Water:
		return FText::FromString(TEXT("Pick up Water"));

	case EJTSResourceType::Food:
		return FText::FromString(TEXT("Pick up Food"));

	default:
		return FText::FromString(TEXT("Pick up Resource"));
	}
}

void AJTSResourcePickupActor::Interact_Implementation(APawn* InteractingPawn)
{
	if (!IsValid(InteractingPawn))
	{
		return;
	}

	UJTSCarryComponent* CarryComponent = InteractingPawn->FindComponentByClass<UJTSCarryComponent>();
	if (IsValid(CarryComponent) && CarryComponent->TryAddResource(ResourceType))
	{
		Destroy();
	}
}
