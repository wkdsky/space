// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Ships/JTSSpacecraftActor.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "space/Components/JTSCarryComponent.h"
#include "space/Core/JTSGameInstance.h"
#include "space/Components/JTSMoonWrappedActorComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "space/Player/JTSCharacter.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* GetResourceTypeName(EJTSResourceType ResourceType)
	{
		switch (ResourceType)
		{
		case EJTSResourceType::Fuel:
			return TEXT("Fuel");

		case EJTSResourceType::Water:
			return TEXT("Water");

		case EJTSResourceType::Food:
			return TEXT("Food");

		case EJTSResourceType::Rock:
			return TEXT("Rock");

		case EJTSResourceType::Ore:
			return TEXT("Ore");

		default:
			return TEXT("Unknown");
		}
	}

	bool IsSupportedResourceType(EJTSResourceType ResourceType)
	{
		return ResourceType >= EJTSResourceType::Fuel && ResourceType <= EJTSResourceType::Ore;
	}
}

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

	MoonWrappedActorComponent = CreateDefaultSubobject<UJTSMoonWrappedActorComponent>(TEXT("MoonWrappedActorComponent"));
}

void AJTSSpacecraftActor::BeginPlay()
{
	Super::BeginPlay();

	if (!IsValid(BoardingTrigger))
	{
		UE_LOG(LogTemp, Warning, TEXT("Spacecraft BoardingTrigger Overlap Not Bound"));
	}
	else if (!BoardingTrigger->OnComponentBeginOverlap.IsAlreadyBound(this, &AJTSSpacecraftActor::HandleBoardingTriggerBeginOverlap))
	{
		UE_LOG(LogTemp, Warning, TEXT("Spacecraft BoardingTrigger Overlap Not Bound"));
		BoardingTrigger->OnComponentBeginOverlap.AddDynamic(this, &AJTSSpacecraftActor::HandleBoardingTriggerBeginOverlap);
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	RestoreStorageForMoonTravel();

	if (AJTSGameState* const JTSGameState = World->GetGameState<AJTSGameState>())
	{
		JTSGameState->OnGameplayPhaseChanged.AddDynamic(this, &AJTSSpacecraftActor::HandleGameplayPhaseChanged);
	}

	DepositResourcesFromOverlappingPlayers();

	if (World->GetAuthGameMode<AJTSMoonGameMode>() != nullptr
		&& MoonWrappedActorComponent != nullptr
		&& FakeMoonBendMaterial != nullptr)
	{
		MoonWrappedActorComponent->SetFakeMoonBendMaterial(FakeMoonBendMaterial);
	}
}

void AJTSSpacecraftActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SaveStorageForMoonTravel();

	if (UWorld* const World = GetWorld())
	{
		if (AJTSGameState* const JTSGameState = World->GetGameState<AJTSGameState>())
		{
			JTSGameState->OnGameplayPhaseChanged.RemoveDynamic(this, &AJTSSpacecraftActor::HandleGameplayPhaseChanged);
		}
	}

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
	return DepositPlayerResources(Cast<AJTSCharacter>(InteractingPawn));
}

