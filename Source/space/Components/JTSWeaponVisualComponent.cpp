#include "space/Components/JTSWeaponVisualComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Math/RotationMatrix.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSItemTypes.h"

UJTSWeaponVisualComponent::UJTSWeaponVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetAutoActivate(true);
}

void UJTSWeaponVisualComponent::BeginPlay()
{
	Super::BeginPlay();
	Activate(true);
	EnsureMeshComponents();

	if (UJTSInventoryComponent* const Inventory = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UJTSInventoryComponent>()
		: nullptr)
	{
		Inventory->OnInventoryChanged.AddDynamic(this, &UJTSWeaponVisualComponent::HandleInventoryChanged);
	}

	RefreshWeaponVisual();
}

void UJTSWeaponVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttachmentValidationTimerHandle);
		World->GetTimerManager().ClearTimer(MuzzleFlashTimerHandle);
	}
	bMeleeSwingPresentationActive = false;
	Super::EndPlay(EndPlayReason);
}

void UJTSWeaponVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ShotKickAlpha = FMath::FInterpTo(ShotKickAlpha, 0.0f, DeltaTime, FMath::Max(1.0f, ShotKickRecoverySpeed));
	if (bMeleeSwingPresentationActive)
	{
		MeleeSwingElapsed += DeltaTime;
		if (MeleeSwingElapsed >= MeleeSwingPresentationSeconds)
		{
			bMeleeSwingPresentationActive = false;
		}
	}
	if (ShotKickAlpha < 0.01f)
	{
		ShotKickAlpha = 0.0f;
	}
	UpdatePalmAnchor();
	ApplyPresentationTransform();
	const bool bHeldItemVisible = IsValid(WeaponBody) && WeaponBody->IsVisible();
	if (!bHeldItemVisible && ShotKickAlpha <= 0.0f && !bMeleeSwingPresentationActive)
	{
		SetComponentTickEnabled(false);
	}
}

