#include "space/World/JTSMoonResourceActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "space/Components/JTSCarryComponent.h"
#include "space/Components/JTSMoonWrappedActorComponent.h"
#include "space/Components/JTSPlayerEquipmentComponent.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Items/JTSWorldPickupItemType.h"
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

	const TCHAR* GetMiningNodeName(EJTSResourceType ResourceType)
	{
		return ResourceType == EJTSResourceType::Ore ? TEXT("Ore") : TEXT("LargeRock");
	}
}

AJTSMoonResourceActor::AJTSMoonResourceActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ResourceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ResourceMesh"));
	ResourceMesh->SetupAttachment(SceneRoot);
	ResourceMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ResourceMesh->SetCollisionObjectType(ECC_WorldDynamic);
	ResourceMesh->SetCollisionResponseToAllChannels(ECR_Overlap);
	ResourceMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	ResourceMesh->SetGenerateOverlapEvents(true);
	ResourceMesh->SetCanEverAffectNavigation(false);

	MoonWrappedActorComponent = CreateDefaultSubobject<UJTSMoonWrappedActorComponent>(TEXT("MoonWrappedActorComponent"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshAsset.Succeeded())
	{
		RockMesh = SphereMeshAsset.Object;
		ResourceMesh->SetStaticMesh(RockMesh);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshAsset(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMeshAsset.Succeeded())
	{
		OreMesh = ConeMeshAsset.Object;
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (CylinderMeshAsset.Succeeded())
		{
			OreMesh = CylinderMeshAsset.Object;
		}
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FakeMoonBendMaterialAsset(TEXT("/Game/Space/Materials/FakeMoon/MI_JTSFakeMoon_Prop.MI_JTSFakeMoon_Prop"));
	if (FakeMoonBendMaterialAsset.Succeeded())
	{
		FakeMoonBendMaterial = FakeMoonBendMaterialAsset.Object;
		ResourceMesh->SetMaterial(0, FakeMoonBendMaterialAsset.Object);
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (BasicMaterialAsset.Succeeded())
		{
			ResourceMesh->SetMaterial(0, BasicMaterialAsset.Object);
		}
	}
}

EJTSResourceType AJTSMoonResourceActor::GetResourceType() const
{
	return ResourceType;
}

int32 AJTSMoonResourceActor::GetTotalYieldUnits() const
{
	return FMath::Max(1, TotalYieldUnits);
}

int32 AJTSMoonResourceActor::GetRemainingYieldUnits() const
{
	return FMath::Clamp(RemainingYieldUnits, 0, GetTotalYieldUnits());
}

FText AJTSMoonResourceActor::GetInteractionDisplayName() const
{
	return FText::FromString(ResourceType == EJTSResourceType::Ore ? TEXT("ORE DEPOSIT") : TEXT("LARGE ROCK"));
}

FVector AJTSMoonResourceActor::GetInteractionAnchorWorldLocation() const
{
	if (IsValid(ResourceMesh) && ResourceMesh->IsRegistered())
	{
		const float BoundsScale = FMath::Max(FMath::Abs(ResourceMesh->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = ResourceMesh->Bounds.BoxExtent.GetAbs() / BoundsScale;
		return ResourceMesh->Bounds.Origin + FVector(0.0f, 0.0f, PhysicalExtent.Z + 28.0f);
	}

	return GetActorLocation() + FVector(0.0f, 0.0f, 120.0f);
}

FVector AJTSMoonResourceActor::GetVisualBoundsExtent() const
{
	if (IsValid(ResourceMesh) && ResourceMesh->IsRegistered())
	{
		const float RenderBoundsScale = FMath::Max(FMath::Abs(ResourceMesh->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector BoundsExtent = ResourceMesh->Bounds.BoxExtent.GetAbs() / RenderBoundsScale;
		if (!BoundsExtent.IsNearlyZero())
		{
			return BoundsExtent;
		}
	}

	return FVector::ZeroVector;
}

void AJTSMoonResourceActor::AdjustToGround(const FVector& GroundHitLocation)
{
	const FVector OriginalLocation = GetActorLocation();
	const FVector FinalScale = GetActorScale3D();
	if (!IsValid(ResourceMesh) || !ResourceMesh->IsRegistered())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("JTSMoonResourceActor: ResourceType=%s OriginalLocation=(%.2f, %.2f, %.2f) AdjustedLocation=(%.2f, %.2f, %.2f) BoundsExtentZ=0.00 FinalScale=(%.2f, %.2f, %.2f) ResourceMesh is unavailable."),
			GetResourceTypeName(ResourceType),
			OriginalLocation.X,
			OriginalLocation.Y,
			OriginalLocation.Z,
			OriginalLocation.X,
			OriginalLocation.Y,
			OriginalLocation.Z,
			FinalScale.X,
			FinalScale.Y,
			FinalScale.Z);
		return;
	}

	ResourceMesh->UpdateBounds();

	// Wrapped actors enlarge culling bounds for WPO; placement uses the mesh's physical bounds.
	const float RenderBoundsScale = FMath::Max(FMath::Abs(ResourceMesh->BoundsScale), KINDA_SMALL_NUMBER);
	const FVector MeshBoundsExtent = ResourceMesh->Bounds.BoxExtent.GetAbs() / RenderBoundsScale;
	const float BoundsExtentZ = MeshBoundsExtent.Z;
	if (BoundsExtentZ <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("JTSMoonResourceActor: ResourceType=%s OriginalLocation=(%.2f, %.2f, %.2f) AdjustedLocation=(%.2f, %.2f, %.2f) BoundsExtentZ=%.2f FinalScale=(%.2f, %.2f, %.2f) ResourceMesh has no vertical bounds."),
			GetResourceTypeName(ResourceType),
			OriginalLocation.X,
			OriginalLocation.Y,
			OriginalLocation.Z,
			OriginalLocation.X,
			OriginalLocation.Y,
			OriginalLocation.Z,
			BoundsExtentZ,
			FinalScale.X,
			FinalScale.Y,
			FinalScale.Z);
		return;
	}

	const float MeshBottomZ = ResourceMesh->Bounds.Origin.Z - BoundsExtentZ;
	FVector AdjustedLocation = OriginalLocation;
	AdjustedLocation.Z += GroundHitLocation.Z - MeshBottomZ;
	SetActorLocation(AdjustedLocation, false, nullptr, ETeleportType::TeleportPhysics);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("JTSMoonResourceActor: ResourceType=%s OriginalLocation=(%.2f, %.2f, %.2f) AdjustedLocation=(%.2f, %.2f, %.2f) BoundsExtentZ=%.2f FinalScale=(%.2f, %.2f, %.2f)"),
		GetResourceTypeName(ResourceType),
		OriginalLocation.X,
		OriginalLocation.Y,
		OriginalLocation.Z,
		AdjustedLocation.X,
		AdjustedLocation.Y,
		AdjustedLocation.Z,
		BoundsExtentZ,
		FinalScale.X,
		FinalScale.Y,
		FinalScale.Z);
}

void AJTSMoonResourceActor::InitializeMiningNode(EJTSResourceType NewResourceType, int32 NewTotalYieldUnits)
{
	ResourceType = NewResourceType;
	TotalYieldUnits = FMath::Max(1, NewTotalYieldUnits);
	RemainingYieldUnits = TotalYieldUnits;
	bMiningInProgress = false;
	ApplyResourceAppearance();
}

bool AJTSMoonResourceActor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return IsValid(InteractingPawn)
		&& !bMiningInProgress
		&& !IsPendingKillPending()
		&& GetRemainingYieldUnits() > 0;
}

FText AJTSMoonResourceActor::GetInteractionPrompt_Implementation(APawn* InteractingPawn) const
{
	if (!CanInteract_Implementation(InteractingPawn))
	{
		return FText::GetEmpty();
	}

	return GetMiningPrompt(InteractingPawn);
}

void AJTSMoonResourceActor::Interact_Implementation(APawn* InteractingPawn)
{
	if (!CanInteract_Implementation(InteractingPawn))
	{
		return;
	}

	const UJTSPlayerEquipmentComponent* const EquipmentComponent = InteractingPawn->FindComponentByClass<UJTSPlayerEquipmentComponent>();
	if (!IsValid(EquipmentComponent) || !EquipmentComponent->HasActiveTool(EJTSEquipmentType::Pickaxe))
	{
		const TCHAR* const FailureReason = IsValid(EquipmentComponent)
			&& EquipmentComponent->HasEquippedItem(EJTSEquipmentType::Pickaxe)
			? TEXT("PickaxeNotSelected")
			: TEXT("NeedPickaxe");
		UE_LOG(
			LogTemp,
			Log,
			TEXT("JumpToSpace Mining: Node=%s Remaining=%d Item=%s Success=false Reason=%s"),
			GetMiningNodeName(ResourceType),
			GetRemainingYieldUnits(),
			GetResourceTypeName(ResourceType),
			FailureReason);
		return;
	}

	const EJTSWorldPickupItemType PickupItemType = ResourceType == EJTSResourceType::Ore
		? EJTSWorldPickupItemType::Ore
		: EJTSWorldPickupItemType::Rock;
	UJTSCarryComponent* const CarryComponent = InteractingPawn->FindComponentByClass<UJTSCarryComponent>();
	const bool bAddToInventory = IsValid(CarryComponent) && CarryComponent->CanCarryResource(ResourceType);
	bMiningInProgress = true;
	bool bDelivered = false;
	FString Destination;
	if (bAddToInventory)
	{
		bDelivered = CarryComponent->TryAddResource(ResourceType);
		Destination = TEXT("Inventory");
	}
	else
	{
		AJTSWorldPickupActor* const Pickup = AJTSWorldPickupActor::SpawnGroundedPickup(
			GetWorld(),
			PickupItemType,
			GetActorLocation(),
			InteractingPawn,
			this);
		bDelivered = IsValid(Pickup);
		Destination = TEXT("WorldDrop");
	}

	if (!bDelivered)
	{
		bMiningInProgress = false;
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("JumpToSpace Mining: Node=%s Remaining=%d Item=%s Success=false Reason=DeliveryFailed"),
			GetMiningNodeName(ResourceType),
			GetRemainingYieldUnits(),
			GetResourceTypeName(ResourceType));
		return;
	}

	RemainingYieldUnits = FMath::Max(0, RemainingYieldUnits - 1);
	UE_LOG(
		LogTemp,
		Log,
		TEXT("JumpToSpace Mining: Node=%s Remaining=%d Item=%s Success=true Destination=%s"),
		GetMiningNodeName(ResourceType),
		GetRemainingYieldUnits(),
		GetResourceTypeName(ResourceType),
		*Destination);
	if (RemainingYieldUnits == 0)
	{
		Destroy();
		return;
	}

	bMiningInProgress = false;
}

void AJTSMoonResourceActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyResourceAppearance();

	if (MoonWrappedActorComponent != nullptr)
	{
		UMaterialInterface* const BendMaterial = ResourceMaterial != nullptr
			? ResourceMaterial.Get()
			: FakeMoonBendMaterial.Get();
		if (BendMaterial != nullptr)
		{
			MoonWrappedActorComponent->SetFakeMoonBendMaterial(BendMaterial);
		}
	}
}

void AJTSMoonResourceActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	TotalYieldUnits = FMath::Max(1, TotalYieldUnits);
	RemainingYieldUnits = FMath::Clamp(RemainingYieldUnits, 0, TotalYieldUnits);
	ApplyResourceAppearance();
}

FText AJTSMoonResourceActor::GetMiningPrompt(APawn* InteractingPawn) const
{
	const UJTSPlayerEquipmentComponent* const EquipmentComponent = IsValid(InteractingPawn)
		? InteractingPawn->FindComponentByClass<UJTSPlayerEquipmentComponent>()
		: nullptr;
	if (!IsValid(EquipmentComponent) || !EquipmentComponent->HasEquippedItem(EJTSEquipmentType::Pickaxe))
	{
		return FText::FromString(TEXT("NEED PICKAXE"));
	}
	if (!EquipmentComponent->HasActiveTool(EJTSEquipmentType::Pickaxe))
	{
		const int32 PickaxeSlotIndex = EquipmentComponent->GetEquipmentSlotIndex(EJTSEquipmentType::Pickaxe);
		return FText::FromString(FString::Printf(TEXT("SELECT PICKAXE [%d]"), PickaxeSlotIndex + 1));
	}

	return FText::FromString(TEXT("[E] MINE"));
}

