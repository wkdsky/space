// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Items/JTSResourcePickupActor.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "space/Components/JTSCarryComponent.h"
#include "space/Core/JTSGameState.h"
#include "UObject/ConstructorHelpers.h"

AJTSResourcePickupActor::AJTSResourcePickupActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ResourceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ResourceMesh"));
	ResourceMesh->SetupAttachment(SceneRoot);
	ResourceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ResourceMesh->SetGenerateOverlapEvents(false);
	ResourceMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		ResourceMesh->SetStaticMesh(CubeMeshAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterialAsset.Succeeded())
	{
		ResourceMesh->SetMaterial(0, BasicMaterialAsset.Object);
	}

	PickupTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("PickupTrigger"));
	PickupTrigger->SetupAttachment(SceneRoot);
	PickupTrigger->SetSphereRadius(105.0f);
	PickupTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	PickupTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupTrigger->SetGenerateOverlapEvents(false);
	PickupTrigger->SetCanEverAffectNavigation(false);

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		ApplyResourceAppearance();
	}
}

bool AJTSResourcePickupActor::CanCollectResource(APawn* InteractingPawn, int32 ResourceAmount, bool bRequireEarthCollection)
{
	if (!IsValid(InteractingPawn) || ResourceAmount <= 0)
	{
		return false;
	}

	if (bRequireEarthCollection)
	{
		UWorld* const World = InteractingPawn->GetWorld();
		const AJTSGameState* const JTSGameState = World != nullptr ? World->GetGameState<AJTSGameState>() : nullptr;
		if (!IsValid(JTSGameState) || !JTSGameState->IsEarthCollectionActive())
		{
			return false;
		}
	}

	const UJTSCarryComponent* const CarryComponent = InteractingPawn->FindComponentByClass<UJTSCarryComponent>();
	if (!IsValid(CarryComponent))
	{
		return false;
	}

	return CarryComponent->CanCarryResources(ResourceAmount);
}

bool AJTSResourcePickupActor::TryCollectResource(
	APawn* InteractingPawn,
	EJTSResourceType ResourceType,
	int32 ResourceAmount,
	bool bRequireEarthCollection)
{
	if (!CanCollectResource(InteractingPawn, ResourceAmount, bRequireEarthCollection))
	{
		return false;
	}

	UJTSCarryComponent* const CarryComponent = InteractingPawn->FindComponentByClass<UJTSCarryComponent>();
	if (!IsValid(CarryComponent))
	{
		return false;
	}

	return CarryComponent->TryAddResources(ResourceType, ResourceAmount);
}

EJTSResourceType AJTSResourcePickupActor::GetResourceType() const
{
	return ResourceType;
}

int32 AJTSResourcePickupActor::GetResourceAmount() const
{
	return FMath::Max(1, ResourceAmount);
}

FVector AJTSResourcePickupActor::GetVisualBoundsExtent() const
{
	if (IsValid(ResourceMesh) && ResourceMesh->IsRegistered())
	{
		const FVector BoundsExtent = ResourceMesh->Bounds.BoxExtent.GetAbs();
		if (!BoundsExtent.IsNearlyZero())
		{
			return BoundsExtent;
		}
	}

	return FVector(50.0f);
}

FText AJTSResourcePickupActor::GetInteractionDisplayName() const
{
	return FText::FromString(ResourceTypeToPromptName(ResourceType));
}

FVector AJTSResourcePickupActor::GetInteractionAnchorWorldLocation() const
{
	if (IsValid(ResourceMesh) && ResourceMesh->IsRegistered())
	{
		const FVector PhysicalExtent = ResourceMesh->Bounds.BoxExtent.GetAbs();
		return ResourceMesh->Bounds.Origin + FVector(0.0f, 0.0f, PhysicalExtent.Z + 24.0f);
	}

	return GetActorLocation() + FVector(0.0f, 0.0f, 74.0f);
}

void AJTSResourcePickupActor::InitializeResource(EJTSResourceType NewResourceType, int32 NewResourceAmount)
{
	ResourceType = NewResourceType;
	ResourceAmount = FMath::Max(1, NewResourceAmount);
	ApplyResourceAppearance();
}

void AJTSResourcePickupActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyResourceAppearance();
}

void AJTSResourcePickupActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ApplyResourceAppearance();
}

void AJTSResourcePickupActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyResourceAppearance();
}