void UJTSWeaponVisualComponent::EnsureMeshComponents()
{
	if (GetOwner() == nullptr || bMeshComponentsInitialized)
	{
		return;
	}

	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!IsValid(CubeMesh))
	{
		UE_LOG(LogTemp, Warning, TEXT("JTS weapon visual: Cube mesh failed to load for %s."), *GetNameSafe(GetOwner()));
		return;
	}
	BasicShapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	USkeletalMeshComponent* const CharacterMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	if (IsValid(CharacterMesh)) AddTickPrerequisiteComponent(CharacterMesh);
	USceneComponent* const AttachParent = IsValid(CharacterMesh) ? CharacterMesh : GetOwner()->GetRootComponent();
	if (!IsValid(AttachParent))
	{
		return;
	}

	// The imported skeleton applies a large animated scale at the hand.  A zero-offset scene
	// component can still inherit the hand's position and rotation while ignoring that scale.
	// The visible pieces attach to this anchor, so their authored centimetre offsets never get
	// multiplied by the skeletal socket's scale during an animation update.
	HandAttachmentAnchor = NewObject<USceneComponent>(GetOwner(), TEXT("WeaponHandAttachmentAnchor"));
	if (!IsValid(HandAttachmentAnchor))
	{
		return;
	}
	GetOwner()->AddInstanceComponent(HandAttachmentAnchor);
	HandAttachmentAnchor->SetMobility(EComponentMobility::Movable);
	HandAttachmentAnchor->AttachToComponent(AttachParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	HandAttachmentAnchor->SetAbsolute(false, false, true);
	if (UWorld* const World = GetOwner()->GetWorld())
	{
		HandAttachmentAnchor->RegisterComponentWithWorld(World);
	}

	auto CreateScene = [this](const FName Name, USceneComponent* Parent) -> USceneComponent*
	{
		if (!IsValid(Parent))
		{
			return nullptr;
		}

		USceneComponent* Component = NewObject<USceneComponent>(GetOwner(), Name);
		if (!IsValid(Component))
		{
			return nullptr;
		}

		GetOwner()->AddInstanceComponent(Component);
		Component->SetMobility(EComponentMobility::Movable);
		Component->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		// Each root follows the hand's pose but keeps prototype measurements in centimetres.
		Component->SetAbsolute(false, false, true);
		if (UWorld* const World = GetOwner()->GetWorld())
		{
			Component->RegisterComponentWithWorld(World);
		}
		return Component;
	};

	// Keep presentation offsets, the item-local coordinate system, and the hand socket separate.
	// This lets the model's authored grip point, not its arbitrary mesh origin, land at the hand.
	WeaponPresentationRoot = CreateScene(TEXT("WeaponPresentationRoot"), HandAttachmentAnchor);
	WeaponModelRoot = CreateScene(TEXT("WeaponModelRoot"), WeaponPresentationRoot);
	WeaponMuzzle = CreateScene(TEXT("WeaponMuzzle"), WeaponModelRoot);
	MuzzleFlash = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("WeaponMuzzleFlash"));
	if (IsValid(MuzzleFlash) && IsValid(WeaponMuzzle))
	{
		GetOwner()->AddInstanceComponent(MuzzleFlash);
		MuzzleFlash->SetMobility(EComponentMobility::Movable);
		MuzzleFlash->AttachToComponent(WeaponMuzzle, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		MuzzleFlash->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MuzzleFlash->SetGenerateOverlapEvents(false);
		MuzzleFlash->SetCanEverAffectNavigation(false);
		MuzzleFlash->SetCastShadow(false);
		MuzzleFlash->SetAbsolute(false, false, true);
		MuzzleFlash->SetRelativeLocation(FVector(4.0f, 0.0f, 0.0f));
		MuzzleFlash->SetWorldScale3D(FVector(0.15f, 0.07f, 0.07f));
		MuzzleFlash->SetVisibility(false);
		if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
		{
			MuzzleFlash->SetStaticMesh(Sphere);
		}
		if (UWorld* World = GetWorld())
		{
			MuzzleFlash->RegisterComponentWithWorld(World);
		}
	}
	MuzzleLight = NewObject<UPointLightComponent>(GetOwner(), TEXT("WeaponMuzzleLight"));
	if (IsValid(MuzzleLight) && IsValid(WeaponMuzzle))
	{
		GetOwner()->AddInstanceComponent(MuzzleLight);
		MuzzleLight->AttachToComponent(WeaponMuzzle, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		MuzzleLight->SetIntensity(3500.0f);
		MuzzleLight->SetAttenuationRadius(220.0f);
		MuzzleLight->SetVisibility(false);
		if (UWorld* World = GetWorld())
		{
			MuzzleLight->RegisterComponentWithWorld(World);
		}
	}

	auto CreateMesh = [this, AttachParent](const FName Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(GetOwner(), Name);
		if (!IsValid(Component))
		{
			return nullptr;
		}
		GetOwner()->AddInstanceComponent(Component);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(CubeMesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(true);
		Component->SetHiddenInGame(false);
		Component->SetVisibility(false, true);
		if (IsValid(BasicShapeMaterial))
		{
			Component->SetMaterial(0, BasicShapeMaterial);
		}
		USceneComponent* const PieceParent = IsValid(WeaponModelRoot)
			? WeaponModelRoot.Get()
			: (IsValid(HandAttachmentAnchor) ? HandAttachmentAnchor.Get() : AttachParent);
		Component->AttachToComponent(PieceParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		// Imported character skeletons can carry a non-unit bone scale. The primitive is sized in
		// world centimetres, so it must never inherit that scale from the mesh or a hand socket.
		Component->SetAbsolute(false, false, true);
		if (UWorld* const World = GetOwner()->GetWorld())
		{
			Component->RegisterComponentWithWorld(World);
		}
		return Component;
	};

	WeaponGrip = CreateMesh(TEXT("WeaponVisualGrip"));
	WeaponBody = CreateMesh(TEXT("WeaponVisualBody"));
	WeaponBarrel = CreateMesh(TEXT("WeaponVisualBarrel"));
	WeaponSight = CreateMesh(TEXT("WeaponVisualSight"));
	WeaponGripMaterial = IsValid(WeaponGrip) ? WeaponGrip->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
	WeaponBodyMaterial = IsValid(WeaponBody) ? WeaponBody->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
	WeaponBarrelMaterial = IsValid(WeaponBarrel) ? WeaponBarrel->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
	WeaponSightMaterial = IsValid(WeaponSight) ? WeaponSight->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
	ConfigureAttachment();
	bMeshComponentsInitialized = IsValid(HandAttachmentAnchor)
		&& IsValid(WeaponPresentationRoot)
		&& IsValid(WeaponModelRoot)
		&& IsValid(WeaponMuzzle)
		&& IsValid(MuzzleFlash)
		&& IsValid(MuzzleLight)
		&& IsValid(WeaponGrip)
		&& IsValid(WeaponBody)
		&& IsValid(WeaponBarrel)
		&& IsValid(WeaponSight);
	UE_LOG(
		LogTemp,
		Log,
		TEXT("JTS weapon visual: created for %s Grip=%s Body=%s Barrel=%s Sight=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(WeaponGrip),
		*GetNameSafe(WeaponBody),
		*GetNameSafe(WeaponBarrel),
		*GetNameSafe(WeaponSight));
}

void UJTSWeaponVisualComponent::ConfigureAttachment()
{
	bUsingFallbackAttachment = false;
	if (GetOwner() == nullptr || !IsValid(HandAttachmentAnchor))
	{
		return;
	}

	USkeletalMeshComponent* const CharacterMesh = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<USkeletalMeshComponent>()
		: nullptr;
	auto AttachAnchor = [this](USceneComponent* Parent, const FName SocketName)
	{
		if (!IsValid(HandAttachmentAnchor) || !IsValid(Parent))
		{
			return;
		}
		HandAttachmentAnchor->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
		HandAttachmentAnchor->SetRelativeLocation(HandSocketRelativeLocation);
		HandAttachmentAnchor->SetRelativeRotation(HandSocketRelativeRotation);
		// The anchor must follow the socket's location/rotation exactly but never its animated scale.
		HandAttachmentAnchor->SetAbsolute(false, false, true);
	};

	if (IsValid(CharacterMesh))
	{
		// The palm is computed each pose from Wrist_R toward Index1_R. Attaching to
		// weapon_r instead multiplied its few-centimetre offset by the skeleton's
		// bone scale and threw the item far from the body.
		static const FName SocketCandidates[] = {
			TEXT("hand_r"), TEXT("hand_rSocket"), TEXT("RightHand"), TEXT("Hand_R"), TEXT("RightHandSocket")
		};
		for (const FName SocketName : SocketCandidates)
		{
			if (!CharacterMesh->DoesSocketExist(SocketName))
			{
				continue;
			}

			AttachAnchor(CharacterMesh, SocketName);
			UE_LOG(LogTemp, Log, TEXT("JTS weapon visual: attached %s to hand socket %s."), *GetNameSafe(GetOwner()), *SocketName.ToString());
			return;
		}
	}

	AttachToRootFallback();
}

void UJTSWeaponVisualComponent::UpdatePalmAnchor()
{
	if (bUsingFallbackAttachment || !IsValid(HandAttachmentAnchor) || GetOwner() == nullptr)
	{
		return;
	}

	USkeletalMeshComponent* const CharacterMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	if (!IsValid(CharacterMesh))
	{
		return;
	}

	const FName WristBone(TEXT("Wrist_R"));
	const FName IndexBone(TEXT("Index1_R"));
	if (CharacterMesh->GetBoneIndex(WristBone) == INDEX_NONE
		|| CharacterMesh->GetBoneIndex(IndexBone) == INDEX_NONE)
	{
		return;
	}

	const FVector Wrist = CharacterMesh->GetBoneLocation(WristBone, EBoneSpaces::WorldSpace);
	const FVector Index = CharacterMesh->GetBoneLocation(IndexBone, EBoneSpaces::WorldSpace);
	const FVector AlongFingers = Index - Wrist;
	if (AlongFingers.SizeSquared() < 1.0f)
	{
		return;
	}

	// Index1_R is the knuckle. A short step past it lands in the palm instead of the wrist.
	// Location is absolute so the skeleton's bone scale cannot multiply this centimetre
	// offset and throw the item away from the hand.
	const FVector Palm = Wrist + AlongFingers * 1.35f;
	const FVector AlongHand = AlongFingers.GetSafeNormal();
	const FName ForearmBone(TEXT("LowerArm_R"));
	FVector AlongForearm = AlongHand;
	if (CharacterMesh->GetBoneIndex(ForearmBone) != INDEX_NONE)
	{
		const FVector Elbow = CharacterMesh->GetBoneLocation(ForearmBone, EBoneSpaces::WorldSpace);
		const FVector Forearm = (Wrist - Elbow).GetSafeNormal();
		if (!Forearm.IsNearlyZero())
		{
			AlongForearm = Forearm;
		}
	}

	const AActor* const Owner = GetOwner();
	const FVector Up = Owner->GetActorUpVector().GetSafeNormal();
	FQuat PalmRotation = Owner->GetActorQuat();
	if (bRangedVisible)
	{
		// The barrel is the forearm. Wherever the arm points, the muzzle points.
		const FVector Side = FVector::CrossProduct(Up, AlongForearm).GetSafeNormal();
		const FVector PalmUp = Side.IsNearlyZero()
			? Up
			: FVector::CrossProduct(AlongForearm, Side).GetSafeNormal();
		PalmRotation = FRotationMatrix::MakeFromXZ(AlongForearm, PalmUp).ToQuat();
	}
	else
	{
		// At rest the shaft stands on the actor up. A chop swings that same axis
		// with the forearm, so the whole tool travels the arc the hand travels.
		const float Swing = FVector::DotProduct(AlongForearm, Up);
		const FVector Shaft = (Up + AlongForearm * FMath::Max(0.0f, -Swing)).GetSafeNormal();
		const FVector Forward = FVector::VectorPlaneProject(Owner->GetActorForwardVector(), Shaft).GetSafeNormal();
		if (!Shaft.IsNearlyZero() && !Forward.IsNearlyZero())
		{
			PalmRotation = FRotationMatrix::MakeFromXZ(Shaft, Forward).ToQuat();
		}
	}

	HandAttachmentAnchor->SetAbsolute(true, true, true);
	HandAttachmentAnchor->SetWorldLocation(Palm);
	HandAttachmentAnchor->SetWorldRotation(PalmRotation);
}

void UJTSWeaponVisualComponent::AttachToRootFallback()
{
	if (GetOwner() == nullptr)
	{
		return;
	}

	USceneComponent* const RootComponent = GetOwner()->GetRootComponent();
	if (!IsValid(RootComponent))
	{
		return;
	}

	if (!IsValid(HandAttachmentAnchor))
	{
		return;
	}

	HandAttachmentAnchor->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	HandAttachmentAnchor->SetAbsolute(false, false, true);
	HandAttachmentAnchor->SetRelativeRotation(FRotator::ZeroRotator);
	HandAttachmentAnchor->SetRelativeLocation(FallbackHandRelativeLocation);
	bUsingFallbackAttachment = true;
	UE_LOG(LogTemp, Log, TEXT("JTS weapon visual: attached %s to root fallback at %s."), *GetNameSafe(GetOwner()), *FallbackHandRelativeLocation.ToCompactString());

	ApplyPresentationTransform();
}

bool UJTSWeaponVisualComponent::IsCurrentAttachmentPlausible() const
{
	if (GetOwner() == nullptr || !IsValid(HandAttachmentAnchor))
	{
		return false;
	}

	const float DistanceFromCharacter = FVector::Distance(HandAttachmentAnchor->GetComponentLocation(), GetOwner()->GetActorLocation());
	return DistanceFromCharacter <= FMath::Max(1.0f, MaximumTrustedSocketDistance);
}

void UJTSWeaponVisualComponent::ValidateAttachmentAfterPose()
{
	if (bUsingFallbackAttachment || !IsValid(HandAttachmentAnchor) || !IsValid(WeaponBody) || !WeaponBody->IsVisible())
	{
		return;
	}

	if (IsCurrentAttachmentPlausible())
	{
		return;
	}

	const float DistanceFromCharacter = GetOwner() != nullptr
		? FVector::Distance(HandAttachmentAnchor->GetComponentLocation(), GetOwner()->GetActorLocation())
		: TNumericLimits<float>::Max();
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("JTS weapon visual: animated hand anchor on %s resolved %.1f cm from character; retaining direct hand attachment."),
		*GetNameSafe(GetOwner()),
		DistanceFromCharacter);
}

void UJTSWeaponVisualComponent::RefreshWeaponVisual()
{
	EnsureMeshComponents();
	if (!IsValid(WeaponPresentationRoot)
		|| !IsValid(WeaponModelRoot)
		|| !IsValid(WeaponMuzzle)
		|| !IsValid(WeaponGrip)
		|| !IsValid(WeaponBody)
		|| !IsValid(WeaponBarrel)
		|| !IsValid(WeaponSight))
	{
		UE_LOG(LogTemp, Warning, TEXT("JTS weapon visual: missing mesh components for %s."), *GetNameSafe(GetOwner()));
		return;
	}

	const UJTSInventoryComponent* const Inventory = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UJTSInventoryComponent>()
		: nullptr;
	const EJTSItemId ItemId = IsValid(Inventory) ? Inventory->GetActiveItemId() : EJTSItemId::None;
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
	if (!IsValid(Definition) || !Definition->IsHoldable())
	{
		SetVisible(false);
		UE_LOG(LogTemp, Log, TEXT("JTS weapon visual: hidden for %s ActiveItem=%d."), *GetNameSafe(GetOwner()), static_cast<int32>(ItemId));
		return;
	}
	bRangedVisible = Definition->IsRangedWeapon();
	// Mesh +X is the barrel or the shaft. The palm anchor already aims that axis:
	// a gun along the forearm, a tool along the chop. No extra local pitch here.
	DefaultCarryRotation = FRotator::ZeroRotator;
	SetComponentTickEnabled(true);

	FVector BodyScale(0.48f, 0.18f, 0.14f);
	FVector BarrelScale(0.26f, 0.08f, 0.08f);
	FVector SightScale(0.08f, 0.06f, 0.10f);
	DefaultBodyLocation = FVector(13.0f, 0.0f, 0.0f);
	DefaultBarrelLocation = FVector(48.0f, 0.0f, 0.0f);
	DefaultSightLocation = FVector(19.0f, 0.0f, 12.0f);
	DefaultGripTransform = FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -8.0f), FVector::OneVector);
	DefaultMuzzleTransform = FTransform(FQuat::Identity, FVector(61.0f, 0.0f, 0.0f), FVector::OneVector);
	DefaultGripScale = FVector(0.16f, 0.12f, 0.32f);

	switch (ItemId)
	{
	case EJTSItemId::MachineGun:
		BodyScale = FVector(0.88f, 0.22f, 0.18f);
		BarrelScale = FVector(0.38f, 0.10f, 0.10f);
		SightScale = FVector(0.12f, 0.08f, 0.12f);
		DefaultBodyLocation = FVector(18.0f, 0.0f, 0.0f);
		DefaultBarrelLocation = FVector(76.0f, 0.0f, 0.0f);
		DefaultSightLocation = FVector(28.0f, 0.0f, 16.0f);
		DefaultGripTransform = FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -10.0f), FVector::OneVector);
		DefaultMuzzleTransform = FTransform(FQuat::Identity, FVector(95.0f, 0.0f, 0.0f), FVector::OneVector);
		DefaultGripScale = FVector(0.22f, 0.16f, 0.40f);
		break;
	case EJTSItemId::Sniper:
		BodyScale = FVector(1.10f, 0.20f, 0.14f);
		BarrelScale = FVector(0.62f, 0.08f, 0.08f);
		SightScale = FVector(0.12f, 0.09f, 0.18f);
		DefaultBodyLocation = FVector(23.0f, 0.0f, 0.0f);
		DefaultBarrelLocation = FVector(96.0f, 0.0f, 0.0f);
		DefaultSightLocation = FVector(30.0f, 0.0f, 17.0f);
		DefaultGripTransform = FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -10.0f), FVector::OneVector);
		DefaultMuzzleTransform = FTransform(FQuat::Identity, FVector(127.0f, 0.0f, 0.0f), FVector::OneVector);
		DefaultGripScale = FVector(0.22f, 0.15f, 0.42f);
		break;
	case EJTSItemId::Pistol:
	default:
		break;
	}

	if (!Definition->IsRangedWeapon())
	{
		SightScale = FVector::ZeroVector;
		switch (ItemId)
		{
		case EJTSItemId::Knife:
			BodyScale = FVector(0.30f, 0.09f, 0.10f);
			BarrelScale = FVector(0.46f, 0.06f, 0.05f);
			DefaultBodyLocation = FVector(4.0f, 0.0f, 0.0f);
			DefaultBarrelLocation = FVector(42.0f, 0.0f, 0.0f);
			DefaultGripTransform = FTransform(FQuat::Identity, FVector(0.0f, 0.0f, 0.0f), FVector::OneVector);
			DefaultMuzzleTransform = FTransform(FQuat::Identity, FVector(65.0f, 0.0f, 0.0f), FVector::OneVector);
			DefaultGripScale = FVector(0.22f, 0.12f, 0.12f);
			break;
		case EJTSItemId::Pickaxe:
		case EJTSItemId::Axe:
			BodyScale = FVector(0.76f, 0.07f, 0.07f);
			BarrelScale = FVector(0.30f, 0.26f, 0.18f);
			DefaultBodyLocation = FVector(5.0f, 0.0f, 0.0f);
			DefaultBarrelLocation = FVector(53.0f, 0.0f, 5.0f);
			DefaultGripTransform = FTransform(FQuat::Identity, FVector(-6.0f, 0.0f, 0.0f), FVector::OneVector);
			DefaultMuzzleTransform = FTransform(FQuat::Identity, FVector(68.0f, 0.0f, 5.0f), FVector::OneVector);
			DefaultGripScale = FVector(0.26f, 0.12f, 0.14f);
			break;
		default:
			break;
		}
	}

	// A real weapon mesh can keep its own arbitrary import origin.  Artists author the grip and
	// muzzle pivots on the item Data Asset; gameplay never needs to know which mesh is selected.
	if (Definition->HeldPresentation.bOverridePrototypeProfile)
	{
		DefaultGripTransform = Definition->HeldPresentation.GripTransform;
		DefaultMuzzleTransform = Definition->HeldPresentation.MuzzleTransform;
		DefaultGripScale = Definition->HeldPresentation.GripScale;
	}

	WeaponGrip->SetVisibility(true);
	WeaponBody->SetVisibility(true);
	WeaponBarrel->SetVisibility(true);
	WeaponSight->SetVisibility(SightScale.X > 0.0f);
	WeaponGrip->SetHiddenInGame(false);
	WeaponBody->SetHiddenInGame(false);
	WeaponBarrel->SetHiddenInGame(false);
	WeaponSight->SetHiddenInGame(false);
	// These profile values are world-space dimensions relative to the 100 cm engine cube.
	// SetWorldScale3D together with absolute scale above prevents skeletal socket import scale
	// from silently multiplying every held item by 100.
	WeaponGrip->SetWorldScale3D(DefaultGripScale);
	WeaponBody->SetWorldScale3D(BodyScale);
	WeaponBarrel->SetWorldScale3D(BarrelScale);
	WeaponSight->SetWorldScale3D(SightScale);

	FLinearColor ItemColor = Definition->AccentColor;
	if (ItemId == EJTSItemId::Knife)
	{
		ItemColor = FLinearColor(0.78f, 0.88f, 1.0f, 1.0f);
	}
	else if (ItemId == EJTSItemId::Axe || ItemId == EJTSItemId::Pickaxe)
	{
		ItemColor = FLinearColor(1.0f, 0.42f, 0.08f, 1.0f);
	}

	auto ApplyMaterialColor = [](UMaterialInstanceDynamic* Material, const FLinearColor& Color)
	{
		if (!IsValid(Material))
		{
			return;
		}
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
		Material->SetVectorParameterValue(TEXT("Tint"), Color);
		Material->SetVectorParameterValue(TEXT("EmissiveColor"), Color * 0.15f);
	};
	ApplyMaterialColor(WeaponGripMaterial, FLinearColor(0.035f, 0.05f, 0.07f, 1.0f));
	ApplyMaterialColor(WeaponBodyMaterial, ItemColor);
	ApplyMaterialColor(WeaponBarrelMaterial, ItemColor);
	ApplyMaterialColor(WeaponSightMaterial, ItemColor);
	SetAimAlpha(AimAlpha);
	UpdatePalmAnchor();
	ValidateAttachmentAfterPose();
	if (!bUsingFallbackAttachment)
	{
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AttachmentValidationTimerHandle);
			// Some imported animation graphs update socket transforms after BeginPlay. Recheck once
			// after that pose evaluation instead of trusting only the initial socket transform.
			World->GetTimerManager().SetTimer(
				AttachmentValidationTimerHandle,
				this,
				&UJTSWeaponVisualComponent::ValidateAttachmentAfterPose,
				0.15f,
				false);
		}
	}
	UE_LOG(
		LogTemp,
		Log,
		TEXT("JTS weapon visual: shown for %s ActiveItem=%d Grip=%s Muzzle=%s BodyScale=%s WorldLocation=%s WorldRotation=%s WorldScale=%s"),
		*GetNameSafe(GetOwner()),
		static_cast<int32>(ItemId),
		*WeaponGrip->GetComponentLocation().ToCompactString(),
		*WeaponMuzzle->GetComponentLocation().ToCompactString(),
		*BodyScale.ToCompactString(),
		*WeaponBody->GetComponentLocation().ToCompactString(),
		*WeaponBody->GetComponentRotation().ToCompactString(),
		*WeaponBody->GetComponentScale().ToCompactString());
}

