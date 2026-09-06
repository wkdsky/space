#include "space/Items/JTSWorldPickupActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionQueryParams.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "space/Components/JTSCarryComponent.h"
#include "space/Components/JTSMoonWrappedActorComponent.h"
#include "space/Components/JTSPlayerEquipmentComponent.h"
#include "space/Items/JTSResourceType.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Systems/JTSWorldPickupRegistrySubsystem.h"
#include "space/World/JTSMoonResourceActor.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float DefaultPickupDropUpwardSpeed = 340.0f;
	constexpr float DefaultPickupDropHorizontalSpeed = 150.0f;
	constexpr float PickupDropSpawnHeight = 85.0f;
	constexpr float PickupDropHorizontalDamping = 3.5f;
	constexpr float PickupDropMaximumDuration = 12.0f;

	FVector GetPickupGravityAcceleration(UWorld* World, APawn* SafetyPawn)
	{
		if (const ACharacter* const Character = Cast<ACharacter>(SafetyPawn))
		{
			if (const UCharacterMovementComponent* const MovementComponent = Character->GetCharacterMovement())
			{
				FVector GravityDirection = MovementComponent->GetGravityDirection();
				if (!GravityDirection.Normalize())
				{
					GravityDirection = FVector::DownVector;
				}
				return GravityDirection * FMath::Abs(MovementComponent->GetGravityZ());
			}
		}

		return World != nullptr ? FVector(0.0f, 0.0f, World->GetGravityZ()) : FVector::ZeroVector;
	}

	void GetPickupDropLaunchSpeeds(UWorld* World, float& OutUpwardSpeed, float& OutHorizontalSpeed)
	{
		OutUpwardSpeed = DefaultPickupDropUpwardSpeed;
		OutHorizontalSpeed = DefaultPickupDropHorizontalSpeed;
		if (const AJTSMoonGameMode* const MoonGameMode = World != nullptr
			? World->GetAuthGameMode<AJTSMoonGameMode>()
			: nullptr)
		{
			OutUpwardSpeed = MoonGameMode->GetPickupDropUpwardSpeed();
			OutHorizontalSpeed = MoonGameMode->GetPickupDropHorizontalSpeed();
		}
	}

	bool TryGetResourceType(EJTSWorldPickupItemType ItemType, EJTSResourceType& OutResourceType)
	{
		switch (ItemType)
		{
		case EJTSWorldPickupItemType::Fuel:
			OutResourceType = EJTSResourceType::Fuel;
			return true;

		case EJTSWorldPickupItemType::Water:
			OutResourceType = EJTSResourceType::Water;
			return true;

		case EJTSWorldPickupItemType::Food:
			OutResourceType = EJTSResourceType::Food;
			return true;

		case EJTSWorldPickupItemType::Rock:
			OutResourceType = EJTSResourceType::Rock;
			return true;

		case EJTSWorldPickupItemType::Ore:
			OutResourceType = EJTSResourceType::Ore;
			return true;

		default:
			return false;
		}
	}

	bool TryGetEquipmentType(EJTSWorldPickupItemType ItemType, EJTSEquipmentType& OutEquipmentType)
	{
		switch (ItemType)
		{
		case EJTSWorldPickupItemType::Pickaxe:
			OutEquipmentType = EJTSEquipmentType::Pickaxe;
			return true;

		case EJTSWorldPickupItemType::Backpack:
			OutEquipmentType = EJTSEquipmentType::Backpack;
			return true;

		default:
			return false;
		}
	}
}

