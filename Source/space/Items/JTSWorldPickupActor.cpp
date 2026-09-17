#include "space/Items/JTSWorldPickupActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSPlayerEquipmentComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSResourceType.h"
#include "space/Player/JTSCharacter.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Systems/JTSWorldPickupRegistrySubsystem.h"
#include "space/World/JTSMoonResourceActor.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "space/World/JTSMoonSurfaceGameplaySettings.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSurfacePlacementBounds.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float DefaultPickupDropUpwardSpeed = 340.0f;
	constexpr float DefaultPickupDropHorizontalSpeed = 150.0f;
	constexpr float PickupDropSpawnHeight = 85.0f;
	constexpr float PickupDropHorizontalDamping = 3.5f;
	constexpr float PickupDropMaximumDuration = 12.0f;

	bool IsInCurrentMoonSurface(const AJTSMoonSurfaceController* SurfaceController, const AActor* Candidate)
	{
		return !IsValid(SurfaceController) || SurfaceController->OwnsSurfaceActor(Candidate);
	}

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
		if (const AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(World))
		{
			if (const IJTSMoonSurfaceGameplaySettings* const MoonSettings = SurfaceController->GetMoonSettings())
			{
				OutUpwardSpeed = MoonSettings->GetPickupDropUpwardSpeed();
				OutHorizontalSpeed = MoonSettings->GetPickupDropHorizontalSpeed();
			}
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

		case EJTSWorldPickupItemType::MoonAntCorpse:
			OutResourceType = EJTSResourceType::MoonAntCorpse;
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

		case EJTSWorldPickupItemType::Knife:
			OutEquipmentType = EJTSEquipmentType::Knife;
			return true;

		case EJTSWorldPickupItemType::Axe:
			OutEquipmentType = EJTSEquipmentType::Axe;
			return true;

		default:
			return false;
		}
	}

	AJTSPlanetAnchor* FindRealSurfacePlanet(const UObject* WorldContextObject)
	{
		const AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(WorldContextObject);
		return IsValid(SurfaceController) && SurfaceController->IsUsingRealPlanetSurfaceGameplay()
			? SurfaceController->GetOwningPlanet()
			: nullptr;
	}

	FVector BuildTangentDirection(
		const AJTSPlanetAnchor* Planet,
		const FVector& Origin,
		const FVector& PreferredDirection,
		const float RandomAngleRadians)
	{
		if (!IsValid(Planet))
		{
			return FVector::ZeroVector;
		}

		FJTSPlanetSurfaceFrame SurfaceFrame;
		if (!Planet->GetSurfaceFrameAt(Origin, PreferredDirection, SurfaceFrame))
		{
			return Planet->ProjectDirectionToSurfaceTangent(PreferredDirection, Origin);
		}

		const FVector BaseDirection = Planet->ProjectDirectionToSurfaceTangent(PreferredDirection, Origin);
		if (!BaseDirection.IsNearlyZero())
		{
			return FQuat(SurfaceFrame.Up, RandomAngleRadians).RotateVector(BaseDirection).GetSafeNormal();
		}

		return (SurfaceFrame.Forward * FMath::Cos(RandomAngleRadians)
			+ SurfaceFrame.Right * FMath::Sin(RandomAngleRadians)).GetSafeNormal();
	}
}

AJTSWorldPickupActor* AJTSWorldPickupActor::SpawnInitialGroundedPickup(
	UWorld* World,
	EJTSWorldPickupItemType NewItemType,
	const FVector& GroundLocation,
	AActor* SourceActor)
{
	if (World == nullptr || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	AJTSPlanetAnchor* const Planet = FindRealSurfacePlanet(SourceActor);
	FRotator SpawnRotation(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);
	if (IsValid(Planet))
	{
		FJTSPlanetSurfaceFrame SurfaceFrame;
		if (Planet->GetSurfaceFrameAt(GroundLocation, FVector::ForwardVector, SurfaceFrame))
		{
			const FVector SurfaceForward = FQuat(
				SurfaceFrame.Up,
				FMath::DegreesToRadians(SpawnRotation.Yaw)).RotateVector(SurfaceFrame.Forward);
			SpawnRotation = FRotationMatrix::MakeFromXZ(SurfaceForward, SurfaceFrame.Up).Rotator();
		}
	}
	const FTransform SpawnTransform(
		SpawnRotation,
		GroundLocation);
	AJTSWorldPickupActor* const Pickup = World->SpawnActorDeferred<AJTSWorldPickupActor>(
		AJTSWorldPickupActor::StaticClass(),
		SpawnTransform,
		SourceActor,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Pickup))
	{
		return nullptr;
	}

	Pickup->InitializeItem(NewItemType);
	Pickup->FinishSpawning(SpawnTransform);
	if (IsValid(Planet))
	{
		Pickup->PlaceOnPlanetSurface(Planet, GroundLocation, SpawnRotation.Vector());
	}
	else
	{
		Pickup->AdjustToGround(GroundLocation);
	}
	return Pickup;
}