bool AJTSResourcePickupActor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	if (bPickupConsumed || IsPendingKillPending() || !IsValid(InteractingPawn))
	{
		return false;
	}

	const UWorld* const World = InteractingPawn->GetWorld();
	const AJTSGameState* const JTSGameState = World != nullptr ? World->GetGameState<AJTSGameState>() : nullptr;
	return IsValid(JTSGameState) && JTSGameState->IsEarthCollectionActive();
}

FText AJTSResourcePickupActor::GetInteractionPrompt_Implementation(APawn* InteractingPawn) const
{
	if (!CanInteract_Implementation(InteractingPawn))
	{
		return FText::GetEmpty();
	}

	const FText FailureFeedback = GetFailureFeedback();
	return FailureFeedback.IsEmpty() ? FText::FromString(TEXT("[E] PICK UP")) : FailureFeedback;
}

void AJTSResourcePickupActor::Interact_Implementation(APawn* InteractingPawn)
{
	TryPickup(InteractingPawn);
}

bool AJTSResourcePickupActor::TryPickup(APawn* InteractingPawn)
{
	if (!CanInteract_Implementation(InteractingPawn))
	{
		return false;
	}

	if (!CanCollectResource(InteractingPawn, GetResourceAmount(), true))
	{
		ShowFailureFeedback(TEXT("InventoryFull"));
		return false;
	}

	bPickupConsumed = true;
	if (!TryCollectResource(InteractingPawn, ResourceType, GetResourceAmount(), true))
	{
		bPickupConsumed = false;
		ShowFailureFeedback(TEXT("InventoryFull"));
		return false;
	}

	Destroy();
	return true;
}

void AJTSResourcePickupActor::ApplyResourceAppearance()
{
	if (!IsValid(ResourceMesh))
	{
		return;
	}

	if (!IsValid(ResourceMaterial))
	{
		ResourceMaterial = ResourceMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (!IsValid(ResourceMaterial))
	{
		return;
	}

	FLinearColor ResourceColor;
	switch (ResourceType)
	{
	case EJTSResourceType::Fuel:
		ResourceColor = FLinearColor(1.0f, 0.30f, 0.03f, 1.0f);
		break;

	case EJTSResourceType::Water:
		ResourceColor = FLinearColor(0.03f, 0.40f, 1.0f, 1.0f);
		break;

	case EJTSResourceType::Food:
		ResourceColor = FLinearColor(0.20f, 0.85f, 0.10f, 1.0f);
		break;

	case EJTSResourceType::Rock:
		ResourceColor = FLinearColor(0.35f, 0.35f, 0.38f, 1.0f);
		break;

	case EJTSResourceType::Ore:
		ResourceColor = FLinearColor(0.15f, 0.65f, 0.85f, 1.0f);
		break;

	default:
		ResourceColor = FLinearColor::White;
		break;
	}

	ResourceMaterial->SetVectorParameterValue(TEXT("Color"), ResourceColor);
	ResourceMaterial->SetVectorParameterValue(TEXT("BaseColor"), ResourceColor);
	ResourceMaterial->SetVectorParameterValue(TEXT("Tint"), ResourceColor);
}

void AJTSResourcePickupActor::ShowFailureFeedback(const FString& FailureReason)
{
	FailureFeedbackText = FailureReason == TEXT("InventoryFull")
		? TEXT("INVENTORY FULL")
		: TEXT("PICKUP FAILED");
	const UWorld* const World = GetWorld();
	FailureFeedbackEndTime = World != nullptr
		? static_cast<double>(World->GetTimeSeconds()) + static_cast<double>(FMath::Max(0.1f, FailureFeedbackDuration))
		: 0.0;
}

FText AJTSResourcePickupActor::GetFailureFeedback() const
{
	const UWorld* const World = GetWorld();
	if (World != nullptr
		&& !FailureFeedbackText.IsEmpty()
		&& static_cast<double>(World->GetTimeSeconds()) < FailureFeedbackEndTime)
	{
		return FText::FromString(FailureFeedbackText);
	}

	return FText::GetEmpty();
}

FString AJTSResourcePickupActor::ResourceTypeToPromptName(EJTSResourceType InResourceType)
{
	switch (InResourceType)
	{
	case EJTSResourceType::Fuel:
		return TEXT("FUEL");

	case EJTSResourceType::Water:
		return TEXT("WATER");

	case EJTSResourceType::Food:
		return TEXT("FOOD");

	case EJTSResourceType::Rock:
		return TEXT("ROCK");

	case EJTSResourceType::Ore:
		return TEXT("ORE");

	default:
		return TEXT("RESOURCE");
	}
}
