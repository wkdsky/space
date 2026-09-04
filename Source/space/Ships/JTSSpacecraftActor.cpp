// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Ships/JTSSpacecraftActor.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "space/Components/JTSCarryComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSCharacter.h"
#include "UObject/ConstructorHelpers.h"

AJTSSpacecraftActor::AJTSSpacecraftActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpacecraftMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpacecraftMesh"));
	SpacecraftMesh->SetupAttachment(SceneRoot);
	SpacecraftMesh->SetRelativeScale3D(FVector(3.0f, 2.0f, 1.5f));
	SpacecraftMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SpacecraftMesh->SetCollisionResponseToAllChannels(ECR_Block);
	SpacecraftMesh->SetGenerateOverlapEvents(true);
	SpacecraftMesh->SetCanEverAffectNavigation(false);
	SpacecraftMesh->SetSimulatePhysics(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		SpacecraftMesh->SetStaticMesh(CubeMeshAsset.Object);
	}

	BoardingTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("BoardingTrigger"));
	BoardingTrigger->SetupAttachment(SceneRoot);
	BoardingTrigger->SetSphereRadius(360.0f);
	BoardingTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoardingTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	BoardingTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoardingTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	BoardingTrigger->SetGenerateOverlapEvents(true);
	BoardingTrigger->SetCanEverAffectNavigation(false);
	BoardingTrigger->OnComponentBeginOverlap.AddDynamic(this, &AJTSSpacecraftActor::HandleBoardingTriggerBeginOverlap);
	BoardingTrigger->OnComponentEndOverlap.AddDynamic(this, &AJTSSpacecraftActor::HandleBoardingTriggerEndOverlap);

	BoardingPoint = CreateDefaultSubobject<USceneComponent>(TEXT("BoardingPoint"));
	BoardingPoint->SetupAttachment(SceneRoot);
	BoardingPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 190.0f));

	ExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ExitPoint"));
	ExitPoint->SetupAttachment(SceneRoot);
	ExitPoint->SetRelativeLocation(FVector(0.0f, -380.0f, 105.0f));
}

void AJTSSpacecraftActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AJTSCharacter* const BoardedCharacter = BoardedPlayer.Get();
	AJTSCharacter* const NearbyCharacter = NearbyPlayer.Get();
	if (BoardedCharacter != nullptr)
	{
		BoardedCharacter->HandleSpacecraftInvalidated(this);
	}
	if (NearbyCharacter != nullptr && NearbyCharacter != BoardedCharacter)
	{
		NearbyCharacter->HandleSpacecraftInvalidated(this);
	}

	BoardedPlayer = nullptr;
	NearbyPlayer = nullptr;

	Super::EndPlay(EndPlayReason);
}

bool AJTSSpacecraftActor::TryDepositResourcesFromPawn(APawn* InteractingPawn)
{
	if (!IsEarthCollectionActive() || !IsValid(InteractingPawn))
	{
		return false;
	}

	UJTSCarryComponent* const CarryComponent = InteractingPawn->FindComponentByClass<UJTSCarryComponent>();
	if (!IsValid(CarryComponent))
	{
		return false;
	}

	TArray<EJTSResourceType> ResourcesToDeposit;
	if (!CarryComponent->TryTakeAllResources(ResourcesToDeposit) || ResourcesToDeposit.IsEmpty())
	{
		return false;
	}

	if (DepositResources(ResourcesToDeposit))
	{
		return true;
	}

	for (const EJTSResourceType ResourceType : ResourcesToDeposit)
	{
		CarryComponent->TryAddResource(ResourceType);
	}
	return false;
}

bool AJTSSpacecraftActor::TryBoardPlayer(APawn* InteractingPawn)
{
	AJTSCharacter* const Character = Cast<AJTSCharacter>(InteractingPawn);
	if (!IsEarthCollectionActive() || !IsValid(Character) || HasBoardedPlayer() || !IsPawnInBoardingRange(Character))
	{
		return false;
	}

	TryDepositResourcesFromPawn(Character);

	if (!Character->EnterBoardedState(this))
	{
		return false;
	}

	BoardedPlayer = Character;
	NearbyPlayer = Character;
	return true;
}

bool AJTSSpacecraftActor::TryDisembarkPlayer(APawn* InteractingPawn)
{
	AJTSCharacter* const Character = Cast<AJTSCharacter>(InteractingPawn);
	if (!IsEarthCollectionActive() || !IsValid(Character) || BoardedPlayer.Get() != Character)
	{
		return false;
	}

	BoardedPlayer = nullptr;
	NearbyPlayer = nullptr;
	Character->ExitBoardedState(this);
	return true;
}