AJTSWorldPickupActor* AJTSWorldPickupActor::SpawnGameplayDrop(
	UWorld* World,
	EJTSWorldPickupItemType NewItemType,
	const FVector& Origin,
	APawn* SafetyPawn,
	AActor* SourceActor,
	const FVector& PreferredDirection)
{
	if (World == nullptr || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(
		SourceActor != nullptr ? static_cast<const UObject*>(SourceActor) : static_cast<const UObject*>(SafetyPawn));
	AJTSPlanetAnchor* const Planet = IsValid(SurfaceController) && SurfaceController->IsUsingRealPlanetSurfaceGameplay()
		? SurfaceController->GetOwningPlanet()
		: nullptr;
	FVector GravityAcceleration = GetPickupGravityAcceleration(World, SafetyPawn);
	if (IsValid(Planet))
	{
		GravityAcceleration = Planet->GetGravityDirection(Origin) * Planet->GetGravityStrength();
	}
	FVector DropUpDirection = -GravityAcceleration.GetSafeNormal();
	if (DropUpDirection.IsNearlyZero())
	{
		DropUpDirection = IsValid(Planet) ? Planet->GetRadialUpVector(Origin) : FVector::UpVector;
	}

	float DropUpwardSpeed = DefaultPickupDropUpwardSpeed;
	float DropHorizontalSpeed = DefaultPickupDropHorizontalSpeed;
	GetPickupDropLaunchSpeeds(World, DropUpwardSpeed, DropHorizontalSpeed);

	FVector SafePreferredDirection = IsValid(Planet)
		? Planet->ProjectDirectionToSurfaceTangent(PreferredDirection, Origin)
		: FVector(PreferredDirection.X, PreferredDirection.Y, 0.0f).GetSafeNormal();
	TArray<AJTSWorldPickupActor*> ExistingPickups;
	if (UJTSWorldPickupRegistrySubsystem* const Registry = World->GetSubsystem<UJTSWorldPickupRegistrySubsystem>())
	{
		Registry->GetRegisteredPickups(ExistingPickups);
	}
	for (int32 AttemptIndex = 0; AttemptIndex < 12; ++AttemptIndex)
	{
		FVector HorizontalDirection;
		const bool bUsePreferredDirection = !SafePreferredDirection.IsNearlyZero() && AttemptIndex < 4;
		if (IsValid(Planet))
		{
			const float DirectionAngle = bUsePreferredDirection
				? FMath::DegreesToRadians(FMath::FRandRange(-36.0f, 36.0f))
				: FMath::FRandRange(0.0f, UE_TWO_PI);
			HorizontalDirection = BuildTangentDirection(
				Planet,
				Origin,
				bUsePreferredDirection ? SafePreferredDirection : FVector::ZeroVector,
				DirectionAngle);
		}
		else if (bUsePreferredDirection)
		{
			const float SideAngle = FMath::DegreesToRadians(FMath::FRandRange(-36.0f, 36.0f));
			HorizontalDirection = SafePreferredDirection.RotateAngleAxis(SideAngle, FVector::UpVector);
		}
		else
		{
			const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
			HorizontalDirection = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		}

		const float Distance = bUsePreferredDirection
			? FMath::FRandRange(140.0f, 230.0f)
			: FMath::FRandRange(70.0f, 170.0f);
		if (HorizontalDirection.IsNearlyZero())
		{
			continue;
		}

		FVector CandidateLocation = Origin + HorizontalDirection * Distance;
		FVector GroundLocation;
		if (IsValid(Planet))
		{
			FJTSPlanetSurfaceHit SurfaceHit;
			if (!Planet->ProjectPointToSurface(CandidateLocation, SurfaceHit))
			{
				continue;
			}
			CandidateLocation = SurfaceHit.ImpactPoint;
			GroundLocation = SurfaceHit.ImpactPoint;
		}

		if (IsValid(SafetyPawn))
		{
			const float PawnRadius = Cast<ACharacter>(SafetyPawn) != nullptr
				? Cast<ACharacter>(SafetyPawn)->GetSimpleCollisionRadius()
				: 45.0f;
			const bool bTooCloseToPawn = IsValid(Planet)
				? Planet->ApproximateSurfaceArcDistance(CandidateLocation, SafetyPawn->GetActorLocation()) < PawnRadius + 85.0f
				: FVector::DistSquared2D(CandidateLocation, SafetyPawn->GetActorLocation()) < FMath::Square(PawnRadius + 85.0f);
			if (bTooCloseToPawn)
			{
				continue;
			}
		}

		bool bOverlapsShip = false;
		auto DoesOverlapShip = [Planet, &CandidateLocation, &bOverlapsShip](const AJTSSpacecraftActor* Spacecraft)
		{
			if (!IsValid(Spacecraft))
			{
				return;
			}

			const FBox ShipBounds = Spacecraft->GetResourceExclusionBounds();
			if (ShipBounds.IsValid)
			{
				if (IsValid(Planet))
				{
					const float SurfaceClearance = ShipBounds.GetExtent().Size() + 70.0f;
					bOverlapsShip = Planet->ApproximateSurfaceArcDistance(
						CandidateLocation,
						Spacecraft->GetActorLocation()) <= SurfaceClearance;
					return;
				}

				const FVector ShipCenter = ShipBounds.GetCenter();
				const FVector ShipExtent = ShipBounds.GetExtent() + FVector(70.0f, 70.0f, 0.0f);
				if (FMath::Abs(CandidateLocation.X - ShipCenter.X) <= ShipExtent.X
					&& FMath::Abs(CandidateLocation.Y - ShipCenter.Y) <= ShipExtent.Y)
				{
					bOverlapsShip = true;
					return;
				}
			}
		};
		if (IsValid(SurfaceController))
		{
			DoesOverlapShip(SurfaceController->GetSpacecraft());
		}
		else
		{
			for (TActorIterator<AJTSSpacecraftActor> ShipIt(World); ShipIt; ++ShipIt)
			{
				DoesOverlapShip(*ShipIt);
				if (bOverlapsShip)
				{
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
				&& IsInCurrentMoonSurface(SurfaceController, ExistingPickup)
				&& (IsValid(Planet)
					? Planet->ApproximateSurfaceArcDistance(CandidateLocation, ExistingPickup->GetActorLocation()) < 60.0f
					: FVector::DistSquared2D(CandidateLocation, ExistingPickup->GetActorLocation()) < FMath::Square(60.0f)))
			{
				bTooCloseToPickup = true;
				break;
			}
		}
		if (bTooCloseToPickup)
		{
			continue;
		}

		if (!IsValid(Planet))
		{
			FCollisionQueryParams GroundTraceParams(SCENE_QUERY_STAT(JTSWorldPickupGroundTrace), false, SourceActor);
			if (SourceActor != nullptr)
			{
				GroundTraceParams.AddIgnoredActor(SourceActor);
			}
			if (SafetyPawn != nullptr)
			{
				GroundTraceParams.AddIgnoredActor(SafetyPawn);
			}
			if (IsValid(SurfaceController))
			{
				GroundTraceParams.AddIgnoredActor(SurfaceController->GetSpacecraft());
				if (ULevel* const SurfaceLevel = SurfaceController->GetSurfaceLevel())
				{
					for (AActor* const Actor : SurfaceLevel->Actors)
					{
						if (AJTSMoonResourceActor* const Resource = Cast<AJTSMoonResourceActor>(Actor); IsValid(Resource))
						{
							GroundTraceParams.AddIgnoredActor(Resource);
						}
					}
				}
			}
			else
			{
				for (TActorIterator<AJTSSpacecraftActor> ShipIt(World); ShipIt; ++ShipIt)
				{
					GroundTraceParams.AddIgnoredActor(*ShipIt);
				}
				for (TActorIterator<AJTSMoonResourceActor> ResourceIt(World); ResourceIt; ++ResourceIt)
				{
					GroundTraceParams.AddIgnoredActor(*ResourceIt);
				}
			}
			for (AJTSWorldPickupActor* const ExistingPickup : ExistingPickups)
			{
				if (IsValid(ExistingPickup) && IsInCurrentMoonSurface(SurfaceController, ExistingPickup))
				{
					GroundTraceParams.AddIgnoredActor(ExistingPickup);
				}
			}

			FHitResult GroundHit;
			const FVector TraceStart(CandidateLocation.X, CandidateLocation.Y, Origin.Z + 1200.0f);
			const FVector TraceEnd(CandidateLocation.X, CandidateLocation.Y, Origin.Z - 2500.0f);
			if (!World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, GroundTraceParams)
				|| !GroundHit.bBlockingHit)
			{
				continue;
			}

			GroundLocation = GroundHit.ImpactPoint;
		}

		FRotator SpawnRotation(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);
		if (IsValid(Planet))
		{
			FJTSPlanetSurfaceFrame SurfaceFrame;
			if (Planet->GetSurfaceFrameAt(GroundLocation, HorizontalDirection, SurfaceFrame))
			{
				const FVector SurfaceForward = FQuat(
					SurfaceFrame.Up,
					FMath::DegreesToRadians(SpawnRotation.Yaw)).RotateVector(SurfaceFrame.Forward);
				SpawnRotation = FRotationMatrix::MakeFromXZ(SurfaceForward, SurfaceFrame.Up).Rotator();
			}
		}
		const FTransform SpawnTransform(
			SpawnRotation,
			GroundLocation + DropUpDirection * PickupDropSpawnHeight);
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
		if (IsValid(SurfaceController))
		{
			SurfaceController->RegisterSurfaceRuntimeActor(Pickup);
		}

		if (IsValid(Planet))
		{
			Pickup->PlaceOnPlanetSurface(Planet, GroundLocation, SpawnRotation.Vector(), PickupDropSpawnHeight);
		}
		else
		{
			const FVector GroundDirection = GravityAcceleration.IsNearlyZero()
				? FVector::DownVector
				: GravityAcceleration.GetSafeNormal();
			const float VisualSupportDistance = Pickup->GetVisualSupportDistance(GroundDirection);
			Pickup->SetActorLocation(
				GroundLocation + DropUpDirection * (VisualSupportDistance + PickupDropSpawnHeight),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}

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
			GroundLocation,
			SourceActor,
			SafetyPawn);
		return Pickup;
	}

	return nullptr;
}

AJTSWorldPickupActor* AJTSWorldPickupActor::SpawnGameplayDrop(
	UWorld* World,
	const FJTSItemInstance& NewItem,
	const FVector& Origin,
	APawn* SafetyPawn,
	AActor* SourceActor,
	const FVector& PreferredDirection)
{
	if (NewItem.IsEmpty())
	{
		return nullptr;
	}

	AJTSWorldPickupActor* const Pickup = SpawnGameplayDrop(
		World,
		ItemIdToItemType(NewItem.ItemId),
		Origin,
		SafetyPawn,
		SourceActor,
		PreferredDirection);
	if (IsValid(Pickup))
	{
		Pickup->InitializeItemInstance(NewItem);
	}
	return Pickup;
}

AJTSWorldPickupActor::AJTSWorldPickupActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetActorTickEnabled(false);
	bReplicates = true;
	SetReplicateMovement(true);

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

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterialAsset.Succeeded())
	{
		RealPlanetSurfaceMaterial = BasicMaterialAsset.Object;
		PickupMesh->SetMaterial(0, BasicMaterialAsset.Object);
	}
}