AJTSWorldPickupActor* AJTSWorldPickupActor::SpawnGroundedPickup(
	UWorld* World,
	EJTSWorldPickupItemType NewItemType,
	const FVector& Origin,
	APawn* SafetyPawn,
	AActor* SourceActor,
	const FVector& PreferredDirection)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	const FVector GravityAcceleration = GetPickupGravityAcceleration(World, SafetyPawn);
	FVector DropUpDirection = -GravityAcceleration.GetSafeNormal();
	if (DropUpDirection.IsNearlyZero())
	{
		DropUpDirection = FVector::UpVector;
	}

	float DropUpwardSpeed = DefaultPickupDropUpwardSpeed;
	float DropHorizontalSpeed = DefaultPickupDropHorizontalSpeed;
	GetPickupDropLaunchSpeeds(World, DropUpwardSpeed, DropHorizontalSpeed);

	FVector SafePreferredDirection = PreferredDirection;
	SafePreferredDirection.Z = 0.0f;
	SafePreferredDirection = SafePreferredDirection.GetSafeNormal();
	TArray<AJTSWorldPickupActor*> ExistingPickups;
	if (UJTSWorldPickupRegistrySubsystem* const Registry = World->GetSubsystem<UJTSWorldPickupRegistrySubsystem>())
	{
		Registry->GetRegisteredPickups(ExistingPickups);
	}

	for (int32 AttemptIndex = 0; AttemptIndex < 12; ++AttemptIndex)
	{
		FVector HorizontalDirection;
		if (!SafePreferredDirection.IsNearlyZero() && AttemptIndex < 4)
		{
			const float SideAngle = FMath::DegreesToRadians(FMath::FRandRange(-36.0f, 36.0f));
			HorizontalDirection = SafePreferredDirection.RotateAngleAxis(SideAngle, FVector::UpVector);
		}
		else
		{
			const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
			HorizontalDirection = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		}

		const float Distance = !SafePreferredDirection.IsNearlyZero() && AttemptIndex < 4
			? FMath::FRandRange(140.0f, 230.0f)
			: FMath::FRandRange(70.0f, 170.0f);
		const FVector CandidateXY = Origin + HorizontalDirection * Distance;

		if (IsValid(SafetyPawn))
		{
			const float PawnRadius = Cast<ACharacter>(SafetyPawn) != nullptr
				? Cast<ACharacter>(SafetyPawn)->GetSimpleCollisionRadius()
				: 45.0f;
			if (FVector::DistSquared2D(CandidateXY, SafetyPawn->GetActorLocation()) < FMath::Square(PawnRadius + 85.0f))
			{
				continue;
			}
		}

		bool bOverlapsShip = false;
		for (TActorIterator<AJTSSpacecraftActor> ShipIt(World); ShipIt; ++ShipIt)
		{
			const AJTSSpacecraftActor* const Spacecraft = *ShipIt;
			if (!IsValid(Spacecraft))
			{
				continue;
			}

			const FBox ShipBounds = Spacecraft->GetResourceExclusionBounds();
			if (ShipBounds.IsValid)
			{
				const FVector ShipCenter = ShipBounds.GetCenter();
				const FVector ShipExtent = ShipBounds.GetExtent() + FVector(70.0f, 70.0f, 0.0f);
				if (FMath::Abs(CandidateXY.X - ShipCenter.X) <= ShipExtent.X
					&& FMath::Abs(CandidateXY.Y - ShipCenter.Y) <= ShipExtent.Y)
				{
					bOverlapsShip = true;
					break;
				}
			}
		}
		if (bOverlapsShip)
		{
			continue;
		}

		bool bTooCloseToPickup = false;
		for (const AJTSWorldPickupActor* const ExistingPickup : ExistingPickups)
		{
			if (IsValid(ExistingPickup)
				&& FVector::DistSquared2D(CandidateXY, ExistingPickup->GetActorLocation()) < FMath::Square(60.0f))
			{
				bTooCloseToPickup = true;
				break;
			}
		}
		if (bTooCloseToPickup)
		{
			continue;
		}

		FCollisionQueryParams GroundTraceParams(SCENE_QUERY_STAT(JTSWorldPickupGroundTrace), false, SourceActor);
		if (SourceActor != nullptr)
		{
			GroundTraceParams.AddIgnoredActor(SourceActor);
		}
		if (SafetyPawn != nullptr)
		{
			GroundTraceParams.AddIgnoredActor(SafetyPawn);
		}
		for (TActorIterator<AJTSSpacecraftActor> ShipIt(World); ShipIt; ++ShipIt)
		{
			if (IsValid(*ShipIt))
			{
				GroundTraceParams.AddIgnoredActor(*ShipIt);
			}
		}
		for (TActorIterator<AJTSMoonResourceActor> ResourceIt(World); ResourceIt; ++ResourceIt)
		{
			if (IsValid(*ResourceIt))
			{
				GroundTraceParams.AddIgnoredActor(*ResourceIt);
			}
		}
		for (AJTSWorldPickupActor* const ExistingPickup : ExistingPickups)
		{
			if (IsValid(ExistingPickup))
			{
				GroundTraceParams.AddIgnoredActor(ExistingPickup);
			}
		}

		FHitResult GroundHit;
		const FVector TraceStart(CandidateXY.X, CandidateXY.Y, Origin.Z + 1200.0f);
		const FVector TraceEnd(CandidateXY.X, CandidateXY.Y, Origin.Z - 2500.0f);
		if (!World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, GroundTraceParams)
			|| !GroundHit.bBlockingHit)
		{
			continue;
		}

		const FTransform SpawnTransform(
			FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f),
			GroundHit.ImpactPoint + DropUpDirection * PickupDropSpawnHeight);
		AJTSWorldPickupActor* const Pickup = World->SpawnActorDeferred<AJTSWorldPickupActor>(
			AJTSWorldPickupActor::StaticClass(),
			SpawnTransform,
			SourceActor,
			SafetyPawn,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!IsValid(Pickup))
		{
			continue;
		}

		Pickup->InitializeItem(NewItemType);
		Pickup->FinishSpawning(SpawnTransform);

		const FVector GroundDirection = GravityAcceleration.IsNearlyZero()
			? FVector::DownVector
			: GravityAcceleration.GetSafeNormal();
		const float VisualSupportDistance = Pickup->GetVisualSupportDistance(GroundDirection);
		Pickup->SetActorLocation(
			GroundHit.ImpactPoint + DropUpDirection * (VisualSupportDistance + PickupDropSpawnHeight),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);

		FVector HorizontalLaunchDirection = GravityAcceleration.IsNearlyZero()
			? HorizontalDirection
			: FVector::VectorPlaneProject(HorizontalDirection, GravityAcceleration.GetSafeNormal()).GetSafeNormal();
		if (HorizontalLaunchDirection.IsNearlyZero())
		{
			HorizontalLaunchDirection = HorizontalDirection;
		}
		const float HorizontalSpeed = DropHorizontalSpeed * FMath::FRandRange(0.65f, 1.0f);
		Pickup->StartDropMotion(
			DropUpDirection * DropUpwardSpeed + HorizontalLaunchDirection * HorizontalSpeed,
			GravityAcceleration,
			GroundHit.ImpactPoint,
			SourceActor,
			SafetyPawn);
		return Pickup;
	}

	return nullptr;
}