bool AJTSSpacecraftActor::IsPlayerBoarded(const APawn* InteractingPawn) const
{
	return IsValid(InteractingPawn) && BoardedPlayer.Get() == InteractingPawn;
}

bool AJTSSpacecraftActor::HasBoardedPlayer() const
{
	return IsValid(BoardedPlayer);
}

AJTSCharacter* AJTSSpacecraftActor::GetBoardedPlayer() const
{
	return BoardedPlayer.Get();
}

bool AJTSSpacecraftActor::IsPawnInBoardingRange(const APawn* InteractingPawn) const
{
	if (!IsValid(InteractingPawn) || !IsValid(BoardingTrigger))
	{
		return false;
	}

	return BoardingTrigger->IsOverlappingActor(InteractingPawn)
		|| FVector::DistSquared(BoardingTrigger->GetComponentLocation(), InteractingPawn->GetActorLocation())
			<= FMath::Square(BoardingTrigger->GetScaledSphereRadius());
}

USceneComponent* AJTSSpacecraftActor::GetBoardingPoint() const
{
	return BoardingPoint.Get();
}

USceneComponent* AJTSSpacecraftActor::GetExitPoint() const
{
	return ExitPoint.Get();
}

bool AJTSSpacecraftActor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	if (!IsEarthCollectionActive() || !IsValid(InteractingPawn))
	{
		return false;
	}

	const UJTSCarryComponent* CarryComponent = InteractingPawn->FindComponentByClass<UJTSCarryComponent>();
	return IsValid(CarryComponent) && CarryComponent->GetCarriedItemCount() > 0;
}

FText AJTSSpacecraftActor::GetInteractionPrompt_Implementation(APawn* InteractingPawn) const
{
	return CanInteract_Implementation(InteractingPawn)
		? FText::FromString(TEXT("Deposit Resources"))
		: FText::GetEmpty();
}

void AJTSSpacecraftActor::Interact_Implementation(APawn* InteractingPawn)
{
	TryDepositResourcesFromPawn(InteractingPawn);
}

int32 AJTSSpacecraftActor::GetFuelCount() const
{
	return FuelCount;
}

int32 AJTSSpacecraftActor::GetWaterCount() const
{
	return WaterCount;
}

int32 AJTSSpacecraftActor::GetFoodCount() const
{
	return FoodCount;
}

int32 AJTSSpacecraftActor::GetTotalResourceCount() const
{
	return GetFuelCount() + GetWaterCount() + GetFoodCount();
}

bool AJTSSpacecraftActor::DepositResources(const TArray<EJTSResourceType>& Resources)
{
	if (!IsEarthCollectionActive() || Resources.IsEmpty())
	{
		return false;
	}

	for (const EJTSResourceType ResourceType : Resources)
	{
		if (ResourceType != EJTSResourceType::Fuel
			&& ResourceType != EJTSResourceType::Water
			&& ResourceType != EJTSResourceType::Food)
		{
			return false;
		}
	}

	for (const EJTSResourceType ResourceType : Resources)
	{
		switch (ResourceType)
		{
		case EJTSResourceType::Fuel:
			++FuelCount;
			break;

		case EJTSResourceType::Water:
			++WaterCount;
			break;

		case EJTSResourceType::Food:
			++FoodCount;
			break;

		default:
			break;
		}
	}

	OnShipResourcesChanged.Broadcast(FuelCount, WaterCount, FoodCount);
	return true;
}

bool AJTSSpacecraftActor::IsEarthCollectionActive() const
{
	UWorld* const World = GetWorld();
	const AJTSGameState* const JTSGameState = World != nullptr ? World->GetGameState<AJTSGameState>() : nullptr;
	return IsValid(JTSGameState) && JTSGameState->IsEarthCollectionActive();
}

void AJTSSpacecraftActor::HandleBoardingTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	AJTSCharacter* const Character = Cast<AJTSCharacter>(OtherActor);
	if (!IsValid(Character))
	{
		return;
	}

	NearbyPlayer = Character;
	TryDepositResourcesFromPawn(Character);
	Character->NotifySpacecraftEntered(this);
}

void AJTSSpacecraftActor::HandleBoardingTriggerEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	AJTSCharacter* const Character = Cast<AJTSCharacter>(OtherActor);
	if (!IsValid(Character) || NearbyPlayer.Get() != Character)
	{
		return;
	}

	NearbyPlayer = nullptr;
	Character->NotifySpacecraftExited(this);
}
