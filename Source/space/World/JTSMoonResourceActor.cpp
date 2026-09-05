#include "space/World/JTSMoonResourceActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "space/Components/JTSMoonWrappedActorComponent.h"
#include "space/Items/JTSResourcePickupActor.h"
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
}

AJTSMoonResourceActor::AJTSMoonResourceActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ResourceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ResourceMesh"));
	ResourceMesh->SetupAttachment(SceneRoot);
	ResourceMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ResourceMesh->SetCollisionObjectType(ECC_WorldDynamic);
	ResourceMesh->SetCollisionResponseToAllChannels(ECR_Overlap);
	ResourceMesh->SetGenerateOverlapEvents(true);
	ResourceMesh->SetCanEverAffectNavigation(false);

	MoonWrappedActorComponent = CreateDefaultSubobject<UJTSMoonWrappedActorComponent>(TEXT("MoonWrappedActorComponent"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshAsset.Succeeded())
	{
		ResourceMesh->SetStaticMesh(SphereMeshAsset.Object);
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

int32 AJTSMoonResourceActor::GetResourceAmount() const
{
	return FMath::Max(1, ResourceAmount);
}

bool AJTSMoonResourceActor::CanBePickedUp() const
{
	return bCanPickup;
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

void AJTSMoonResourceActor::InitializeResource(
	EJTSResourceType NewResourceType,
	int32 NewResourceAmount,
	bool bNewCanPickup,
	const FText& NewPickupText)
{
	ResourceType = NewResourceType;
	ResourceAmount = FMath::Max(1, NewResourceAmount);
	bCanPickup = bNewCanPickup;
	PickupText = NewPickupText;
	bResourceConsumed = false;
	ApplyResourceAppearance();
}

bool AJTSMoonResourceActor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return bCanPickup
		&& !bResourceConsumed
		&& !IsPendingKillPending()
		&& AJTSResourcePickupActor::CanCollectResource(InteractingPawn, GetResourceAmount(), false);
}

FText AJTSMoonResourceActor::GetInteractionPrompt_Implementation(APawn* InteractingPawn) const
{
	if (!CanInteract_Implementation(InteractingPawn))
	{
		return FText::GetEmpty();
	}

	return PickupText.IsEmpty() ? MakeDefaultPickupText() : PickupText;
}

void AJTSMoonResourceActor::Interact_Implementation(APawn* InteractingPawn)
{
	if (!CanInteract_Implementation(InteractingPawn))
	{
		return;
	}

	bResourceConsumed = true;
	if (!AJTSResourcePickupActor::TryCollectResource(InteractingPawn, ResourceType, GetResourceAmount(), false))
	{
		bResourceConsumed = false;
		return;
	}

	Destroy();
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
	ResourceAmount = FMath::Max(1, ResourceAmount);
	ApplyResourceAppearance();
}

FText AJTSMoonResourceActor::MakeDefaultPickupText() const
{
	const TCHAR* ResourceName = TEXT("Resource");
	switch (ResourceType)
	{
	case EJTSResourceType::Rock:
		ResourceName = TEXT("Rock");
		break;

	case EJTSResourceType::Ore:
		ResourceName = TEXT("Ore");
		break;

	default:
		break;
	}

	return FText::Format(FText::FromString(TEXT("Press E Collect {0}")), FText::FromString(ResourceName));
}

void AJTSMoonResourceActor::ApplyResourceAppearance()
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

	const bool bIsOre = ResourceType == EJTSResourceType::Ore;
	const FLinearColor ResourceColor = bIsOre
		? FLinearColor(0.20f, 0.18f, 0.31f, 1.0f)
		: FLinearColor(0.14f, 0.15f, 0.17f, 1.0f);
	const float Roughness = bIsOre ? 0.32f : 0.90f;
	const float Metallic = bIsOre ? 0.35f : 0.0f;
	ResourceMaterial->SetVectorParameterValue(TEXT("Color"), ResourceColor);
	ResourceMaterial->SetVectorParameterValue(TEXT("BaseColor"), ResourceColor);
	ResourceMaterial->SetVectorParameterValue(TEXT("Tint"), ResourceColor);
	ResourceMaterial->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	ResourceMaterial->SetScalarParameterValue(TEXT("Metallic"), Metallic);
}