AJTSWorldPickupActor::AJTSWorldPickupActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetActorTickEnabled(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(SceneRoot);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetGenerateOverlapEvents(false);
	PickupMesh->SetSimulatePhysics(false);
	PickupMesh->SetCanEverAffectNavigation(false);
	PickupMesh->SetCastShadow(false);
	PickupMesh->bCastDynamicShadow = false;

	MoonWrappedActorComponent = CreateDefaultSubobject<UJTSMoonWrappedActorComponent>(TEXT("MoonWrappedActorComponent"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshAsset.Succeeded())
	{
		RockMesh = SphereMeshAsset.Object;
		PickupMesh->SetStaticMesh(RockMesh);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshAsset(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMeshAsset.Succeeded())
	{
		OreMesh = ConeMeshAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		EquipmentMesh = CubeMeshAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FakeMoonBendMaterialAsset(TEXT("/Game/Space/Materials/FakeMoon/MI_JTSFakeMoon_Prop.MI_JTSFakeMoon_Prop"));
	if (FakeMoonBendMaterialAsset.Succeeded())
	{
		FakeMoonBendMaterial = FakeMoonBendMaterialAsset.Object;
		PickupMesh->SetMaterial(0, FakeMoonBendMaterialAsset.Object);
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (BasicMaterialAsset.Succeeded())
		{
			PickupMesh->SetMaterial(0, BasicMaterialAsset.Object);
		}
	}
}

EJTSWorldPickupItemType AJTSWorldPickupActor::GetItemType() const
{
	return ItemType;
}

FText AJTSWorldPickupActor::GetItemDisplayName() const
{
	return FText::FromString(ItemTypeToString(ItemType));
}

FVector AJTSWorldPickupActor::GetInteractionTargetWorldLocation() const
{
	if (IsValid(PickupMesh) && PickupMesh->IsRegistered())
	{
		return PickupMesh->Bounds.Origin;
	}

	return GetActorLocation();
}

FVector AJTSWorldPickupActor::GetInteractionAnchorWorldLocation() const
{
	if (IsValid(PickupMesh) && PickupMesh->IsRegistered())
	{
		const float BoundsScale = FMath::Max(FMath::Abs(PickupMesh->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = PickupMesh->Bounds.BoxExtent.GetAbs() / BoundsScale;
		return PickupMesh->Bounds.Origin + FVector(0.0f, 0.0f, PhysicalExtent.Z + 22.0f);
	}

	return GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
}

void AJTSWorldPickupActor::InitializeItem(EJTSWorldPickupItemType NewItemType)
{
	ItemType = NewItemType;
	bPickupConsumed = false;
	ApplyItemAppearance();
}

FVector AJTSWorldPickupActor::GetVisualBoundsExtent() const
{
	if (IsValid(PickupMesh) && PickupMesh->IsRegistered())
	{
		const float BoundsScale = FMath::Max(FMath::Abs(PickupMesh->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector BoundsExtent = PickupMesh->Bounds.BoxExtent.GetAbs() / BoundsScale;
		if (!BoundsExtent.IsNearlyZero())
		{
			return BoundsExtent;
		}
	}

	return FVector(25.0f);
}

float AJTSWorldPickupActor::GetVisualSupportDistance(const FVector& GravityDirection) const
{
	const FVector VisualExtent = GetVisualBoundsExtent();
	FVector SafeGravityDirection = GravityDirection;
	if (!SafeGravityDirection.Normalize())
	{
		SafeGravityDirection = FVector::DownVector;
	}
	return FMath::Abs(SafeGravityDirection.X) * VisualExtent.X
		+ FMath::Abs(SafeGravityDirection.Y) * VisualExtent.Y
		+ FMath::Abs(SafeGravityDirection.Z) * VisualExtent.Z
		+ 3.0f;
}

void AJTSWorldPickupActor::AdjustToGround(const FVector& GroundHitLocation)
{
	if (!IsValid(PickupMesh) || !PickupMesh->IsRegistered())
	{
		return;
	}

	PickupMesh->UpdateBounds();
	const FVector BoundsExtent = GetVisualBoundsExtent();
	SetActorLocation(
		FVector(GetActorLocation().X, GetActorLocation().Y, GroundHitLocation.Z + BoundsExtent.Z + 3.0f),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	UpdateMoonWrappedLogicalPosition();
}

void AJTSWorldPickupActor::StartDropMotion(
	const FVector& InitialVelocity,
	const FVector& GravityAcceleration,
	const FVector& SafeGroundLocation,
	AActor* SourceActor,
	APawn* SafetyPawn)
{
	DropVelocity = InitialVelocity;
	DropGravityAcceleration = GravityAcceleration;
	PlannedGroundLocation = SafeGroundLocation;
	DropElapsedSeconds = 0.0f;
	BuildDropTraceIgnoredActors(SourceActor, SafetyPawn);
	UpdateMoonWrappedLogicalPosition();

	if (DropGravityAcceleration.IsNearlyZero())
	{
		AdjustToGround(PlannedGroundLocation);
		DropVelocity = FVector::ZeroVector;
		bIsDropping = false;
		SetActorTickEnabled(false);
		DropTraceIgnoredActors.Reset();
		UE_LOG(LogTemp, Log, TEXT("JumpToSpace Pickup Landed: Item=%s Reason=ZeroGravity"), *ItemTypeToString(ItemType));
		return;
	}

	bIsDropping = true;
	SetActorTickEnabled(true);
	UE_LOG(
		LogTemp,
		Log,
		TEXT("JumpToSpace Pickup Drop: Item=%s Source=%s"),
		*ItemTypeToString(ItemType),
		*GetNameSafe(SourceActor));
}

bool AJTSWorldPickupActor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return !bIsDropping && !bPickupConsumed && !IsPendingKillPending() && IsValid(InteractingPawn);
}

FText AJTSWorldPickupActor::GetInteractionPrompt_Implementation(APawn* InteractingPawn) const
{
	if (!CanInteract_Implementation(InteractingPawn))
	{
		return FText::GetEmpty();
	}

	const FText FailureFeedback = GetFailureFeedback();
	return FailureFeedback.IsEmpty() ? FText::FromString(TEXT("[E] PICK UP")) : FailureFeedback;
}

void AJTSWorldPickupActor::Interact_Implementation(APawn* InteractingPawn)
{
	if (!CanInteract_Implementation(InteractingPawn))
	{
		return;
	}

	bPickupConsumed = true;
	FString FailureReason;
	if (TryPickup(InteractingPawn, FailureReason))
	{
		UE_LOG(LogTemp, Log, TEXT("JumpToSpace Pickup: Item=%s Success=true"), *ItemTypeToString(ItemType));
		Destroy();
		return;
	}

	bPickupConsumed = false;
	ShowFailureFeedback(FailureReason);
	UE_LOG(LogTemp, Log, TEXT("JumpToSpace Pickup: Item=%s Success=false Reason=%s"), *ItemTypeToString(ItemType), *FailureReason);
}

void AJTSWorldPickupActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyItemAppearance();

	if (UJTSWorldPickupRegistrySubsystem* const Registry = GetWorld() != nullptr
		? GetWorld()->GetSubsystem<UJTSWorldPickupRegistrySubsystem>()
		: nullptr)
	{
		Registry->RegisterPickup(this);
	}

	if (MoonWrappedActorComponent != nullptr)
	{
		UMaterialInterface* const BendMaterial = PickupMaterial != nullptr
			? PickupMaterial.Get()
			: FakeMoonBendMaterial.Get();
		if (BendMaterial != nullptr)
		{
			MoonWrappedActorComponent->SetFakeMoonBendMaterial(BendMaterial);
		}
	}
}

void AJTSWorldPickupActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bIsDropping = false;
	SetActorTickEnabled(false);
	DropTraceIgnoredActors.Reset();

	if (UJTSWorldPickupRegistrySubsystem* const Registry = GetWorld() != nullptr
		? GetWorld()->GetSubsystem<UJTSWorldPickupRegistrySubsystem>()
		: nullptr)
	{
		Registry->UnregisterPickup(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AJTSWorldPickupActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyItemAppearance();
}

void AJTSWorldPickupActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIsDropping)
	{
		SetActorTickEnabled(false);
		return;
	}

	const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	DropElapsedSeconds += SafeDeltaSeconds;
	if (DropElapsedSeconds >= PickupDropMaximumDuration)
	{
		SettleDropOnGround(PlannedGroundLocation);
		return;
	}

	const FVector GravityDirection = DropGravityAcceleration.GetSafeNormal();
	if (GravityDirection.IsNearlyZero())
	{
		SettleDropOnGround(PlannedGroundLocation);
		return;
	}

	DropVelocity += DropGravityAcceleration * SafeDeltaSeconds;
	const float GravitySpeed = FVector::DotProduct(DropVelocity, GravityDirection);
	const FVector HorizontalVelocity = DropVelocity - GravityDirection * GravitySpeed;
	DropVelocity = HorizontalVelocity * FMath::Exp(-PickupDropHorizontalDamping * SafeDeltaSeconds)
		+ GravityDirection * GravitySpeed;

	const FVector CurrentLocation = GetActorLocation();
	const FVector NextLocation = CurrentLocation + DropVelocity * SafeDeltaSeconds;
	const float VisualSupportDistance = GetVisualSupportDistance(GravityDirection);
	const FVector TraceStart = CurrentLocation + GravityDirection * FMath::Max(0.0f, VisualSupportDistance - 2.0f);
	const FVector TraceEnd = NextLocation + GravityDirection * (VisualSupportDistance + 2.0f);

	FHitResult GroundHit;
	if (TraceDropGround(TraceStart, TraceEnd, GroundHit))
	{
		SettleDropOnGround(GroundHit.ImpactPoint);
		return;
	}

	SetActorLocation(NextLocation, false, nullptr, ETeleportType::TeleportPhysics);
	UpdateMoonWrappedLogicalPosition();
}

bool AJTSWorldPickupActor::TraceDropGround(
	const FVector& TraceStart,
	const FVector& TraceEnd,
	FHitResult& OutGroundHit) const
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	FCollisionQueryParams GroundTraceParams(SCENE_QUERY_STAT(JTSWorldPickupDropGroundTrace), false, this);
	GroundTraceParams.AddIgnoredActor(this);
	for (const TWeakObjectPtr<AActor>& IgnoredActor : DropTraceIgnoredActors)
	{
		if (const AActor* const Actor = IgnoredActor.Get())
		{
			GroundTraceParams.AddIgnoredActor(Actor);
		}
	}

	return World->LineTraceSingleByChannel(OutGroundHit, TraceStart, TraceEnd, ECC_Visibility, GroundTraceParams)
		&& OutGroundHit.bBlockingHit;
}

void AJTSWorldPickupActor::BuildDropTraceIgnoredActors(AActor* SourceActor, APawn* SafetyPawn)
{
	DropTraceIgnoredActors.Reset();
	auto AddIgnoredActor = [this](AActor* Actor)
	{
		if (IsValid(Actor) && Actor != this)
		{
			DropTraceIgnoredActors.Add(Actor);
		}
	};

	AddIgnoredActor(SourceActor);
	AddIgnoredActor(SafetyPawn);

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<AJTSSpacecraftActor> ShipIt(World); ShipIt; ++ShipIt)
	{
		AddIgnoredActor(*ShipIt);
	}
	for (TActorIterator<AJTSMoonResourceActor> ResourceIt(World); ResourceIt; ++ResourceIt)
	{
		AddIgnoredActor(*ResourceIt);
	}
	for (TActorIterator<APawn> PawnIt(World); PawnIt; ++PawnIt)
	{
		AddIgnoredActor(*PawnIt);
	}
}

void AJTSWorldPickupActor::SettleDropOnGround(const FVector& GroundHitLocation)
{
	FVector GravityDirection = DropGravityAcceleration.GetSafeNormal();
	if (GravityDirection.IsNearlyZero())
	{
		GravityDirection = FVector::DownVector;
	}

	const float VisualSupportDistance = GetVisualSupportDistance(GravityDirection);
	SetActorLocation(
		GroundHitLocation - GravityDirection * VisualSupportDistance,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	UpdateMoonWrappedLogicalPosition();

	DropVelocity = FVector::ZeroVector;
	bIsDropping = false;
	SetActorTickEnabled(false);
	DropTraceIgnoredActors.Reset();
	UE_LOG(LogTemp, Log, TEXT("JumpToSpace Pickup Landed: Item=%s"), *ItemTypeToString(ItemType));
}

void AJTSWorldPickupActor::UpdateMoonWrappedLogicalPosition()
{
	if (MoonWrappedActorComponent != nullptr && MoonWrappedActorComponent->IsMoonWrappingEnabled())
	{
		MoonWrappedActorComponent->SetLogicalPositionFromWorld();
	}
}

bool AJTSWorldPickupActor::TryPickup(APawn* InteractingPawn, FString& OutFailureReason)
{
	EJTSResourceType ResourceType = EJTSResourceType::Rock;
	if (TryGetResourceType(ItemType, ResourceType))
	{
		UJTSCarryComponent* const CarryComponent = InteractingPawn->FindComponentByClass<UJTSCarryComponent>();
		if (!IsValid(CarryComponent))
		{
			OutFailureReason = TEXT("InventoryUnavailable");
			return false;
		}
		if (!CarryComponent->TryAddResource(ResourceType))
		{
			OutFailureReason = TEXT("InventoryFull");
			return false;
		}

		return true;
	}

	EJTSEquipmentType EquipmentType = EJTSEquipmentType::None;
	if (TryGetEquipmentType(ItemType, EquipmentType))
	{
		UJTSPlayerEquipmentComponent* const EquipmentComponent = InteractingPawn->FindComponentByClass<UJTSPlayerEquipmentComponent>();
		if (!IsValid(EquipmentComponent))
		{
			OutFailureReason = TEXT("EquipmentUnavailable");
			return false;
		}
		if (EquipmentComponent->HasEquippedItem(EquipmentType))
		{
			OutFailureReason = TEXT("AlreadyEquipped");
			return false;
		}
		if (!EquipmentComponent->TryEquipItem(EquipmentType))
		{
			OutFailureReason = TEXT("EquipmentFull");
			return false;
		}

		return true;
	}

	OutFailureReason = TEXT("UnsupportedItem");
	return false;
}

bool AJTSWorldPickupActor::IsResourceItem() const
{
	EJTSResourceType ResourceType = EJTSResourceType::Rock;
	return TryGetResourceType(ItemType, ResourceType);
}

void AJTSWorldPickupActor::ConfigureAppearance()
{
	if (!IsValid(PickupMesh))
	{
		return;
	}

	UStaticMesh* DesiredMesh = RockMesh.Get();
	FVector DesiredScale(0.32f);
	if (ItemType == EJTSWorldPickupItemType::Ore)
	{
		DesiredMesh = OreMesh.Get();
		DesiredScale = FVector(0.28f, 0.28f, 0.42f);
	}
	else if (!IsResourceItem())
	{
		DesiredMesh = EquipmentMesh.Get();
		DesiredScale = ItemType == EJTSWorldPickupItemType::Backpack
			? FVector(0.33f, 0.24f, 0.38f)
			: FVector(0.42f, 0.12f, 0.10f);
	}

	if (IsValid(DesiredMesh) && PickupMesh->GetStaticMesh() != DesiredMesh)
	{
		PickupMesh->SetStaticMesh(DesiredMesh);
	}
	PickupMesh->SetRelativeScale3D(DesiredScale);
	PickupMesh->UpdateBounds();
}

void AJTSWorldPickupActor::ApplyItemAppearance()
{
	if (!IsValid(PickupMesh))
	{
		return;
	}

	ConfigureAppearance();
	if (!IsValid(PickupMaterial))
	{
		PickupMaterial = PickupMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (!IsValid(PickupMaterial))
	{
		return;
	}

	FLinearColor ItemColor = FLinearColor(0.35f, 0.35f, 0.40f, 1.0f);
	switch (ItemType)
	{
	case EJTSWorldPickupItemType::Ore:
		ItemColor = FLinearColor(0.10f, 0.62f, 0.84f, 1.0f);
		break;

	case EJTSWorldPickupItemType::Pickaxe:
		ItemColor = FLinearColor(0.90f, 0.68f, 0.18f, 1.0f);
		break;

	case EJTSWorldPickupItemType::Backpack:
		ItemColor = FLinearColor(0.24f, 0.76f, 0.42f, 1.0f);
		break;

	default:
		break;
	}

	PickupMaterial->SetVectorParameterValue(TEXT("ResourceColor"), ItemColor);
	PickupMaterial->SetVectorParameterValue(TEXT("Color"), ItemColor);
	PickupMaterial->SetVectorParameterValue(TEXT("BaseColor"), ItemColor);
	PickupMaterial->SetVectorParameterValue(TEXT("Tint"), ItemColor);
}

void AJTSWorldPickupActor::ShowFailureFeedback(const FString& FailureReason)
{
	FailureFeedbackText = FailureReason;
	if (FailureReason == TEXT("InventoryFull"))
	{
		FailureFeedbackText = TEXT("INVENTORY FULL");
	}
	else if (FailureReason == TEXT("InventoryUnavailable"))
	{
		FailureFeedbackText = TEXT("INVENTORY UNAVAILABLE");
	}
	else if (FailureReason == TEXT("EquipmentUnavailable"))
	{
		FailureFeedbackText = TEXT("EQUIPMENT UNAVAILABLE");
	}
	else if (FailureReason == TEXT("AlreadyEquipped"))
	{
		FailureFeedbackText = TEXT("ALREADY EQUIPPED");
	}
	else if (FailureReason == TEXT("EquipmentFull"))
	{
		FailureFeedbackText = TEXT("EQUIPMENT FULL");
	}
	else if (FailureFeedbackText.IsEmpty())
	{
		FailureFeedbackText = TEXT("CANNOT PICK UP");
	}

	const UWorld* const World = GetWorld();
	FailureFeedbackEndTime = World != nullptr
		? static_cast<double>(World->GetTimeSeconds()) + static_cast<double>(FMath::Max(0.1f, FailureFeedbackDuration))
		: 0.0;
}

FText AJTSWorldPickupActor::GetFailureFeedback() const
{
	const UWorld* const World = GetWorld();
	if (!FailureFeedbackText.IsEmpty()
		&& World != nullptr
		&& static_cast<double>(World->GetTimeSeconds()) < FailureFeedbackEndTime)
	{
		return FText::FromString(FailureFeedbackText);
	}

	return FText::GetEmpty();
}

FString AJTSWorldPickupActor::ItemTypeToString(EJTSWorldPickupItemType InItemType)
{
	switch (InItemType)
	{
	case EJTSWorldPickupItemType::Fuel:
		return TEXT("FUEL");

	case EJTSWorldPickupItemType::Water:
		return TEXT("WATER");

	case EJTSWorldPickupItemType::Food:
		return TEXT("FOOD");

	case EJTSWorldPickupItemType::Rock:
		return TEXT("ROCK");

	case EJTSWorldPickupItemType::Ore:
		return TEXT("ORE");

	case EJTSWorldPickupItemType::Pickaxe:
		return TEXT("PICKAXE");

	case EJTSWorldPickupItemType::Backpack:
		return TEXT("BACKPACK");

	default:
		return TEXT("UNKNOWN");
	}
}