void AJTSMoonResourceActor::ConfigureResourceMesh()
{
	if (!IsValid(ResourceMesh))
	{
		return;
	}

	UStaticMesh* const DesiredMesh = ResourceType == EJTSResourceType::Ore
		? OreMesh.Get()
		: RockMesh.Get();
	if (!IsValid(DesiredMesh) || ResourceMesh->GetStaticMesh() == DesiredMesh)
	{
		return;
	}

	ResourceMesh->SetStaticMesh(DesiredMesh);
	ResourceMesh->UpdateBounds();
}

void AJTSMoonResourceActor::ApplyResourceAppearance()
{
	if (!IsValid(ResourceMesh))
	{
		return;
	}

	ConfigureResourceMesh();

	if (!IsValid(ResourceMaterial))
	{
		ResourceMaterial = ResourceMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (!IsValid(ResourceMaterial))
	{
		return;
	}

	const bool bIsOre = ResourceType == EJTSResourceType::Ore;
	const FLinearColor ResourceColor = bIsOre
		? FLinearColor(0.08f, 0.20f, 0.32f, 1.0f)
		: FLinearColor(0.10f, 0.11f, 0.13f, 1.0f);
	const float ResourceRoughness = bIsOre ? 0.26f : 0.90f;
	const float ResourceMetallic = bIsOre ? 0.62f : 0.0f;
	ResourceMaterial->SetVectorParameterValue(TEXT("ResourceColor"), ResourceColor);
	ResourceMaterial->SetScalarParameterValue(TEXT("ResourceRoughness"), ResourceRoughness);
	ResourceMaterial->SetScalarParameterValue(TEXT("ResourceMetallic"), ResourceMetallic);
}