bool UJTSWeaponVisualComponent::GetMuzzleWorldLocation(FVector& OutLocation) const
{
	OutLocation = FVector::ZeroVector;
	if (!IsValid(WeaponMuzzle) || !IsValid(WeaponBody) || !WeaponBody->IsVisible())
	{
		return false;
	}

	OutLocation = WeaponMuzzle->GetComponentLocation();
	return true;
}

void UJTSWeaponVisualComponent::SetAimAlpha(float NewAimAlpha)
{
	AimAlpha = FMath::Clamp(NewAimAlpha, 0.0f, 1.0f);
	ApplyPresentationTransform();
}

void UJTSWeaponVisualComponent::PlayShotPresentation(UMaterialInterface* GlowMaterial,
	const FLinearColor& Color)
{
	if (!IsValid(WeaponBody) || !WeaponBody->IsVisible())
	{
		return;
	}
	ShotKickAlpha = 1.0f;
	SetComponentTickEnabled(true);
	ApplyPresentationTransform();
	if (IsValid(MuzzleFlash))
	{
		if (IsValid(GlowMaterial))
		{
			MuzzleFlash->SetMaterial(0, GlowMaterial);
		}
		if (UMaterialInstanceDynamic* FlashMaterial = MuzzleFlash->CreateAndSetMaterialInstanceDynamic(0))
		{
			FlashMaterial->SetVectorParameterValue(TEXT("GlowColor"), Color * 12.0f);
			FlashMaterial->SetVectorParameterValue(TEXT("Color"), Color * 12.0f);
		}
		MuzzleFlash->SetVisibility(true);
	}
	if (IsValid(MuzzleLight))
	{
		MuzzleLight->SetLightColor(Color);
		MuzzleLight->SetVisibility(true);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MuzzleFlashTimerHandle);
		World->GetTimerManager().SetTimer(MuzzleFlashTimerHandle, this,
			&UJTSWeaponVisualComponent::HideShotFlash, FMath::Max(0.01f, MuzzleFlashSeconds), false);
	}
}

