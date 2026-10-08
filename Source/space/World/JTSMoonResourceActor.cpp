#include "space/World/JTSMoonResourceActor.h"
#include "space/Components/JTSStellarAbilityComponent.h"
#include "space/Components/JTSStellarWeaponComponent.h"
#include "space/Components/JTSStellarTargetComponent.h"
#include "space/Components/JTSHealthComponent.h"


#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSWeaponProgression.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSurfacePlacementBounds.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* GetMoonResourceTypeName(EJTSResourceType ResourceType)
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
	bReplicates = true;
	SetReplicateMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ResourceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ResourceMesh"));
	ResourceMesh->SetupAttachment(SceneRoot);
	ResourceMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ResourceMesh->SetCollisionObjectType(ECC_WorldDynamic);
	ResourceMesh->SetCollisionResponseToAllChannels(ECR_Overlap);
	ResourceMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	ResourceMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ResourceMesh->SetGenerateOverlapEvents(true);
	ResourceMesh->SetCanEverAffectNavigation(false);

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

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterialAsset.Succeeded())
	{
		RealPlanetSurfaceMaterial = BasicMaterialAsset.Object;
		ResourceMesh->SetMaterial(0, BasicMaterialAsset.Object);
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

EJTSMoonResourceNodeSize AJTSMoonResourceActor::GetNodeSize() const
{
	return NodeSize;
}

float AJTSMoonResourceActor::GetRemainingMiningWork() const
{
	return FMath::Clamp(RemainingMiningWork, 0.0f, FMath::Max(0.1f, TotalMiningWork));
}

int32 AJTSMoonResourceActor::GetMetalYieldUnits() const
{
	return FMath::Clamp(MetalYieldUnits, 0, GetTotalYieldUnits());
}

FText AJTSMoonResourceActor::GetInteractionDisplayName() const
{
	return FText::FromString(TEXT("Rocks"));
}

FVector AJTSMoonResourceActor::GetInteractionAnchorWorldLocation() const
{
	if (IsValid(ResourceMesh) && ResourceMesh->IsRegistered())
	{
		FVector TopCenter;
		if (JTSSurfacePlacementBounds::GetVisualTopCenter(ResourceMesh, SurfaceUp, 28.0f, TopCenter))
		{
			return TopCenter;
		}
	}

	return GetActorLocation() + SurfaceUp * 120.0f;
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
	bUsesRealPlanetSurface = false;
	SurfaceUp = FVector::UpVector;
	ApplyResourceAppearance();
	const FVector OriginalLocation = GetActorLocation();
	const FVector FinalScale = GetActorScale3D();
	if (!IsValid(ResourceMesh) || !ResourceMesh->IsRegistered())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("JTSMoonResourceActor: ResourceType=%s OriginalLocation=(%.2f, %.2f, %.2f) AdjustedLocation=(%.2f, %.2f, %.2f) BoundsExtentZ=0.00 FinalScale=(%.2f, %.2f, %.2f) ResourceMesh is unavailable."),
			GetMoonResourceTypeName(ResourceType),
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
			GetMoonResourceTypeName(ResourceType),
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
		GetMoonResourceTypeName(ResourceType),
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

void AJTSMoonResourceActor::PlaceOnPlanetSurface(
	AJTSPlanetAnchor* Planet,
	const FVector& GroundLocation,
	const FVector& PreferredForward)
{
	if (!IsValid(Planet) || !IsValid(ResourceMesh))
	{
		AdjustToGround(GroundLocation);
		return;
	}

	FJTSPlanetSurfaceFrame SurfaceFrame;
	if (!Planet->GetSurfaceFrameAt(GroundLocation, PreferredForward, SurfaceFrame))
	{
		UE_LOG(LogTemp, Warning, TEXT("JTSMoonResourceActor %s could not resolve a real Moon surface frame."), *GetName());
		return;
	}

	bUsesRealPlanetSurface = true;
	SurfaceUp = SurfaceFrame.Up.GetSafeNormal();
	SetActorRotation(SurfaceFrame.Transform.Rotator(), ETeleportType::TeleportPhysics);
	ApplyResourceAppearance();
	ResourceMesh->UpdateBounds();

	FJTSSurfaceVisualProjectionBounds VisualBounds;
	const float SurfaceSupport = JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(
		ResourceMesh,
		GetActorLocation(),
		SurfaceUp,
		VisualBounds)
		? VisualBounds.GetRootToLowestSupport()
		: 0.0f;
	SetActorLocation(
		SurfaceFrame.Location + SurfaceUp * (SurfaceSupport + JTSSurfacePlacementBounds::DefaultSurfaceClearance),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

}

void AJTSMoonResourceActor::InitializeMiningNode(
	EJTSResourceType NewResourceType,
	int32 NewTotalYieldUnits,
	EJTSMoonResourceNodeSize NewNodeSize,
	int32 NewMetalYieldUnits,
	int32 NewVisualVariantIndex)
{
	if (!HasAuthority())
	{
		return;
	}
	ResourceType = NewResourceType;
	NodeSize = NewResourceType == EJTSResourceType::Ore ? EJTSMoonResourceNodeSize::OreVein : NewNodeSize;
	const bool bMixedNode = NodeSize == EJTSMoonResourceNodeSize::MediumMetalRock || NodeSize == EJTSMoonResourceNodeSize::LargeMetalRock;
	MetalYieldUnits = bMixedNode ? FMath::Max(0, NewMetalYieldUnits) : 0;
	VisualVariantIndex = FMath::Max(0, NewVisualVariantIndex);
	const int32 PrimaryYieldUnits = FMath::Max(1, NewTotalYieldUnits);
	TotalYieldUnits = PrimaryYieldUnits + MetalYieldUnits;
	RemainingYieldUnits = TotalYieldUnits;
	const float WorkPerDrop = NodeSize == EJTSMoonResourceNodeSize::MediumRock ? 2.5f
		: NodeSize == EJTSMoonResourceNodeSize::LargeRock ? 4.0f : 5.0f;
	TotalMiningWork = FMath::Max(1.0f, static_cast<float>(PrimaryYieldUnits) * WorkPerDrop);
	RemainingMiningWork = TotalMiningWork;
	bMiningInProgress = false;
	ApplyResourceAppearance();
}

bool AJTSMoonResourceActor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return IsValid(InteractingPawn)
		&& !IsPendingKillPending()
		&& GetRemainingMiningWork() > KINDA_SMALL_NUMBER;
}

FText AJTSMoonResourceActor::GetInteractionPrompt_Implementation(APawn* InteractingPawn) const
{
	return CanInteract_Implementation(InteractingPawn)
		? FText::FromString(TEXT("[E] Rocks")) : FText::GetEmpty();
}

void AJTSMoonResourceActor::Interact_Implementation(APawn* InteractingPawn)
{
	if (!HasAuthority() || !CanInteract_Implementation(InteractingPawn))
	{
		return;
	}

	EJTSItemId ItemId = EJTSItemId::None;
	float Work = 0.0f;
	if (!ResolveHeldMiningWork(InteractingPawn, ItemId, Work))
	{
		UE_LOG(LogTemp, Log, TEXT("JumpToSpace Mining: Node=%s Success=false Reason=NeedHeldMiningItem"), *GetName());
		return;
	}
	ApplyMiningWork(InteractingPawn, ItemId, Work);
}

bool AJTSMoonResourceActor::ResolveHeldMiningWork(APawn* Miner, EJTSItemId& OutItemId, float& OutWork) const
{
	OutItemId = EJTSItemId::None;
	OutWork = 0.0f;
	const UJTSInventoryComponent* const Inventory = IsValid(Miner)
		? Miner->FindComponentByClass<UJTSInventoryComponent>()
		: nullptr;
	if (!IsValid(Inventory))
	{
		return false;
	}
	const FJTSItemInstance ActiveItem = Inventory->GetActiveItem();
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ActiveItem.ItemId);
	if (ActiveItem.IsEmpty() || !IsValid(Definition) || Definition->MiningWork <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	OutItemId = ActiveItem.ItemId;
	OutWork = FJTSWeaponProgression::Resolve(ActiveItem).ScaleDamage(Definition->MiningWork);
	return true;
}

bool AJTSMoonResourceActor::SpawnAllResourceDrops(APawn* Miner)
{
	if (!HasAuthority() || GetWorld() == nullptr)
	{
		return false;
	}
	const EJTSItemId DropItemId = UJTSItemDefinitionLibrary::GetItemIdForResource(ResourceType);
	if (DropItemId == EJTSItemId::None)
	{
		return false;
	}

	TArray<AJTSWorldPickupActor*> SpawnedPickups;
	for (int32 DropIndex = 0; DropIndex < GetTotalYieldUnits(); ++DropIndex)
	{
		const EJTSItemId ItemId = DropIndex < GetTotalYieldUnits() - GetMetalYieldUnits()
			? DropItemId : EJTSItemId::Ore;
		AJTSWorldPickupActor* const Pickup = AJTSWorldPickupActor::SpawnGameplayDrop(
			GetWorld(),
			UJTSItemDefinitionLibrary::MakeInstance(ItemId),
			GetActorLocation(),
			Miner,
			this,
			Miner != nullptr ? Miner->GetActorForwardVector() : FVector::ForwardVector);
		if (!IsValid(Pickup))
		{
			for (AJTSWorldPickupActor* const SpawnedPickup : SpawnedPickups)
			{
				if (IsValid(SpawnedPickup))
				{
					SpawnedPickup->Destroy();
				}
			}
			return false;
		}
		SpawnedPickups.Add(Pickup);
	}
	return true;
}

bool AJTSMoonResourceActor::ApplyMiningWork(APawn* Miner, EJTSItemId SourceItemId, float WorkAmount)
{
	if (!HasAuthority() || !CanInteract_Implementation(Miner) || WorkAmount <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	EJTSItemId VerifiedItemId = EJTSItemId::None;
	float VerifiedWork = 0.0f;
	if (!ResolveHeldMiningWork(Miner, VerifiedItemId, VerifiedWork) || VerifiedItemId != SourceItemId)
	{
		return false;
	}
	const float AppliedWork = FMath::Clamp(WorkAmount, 0.0f, VerifiedWork);
	const float NewRemainingWork = FMath::Max(0.0f, RemainingMiningWork - AppliedWork);
	if (NewRemainingWork <= KINDA_SMALL_NUMBER && !SpawnAllResourceDrops(Miner))
	{
		UE_LOG(LogTemp, Warning, TEXT("JumpToSpace Mining: Node=%s Success=false Reason=DropSpawnFailed"), *GetName());
		return false;
	}

	RemainingMiningWork = NewRemainingWork;
	if (RemainingMiningWork <= KINDA_SMALL_NUMBER)
	{
		RemainingYieldUnits = 0;
		UE_LOG(LogTemp, Log, TEXT("JumpToSpace Mining: Node=%s Item=%d Work=%.2f Destroyed=true Drops=%d"), *GetName(), static_cast<int32>(SourceItemId), AppliedWork, TotalYieldUnits);
		Destroy();
		return true;
	}

	UE_LOG(LogTemp, Verbose, TEXT("JumpToSpace Mining: Node=%s Item=%d Work=%.2f Remaining=%.2f"), *GetName(), static_cast<int32>(SourceItemId), AppliedWork, RemainingMiningWork);
	return true;
}

bool AJTSMoonResourceActor::CanReceiveMeleeHit_Implementation(APawn* AttackingPawn) const
{
	return CanInteract_Implementation(AttackingPawn);
}

void AJTSMoonResourceActor::ReceiveMeleeHit_Implementation(APawn* AttackingPawn, EJTSMeleeAttackType AttackType)
{
	EJTSItemId ItemId = EJTSItemId::None;
	float Work = 0.0f;
	if (ResolveHeldMiningWork(AttackingPawn, ItemId, Work))
	{
		ApplyMiningWork(AttackingPawn, ItemId, Work);
	}
}

FText AJTSMoonResourceActor::GetMeleeTargetDisplayName_Implementation() const
{
	return GetInteractionDisplayName();
}

FText AJTSMoonResourceActor::GetMeleeTargetPrompt_Implementation(APawn* AttackingPawn) const
{
	return GetInteractionPrompt_Implementation(AttackingPawn);
}

FVector AJTSMoonResourceActor::GetMeleeTargetAnchorWorldLocation_Implementation() const
{
	return GetInteractionAnchorWorldLocation();
}

void AJTSMoonResourceActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyResourceAppearance();
}

void AJTSMoonResourceActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	TotalYieldUnits = FMath::Max(1, TotalYieldUnits);
	RemainingYieldUnits = FMath::Clamp(RemainingYieldUnits, 0, TotalYieldUnits);
	TotalMiningWork = FMath::Max(0.1f, TotalMiningWork);
	RemainingMiningWork = FMath::Clamp(RemainingMiningWork, 0.0f, TotalMiningWork);
	ApplyResourceAppearance();
}

void AJTSMoonResourceActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AJTSMoonResourceActor, ResourceType);
	DOREPLIFETIME(AJTSMoonResourceActor, TotalYieldUnits);
	DOREPLIFETIME(AJTSMoonResourceActor, RemainingYieldUnits);
	DOREPLIFETIME(AJTSMoonResourceActor, MetalYieldUnits);
	DOREPLIFETIME(AJTSMoonResourceActor, VisualVariantIndex);
	DOREPLIFETIME(AJTSMoonResourceActor, NodeSize);
	DOREPLIFETIME(AJTSMoonResourceActor, TotalMiningWork);
	DOREPLIFETIME(AJTSMoonResourceActor, RemainingMiningWork);
	DOREPLIFETIME(AJTSMoonResourceActor, SurfaceUp);
	DOREPLIFETIME(AJTSMoonResourceActor, bUsesRealPlanetSurface);
}