EJTSWorldPickupItemType AJTSWorldPickupActor::GetItemType() const
{
	return ItemType;
}

FText AJTSWorldPickupActor::GetItemDisplayName() const
{
	return ItemInstance.IsEmpty()
		? FText::FromString(ItemTypeToString(ItemType))
		: UJTSItemDefinitionLibrary::GetItemDisplayName(ItemInstance.ItemId);
}

FJTSItemInstance AJTSWorldPickupActor::GetItemInstance() const
{
	return ItemInstance;
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
		if (bUsesRealPlanetSurface)
		{
			FJTSSurfaceVisualProjectionBounds VisualBounds;
			if (JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(
				PickupMesh,
				GetActorLocation(),
				SurfaceUp,
				VisualBounds))
			{
				return VisualBounds.HighestPoint + SurfaceUp * 22.0f;
			}
		}

		const float BoundsScale = FMath::Max(FMath::Abs(PickupMesh->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = PickupMesh->Bounds.BoxExtent.GetAbs() / BoundsScale;
		return PickupMesh->Bounds.Origin + SurfaceUp * (PhysicalExtent.Z + 22.0f);
	}

	return GetActorLocation() + SurfaceUp * 70.0f;
}

void AJTSWorldPickupActor::InitializeItem(EJTSWorldPickupItemType NewItemType)
{
	if (!HasAuthority())
	{
		return;
	}
	ItemType = NewItemType;
	ItemInstance = UJTSItemDefinitionLibrary::MakeInstance(ItemTypeToItemId(NewItemType));
	bPickupConsumed = false;
	ApplyItemAppearance();
}

void AJTSWorldPickupActor::InitializeItemInstance(const FJTSItemInstance& NewItem)
{
	if (!HasAuthority() || NewItem.IsEmpty())
	{
		return;
	}

	ItemInstance = NewItem;
	if (!ItemInstance.InstanceId.IsValid())
	{
		ItemInstance.InstanceId = FGuid::NewGuid();
	}
	ItemType = ItemIdToItemType(ItemInstance.ItemId);
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
	bUsesRealPlanetSurface = false;
	SurfacePlanet.Reset();
	SurfaceUp = FVector::UpVector;
	ApplyItemAppearance();
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
}

void AJTSWorldPickupActor::PlaceOnPlanetSurface(
	AJTSPlanetAnchor* Planet,
	const FVector& GroundLocation,
	const FVector& PreferredForward,
	const float SurfaceOffset)
{
	if (!IsValid(Planet) || !IsValid(PickupMesh))
	{
		AdjustToGround(GroundLocation);
		return;
	}

	FJTSPlanetSurfaceFrame SurfaceFrame;
	if (!Planet->GetSurfaceFrameAt(GroundLocation, PreferredForward, SurfaceFrame))
	{
		UE_LOG(LogTemp, Warning, TEXT("JTSWorldPickupActor %s could not resolve a real Moon surface frame."), *GetName());
		return;
	}

	bUsesRealPlanetSurface = true;
	SurfacePlanet = Planet;
	SurfaceUp = SurfaceFrame.Up.GetSafeNormal();
	SetActorRotation(SurfaceFrame.Transform.Rotator(), ETeleportType::TeleportPhysics);
	ApplyItemAppearance();
	PickupMesh->UpdateBounds();
	FJTSSurfaceVisualProjectionBounds VisualBounds;
	const float VisualSupportDistance = JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(
		PickupMesh,
		GetActorLocation(),
		SurfaceUp,
		VisualBounds)
		? VisualBounds.GetRootToLowestSupport()
		: 0.0f;
	SetActorLocation(
		SurfaceFrame.Location + SurfaceUp * (
			VisualSupportDistance
			+ JTSSurfacePlacementBounds::DefaultSurfaceClearance
			+ FMath::Max(0.0f, SurfaceOffset)),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

}

bool AJTSWorldPickupActor::IsUsingRealPlanetSurface() const
{
	return bUsesRealPlanetSurface && SurfacePlanet.IsValid();
}

AJTSPlanetAnchor* AJTSWorldPickupActor::GetSurfacePlanet() const
{
	return SurfacePlanet.Get();
}

void AJTSWorldPickupActor::StartDropMotion(
	const FVector& InitialVelocity,
	const FVector& GravityAcceleration,
	const FVector& SafeGroundLocation,
	AActor* SourceActor,
	APawn* SafetyPawn)
{
	if (!HasAuthority())
	{
		return;
	}
	DropVelocity = InitialVelocity;
	DropGravityAcceleration = GravityAcceleration;
	PlannedGroundLocation = SafeGroundLocation;
	DropElapsedSeconds = 0.0f;
	BuildDropTraceIgnoredActors(SourceActor, SafetyPawn);

	if (DropGravityAcceleration.IsNearlyZero())
	{
		if (AJTSPlanetAnchor* const Planet = GetSurfacePlanet())
		{
			PlaceOnPlanetSurface(Planet, PlannedGroundLocation, GetActorForwardVector());
		}
		else
		{
			AdjustToGround(PlannedGroundLocation);
		}
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
	if (!HasAuthority() || !CanInteract_Implementation(InteractingPawn))
	{
		return;
	}

	bPickupConsumed = true;
	FString FailureReason;
	if (TryPickup(InteractingPawn, FailureReason))
	{
		if (AJTSCharacter* const Character = Cast<AJTSCharacter>(InteractingPawn))
		{
			// The ship overlap owns material submission. Picking up beside it should fund the
			// shared wallet immediately, without a second deposit button or interaction.
			Character->TryDepositCarriedResourcesToNearbySpacecraft();
		}
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
	if (HasAuthority() && ItemInstance.IsEmpty())
	{
		InitializeItem(ItemType);
	}
	ApplyItemAppearance();

	if (UJTSWorldPickupRegistrySubsystem* const Registry = GetWorld() != nullptr
		? GetWorld()->GetSubsystem<UJTSWorldPickupRegistrySubsystem>()
		: nullptr)
	{
		Registry->RegisterPickup(this);
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
	if (!HasAuthority())
	{
		SetActorTickEnabled(false);
		return;
	}

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

	const FVector CurrentLocation = GetActorLocation();
	if (AJTSPlanetAnchor* const Planet = GetSurfacePlanet())
	{
		DropGravityAcceleration = Planet->GetGravityDirection(CurrentLocation) * Planet->GetGravityStrength();
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
}

void AJTSWorldPickupActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AJTSWorldPickupActor, ItemType);
	DOREPLIFETIME(AJTSWorldPickupActor, ItemInstance);
	DOREPLIFETIME(AJTSWorldPickupActor, SurfaceUp);
	DOREPLIFETIME(AJTSWorldPickupActor, bIsDropping);
	DOREPLIFETIME(AJTSWorldPickupActor, bPickupConsumed);
	DOREPLIFETIME(AJTSWorldPickupActor, bUsesRealPlanetSurface);
}

void AJTSWorldPickupActor::OnRep_ItemState()
{
	ApplyItemAppearance();
}

void AJTSWorldPickupActor::OnRep_DropState()
{
	// Movement is replicated by the server; clients never simulate a second drop trajectory.
	SetActorTickEnabled(false);
}

void AJTSWorldPickupActor::OnRep_SurfacePresentation()
{
	ApplyItemAppearance();
}

bool AJTSWorldPickupActor::TraceDropGround(
	const FVector& TraceStart,
	const FVector& TraceEnd,
	FHitResult& OutGroundHit) const
{
	if (AJTSPlanetAnchor* const Planet = GetSurfacePlanet())
	{
		FJTSPlanetSurfaceHit SurfaceHit;
		if (!Planet->ProbeSurfaceAlongGravity(
			TraceStart,
			FVector::Distance(TraceStart, TraceEnd),
			SurfaceHit))
		{
			return false;
		}

		OutGroundHit = FHitResult();
		OutGroundHit.bBlockingHit = true;
		OutGroundHit.ImpactPoint = SurfaceHit.ImpactPoint;
		OutGroundHit.ImpactNormal = SurfaceHit.ImpactNormal;
		return true;
	}

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

	AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this);
	if (IsValid(SurfaceController))
	{
		AddIgnoredActor(SurfaceController->GetSpacecraft());
		if (ULevel* const SurfaceLevel = SurfaceController->GetSurfaceLevel())
		{
			for (AActor* const Actor : SurfaceLevel->Actors)
			{
				if (AJTSMoonResourceActor* const Resource = Cast<AJTSMoonResourceActor>(Actor); IsValid(Resource))
				{
					AddIgnoredActor(Resource);
				}
			}
		}
	}
	else
	{
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
}

void AJTSWorldPickupActor::SettleDropOnGround(const FVector& GroundHitLocation)
{
	if (AJTSPlanetAnchor* const Planet = GetSurfacePlanet())
	{
		PlaceOnPlanetSurface(Planet, GroundHitLocation, GetActorForwardVector());
		DropVelocity = FVector::ZeroVector;
		bIsDropping = false;
		SetActorTickEnabled(false);
		DropTraceIgnoredActors.Reset();
		UE_LOG(LogTemp, Log, TEXT("JumpToSpace Pickup Landed: Item=%s"), *ItemTypeToString(ItemType));
		return;
	}

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

	DropVelocity = FVector::ZeroVector;
	bIsDropping = false;
	SetActorTickEnabled(false);
	DropTraceIgnoredActors.Reset();
	UE_LOG(LogTemp, Log, TEXT("JumpToSpace Pickup Landed: Item=%s"), *ItemTypeToString(ItemType));
}

UStaticMeshComponent* AJTSWorldPickupActor::GetPickupMeshComponent() const
{
	return PickupMesh.Get();
}

bool AJTSWorldPickupActor::TryPickup(APawn* InteractingPawn, FString& OutFailureReason)
{
	if (!IsValid(InteractingPawn) || ItemInstance.IsEmpty())
	{
		OutFailureReason = TEXT("UnsupportedItem");
		return false;
	}

	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemInstance.ItemId);
	if (!IsValid(Definition))
	{
		OutFailureReason = TEXT("UnsupportedItem");
		return false;
	}

	if (Definition->IsWearable())
	{
		UJTSPlayerEquipmentComponent* const Wearables = InteractingPawn->FindComponentByClass<UJTSPlayerEquipmentComponent>();
		if (!IsValid(Wearables))
		{
			OutFailureReason = TEXT("WearablesUnavailable");
			return false;
		}
		if (!Wearables->TryEquipItem(ItemInstance))
		{
			OutFailureReason = TEXT("WearableSlotOccupied");
			return false;
		}
		return true;
	}

	UJTSInventoryComponent* const Inventory = InteractingPawn->FindComponentByClass<UJTSInventoryComponent>();
	if (!IsValid(Inventory))
	{
		OutFailureReason = TEXT("InventoryUnavailable");
		return false;
	}
	if (!Inventory->CanAddItem(ItemInstance.ItemId, ItemInstance.StackCount))
	{
		OutFailureReason = TEXT("InventoryFull");
		return false;
	}
	int32 Remaining = ItemInstance.StackCount;
	if (!Inventory->TryAddItem(ItemInstance, Remaining) || Remaining != 0)
	{
		OutFailureReason = TEXT("InventoryFull");
		return false;
	}
	return true;
}

bool AJTSWorldPickupActor::IsResourceItem() const
{
	EJTSResourceType ResourceType = EJTSResourceType::Rock;
	return UJTSItemDefinitionLibrary::TryGetResourceType(ItemInstance.IsEmpty() ? ItemTypeToItemId(ItemType) : ItemInstance.ItemId, ResourceType);
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
		if (ItemType == EJTSWorldPickupItemType::Backpack)
		{
			DesiredScale = FVector(0.33f, 0.24f, 0.38f);
		}
		else if (ItemType == EJTSWorldPickupItemType::Axe)
		{
			DesiredScale = FVector(0.44f, 0.18f, 0.28f);
		}
		else if (ItemType == EJTSWorldPickupItemType::MachineGun)
		{
			DesiredScale = FVector(0.62f, 0.18f, 0.14f);
		}
		else if (ItemType == EJTSWorldPickupItemType::Pistol)
		{
			DesiredScale = FVector(0.32f, 0.12f, 0.13f);
		}
		else
		{
			DesiredScale = FVector(0.42f, 0.12f, 0.10f);
		}
	}

	if (IsValid(DesiredMesh) && PickupMesh->GetStaticMesh() != DesiredMesh)
	{
		PickupMesh->SetStaticMesh(DesiredMesh);
	}
	PickupMesh->SetRelativeScale3D(DesiredScale);
	PickupMesh->UpdateBounds();
}

void AJTSWorldPickupActor::ApplySurfacePresentationMaterial()
{
	if (!IsValid(PickupMesh))
	{
		return;
	}

	UMaterialInterface* const DesiredMaterial = RealPlanetSurfaceMaterial.Get();
	if (!IsValid(DesiredMaterial) || AppliedPresentationMaterial == DesiredMaterial)
	{
		return;
	}

	PickupMaterial = nullptr;
	PickupMesh->SetMaterial(0, DesiredMaterial);
	AppliedPresentationMaterial = DesiredMaterial;
}

void AJTSWorldPickupActor::ApplyItemAppearance()
{
	if (!IsValid(PickupMesh))
	{
		return;
	}

	ConfigureAppearance();
	ApplySurfacePresentationMaterial();
	if (!IsValid(PickupMaterial))
	{
		PickupMaterial = PickupMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (!IsValid(PickupMaterial))
	{
		return;
	}
	if (!ItemInstance.IsEmpty())
	{
		if (const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemInstance.ItemId))
		{
			PickupMaterial->SetVectorParameterValue(TEXT("BaseColor"), Definition->AccentColor);
			PickupMaterial->SetVectorParameterValue(TEXT("Color"), Definition->AccentColor);
		}
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

	case EJTSWorldPickupItemType::Knife:
		ItemColor = FLinearColor(0.82f, 0.86f, 0.92f, 1.0f);
		break;

	case EJTSWorldPickupItemType::Pistol:
		ItemColor = FLinearColor(0.24f, 0.62f, 1.0f, 1.0f);
		break;

	case EJTSWorldPickupItemType::MachineGun:
		ItemColor = FLinearColor(1.0f, 0.30f, 0.16f, 1.0f);
		break;

	case EJTSWorldPickupItemType::Axe:
		ItemColor = FLinearColor(0.88f, 0.30f, 0.12f, 1.0f);
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

	case EJTSWorldPickupItemType::MoonAntCorpse:
		return TEXT("MOON ANT CORPSE");

	case EJTSWorldPickupItemType::Pickaxe:
		return TEXT("PICKAXE");

	case EJTSWorldPickupItemType::Backpack:
		return TEXT("BACKPACK");

	case EJTSWorldPickupItemType::Knife:
		return TEXT("KNIFE");

	case EJTSWorldPickupItemType::Pistol:
		return TEXT("PISTOL");

	case EJTSWorldPickupItemType::MachineGun:
		return TEXT("MACHINE GUN");

	case EJTSWorldPickupItemType::Axe:
		return TEXT("AXE");

	default:
		return TEXT("UNKNOWN");
	}
}

EJTSItemId AJTSWorldPickupActor::ItemTypeToItemId(EJTSWorldPickupItemType InItemType)
{
	switch (InItemType)
	{
	case EJTSWorldPickupItemType::Fuel: return EJTSItemId::Fuel;
	case EJTSWorldPickupItemType::Water: return EJTSItemId::Water;
	case EJTSWorldPickupItemType::Food: return EJTSItemId::Food;
	case EJTSWorldPickupItemType::Rock: return EJTSItemId::Rock;
	case EJTSWorldPickupItemType::Ore: return EJTSItemId::Ore;
	case EJTSWorldPickupItemType::MoonAntCorpse: return EJTSItemId::MoonAntCorpse;
	case EJTSWorldPickupItemType::Pickaxe: return EJTSItemId::Pickaxe;
	case EJTSWorldPickupItemType::Backpack: return EJTSItemId::Backpack;
	case EJTSWorldPickupItemType::Knife: return EJTSItemId::Knife;
	case EJTSWorldPickupItemType::Pistol: return EJTSItemId::Pistol;
	case EJTSWorldPickupItemType::MachineGun: return EJTSItemId::MachineGun;
	case EJTSWorldPickupItemType::Axe: return EJTSItemId::Axe;
	default: return EJTSItemId::None;
	}
}

EJTSWorldPickupItemType AJTSWorldPickupActor::ItemIdToItemType(EJTSItemId ItemId)
{
	switch (ItemId)
	{
	case EJTSItemId::Fuel: return EJTSWorldPickupItemType::Fuel;
	case EJTSItemId::Water: return EJTSWorldPickupItemType::Water;
	case EJTSItemId::Food: return EJTSWorldPickupItemType::Food;
	case EJTSItemId::Rock: return EJTSWorldPickupItemType::Rock;
	case EJTSItemId::Ore: return EJTSWorldPickupItemType::Ore;
	case EJTSItemId::MoonAntCorpse: return EJTSWorldPickupItemType::MoonAntCorpse;
	case EJTSItemId::Pickaxe: return EJTSWorldPickupItemType::Pickaxe;
	case EJTSItemId::Backpack: return EJTSWorldPickupItemType::Backpack;
	case EJTSItemId::Knife: return EJTSWorldPickupItemType::Knife;
	case EJTSItemId::Pistol: return EJTSWorldPickupItemType::Pistol;
	case EJTSItemId::MachineGun: return EJTSWorldPickupItemType::MachineGun;
	case EJTSItemId::Axe: return EJTSWorldPickupItemType::Axe;
	default: return EJTSWorldPickupItemType::Rock;
	}
}