void UJTSWeaponVisualComponent::HideShotFlash()
{
	if (IsValid(MuzzleFlash)) MuzzleFlash->SetVisibility(false);
	if (IsValid(MuzzleLight)) MuzzleLight->SetVisibility(false);
}

void UJTSWeaponVisualComponent::PlayMeleeSwingPresentation()
{
	if (!IsValid(WeaponBody) || !WeaponBody->IsVisible())
	{
		return;
	}

	bMeleeSwingPresentationActive = true;
	bMeleeSwingReverse = !bMeleeSwingReverse;
	MeleeSwingElapsed = 0.0f;
	SetComponentTickEnabled(true);
	ApplyPresentationTransform();
}

void UJTSWeaponVisualComponent::ApplyPresentationTransform()
{
	const FVector AimOffset(-5.0f * ShotKickAlpha, 1.5f * AimAlpha, 2.0f * AimAlpha + 1.5f * ShotKickAlpha);
	if (IsValid(WeaponPresentationRoot))
	{
		// Aim and melee animation rotate the whole held item around its actual grip, not around
		// the origin of each primitive piece.
		WeaponPresentationRoot->SetRelativeLocation(AimOffset);
		WeaponPresentationRoot->SetAbsolute(false, false, true);
		// Relative to the palm frame: ranged stays forward, melee pitch stands the shaft up.
		// The grip inverse still pins the handle on the palm, so this rotation cannot
		// throw the item off the hand the way a world rotation on this root did.
		const FRotator Kick(4.0f * ShotKickAlpha, 0.0f, 0.0f);
		WeaponPresentationRoot->SetRelativeRotation(DefaultCarryRotation + Kick);
	}
	if (IsValid(WeaponModelRoot))
	{
		FTransform GripInverse = DefaultGripTransform;
		GripInverse.SetScale3D(FVector::OneVector);
		WeaponModelRoot->SetRelativeTransform(GripInverse.Inverse());
		WeaponModelRoot->SetAbsolute(false, false, true);
	}
	if (IsValid(WeaponGrip))
	{
		FTransform VisibleGripTransform = DefaultGripTransform;
		VisibleGripTransform.SetScale3D(FVector::OneVector);
		WeaponGrip->SetRelativeTransform(VisibleGripTransform);
		WeaponGrip->SetWorldScale3D(DefaultGripScale);
	}
	if (IsValid(WeaponMuzzle))
	{
		FTransform MuzzleTransform = DefaultMuzzleTransform;
		MuzzleTransform.SetScale3D(FVector::OneVector);
		WeaponMuzzle->SetRelativeTransform(MuzzleTransform);
		WeaponMuzzle->SetAbsolute(false, false, true);
	}
	if (IsValid(WeaponBody))
	{
		WeaponBody->SetRelativeRotation(FRotator::ZeroRotator);
		WeaponBody->SetRelativeLocation(DefaultBodyLocation);
	}
	if (IsValid(WeaponBarrel))
	{
		WeaponBarrel->SetRelativeRotation(FRotator::ZeroRotator);
		WeaponBarrel->SetRelativeLocation(DefaultBarrelLocation);
	}
	if (IsValid(WeaponSight))
	{
		WeaponSight->SetRelativeRotation(FRotator::ZeroRotator);
		WeaponSight->SetRelativeLocation(DefaultSightLocation);
	}
}