void AJTSMoonResourceActor::OnRep_ResourceData()
{
	ApplyResourceAppearance();
}

void AJTSMoonResourceActor::OnRep_SurfacePresentation()
{
	ApplyResourceAppearance();
}

void AJTSMoonResourceActor::ConfigureResourceMesh()
{
	if (!IsValid(ResourceMesh))
	{
		return;
	}

	const TArray<TObjectPtr<UStaticMesh>>* Meshes = nullptr;
	switch (NodeSize)
	{
	case EJTSMoonResourceNodeSize::MediumRock: Meshes = &MediumRockMeshes; break;
	case EJTSMoonResourceNodeSize::LargeRock: Meshes = &LargeRockMeshes; break;
	case EJTSMoonResourceNodeSize::MediumMetalRock: Meshes = &MediumMetalRockMeshes; break;
	case EJTSMoonResourceNodeSize::LargeMetalRock: Meshes = &LargeMetalRockMeshes; break;
	default: break;
	}
	UStaticMesh* const ConfiguredMesh = Meshes && !Meshes->IsEmpty()
		? (*Meshes)[FMath::Max(0, VisualVariantIndex) % Meshes->Num()].Get() : nullptr;
	UStaticMesh* const DesiredMesh = IsValid(ConfiguredMesh) ? ConfiguredMesh : ResourceType == EJTSResourceType::Ore
		? OreMesh.Get()
		: RockMesh.Get();
	if (!IsValid(DesiredMesh))
	{
		return;
	}

	if (ResourceMesh->GetStaticMesh() != DesiredMesh)
	{
		ResourceMesh->SetStaticMesh(DesiredMesh);
		ResourceMesh->EmptyOverrideMaterials();
		ResourceMaterial = nullptr;
		AppliedPresentationMaterial = nullptr;
	}
	const FBox Bounds = DesiredMesh->GetBoundingBox();
	const float Scale = IsValid(ConfiguredMesh) ? 100.0f / FMath::Max(1.0f, Bounds.GetSize().GetAbs().GetMax()) : 1.0f;
	ResourceMesh->SetRelativeScale3D(FVector(Scale));
	ResourceMesh->SetRelativeLocation(IsValid(ConfiguredMesh) ? -Bounds.GetCenter() * Scale : FVector::ZeroVector);
	ResourceMesh->UpdateBounds();
}