bool AJTSSpacecraftActor::TryBoardPlayer(APawn* InteractingPawn)
{
	AJTSCharacter* const Character = Cast<AJTSCharacter>(InteractingPawn);
	if (!IsEarthCollectionActive() || !IsValid(Character) || HasBoardedPlayer() || !IsPawnInBoardingRange(Character))
	{
		return false;
	}

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
	if ((!IsEarthCollectionActive() && !IsMoonExplorationActive()) || !IsValid(Character) || BoardedPlayer.Get() != Character)
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

FBox AJTSSpacecraftActor::GetResourceExclusionBounds() const
{
	if (IsValid(SpacecraftMesh) && SpacecraftMesh->IsRegistered())
	{
		const float BoundsScale = FMath::Max(FMath::Abs(SpacecraftMesh->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = SpacecraftMesh->Bounds.BoxExtent.GetAbs() / BoundsScale;
		return FBox(
			SpacecraftMesh->Bounds.Origin - PhysicalExtent,
			SpacecraftMesh->Bounds.Origin + PhysicalExtent);
	}

	return GetComponentsBoundingBox(true);
}

FVector AJTSSpacecraftActor::GetNavigationMarkerWorldLocation() const
{
	if (IsValid(SpacecraftMesh) && SpacecraftMesh->IsRegistered())
	{
		const float BoundsScale = FMath::Max(FMath::Abs(SpacecraftMesh->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = SpacecraftMesh->Bounds.BoxExtent.GetAbs() / BoundsScale;
		return SpacecraftMesh->Bounds.Origin + FVector(0.0f, 0.0f, PhysicalExtent.Z + NavigationMarkerHeightOffset);
	}

	const FBox ActorBounds = GetComponentsBoundingBox(true);
	return ActorBounds.IsValid
		? FVector(ActorBounds.GetCenter().X, ActorBounds.GetCenter().Y, ActorBounds.Max.Z + NavigationMarkerHeightOffset)
		: GetActorLocation() + FVector(0.0f, 0.0f, NavigationMarkerHeightOffset);
}

bool AJTSSpacecraftActor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return IsValid(InteractingPawn)
		&& (IsEarthCollectionActive() || IsMoonExplorationActive())
		&& IsPawnInBoardingRange(InteractingPawn);
}

FText AJTSSpacecraftActor::GetInteractionPrompt_Implementation(APawn* InteractingPawn) const
{
	if (!CanInteract_Implementation(InteractingPawn))
	{
		return FText::GetEmpty();
	}

	if (IsPlayerBoarded(InteractingPawn))
	{
		return FText::FromString(TEXT("[E] EXIT"));
	}

	return IsMoonExplorationActive()
		? FText::FromString(TEXT("[E] WORKSHOP"))
		: FText::FromString(TEXT("HOLD [E] BOARD"));
}

void AJTSSpacecraftActor::Interact_Implementation(APawn* /*InteractingPawn*/)
{
}

int32 AJTSSpacecraftActor::GetResourceAmount(EJTSResourceType ResourceType) const
{
	const int32* const ResourceAmount = Storage.Find(ResourceType);
	return ResourceAmount != nullptr ? FMath::Max(0, *ResourceAmount) : 0;
}

bool AJTSSpacecraftActor::HasResource(EJTSResourceType ResourceType, int32 ResourceAmount) const
{
	return IsSupportedResourceType(ResourceType)
		&& ResourceAmount > 0
		&& GetResourceAmount(ResourceType) >= ResourceAmount;
}

bool AJTSSpacecraftActor::TryConsumeResource(EJTSResourceType ResourceType, int32 ResourceAmount)
{
	TMap<EJTSResourceType, int32> ResourceAmounts;
	ResourceAmounts.Add(ResourceType, ResourceAmount);
	return TryConsumeResourceAmounts(ResourceAmounts);
}

bool AJTSSpacecraftActor::TryConsumeResourceAmounts(const TMap<EJTSResourceType, int32>& ResourceAmounts)
{
	if (ResourceAmounts.IsEmpty())
	{
		return false;
	}

	for (const TPair<EJTSResourceType, int32>& Resource : ResourceAmounts)
	{
		if (!HasResource(Resource.Key, Resource.Value))
		{
			return false;
		}
	}

	for (const TPair<EJTSResourceType, int32>& Resource : ResourceAmounts)
	{
		int32* const StoredAmount = Storage.Find(Resource.Key);
		if (StoredAmount == nullptr)
		{
			return false;
		}

		*StoredAmount -= Resource.Value;
		if (*StoredAmount == 0)
		{
			Storage.Remove(Resource.Key);
		}
	}

	SaveStorageForMoonTravel();
	OnShipResourcesChanged.Broadcast(GetFuelCount(), GetWaterCount(), GetFoodCount());
	return true;
}

int32 AJTSSpacecraftActor::GetFuelCount() const
{
	return GetResourceAmount(EJTSResourceType::Fuel);
}

int32 AJTSSpacecraftActor::GetWaterCount() const
{
	return GetResourceAmount(EJTSResourceType::Water);
}

int32 AJTSSpacecraftActor::GetFoodCount() const
{
	return GetResourceAmount(EJTSResourceType::Food);
}

int32 AJTSSpacecraftActor::GetTotalResourceCount() const
{
	int32 TotalResourceCount = 0;
	for (const TPair<EJTSResourceType, int32>& Resource : Storage)
	{
		TotalResourceCount += FMath::Max(0, Resource.Value);
	}

	return TotalResourceCount;
}

bool AJTSSpacecraftActor::DepositResources(const TArray<EJTSResourceType>& Resources)
{
	if (Resources.IsEmpty())
	{
		return false;
	}

	TMap<EJTSResourceType, int32> ResourceAmounts;
	for (const EJTSResourceType ResourceType : Resources)
	{
		++ResourceAmounts.FindOrAdd(ResourceType);
	}

	return DepositResourceAmounts(ResourceAmounts);
}
bool AJTSSpacecraftActor::DepositResourceAmounts(const TMap<EJTSResourceType, int32>& ResourceAmounts)
{
	if (ResourceAmounts.IsEmpty())
	{
		return false;
	}

	for (const TPair<EJTSResourceType, int32>& Resource : ResourceAmounts)
	{
		if (!IsSupportedResourceType(Resource.Key) || Resource.Value <= 0)
		{
			return false;
		}
	}

	for (const TPair<EJTSResourceType, int32>& Resource : ResourceAmounts)
	{
		Storage.FindOrAdd(Resource.Key) += Resource.Value;
		UE_LOG(LogTemp, Log, TEXT("Spacecraft Deposit: Type=%s Amount=%d"), GetResourceTypeName(Resource.Key), Resource.Value);
	}

	SaveStorageForMoonTravel();
	OnShipResourcesChanged.Broadcast(GetFuelCount(), GetWaterCount(), GetFoodCount());
	return true;
}

const TMap<EJTSResourceType, int32>& AJTSSpacecraftActor::GetStorage() const
{
	return Storage;
}

bool AJTSSpacecraftActor::IsEarthCollectionActive() const
{
	UWorld* const World = GetWorld();
	const AJTSGameState* const JTSGameState = World != nullptr ? World->GetGameState<AJTSGameState>() : nullptr;
	return IsValid(JTSGameState) && JTSGameState->IsEarthCollectionActive();
}

bool AJTSSpacecraftActor::IsMoonExplorationActive() const
{
	UWorld* const World = GetWorld();
	const AJTSGameState* const JTSGameState = World != nullptr ? World->GetGameState<AJTSGameState>() : nullptr;
	return IsValid(JTSGameState) && JTSGameState->IsMoonExploration();
}

bool AJTSSpacecraftActor::DepositPlayerResources(AJTSCharacter* Player)
{
	if (!IsValid(Player))
	{
		return false;
	}

	UJTSCarryComponent* const CarryComponent = Player->FindComponentByClass<UJTSCarryComponent>();
	if (!IsValid(CarryComponent))
	{
		return false;
	}

	TMap<EJTSResourceType, int32> ResourcesToDeposit;
	if (!CarryComponent->TryTakeAllResources(ResourcesToDeposit) || ResourcesToDeposit.IsEmpty())
	{
		return false;
	}

	if (!DepositResourceAmounts(ResourcesToDeposit))
	{
		for (const TPair<EJTSResourceType, int32>& Resource : ResourcesToDeposit)
		{
			CarryComponent->TryAddResources(Resource.Key, Resource.Value);
		}
		return false;
	}

	return true;
}

void AJTSSpacecraftActor::DepositResourcesFromOverlappingPlayers()
{
	if (!IsValid(BoardingTrigger))
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	BoardingTrigger->GetOverlappingActors(OverlappingActors, AJTSCharacter::StaticClass());
	for (AActor* const OverlappingActor : OverlappingActors)
	{
		AJTSCharacter* const Character = Cast<AJTSCharacter>(OverlappingActor);
		if (!IsValid(Character))
		{
			continue;
		}

		NearbyPlayer = Character;
		DepositPlayerResources(Character);
		Character->NotifySpacecraftEntered(this);
	}
}

void AJTSSpacecraftActor::RestoreStorageForMoonTravel()
{
	UWorld* const World = GetWorld();
	if (World == nullptr || World->GetAuthGameMode<AJTSMoonGameMode>() == nullptr)
	{
		return;
	}

	UJTSGameInstance* const GameInstance = World->GetGameInstance<UJTSGameInstance>();
	if (IsValid(GameInstance) && GameInstance->HasPersistedSpacecraftStorage())
	{
		Storage = GameInstance->GetPersistedSpacecraftStorage();
		UE_LOG(
			LogTemp,
			Log,
			TEXT("JumpToSpace Moon Storage Restored: Fuel=%d Water=%.1f Food=%.1f Rock=%d Ore=%d"),
			GetResourceAmount(EJTSResourceType::Fuel),
			static_cast<float>(GetResourceAmount(EJTSResourceType::Water)),
			static_cast<float>(GetResourceAmount(EJTSResourceType::Food)),
			GetResourceAmount(EJTSResourceType::Rock),
			GetResourceAmount(EJTSResourceType::Ore));
	}
}

void AJTSSpacecraftActor::SaveStorageForMoonTravel() const
{
	UWorld* const World = GetWorld();
	if (World == nullptr || World->GetAuthGameMode<AJTSMoonGameMode>() == nullptr)
	{
		return;
	}

	if (UJTSGameInstance* const GameInstance = World->GetGameInstance<UJTSGameInstance>())
	{
		GameInstance->SetPersistedSpacecraftStorage(Storage);
	}
}

void AJTSSpacecraftActor::HandleGameplayPhaseChanged(EJTSGameplayPhase NewGameplayPhase)
{
	if (NewGameplayPhase == EJTSGameplayPhase::EarthCollection
		|| NewGameplayPhase == EJTSGameplayPhase::MoonExploration)
	{
		DepositResourcesFromOverlappingPlayers();
	}
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

	UE_LOG(LogTemp, Log, TEXT("Spacecraft BoardingTrigger Enter Player=%s"), *Character->GetName());

	NearbyPlayer = Character;
	DepositPlayerResources(Character);
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