void UJTSWeaponVisualComponent::SetVisible(bool bVisible)
{
	if (!bVisible)
	{
		bRangedVisible = false;
		bMeleeSwingPresentationActive = false;
		ShotKickAlpha = 0.0f;
		SetComponentTickEnabled(false);
		HideShotFlash();
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AttachmentValidationTimerHandle);
			World->GetTimerManager().ClearTimer(MuzzleFlashTimerHandle);
		}
	}
	if (IsValid(WeaponGrip)) WeaponGrip->SetVisibility(bVisible);
	if (IsValid(WeaponBody)) WeaponBody->SetVisibility(bVisible);
	if (IsValid(WeaponBarrel)) WeaponBarrel->SetVisibility(bVisible);
	if (IsValid(WeaponSight)) WeaponSight->SetVisibility(bVisible && WeaponSight->GetStaticMesh() != nullptr);
	if (IsValid(WeaponGrip)) WeaponGrip->SetHiddenInGame(!bVisible);
	if (IsValid(WeaponBody)) WeaponBody->SetHiddenInGame(!bVisible);
	if (IsValid(WeaponBarrel)) WeaponBarrel->SetHiddenInGame(!bVisible);
	if (IsValid(WeaponSight)) WeaponSight->SetHiddenInGame(!bVisible);
}

void UJTSWeaponVisualComponent::HandleInventoryChanged(int32 UsedSlots, int32 Capacity)
{
	static_cast<void>(UsedSlots);
	static_cast<void>(Capacity);
	RefreshWeaponVisual();
}