void AJTSMoonResourceActor::ApplySurfacePresentationMaterial()
{
	if (!IsValid(ResourceMesh))
	{
		return;
	}

	UMaterialInterface* const DesiredMaterial = RealPlanetSurfaceMaterial.Get();
	if (!IsValid(DesiredMaterial) || AppliedPresentationMaterial == DesiredMaterial)
	{
		return;
	}

	ResourceMaterial = nullptr;
	ResourceMesh->SetMaterial(0, DesiredMaterial);
	AppliedPresentationMaterial = DesiredMaterial;
}

void AJTSMoonResourceActor::ApplyResourceAppearance()
{
	if (!IsValid(ResourceMesh))
	{
		return;
	}

	ConfigureResourceMesh();
	// Authored models retain all their rock/metal material slots.
	if (ResourceMesh->GetStaticMesh() != RockMesh && ResourceMesh->GetStaticMesh() != OreMesh)
	{
		return;
	}
	ApplySurfacePresentationMaterial();

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
		? FLinearColor(0.10f, 0.52f, 0.84f, 1.0f)
		: FLinearColor(0.34f, 0.30f, 0.26f, 1.0f);
	const float ResourceRoughness = bIsOre ? 0.26f : 0.90f;
	const float ResourceMetallic = bIsOre ? 0.62f : 0.0f;
	ResourceMaterial->SetVectorParameterValue(TEXT("ResourceColor"), ResourceColor);
	ResourceMaterial->SetVectorParameterValue(TEXT("Color"), ResourceColor);
	ResourceMaterial->SetVectorParameterValue(TEXT("BaseColor"), ResourceColor);
	ResourceMaterial->SetVectorParameterValue(TEXT("Tint"), ResourceColor);
	ResourceMaterial->SetScalarParameterValue(TEXT("ResourceRoughness"), ResourceRoughness);
	ResourceMaterial->SetScalarParameterValue(TEXT("ResourceMetallic"), ResourceMetallic);
}

