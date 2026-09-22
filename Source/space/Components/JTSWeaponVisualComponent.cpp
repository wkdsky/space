#include "space/Components/JTSWeaponVisualComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSItemTypes.h"

UJTSWeaponVisualComponent::UJTSWeaponVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UJTSWeaponVisualComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureMeshComponents();

	if (UJTSInventoryComponent* const Inventory = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UJTSInventoryComponent>()
		: nullptr)
	{
		Inventory->OnInventoryChanged.AddDynamic(this, &UJTSWeaponVisualComponent::HandleInventoryChanged);
	}

	RefreshWeaponVisual();
}

void UJTSWeaponVisualComponent::EnsureMeshComponents()
{
	if (GetOwner() == nullptr)
	{
		return;
	}

	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!IsValid(CubeMesh) || !IsValid(CylinderMesh))
	{
		return;
	}

	USkeletalMeshComponent* const CharacterMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	USceneComponent* const AttachParent = IsValid(CharacterMesh) ? CharacterMesh : GetOwner()->GetRootComponent();
	if (!IsValid(AttachParent))
	{
		return;
	}

	auto CreateMesh = [this, AttachParent](const FName Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(GetOwner(), Name);
		if (!IsValid(Component))
		{
			return nullptr;
		}
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(false);
		Component->AttachToComponent(AttachParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Component->RegisterComponent();
		return Component;
	};

	WeaponBody = CreateMesh(TEXT("WeaponVisualBody"));
	WeaponBarrel = CreateMesh(TEXT("WeaponVisualBarrel"));
	WeaponSight = CreateMesh(TEXT("WeaponVisualSight"));
	ConfigureAttachment();
}

void UJTSWeaponVisualComponent::ConfigureAttachment()
{
	USkeletalMeshComponent* const CharacterMesh = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<USkeletalMeshComponent>()
		: nullptr;
	if (!IsValid(CharacterMesh))
	{
		return;
	}

	static const FName SocketCandidates[] = {
		TEXT("hand_r"), TEXT("hand_rSocket"), TEXT("RightHand"), TEXT("Hand_R"), TEXT("RightHandSocket")
	};
	for (const FName SocketName : SocketCandidates)
	{
		if (CharacterMesh->DoesSocketExist(SocketName))
		{
			if (IsValid(WeaponBody)) WeaponBody->AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
			if (IsValid(WeaponBarrel)) WeaponBarrel->AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
			if (IsValid(WeaponSight)) WeaponSight->AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
			break;
		}
	}
}

void UJTSWeaponVisualComponent::RefreshWeaponVisual()
{
	if (!IsValid(WeaponBody) || !IsValid(WeaponBarrel) || !IsValid(WeaponSight))
	{
		return;
	}

	const UJTSInventoryComponent* const Inventory = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UJTSInventoryComponent>()
		: nullptr;
	const EJTSItemId ItemId = IsValid(Inventory) ? Inventory->GetActiveItemId() : EJTSItemId::None;
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
	if (!IsValid(Definition) || !Definition->IsRangedWeapon())
	{
		SetVisible(false);
		return;
	}

	WeaponBody->SetStaticMesh(CubeMesh);
	WeaponBarrel->SetStaticMesh(CylinderMesh);
	WeaponSight->SetStaticMesh(CylinderMesh);
	WeaponBody->SetRelativeRotation(FRotator::ZeroRotator);
	WeaponBarrel->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	WeaponSight->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));

	FVector BodyScale(0.55f, 0.14f, 0.14f);
	FVector BarrelScale(0.16f, 0.16f, 0.42f);
	FVector SightScale(0.0f, 0.0f, 0.0f);
	DefaultBodyLocation = FVector(9.0f, 0.0f, 0.0f);
	DefaultBarrelLocation = FVector(32.0f, 0.0f, 0.0f);
	DefaultSightLocation = FVector(13.0f, 0.0f, 9.0f);

	switch (ItemId)
	{
	case EJTSItemId::MachineGun:
		BodyScale = FVector(0.95f, 0.18f, 0.16f);
		BarrelScale = FVector(0.24f, 0.24f, 0.72f);
		DefaultBodyLocation = FVector(16.0f, 0.0f, 0.0f);
		DefaultBarrelLocation = FVector(54.0f, 0.0f, 0.0f);
		break;
	case EJTSItemId::Sniper:
		BodyScale = FVector(1.25f, 0.16f, 0.12f);
		BarrelScale = FVector(0.18f, 0.18f, 1.10f);
		SightScale = FVector(0.10f, 0.10f, 0.42f);
		DefaultBodyLocation = FVector(22.0f, 0.0f, 0.0f);
		DefaultBarrelLocation = FVector(82.0f, 0.0f, 0.0f);
		DefaultSightLocation = FVector(18.0f, 0.0f, 11.0f);
		break;
	case EJTSItemId::Pistol:
	default:
		break;
	}

	WeaponBody->SetVisibility(true);
	WeaponBarrel->SetVisibility(true);
	WeaponSight->SetVisibility(SightScale.X > 0.0f);
	WeaponBody->SetRelativeScale3D(BodyScale);
	WeaponBarrel->SetRelativeScale3D(BarrelScale);
	WeaponSight->SetRelativeScale3D(SightScale);
	SetAimAlpha(AimAlpha);
}

void UJTSWeaponVisualComponent::SetAimAlpha(float NewAimAlpha)
{
	AimAlpha = FMath::Clamp(NewAimAlpha, 0.0f, 1.0f);
	if (IsValid(WeaponBody))
	{
		WeaponBody->SetRelativeLocation(DefaultBodyLocation + FVector(0.0f, 1.5f * AimAlpha, 2.0f * AimAlpha));
	}
	if (IsValid(WeaponBarrel))
	{
		WeaponBarrel->SetRelativeLocation(DefaultBarrelLocation + FVector(0.0f, 1.5f * AimAlpha, 2.0f * AimAlpha));
	}
	if (IsValid(WeaponSight))
	{
		WeaponSight->SetRelativeLocation(DefaultSightLocation + FVector(0.0f, 1.5f * AimAlpha, 2.0f * AimAlpha));
	}
}

void UJTSWeaponVisualComponent::SetVisible(bool bVisible)
{
	if (IsValid(WeaponBody)) WeaponBody->SetVisibility(bVisible);
	if (IsValid(WeaponBarrel)) WeaponBarrel->SetVisibility(bVisible);
	if (IsValid(WeaponSight)) WeaponSight->SetVisibility(bVisible && WeaponSight->GetStaticMesh() != nullptr);
}

void UJTSWeaponVisualComponent::HandleInventoryChanged(int32 UsedSlots, int32 Capacity)
{
	static_cast<void>(UsedSlots);
	static_cast<void>(Capacity);
	RefreshWeaponVisual();
}
