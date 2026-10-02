#include "space/Components/JTSWeaponVisualComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Math/RotationMatrix.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSWallClimbComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSItemTypes.h"
#include "space/Player/JTSCharacter.h"

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
	UpdateLeftPistolAnchor();
	UpdateWaistLamp();
	ApplyPresentationTransform();
	const bool bHeldItemVisible = IsValid(WeaponBody) && WeaponBody->IsVisible();
	const bool bLampVisible = IsValid(WaistLampLight) && WaistLampLight->IsVisible();
	if (!bHeldItemVisible && !bLampVisible && !bTwoHandVisible && ShotKickAlpha <= 0.0f && !bMeleeSwingPresentationActive)
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
	auto MakePiece = [this](const FName Name, USceneComponent* Parent) -> UStaticMeshComponent*
	{
		if (!IsValid(Parent) || !IsValid(CubeMesh))
		{
			return nullptr;
		}
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
		Component->SetVisibility(false);
		Component->SetHiddenInGame(true);
		if (IsValid(BasicShapeMaterial))
		{
			Component->SetMaterial(0, BasicShapeMaterial);
		}
		Component->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Component->SetAbsolute(false, false, true);
		if (UWorld* const World = GetOwner()->GetWorld())
		{
			Component->RegisterComponentWithWorld(World);
		}
		return Component;
	};

	LeftHandAttachmentAnchor = NewObject<USceneComponent>(GetOwner(), TEXT("LeftPistolAnchor"));
	if (IsValid(LeftHandAttachmentAnchor))
	{
		GetOwner()->AddInstanceComponent(LeftHandAttachmentAnchor);
		LeftHandAttachmentAnchor->SetMobility(EComponentMobility::Movable);
		LeftHandAttachmentAnchor->AttachToComponent(AttachParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		LeftHandAttachmentAnchor->SetAbsolute(true, true, true);
		if (UWorld* const World = GetOwner()->GetWorld())
		{
			LeftHandAttachmentAnchor->RegisterComponentWithWorld(World);
		}
		LeftPistolGrip = MakePiece(TEXT("LeftPistolGrip"), LeftHandAttachmentAnchor);
		LeftPistolBody = MakePiece(TEXT("LeftPistolBody"), LeftHandAttachmentAnchor);
		LeftPistolBarrel = MakePiece(TEXT("LeftPistolBarrel"), LeftHandAttachmentAnchor);
		LeftPistolMaterial = IsValid(LeftPistolBody) ? LeftPistolBody->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
		if (IsValid(LeftPistolBarrel) && IsValid(LeftPistolMaterial))
		{
			LeftPistolBarrel->SetMaterial(0, LeftPistolMaterial);
		}
		if (IsValid(LeftPistolGrip))
		{
			LeftPistolGrip->SetRelativeLocation(FVector(0.0f, 0.0f, -8.0f));
			LeftPistolGrip->SetRelativeRotation(FRotator::ZeroRotator);
			LeftPistolGrip->SetWorldScale3D(FVector(0.16f, 0.12f, 0.32f));
		}
		if (IsValid(LeftPistolBody))
		{
			LeftPistolBody->SetRelativeLocation(FVector(13.0f, 0.0f, 0.0f));
			LeftPistolBody->SetRelativeRotation(FRotator::ZeroRotator);
			LeftPistolBody->SetWorldScale3D(FVector(0.48f, 0.18f, 0.14f));
		}
		if (IsValid(LeftPistolBarrel))
		{
			LeftPistolBarrel->SetRelativeLocation(FVector(48.0f, 0.0f, 0.0f));
			LeftPistolBarrel->SetRelativeRotation(FRotator::ZeroRotator);
			LeftPistolBarrel->SetWorldScale3D(FVector(0.26f, 0.08f, 0.08f));
		}
	}

	// One anchor follows the forehead. The housing and the single beam hang off it.
	USceneComponent* const LampParent = IsValid(CharacterMesh) ? CharacterMesh : AttachParent;
	WaistLampAnchor = CreateScene(TEXT("WaistLampAnchor"), LampParent);
	if (IsValid(WaistLampAnchor))
	{
		WaistLampAnchor->SetAbsolute(true, true, true);
	}
	// A small helmet lamp: dark housing, one horizontal warm lens. Absolute scale
	// keeps the skeletal mesh from blowing the pieces up.
	WaistLampBody = MakePiece(TEXT("WaistLampBody"), IsValid(WaistLampAnchor) ? WaistLampAnchor.Get() : LampParent);
	if (IsValid(WaistLampBody))
	{
		WaistLampBody->SetAbsolute(false, false, true);
		WaistLampBody->SetRelativeLocation(FVector::ZeroVector);
		WaistLampBody->SetRelativeRotation(FRotator::ZeroRotator);
		WaistLampBody->SetWorldScale3D(FVector(0.022f, 0.062f, 0.028f));
		WaistLampBody->SetCastShadow(true);
	}
	WaistLampLens = MakePiece(TEXT("WaistLampLens"), IsValid(WaistLampBody) ? WaistLampBody.Get() : WaistLampAnchor.Get());
	if (IsValid(WaistLampLens))
	{
		WaistLampLens->SetAbsolute(false, false, true);
		// Housing is a 100 cm cube. 56 sits just proud of the front face, and the
		// lens itself is a wide short oval in the same proportions as the beam.
		WaistLampLens->SetWorldScale3D(FVector(0.006f, 0.050f, 0.016f));
		WaistLampLens->SetRelativeLocation(FVector(56.0f, 0.0f, 0.0f));
		WaistLampLens->SetCastShadow(false);
	}
	const FLinearColor LampColor(1.0f, 0.90f, 0.66f);
	WaistLampLight = NewObject<USpotLightComponent>(GetOwner(), TEXT("WaistLampLight"));
	if (IsValid(WaistLampLight) && IsValid(WaistLampBody))
	{
		GetOwner()->AddInstanceComponent(WaistLampLight);
		WaistLampLight->SetMobility(EComponentMobility::Movable);
		WaistLampLight->AttachToComponent(WaistLampBody, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		WaistLampLight->SetIntensity(0.0f);
		// 55 m of slow falloff. A short radius dies on the lip, so the floor of
		// a pit in front of the player stays black.
		WaistLampLight->SetAttenuationRadius(5500.0f);
		// Inner well inside outer, so the rim is a wide fade instead of a hard oval.
		WaistLampLight->SetInnerConeAngle(8.0f);
		WaistLampLight->SetOuterConeAngle(58.0f);
		WaistLampLight->SetUseInverseSquaredFalloff(false);
		WaistLampLight->SetLightFalloffExponent(2.2f);
		// A large soft source. A tiny source becomes a clipped white disc.
		WaistLampLight->SetSourceRadius(80.0f);
		WaistLampLight->SetSoftSourceRadius(160.0f);
		WaistLampLight->SetIndirectLightingIntensity(0.0f);
		WaistLampLight->SetVolumetricScatteringIntensity(0.0f);
		WaistLampLight->SetSpecularScale(0.0f);
		// Exposure is fixed on the player camera. This blend would scale the
		// lamp with the old auto-exposure compensation and blow the near patch.
		WaistLampLight->InverseExposureBlend = 0.0f;
		WaistLampLight->SetLightColor(LampColor);
		WaistLampLight->SetCastShadows(false);
		WaistLampLight->SetVisibility(false);
		// Written in world space every pose. A parented rotation was leaving
		// the beam on the nearest ground instead of following the view.
		WaistLampLight->SetAbsolute(true, true, true);
		WaistLampLight->SetWorldLocation(FVector::ZeroVector);
		WaistLampLight->SetWorldRotation(FRotator::ZeroRotator);
		if (UWorld* const World = GetOwner()->GetWorld())
		{
			WaistLampLight->RegisterComponentWithWorld(World);
		}
	}
	WaistLampMaterial = IsValid(WaistLampBody) ? WaistLampBody->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
	WaistLampLensMaterial = IsValid(WaistLampLens) ? WaistLampLens->CreateAndSetMaterialInstanceDynamic(0) : nullptr;

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

void UJTSWeaponVisualComponent::UpdateHandAnchor(
	USceneComponent* Anchor,
	const FName WristBone,
	const FName IndexBone,
	const FName ForearmBone,
	const bool bRightHand)
{
	if (!IsValid(Anchor) || GetOwner() == nullptr)
	{
		return;
	}
	USkeletalMeshComponent* const CharacterMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	if (!IsValid(CharacterMesh)
		|| CharacterMesh->GetBoneIndex(WristBone) == INDEX_NONE
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

	const FVector Palm = Wrist + AlongFingers * 1.35f;
	FVector AlongForearm = AlongFingers.GetSafeNormal();
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
	if (bRangedVisible && (bRightHand || bTwoHandVisible))
	{
		const FVector Side = FVector::CrossProduct(Up, AlongForearm).GetSafeNormal();
		const FVector PalmUp = Side.IsNearlyZero()
			? Up
			: FVector::CrossProduct(AlongForearm, Side).GetSafeNormal();
		PalmRotation = FRotationMatrix::MakeFromXZ(AlongForearm, PalmUp).ToQuat();
	}
	else if (bRightHand)
	{
		const float Swing = FVector::DotProduct(AlongForearm, Up);
		const FVector Shaft = (Up + AlongForearm * FMath::Max(0.0f, -Swing)).GetSafeNormal();
		const FVector Forward = FVector::VectorPlaneProject(Owner->GetActorForwardVector(), Shaft).GetSafeNormal();
		if (!Shaft.IsNearlyZero() && !Forward.IsNearlyZero())
		{
			PalmRotation = FRotationMatrix::MakeFromXZ(Shaft, Forward).ToQuat();
		}
	}

	Anchor->SetAbsolute(true, true, true);
	Anchor->SetWorldLocation(Palm);
	Anchor->SetWorldRotation(PalmRotation);
}

void UJTSWeaponVisualComponent::UpdatePalmAnchor()
{
	if (bUsingFallbackAttachment || !IsValid(HandAttachmentAnchor))
	{
		return;
	}
	UpdateHandAnchor(HandAttachmentAnchor, TEXT("Wrist_R"), TEXT("Index1_R"), TEXT("LowerArm_R"), true);
}

void UJTSWeaponVisualComponent::UpdateLeftPistolAnchor()
{
	if (!bTwoHandVisible || !IsValid(LeftHandAttachmentAnchor))
	{
		return;
	}
	UpdateHandAnchor(LeftHandAttachmentAnchor, TEXT("Wrist_L"), TEXT("Index1_L"), TEXT("LowerArm_L"), false);
}

void UJTSWeaponVisualComponent::UpdateWaistLamp()
{
	if (!IsValid(WaistLampBody) || !IsValid(WaistLampLens) || !IsValid(WaistLampLight) || !IsValid(WaistLampAnchor) || GetOwner() == nullptr)
	{
		return;
	}
	const UJTSInventoryComponent* const Inventory = GetOwner()->FindComponentByClass<UJTSInventoryComponent>();
	const bool bLit = IsValid(Inventory) && Inventory->IsWaistLampEquipped();
	USkeletalMeshComponent* const CharacterMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	const bool bMeshShown = IsValid(CharacterMesh) && !CharacterMesh->bHiddenInGame;
	if (!bLit || !bMeshShown)
	{
		WaistLampBody->SetVisibility(false);
		WaistLampBody->SetHiddenInGame(true);
		WaistLampLens->SetVisibility(false);
		WaistLampLens->SetHiddenInGame(true);
		WaistLampLight->SetVisibility(false);
		WaistLampLight->SetIntensity(0.0f);
		return;
	}

	// Same pitch the planet camera uses. Positive aim pitch looks up. The body
	// forward has no pitch, and a light that stays level keeps its hotspot on
	// the ground under the head.
	const FVector ActorUp = GetOwner()->GetActorUpVector().GetSafeNormal();
	const FVector ActorForward = GetOwner()->GetActorForwardVector().GetSafeNormal();
	const AJTSCharacter* const Character = Cast<AJTSCharacter>(GetOwner());
	const float ViewPitch = IsValid(Character) ? Character->GetAimPitch() : 0.0f;
	const FVector ActorRight = FVector::CrossProduct(ActorUp, ActorForward).GetSafeNormal();
	FVector Beam = ActorForward;
	if (!ActorRight.IsNearlyZero())
	{
		// Actor right is Cross(up, forward). A positive angle around that axis
		// pitches the beam down, so look-up (positive aim pitch) uses the opposite sign.
		Beam = FQuat(ActorRight, FMath::DegreesToRadians(-ViewPitch)).RotateVector(ActorForward).GetSafeNormal();
	}
	const FVector BeamUp = FVector::VectorPlaneProject(ActorUp, Beam).GetSafeNormal();
	FVector HeadLocation = GetOwner()->GetActorLocation() + ActorUp * 168.0f;
	const FName HeadName(TEXT("Head"));
	if (IsValid(CharacterMesh) && CharacterMesh->GetBoneIndex(HeadName) != INDEX_NONE)
	{
		HeadLocation = CharacterMesh->GetBoneLocation(HeadName, EBoneSpaces::WorldSpace);
	}
	const FVector LampLocation = HeadLocation + Beam * 14.0f + ActorUp * 4.0f;
	WaistLampAnchor->SetWorldLocation(LampLocation);
	WaistLampAnchor->SetWorldRotation(FRotationMatrix::MakeFromXZ(
		Beam, BeamUp.IsNearlyZero() ? ActorUp : BeamUp).Rotator());
	WaistLampLight->SetWorldLocation(LampLocation + Beam * 6.0f);
	WaistLampLight->SetWorldRotation(FRotationMatrix::MakeFromXZ(
		Beam, BeamUp.IsNearlyZero() ? ActorUp : BeamUp).Rotator());
	WaistLampBody->SetWorldScale3D(FVector(0.022f, 0.062f, 0.028f));
	WaistLampBody->SetVisibility(true);
	WaistLampBody->SetHiddenInGame(false);
	WaistLampLens->SetWorldScale3D(FVector(0.006f, 0.050f, 0.016f));
	WaistLampLens->SetVisibility(true);
	WaistLampLens->SetHiddenInGame(false);
	if (IsValid(WaistLampMaterial))
	{
		const FLinearColor Housing(0.045f, 0.048f, 0.052f, 1.0f);
		WaistLampMaterial->SetVectorParameterValue(TEXT("Color"), Housing);
		WaistLampMaterial->SetVectorParameterValue(TEXT("BaseColor"), Housing);
		WaistLampMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
	}
	if (IsValid(WaistLampLensMaterial))
	{
		const FLinearColor Glow(1.0f, 0.90f, 0.66f, 1.0f);
		WaistLampLensMaterial->SetVectorParameterValue(TEXT("Color"), Glow);
		WaistLampLensMaterial->SetVectorParameterValue(TEXT("BaseColor"), Glow);
		WaistLampLensMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), Glow * 4.0f);
	}
	// The near ground stays a readable grey. Reach comes from the 55 m radius
	// and the wide outer cone, not from a hot core.
	WaistLampLight->SetIntensity(32.0f);
	WaistLampLight->SetAttenuationRadius(5500.0f);
	WaistLampLight->SetInnerConeAngle(8.0f);
	WaistLampLight->SetOuterConeAngle(58.0f);
	WaistLampLight->SetVisibility(true);
	SetComponentTickEnabled(true);
}

void UJTSWeaponVisualComponent::RestoreAfterCharacterMeshShown()
{
	if (bClimbStowed)
	{
		SetVisible(false);
		return;
	}
	if (const UJTSWallClimbComponent* Climb = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UJTSWallClimbComponent>() : nullptr;
		IsValid(Climb) && Climb->IsClimbing())
	{
		SetVisible(false);
		return;
	}
	if (!IsValid(WeaponBody) || !IsValid(WeaponGrip) || !IsValid(WeaponBarrel))
	{
		return;
	}

	const UJTSInventoryComponent* const Inventory = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UJTSInventoryComponent>()
		: nullptr;
	const EJTSItemId ItemId = IsValid(Inventory) ? Inventory->GetActiveItemId() : EJTSItemId::None;
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
	if (!IsValid(Definition) || !Definition->IsHoldable())
	{
		return;
	}

	// SetVisibility(false, true) on the skeletal mesh walks every attached child and
	// sets bHiddenInGame. The visible flag comes back; this flag does not.
	WeaponGrip->SetHiddenInGame(false);
	WeaponBody->SetHiddenInGame(false);
	WeaponBarrel->SetHiddenInGame(false);
	if (IsValid(WeaponSight))
	{
		WeaponSight->SetHiddenInGame(false);
	}
	WeaponGrip->SetVisibility(true);
	WeaponBody->SetVisibility(true);
	WeaponBarrel->SetVisibility(true);
	if (IsValid(WeaponSight))
	{
		WeaponSight->SetVisibility(WeaponSight->GetStaticMesh() != nullptr);
	}

	SetComponentTickEnabled(true);
	UpdatePalmAnchor();
	ApplyPresentationTransform();
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

void UJTSWeaponVisualComponent::SetClimbStowed(bool bStowed)
{
	if (bClimbStowed == bStowed)
	{
		if (bStowed) SetVisible(false);
		return;
	}
	bClimbStowed = bStowed;
	if (bStowed)
	{
		AimAlpha = 0.0f;
		SetVisible(false);
	}
	else
	{
		RefreshWeaponVisual();
	}
}

void UJTSWeaponVisualComponent::RefreshWeaponVisual()
{
	EnsureMeshComponents();
	const UJTSWallClimbComponent* const Climb = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UJTSWallClimbComponent>() : nullptr;
	if (bClimbStowed || (IsValid(Climb) && Climb->IsClimbing()))
	{
		SetVisible(false);
		UpdateWaistLamp();
		return;
	}
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
		bTwoHandVisible = false;
		SetVisible(false);
		UpdateWaistLamp();
		UE_LOG(LogTemp, Log, TEXT("JTS weapon visual: hidden for %s ActiveItem=%d."), *GetNameSafe(GetOwner()), static_cast<int32>(ItemId));
		return;
	}
	bRangedVisible = Definition->IsRangedWeapon();
	bTwoHandVisible = ItemId == EJTSItemId::IceAxe;
	// Mesh +X follows each forearm for ranged weapons.
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
	else if (ItemId == EJTSItemId::IceAxe)
	{
		ItemColor = FLinearColor(0.35f, 0.72f, 0.95f, 1.0f);
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
	if (bTwoHandVisible)
	{
		ApplyMaterialColor(LeftPistolMaterial, ItemColor);
		auto ShowLeft = [](UStaticMeshComponent* Piece, const bool bShow)
		{
			if (!IsValid(Piece))
			{
				return;
			}
			Piece->SetVisibility(bShow);
			Piece->SetHiddenInGame(!bShow);
		};
		ShowLeft(LeftPistolGrip, true);
		ShowLeft(LeftPistolBody, true);
		ShowLeft(LeftPistolBarrel, true);
	}
	else
	{
		if (IsValid(LeftPistolGrip)) LeftPistolGrip->SetVisibility(false);
		if (IsValid(LeftPistolBody)) LeftPistolBody->SetVisibility(false);
		if (IsValid(LeftPistolBarrel)) LeftPistolBarrel->SetVisibility(false);
	}
	UpdateWaistLamp();
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

bool UJTSWeaponVisualComponent::GetMuzzleWorldLocation(FVector& OutLocation, bool bLeftHand) const
{
	OutLocation = FVector::ZeroVector;
	FTransform MuzzleTransform;
	if (!GetMuzzleWorldTransform(MuzzleTransform, bLeftHand))
	{
		return false;
	}
	OutLocation = MuzzleTransform.GetLocation();
	return true;
}

bool UJTSWeaponVisualComponent::GetMuzzleWorldTransform(FTransform& OutTransform, bool bLeftHand) const
{
	OutTransform = FTransform::Identity;
	if (!IsValid(WeaponMuzzle) || !IsValid(WeaponBody) || !WeaponBody->IsVisible() || bClimbStowed)
	{
		return false;
	}
	if (bLeftHand && bTwoHandVisible && IsValid(LeftHandAttachmentAnchor))
	{
		const FTransform& HandTransform = LeftHandAttachmentAnchor->GetComponentTransform();
		OutTransform = FTransform(
			HandTransform.GetRotation() * DefaultMuzzleTransform.GetRotation(),
			HandTransform.TransformPosition(DefaultMuzzleTransform.GetLocation() - DefaultGripTransform.GetLocation()));
	}
	else
	{
		OutTransform = WeaponMuzzle->GetComponentTransform();
	}
	return !OutTransform.GetUnitAxis(EAxis::X).IsNearlyZero();
}

bool UJTSWeaponVisualComponent::GetHeldItemTipWorldLocation(FVector& OutLocation) const
{
	OutLocation = FVector::ZeroVector;
	return !bRangedVisible && GetMuzzleWorldLocation(OutLocation);
}

void UJTSWeaponVisualComponent::SetAimAlpha(float NewAimAlpha)
{
	AimAlpha = bClimbStowed ? 0.0f : FMath::Clamp(NewAimAlpha, 0.0f, 1.0f);
	ApplyPresentationTransform();
}

void UJTSWeaponVisualComponent::PlayShotPresentation(UMaterialInterface* GlowMaterial,
	const FLinearColor& Color, bool bLeftHand)
{
	if (bClimbStowed || !IsValid(WeaponBody) || !WeaponBody->IsVisible())
	{
		return;
	}
	ShotKickAlpha = 1.0f;
	SetComponentTickEnabled(true);
	ApplyPresentationTransform();
	if (IsValid(MuzzleFlash))
	{
		FVector FlashLocation;
		if (GetMuzzleWorldLocation(FlashLocation, bLeftHand))
		{
			MuzzleFlash->SetAbsolute(true, true, true);
			MuzzleFlash->SetWorldLocation(FlashLocation);
		}
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
		FVector FlashLocation;
		if (GetMuzzleWorldLocation(FlashLocation, bLeftHand))
		{
			MuzzleLight->SetAbsolute(true, true, true);
			MuzzleLight->SetWorldLocation(FlashLocation);
		}
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
		bTwoHandVisible = false;
		bMeleeSwingPresentationActive = false;
		ShotKickAlpha = 0.0f;
		// The head lamp shares this component's pose follow. Stowing a hand item
		// must not freeze its world-space anchor while the character climbs.
		const UJTSInventoryComponent* const Inventory = GetOwner() != nullptr
			? GetOwner()->FindComponentByClass<UJTSInventoryComponent>() : nullptr;
		SetComponentTickEnabled(IsValid(Inventory) && Inventory->IsWaistLampEquipped());
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
	if (!bVisible)
	{
		if (IsValid(LeftPistolGrip)) { LeftPistolGrip->SetVisibility(false); LeftPistolGrip->SetHiddenInGame(true); }
		if (IsValid(LeftPistolBody)) { LeftPistolBody->SetVisibility(false); LeftPistolBody->SetHiddenInGame(true); }
		if (IsValid(LeftPistolBarrel)) { LeftPistolBarrel->SetVisibility(false); LeftPistolBarrel->SetHiddenInGame(true); }
	}
	if (IsValid(WeaponSight)) WeaponSight->SetHiddenInGame(!bVisible);
}

void UJTSWeaponVisualComponent::HandleInventoryChanged(int32 UsedSlots, int32 Capacity)
{
	static_cast<void>(UsedSlots);
	static_cast<void>(Capacity);
	RefreshWeaponVisual();
}