bool AJTSMoonResourceActor::CanDisassemble_Implementation(APawn* Operator) const
{
	const auto* Weapon = IsValid(Operator) ? Operator->FindComponentByClass<UJTSStellarWeaponComponent>() : nullptr;
	const auto* Ability = IsValid(Operator) ? Operator->FindComponentByClass<UJTSStellarAbilityComponent>() : nullptr;
	const auto* Def = Weapon ? Weapon->GetEquippedWeaponDefinition() : nullptr;
	const auto* Health = IsValid(Operator) ? Operator->FindComponentByClass<UJTSHealthComponent>() : nullptr;
	return HasAuthority() && Operator && Operator->HasAuthority() && Operator->GetWorld() == GetWorld() && Health && !Health->IsDead()
		&& !IsActorBeingDestroyed() && !bMiningInProgress && RemainingYieldUnits > 0 && Def && Ability && Ability->IsEngineeringMode()
		&& Def->Mode == EJTSStellarWeaponMode::Disassembly && FVector::DistSquared(Operator->GetPawnViewLocation(), GetActorLocation()) <= FMath::Square(Def->RangeCentimeters)
		&& UJTSStellarTargetComponent::HasLineOfSight(GetWorld(), Operator->GetPawnViewLocation(), const_cast<AJTSMoonResourceActor*>(this), Operator);
}
bool AJTSMoonResourceActor::Disassemble_Implementation(APawn* Operator)
{
	if (!CanDisassemble_Implementation(Operator) || !SpawnAllResourceDrops(Operator)) return false;
	RemainingMiningWork = 0; RemainingYieldUnits = 0; Destroy(); return true;
}
