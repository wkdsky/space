// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Interaction/JTS_TestInteractableActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AJTS_TestInteractableActor::AJTS_TestInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	TestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TestMesh"));
	SetRootComponent(TestMesh);
	TestMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TestMesh->SetCollisionResponseToAllChannels(ECR_Block);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		TestMesh->SetStaticMesh(CubeMeshAsset.Object);
	}
}

bool AJTS_TestInteractableActor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return IsValid(InteractingPawn);
}

FText AJTS_TestInteractableActor::GetInteractionPrompt_Implementation(APawn* InteractingPawn) const
{
	return FText::FromString(TEXT("Press E to Test Interaction"));
}

void AJTS_TestInteractableActor::Interact_Implementation(APawn* InteractingPawn)
{
	UE_LOG(LogTemp, Log, TEXT("Test Interaction Success"));
}
