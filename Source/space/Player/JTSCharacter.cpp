// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionShape.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Math/BoxSphereBounds.h"
#include "Math/RotationMatrix.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSCarryComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSMeleeComponent.h"
#include "space/Components/JTSPlanetGravityComponent.h"
#include "space/Components/JTSRangedWeaponComponent.h"
#include "space/Components/JTSWeaponVisualComponent.h"
#include "space/Interaction/InteractionComponent.h"
#include "space/Modes/JTSSpaceWorldGameMode.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSpaceWorldManager.h"
#include "UObject/ConstructorHelpers.h"

AJTSCharacter::AJTSCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->bOrientRotationToMovement = true;
	MovementComponent->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	MovementComponent->JumpZVelocity = 650.0f;
	MovementComponent->AirControl = 0.55f;
	MovementComponent->AirControlBoostMultiplier = 1.5f;
	MovementComponent->AirControlBoostVelocityThreshold = 150.0f;
	MovementComponent->BrakingDecelerationFalling = 500.0f;
	JumpMaxHoldTime = 0.12f;
	MovementComponent->MaxWalkSpeed = WalkingSpeed;
	MovementComponent->MinAnalogWalkSpeed = 20.0f;
	MovementComponent->BrakingDecelerationWalking = 2000.0f;
	MovementComponent->GravityScale = 1.0f;
	ApplySurfaceMovementSettings();

	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
	InventoryComponent = CreateDefaultSubobject<UJTSInventoryComponent>(TEXT("InventoryComponent"));
	CarryComponent = CreateDefaultSubobject<UJTSCarryComponent>(TEXT("CarryComponent"));
	HealthComponent = CreateDefaultSubobject<UJTSHealthComponent>(TEXT("HealthComponent"));
	MeleeComponent = CreateDefaultSubobject<UJTSMeleeComponent>(TEXT("MeleeComponent"));
	RangedWeaponComponent = CreateDefaultSubobject<UJTSRangedWeaponComponent>(TEXT("RangedWeaponComponent"));
	WeaponVisualComponent = CreateDefaultSubobject<UJTSWeaponVisualComponent>(TEXT("WeaponVisualComponent"));
	PlanetGravityComponent = CreateDefaultSubobject<UJTSPlanetGravityComponent>(TEXT("PlanetGravityComponent"));
	MovementComponent->AddTickPrerequisiteComponent(PlanetGravityComponent);

	CameraPivot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraPivot"));
	CameraPivot->SetupAttachment(GetCapsuleComponent());
	CameraPivot->SetRelativeLocation(FVector(0.0f, 0.0f, CameraPivotHeight));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(CameraPivot);
	CameraBoom->TargetArmLength = ThirdPersonArmLength;
	CameraBoom->SocketOffset = FVector(ThirdPersonShoulderOffset.X, ThirdPersonShoulderOffset.Y, 0.0f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	DebugVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebugVisual"));
	DebugVisual->SetupAttachment(GetCapsuleComponent());
	DebugVisual->SetRelativeLocation(FVector(0.0f, 0.0f, -16.0f));
	DebugVisual->SetRelativeScale3D(FVector(0.55f, 0.45f, 1.6f));
	DebugVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DebugVisual->SetGenerateOverlapEvents(false);
	DebugVisual->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DebugMeshAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (DebugMeshAsset.Succeeded())
	{
		DebugVisual->SetStaticMesh(DebugMeshAsset.Object);
	}
}

UJTSCarryComponent* AJTSCharacter::GetCarryComponent() const
{
	return CarryComponent.Get();
}

UJTSInventoryComponent* AJTSCharacter::GetInventoryComponent() const
{
	return InventoryComponent.Get();
}

UJTSHealthComponent* AJTSCharacter::GetHealthComponent() const
{
	return HealthComponent.Get();
}

bool AJTSCharacter::IsFirstPersonView() const
{
	return bFirstPersonView;
}

void AJTSCharacter::AdjustThirdPersonCameraDistance(float ScrollAmount)
{
	if (bFirstPersonView || CameraBoom == nullptr || FMath::IsNearlyZero(ScrollAmount))
	{
		return;
	}

	InitializeThirdPersonCameraDistance();
	const float MinimumArmLength = FMath::Min(ThirdPersonCameraMinArmLength, ThirdPersonCameraMaxArmLength);
	const float MaximumArmLength = FMath::Max(ThirdPersonCameraMinArmLength, ThirdPersonCameraMaxArmLength);
	CurrentThirdPersonCameraArmLength = FMath::Clamp(
		CurrentThirdPersonCameraArmLength - ScrollAmount * FMath::Max(1.0f, ThirdPersonCameraZoomStep),
		MinimumArmLength,
		MaximumArmLength);
	CameraBoom->TargetArmLength = CurrentThirdPersonCameraArmLength;
}

void AJTSCharacter::SetGameplayPlanet(AJTSPlanetAnchor* InPlanetAnchor)
{
	if (GameplayPlanet.Get() != InPlanetAnchor)
	{
		GameplayPlanet = InPlanetAnchor;
		bPlanetFrameInitialized = false;
		bPlanetCameraFrameInitialized = false;
		PlanetCameraPitch = 0.0f;
	}

	if (IsValid(PlanetGravityComponent))
	{
		PlanetGravityComponent->SetPlanetAnchor(InPlanetAnchor);
	}
}

void AJTSCharacter::ApplySurfaceMovementSettings()
{
	if (UCharacterMovementComponent* const MovementComponent = GetCharacterMovement())
	{
		MovementComponent->SetWalkableFloorAngle(FMath::Clamp(MaxWalkableSlopeDegrees, 0.0f, 89.0f));
	}
}

void AJTSCharacter::ApplyProgressionMovementSpeed()
{
	if (UCharacterMovementComponent* const MovementComponent = GetCharacterMovement())
	{
		const AJTSPlayerState* const ProgressionPlayerState = GetPlayerState<AJTSPlayerState>();
		const float ProgressionMultiplier = ProgressionPlayerState != nullptr ? ProgressionPlayerState->GetRunSpeedMultiplier() : 1.0f;
		const float BaseSpeed = bSprintInputActive && !IsBoarded() ? SprintingSpeed : WalkingSpeed;
		MovementComponent->MaxWalkSpeed = BaseSpeed * FMath::Max(0.1f, ProgressionMultiplier);
	}
}

AJTSPlanetAnchor* AJTSCharacter::GetGameplayPlanet() const
{
	return GameplayPlanet.Get();
}

void AJTSCharacter::InitializePlanetFrame()
{
	AJTSPlanetAnchor* const Planet = GameplayPlanet.Get();
	if (!IsValid(Planet))
	{
		return;
	}

	const FVector PlanetUp = Planet->GetRadialUpVector(GetActorLocation()).GetSafeNormal();
	if (PlanetUp.IsNearlyZero())
	{
		return;
	}

	const FVector PlanetForward = GetStablePlanetTangent(PlanetUp, GetActorForwardVector());
	SetActorRotation(FRotationMatrix::MakeFromXZ(PlanetForward, PlanetUp).ToQuat());
	LastPlanetUp = PlanetUp;
	PlanetBodyForward = PlanetForward;
	PlanetCameraTangentForward = PlanetForward;
	LastPlanetCameraUp = PlanetUp;
	bPlanetFrameInitialized = true;
	bPlanetCameraFrameInitialized = true;
	UpdatePlanetCameraFrame(PlanetUp, 0.0f);
}

void AJTSCharacter::BeginPlanetFalling()
{
	if (UCharacterMovementComponent* const MovementComponent = GetCharacterMovement())
	{
		MovementComponent->SetMovementMode(MOVE_Falling);
	}
}

bool AJTSCharacter::IsPlanetGravityEnabled() const
{
	return IsValid(PlanetGravityComponent) && PlanetGravityComponent->IsUsingPlanetGravity();
}

bool AJTSCharacter::SnapToPlanetSurface(AJTSPlanetAnchor* InPlanetAnchor, const FVector& TraceReferenceLocation)
{
	if (!IsValid(InPlanetAnchor) || !IsValid(GetCapsuleComponent()))
	{
		return false;
	}

	SetGameplayPlanet(InPlanetAnchor);

	FJTSPlanetSurfaceFrame SurfaceFrame;
	FVector SurfaceLocation;
	if (!FindSafeCharacterSurfaceLocation(
		InPlanetAnchor,
		TraceReferenceLocation,
		GetActorForwardVector(),
		nullptr,
		SurfaceLocation,
		&SurfaceFrame))
	{
		return false;
	}

	const FVector SurfaceUp = SurfaceFrame.Up.GetSafeNormal();
	const FVector GravityUp = InPlanetAnchor->GetRadialUpVector(SurfaceFrame.Location).GetSafeNormal();
	if (SurfaceUp.IsNearlyZero() || GravityUp.IsNearlyZero())
	{
		return false;
	}

	if (const UCharacterMovementComponent* const MovementComponent = GetCharacterMovement();
		MovementComponent != nullptr
		&& FVector::DotProduct(GravityUp, SurfaceUp) < MovementComponent->GetWalkableFloorZ())
	{
		return false;
	}

	const FVector SurfaceForward = GetStablePlanetTangent(GravityUp, SurfaceFrame.Forward);
	const FQuat SurfaceRotation = FRotationMatrix::MakeFromXZ(SurfaceForward, GravityUp).ToQuat();

	SetActorLocationAndRotation(SurfaceLocation, SurfaceRotation, false, nullptr, ETeleportType::TeleportPhysics);
	LastPlanetUp = GravityUp;
	PlanetBodyForward = SurfaceForward;
	PlanetCameraTangentForward = SurfaceForward;
	LastPlanetCameraUp = GravityUp;
	bPlanetFrameInitialized = true;
	bPlanetCameraFrameInitialized = true;

	if (UCharacterMovementComponent* const MovementComponent = GetCharacterMovement())
	{
		MovementComponent->Velocity = FVector::ZeroVector;
		MovementComponent->SetMovementMode(MOVE_Walking);
	}

	return true;
}

bool AJTSCharacter::FindSafeCharacterSurfaceLocation(
	AJTSPlanetAnchor* InPlanetAnchor,
	const FVector& TraceReferenceLocation,
	const FVector& PreferredForward,
	const AActor* AdditionalIgnoredActor,
	FVector& OutLocation,
	FJTSPlanetSurfaceFrame* OutSurfaceFrame) const
{
	if (!IsValid(InPlanetAnchor) || !InPlanetAnchor->HasGameplaySurface() || !IsValid(GetCapsuleComponent()))
	{
		return false;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	FJTSPlanetSurfaceFrame SurfaceFrame;
	if (!InPlanetAnchor->GetSurfaceFrameAt(TraceReferenceLocation, PreferredForward, SurfaceFrame)
		|| SurfaceFrame.Up.IsNearlyZero())
	{
		return false;
	}

	const FVector SurfaceUp = SurfaceFrame.Up.GetSafeNormal();
	const FVector GravityUp = InPlanetAnchor->GetRadialUpVector(SurfaceFrame.Location).GetSafeNormal();
	if (SurfaceUp.IsNearlyZero() || GravityUp.IsNearlyZero())
	{
		return false;
	}

	const FVector SurfaceForward = GetStablePlanetTangent(GravityUp, SurfaceFrame.Forward);
	const FQuat CapsuleRotation = FRotationMatrix::MakeFromXZ(SurfaceForward, GravityUp).ToQuat();
	const FVector SurfaceLocation = SurfaceFrame.Location
		+ SurfaceUp * (GetCapsuleSupportDistanceAlongDirection(SurfaceUp, CapsuleRotation) + PlanetSurfaceSnapClearance);
	FCollisionQueryParams PlacementParams(SCENE_QUERY_STAT(JTSCharacterPlanetSurfacePlacement), false, this);
	PlacementParams.AddIgnoredActor(this);
	if (IsValid(AdditionalIgnoredActor))
	{
		PlacementParams.AddIgnoredActor(AdditionalIgnoredActor);
	}

	if (World->OverlapBlockingTestByChannel(
		SurfaceLocation,
		CapsuleRotation,
		ECC_Pawn,
		GetCapsuleComponent()->GetCollisionShape(),
		PlacementParams))
	{
		return false;
	}

	OutLocation = SurfaceLocation;
	if (OutSurfaceFrame != nullptr)
	{
		*OutSurfaceFrame = SurfaceFrame;
	}
	return true;
}

float AJTSCharacter::GetCapsuleSupportDistanceAlongDirection(const FVector& SupportDirection, const FQuat& CapsuleRotation) const
{
	const UCapsuleComponent* const Capsule = GetCapsuleComponent();
	const FVector SafeSupportDirection = SupportDirection.GetSafeNormal();
	if (!IsValid(Capsule) || SafeSupportDirection.IsNearlyZero())
	{
		return 0.0f;
	}

	const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	const float CapsuleCylinderHalfHeight = FMath::Max(0.0f, Capsule->GetScaledCapsuleHalfHeight() - CapsuleRadius);
	return CapsuleRadius + CapsuleCylinderHalfHeight * FMath::Abs(FVector::DotProduct(CapsuleRotation.GetAxisZ(), SafeSupportDirection));
}

float AJTSCharacter::GetRawViewYawDelta() const
{
	if (Controller == nullptr)
	{
		return 0.0f;
	}

	const FRotator RelativeAim = (Controller->GetControlRotation() - GetActorRotation()).GetNormalized();
	return FRotator::NormalizeAxis(RelativeAim.Yaw);
}

float AJTSCharacter::GetPresentationAimYaw() const
{
	// The chest follows the camera only while the camera is still behind it.
	// Past the comfortable cone the chest returns forward. It does not keep
	// twisting to meet a camera that has already swung around to the front.
	const float Limit = FMath::Max(10.0f, UpperBodyYawLimitDegrees);
	const float AbsYaw = FMath::Abs(GetRawViewYawDelta());
	if (AbsYaw <= Limit)
	{
		return GetRawViewYawDelta();
	}

	const float Release = FMath::Clamp((AbsYaw - Limit) / Limit, 0.0f, 1.0f);
	return GetRawViewYawDelta() * (1.0f - Release);
}

float AJTSCharacter::GetAimPitch() const
{
	return AimPitch;
}

void AJTSCharacter::ShiftLocalViewPitch(float DeltaDegrees)
{
	if (!IsLocallyControlled() || FMath::IsNearlyZero(DeltaDegrees))
	{
		return;
	}

	if (IsRealPlanetGameplayActive())
	{
		const float PitchMin = bFirstPersonView ? FirstPersonViewPitchMin : ThirdPersonViewPitchMin;
		const float PitchMax = bFirstPersonView ? FirstPersonViewPitchMax : ThirdPersonViewPitchMax;
		PlanetCameraPitch = FMath::Clamp(PlanetCameraPitch + DeltaDegrees,
			FMath::Min(PitchMin, PitchMax), FMath::Max(PitchMin, PitchMax));
		UpdatePlanetCameraFrame(bPlanetFrameInitialized ? LastPlanetUp : GetDesiredPlanetUp(), 0.0f);
	}
	else if (Controller != nullptr)
	{
		FRotator ViewRotation = Controller->GetControlRotation();
		ViewRotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(ViewRotation.Pitch) + DeltaDegrees,
			FMath::Min(AimPitchMin, AimPitchMax), FMath::Max(AimPitchMin, AimPitchMax));
		Controller->SetControlRotation(ViewRotation);
	}
}

void AJTSCharacter::ApplyWeaponViewKick(float PitchDegrees)
{
	if (!IsLocallyControlled() || PitchDegrees <= 0.0f)
	{
		return;
	}
	const float Before = IsRealPlanetGameplayActive() ? PlanetCameraPitch
		: (Controller != nullptr ? FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch) : 0.0f);
	ShiftLocalViewPitch(FMath::Clamp(PitchDegrees, 0.0f, 8.0f));
	const float After = IsRealPlanetGameplayActive() ? PlanetCameraPitch
		: (Controller != nullptr ? FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch) : Before);
	PendingViewRecoilDegrees = FMath::Clamp(PendingViewRecoilDegrees + FMath::Max(0.0f, After - Before), 0.0f, 12.0f);
}

void AJTSCharacter::ServerUpdateAimPitch_Implementation(float NewPitch)
{
	ReplicatedAimPitch = FMath::Clamp(NewPitch, FMath::Min(AimPitchMin, AimPitchMax), FMath::Max(AimPitchMin, AimPitchMax));
}

int32 AJTSCharacter::GetItemDiscardHoldSlotIndex() const
{
	if (bItemDiscardHoldCompleted || !IsValid(InventoryComponent)
		|| HeldItemDiscardSlotIndex < 0
		|| InventoryComponent->GetItemAtSlot(HeldItemDiscardSlotIndex).IsEmpty())
	{
		return INDEX_NONE;
	}

	return HeldItemDiscardSlotIndex;
}

float AJTSCharacter::GetItemDiscardHoldProgress() const
{
	if (GetItemDiscardHoldSlotIndex() == INDEX_NONE)
	{
		return 0.0f;
	}

	const UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return 0.0f;
	}

	return FMath::Clamp(
		static_cast<float>((static_cast<double>(World->GetTimeSeconds()) - ItemDiscardHoldStartTime)
			/ static_cast<double>(FMath::Max(0.1f, ItemDestroyHoldDuration))),
		0.0f,
		1.0f);
}

bool AJTSCharacter::IsBoardingHoldActive() const
{
	return bBoardingHoldActive;
}

float AJTSCharacter::GetBoardingHoldProgress() const
{
	if (!bBoardingHoldActive)
	{
		return 0.0f;
	}

	const UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return 0.0f;
	}

	return FMath::Clamp(
		static_cast<float>((static_cast<double>(World->GetTimeSeconds()) - BoardingHoldStartTime)
			/ static_cast<double>(FMath::Max(0.1f, BoardingHoldDuration))),
		0.0f,
		1.0f);
}

float AJTSCharacter::GetBoardingHoldRemainingTime() const
{
	if (!bBoardingHoldActive)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, FMath::Max(0.1f, BoardingHoldDuration) * (1.0f - GetBoardingHoldProgress()));
}

bool AJTSCharacter::IsBoarded() const
{
	return IsValid(BoardedSpacecraft.Get());
}

AJTSSpacecraftActor* AJTSCharacter::GetNearbySpacecraft() const
{
	return NearbySpacecraft.Get();
}

AJTSSpacecraftActor* AJTSCharacter::GetBoardedSpacecraft() const
{
	return BoardedSpacecraft.Get();
}

bool AJTSCharacter::TryDepositCarriedResourcesToNearbySpacecraft()
{
	if (!HasAuthority())
	{
		return false;
	}

	AJTSSpacecraftActor* const Spacecraft = NearbySpacecraft.Get();
	return IsValid(Spacecraft)
		&& Spacecraft->IsPawnInBoardingRange(this)
		&& Spacecraft->TryDepositResourcesFromPawn(this);
}

void AJTSCharacter::NotifySpacecraftEntered(AJTSSpacecraftActor* Spacecraft)
{
	if (!IsValid(Spacecraft))
	{
		return;
	}

	if (NearbySpacecraft.Get() != Spacecraft)
	{
		CancelBoardingHold();
	}

	NearbySpacecraft = Spacecraft;
	// A startup overlap can arrive after Enhanced Input has already observed a held F key.
	// Re-evaluate the same view-gated boarding path once the known nearby candidate is available.
	if (bInteractKeyHeld && !bBoardingHoldActive)
	{
		BeginBoardingHold();
	}
}

void AJTSCharacter::NotifySpacecraftExited(AJTSSpacecraftActor* Spacecraft)
{
	if (NearbySpacecraft.Get() != Spacecraft)
	{
		return;
	}

	NearbySpacecraft = nullptr;
	if (BoardedSpacecraft.Get() != Spacecraft)
	{
		bInteractKeyHeld = false;
		CancelBoardingHold();
		if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetController()))
		{
			PlayerController->CloseMoonShop();
		}
	}
}

bool AJTSCharacter::EnterBoardedState(AJTSSpacecraftActor* Spacecraft)
{
	if (!HasAuthority() || !IsValid(Spacecraft) || IsBoarded() || !IsValid(GetCapsuleComponent()))
	{
		return false;
	}

	USceneComponent* const BoardingPoint = Spacecraft->GetBoardingPoint();
	if (!IsValid(BoardingPoint))
	{
		return false;
	}

	CancelBoardingHold();
	CancelItemDiscardHold();
	bInteractKeyHeld = false;
	NearbySpacecraft = Spacecraft;
	BoardedSpacecraft = Spacecraft;
	ApplyBoardedPresentation();

	if (!AttachToComponent(BoardingPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale))
	{
		RestoreAfterBoarding(Spacecraft, false);
		return false;
	}

	SetActorRelativeLocation(FVector::ZeroVector);
	return true;
}

bool AJTSCharacter::ExitBoardedState(AJTSSpacecraftActor* Spacecraft)
{
	if (!HasAuthority() || BoardedSpacecraft.Get() != Spacecraft)
	{
		return false;
	}

	return RestoreAfterBoarding(Spacecraft, true);
}

void AJTSCharacter::HandleSpacecraftInvalidated(AJTSSpacecraftActor* Spacecraft)
{
	if (HasAuthority() && BoardedSpacecraft.Get() == Spacecraft)
	{
		RestoreAfterBoarding(Spacecraft, false);
	}

	if (NearbySpacecraft.Get() == Spacecraft)
	{
		NearbySpacecraft = nullptr;
	}
	if (bBoardingHoldActive)
	{
		bInteractKeyHeld = false;
		CancelBoardingHold();
	}
}

void AJTSCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplySurfaceMovementSettings();

	if (IsValid(HealthComponent))
	{
		HealthComponent->SetMaxHealth(PlayerMaxHealth, true);
		if (!HealthComponent->OnDeath.IsAlreadyBound(this, &AJTSCharacter::HandleHealthDeath))
		{
			HealthComponent->OnDeath.AddDynamic(this, &AJTSCharacter::HandleHealthDeath);
		}
	}
	if (IsValid(MeleeComponent) && !MeleeComponent->OnAttackStarted.IsAlreadyBound(this, &AJTSCharacter::HandleMeleeAttackStarted))
	{
		MeleeComponent->OnAttackStarted.AddDynamic(this, &AJTSCharacter::HandleMeleeAttackStarted);
	}
	if (IsValid(MeleeComponent) && !MeleeComponent->OnAttackFinished.IsAlreadyBound(this, &AJTSCharacter::HandleMeleeAttackFinished))
	{
		MeleeComponent->OnAttackFinished.AddDynamic(this, &AJTSCharacter::HandleMeleeAttackFinished);
	}

	InitializeThirdPersonCameraDistance();
	ApplyCameraView();
	BindPlayerState();
	BindGameState();
	UE_LOG(LogTemp, Log, TEXT("Jump to Space character initialized."));

	if (IsValid(WeaponVisualComponent))
	{
		WeaponVisualComponent->Activate(true);
		WeaponVisualComponent->RefreshWeaponVisual();
	}

	RegisterInputMappingContext();
}

void AJTSCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bUsingRealPlanetFrame = IsRealPlanetGameplayActive();
	if (bUsingRealPlanetFrame)
	{
		UpdatePlanetGameplayFrame(DeltaSeconds);
	}
	else
	{
		UpdateFacingPresentation(DeltaSeconds);
	}
	if (IsLocallyControlled() && PendingViewRecoilDegrees > KINDA_SMALL_NUMBER)
	{
		const float Remaining = FMath::FInterpTo(PendingViewRecoilDegrees, 0.0f, DeltaSeconds, 10.0f);
		ShiftLocalViewPitch(Remaining - PendingViewRecoilDegrees);
		PendingViewRecoilDegrees = Remaining;
	}

	const float MinimumAimPitch = FMath::Min(AimPitchMin, AimPitchMax);
	const float MaximumAimPitch = FMath::Max(AimPitchMin, AimPitchMax);
	if (IsLocallyControlled())
	{
		const float ControllerPitch = bUsingRealPlanetFrame ? PlanetCameraPitch
			: (Controller != nullptr ? FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch) : 0.0f);
		AimPitch = FMath::Clamp(ControllerPitch, MinimumAimPitch, MaximumAimPitch);
		if (HasAuthority())
		{
			ReplicatedAimPitch = AimPitch;
		}
		else if (GetWorld() != nullptr && FMath::Abs(AimPitch - LastSentAimPitch) >= 0.75f
			&& GetWorld()->GetTimeSeconds() - LastAimPitchSendSeconds >= 0.05)
		{
			ServerUpdateAimPitch(AimPitch);
			LastSentAimPitch = AimPitch;
			LastAimPitchSendSeconds = GetWorld()->GetTimeSeconds();
		}
	}
	else
	{
		AimPitch = FMath::FInterpTo(AimPitch, ReplicatedAimPitch, DeltaSeconds, 20.0f);
	}
	UpdateAimCamera(DeltaSeconds);
}

void AJTSCharacter::ApplyThirdPersonCameraOffset()
{
	if (!bFirstPersonView)
	{
		ApplyCameraView();
	}
}

void AJTSCharacter::PossessedBy(AController* NewController)
{
	UnregisterInputMappingContext();
	Super::PossessedBy(NewController);

	ApplyCameraView();
	RegisterInputMappingContext();
}

void AJTSCharacter::UnPossessed()
{
	UnregisterInputMappingContext();
	BoundInputComponent.Reset();
	Super::UnPossessed();
}

void AJTSCharacter::OnRep_Controller()
{
	UnregisterInputMappingContext();
	Super::OnRep_Controller();

	ApplyCameraView();
	RegisterInputMappingContext();
}

void AJTSCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bInteractKeyHeld = false;
	CancelBoardingHold();
	CancelItemDiscardHold();
	UnbindGameState();
	UnbindPlayerState();
	UnregisterInputMappingContext();
	if (IsValid(HealthComponent))
	{
		HealthComponent->OnDeath.RemoveDynamic(this, &AJTSCharacter::HandleHealthDeath);
	}
	if (IsValid(MeleeComponent))
	{
		MeleeComponent->OnAttackStarted.RemoveDynamic(this, &AJTSCharacter::HandleMeleeAttackStarted);
		MeleeComponent->OnAttackFinished.RemoveDynamic(this, &AJTSCharacter::HandleMeleeAttackFinished);
	}

	Super::EndPlay(EndPlayReason);
}

void AJTSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	InitializeInput();

	if (BoundInputComponent.Get() == PlayerInputComponent)
	{
		RegisterInputMappingContext();
		return;
	}

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (EnhancedInputComponent == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Jump to Space requires UEnhancedInputComponent for AJTSCharacter input."));
		return;
	}

	EnhancedInputComponent->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &AJTSCharacter::MoveForward);
	EnhancedInputComponent->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &AJTSCharacter::MoveRight);
	EnhancedInputComponent->BindAction(LookYawAction, ETriggerEvent::Triggered, this, &AJTSCharacter::LookYaw);
	EnhancedInputComponent->BindAction(LookPitchAction, ETriggerEvent::Triggered, this, &AJTSCharacter::LookPitch);
	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AJTSCharacter::HandleJumpStarted);
	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AJTSCharacter::StartSprint);
	EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AJTSCharacter::StopSprint);
	EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Canceled, this, &AJTSCharacter::StopSprint);
	EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AJTSCharacter::HandleInteractStarted);
	EnhancedInputComponent->BindAction(BoardAction, ETriggerEvent::Started, this, &AJTSCharacter::HandleBoardStarted);
	EnhancedInputComponent->BindAction(BoardAction, ETriggerEvent::Triggered, this, &AJTSCharacter::HandleBoardTriggered);
	EnhancedInputComponent->BindAction(BoardAction, ETriggerEvent::Completed, this, &AJTSCharacter::HandleBoardCompleted);
	EnhancedInputComponent->BindAction(BoardAction, ETriggerEvent::Canceled, this, &AJTSCharacter::HandleBoardCanceled);
	EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AJTSCharacter::HandleAttackStarted);
	EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Completed, this, &AJTSCharacter::HandleAttackReleased);
	EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Canceled, this, &AJTSCharacter::HandleAttackReleased);
	EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &AJTSCharacter::HandleAimStarted);
	EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &AJTSCharacter::HandleAimReleased);
	EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Canceled, this, &AJTSCharacter::HandleAimReleased);
	EnhancedInputComponent->BindAction(ToggleCameraAction, ETriggerEvent::Started, this, &AJTSCharacter::HandleToggleCameraStarted);
	EnhancedInputComponent->BindAction(CameraZoomAction, ETriggerEvent::Triggered, this, &AJTSCharacter::HandleCameraZoom);
	if (QuickbarSlotActions.Num() == UJTSInventoryComponent::MaximumQuickbarSlots)
	{
		EnhancedInputComponent->BindAction(QuickbarSlotActions[0], ETriggerEvent::Started, this, &AJTSCharacter::HandleQuickbarSlotOneStarted);
		EnhancedInputComponent->BindAction(QuickbarSlotActions[1], ETriggerEvent::Started, this, &AJTSCharacter::HandleQuickbarSlotTwoStarted);
		EnhancedInputComponent->BindAction(QuickbarSlotActions[2], ETriggerEvent::Started, this, &AJTSCharacter::HandleQuickbarSlotThreeStarted);
		EnhancedInputComponent->BindAction(QuickbarSlotActions[3], ETriggerEvent::Started, this, &AJTSCharacter::HandleQuickbarSlotFourStarted);
		EnhancedInputComponent->BindAction(QuickbarSlotActions[4], ETriggerEvent::Started, this, &AJTSCharacter::HandleQuickbarSlotFiveStarted);
		EnhancedInputComponent->BindAction(QuickbarSlotActions[5], ETriggerEvent::Started, this, &AJTSCharacter::HandleQuickbarSlotSixStarted);
		EnhancedInputComponent->BindAction(QuickbarSlotActions[6], ETriggerEvent::Started, this, &AJTSCharacter::HandleQuickbarSlotSevenStarted);
		EnhancedInputComponent->BindAction(QuickbarSlotActions[7], ETriggerEvent::Started, this, &AJTSCharacter::HandleQuickbarSlotEightStarted);
		EnhancedInputComponent->BindAction(QuickbarSlotActions[8], ETriggerEvent::Started, this, &AJTSCharacter::HandleQuickbarSlotNineStarted);
	}
	EnhancedInputComponent->BindAction(PreviousQuickbarPageAction, ETriggerEvent::Started, this, &AJTSCharacter::HandlePreviousQuickbarPageStarted);
	EnhancedInputComponent->BindAction(NextQuickbarPageAction, ETriggerEvent::Started, this, &AJTSCharacter::HandleNextQuickbarPageStarted);
	EnhancedInputComponent->BindAction(DiscardItemAction, ETriggerEvent::Started, this, &AJTSCharacter::HandleDiscardItemStarted);
	EnhancedInputComponent->BindAction(DiscardItemAction, ETriggerEvent::Completed, this, &AJTSCharacter::HandleDiscardItemReleased);
	EnhancedInputComponent->BindAction(DiscardItemAction, ETriggerEvent::Canceled, this, &AJTSCharacter::HandleDiscardItemReleased);

	BoundInputComponent = PlayerInputComponent;
	RegisterInputMappingContext();
}

void AJTSCharacter::InitializeInput()
{
	if (InputMappingContext != nullptr)
	{
		return;
	}

	InputMappingContext = NewObject<UInputMappingContext>(this, TEXT("JTSInputMappingContext"), RF_Transient);
	MoveForwardAction = NewObject<UInputAction>(this, TEXT("MoveForwardAction"), RF_Transient);
	MoveRightAction = NewObject<UInputAction>(this, TEXT("MoveRightAction"), RF_Transient);
	LookYawAction = NewObject<UInputAction>(this, TEXT("LookYawAction"), RF_Transient);
	LookPitchAction = NewObject<UInputAction>(this, TEXT("LookPitchAction"), RF_Transient);
	JumpAction = NewObject<UInputAction>(this, TEXT("JumpAction"), RF_Transient);
	SprintAction = NewObject<UInputAction>(this, TEXT("SprintAction"), RF_Transient);
	InteractAction = NewObject<UInputAction>(this, TEXT("InteractAction"), RF_Transient);
	BoardAction = NewObject<UInputAction>(this, TEXT("BoardAction"), RF_Transient);
	AttackAction = NewObject<UInputAction>(this, TEXT("AttackAction"), RF_Transient);
	AimAction = NewObject<UInputAction>(this, TEXT("AimAction"), RF_Transient);
	ToggleCameraAction = NewObject<UInputAction>(this, TEXT("ToggleCameraAction"), RF_Transient);
	CameraZoomAction = NewObject<UInputAction>(this, TEXT("CameraZoomAction"), RF_Transient);
	PreviousQuickbarPageAction = NewObject<UInputAction>(this, TEXT("PreviousQuickbarPageAction"), RF_Transient);
	NextQuickbarPageAction = NewObject<UInputAction>(this, TEXT("NextQuickbarPageAction"), RF_Transient);
	DiscardItemAction = NewObject<UInputAction>(this, TEXT("DiscardItemAction"), RF_Transient);
	QuickbarSlotActions.Reset();
	for (int32 SlotIndex = 0; SlotIndex < UJTSInventoryComponent::MaximumQuickbarSlots; ++SlotIndex)
	{
		QuickbarSlotActions.Add(NewObject<UInputAction>(this, *FString::Printf(TEXT("QuickbarSlot%dAction"), SlotIndex + 1), RF_Transient));
	}

	MoveForwardAction->ValueType = EInputActionValueType::Axis1D;
	MoveRightAction->ValueType = EInputActionValueType::Axis1D;
	LookYawAction->ValueType = EInputActionValueType::Axis1D;
	LookPitchAction->ValueType = EInputActionValueType::Axis1D;
	JumpAction->ValueType = EInputActionValueType::Boolean;
	SprintAction->ValueType = EInputActionValueType::Boolean;
	InteractAction->ValueType = EInputActionValueType::Boolean;
	BoardAction->ValueType = EInputActionValueType::Boolean;
	AttackAction->ValueType = EInputActionValueType::Boolean;
	AimAction->ValueType = EInputActionValueType::Boolean;
	ToggleCameraAction->ValueType = EInputActionValueType::Boolean;
	CameraZoomAction->ValueType = EInputActionValueType::Axis1D;
	PreviousQuickbarPageAction->ValueType = EInputActionValueType::Boolean;
	NextQuickbarPageAction->ValueType = EInputActionValueType::Boolean;
	DiscardItemAction->ValueType = EInputActionValueType::Boolean;
	for (UInputAction* const QuickbarSlotAction : QuickbarSlotActions)
	{
		QuickbarSlotAction->ValueType = EInputActionValueType::Boolean;
	}

	InputMappingContext->MapKey(MoveForwardAction, EKeys::W);
	InputMappingContext->MapKey(MoveRightAction, EKeys::D);
	InputMappingContext->MapKey(LookYawAction, EKeys::MouseX);
	InputMappingContext->MapKey(LookPitchAction, EKeys::MouseY);
	InputMappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	InputMappingContext->MapKey(SprintAction, EKeys::LeftShift);
	InputMappingContext->MapKey(InteractAction, EKeys::E);
	InputMappingContext->MapKey(BoardAction, EKeys::F);
	InputMappingContext->MapKey(AttackAction, EKeys::LeftMouseButton);
	InputMappingContext->MapKey(AimAction, EKeys::RightMouseButton);
	InputMappingContext->MapKey(ToggleCameraAction, EKeys::V);
	InputMappingContext->MapKey(CameraZoomAction, EKeys::MouseWheelAxis);
	InputMappingContext->MapKey(PreviousQuickbarPageAction, EKeys::Up);
	InputMappingContext->MapKey(NextQuickbarPageAction, EKeys::Down);
	InputMappingContext->MapKey(DiscardItemAction, EKeys::G);
	if (QuickbarSlotActions.Num() == UJTSInventoryComponent::MaximumQuickbarSlots)
	{
		const TArray<FKey, TInlineAllocator<UJTSInventoryComponent::MaximumQuickbarSlots>> QuickbarKeys = {
			EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
			EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
		for (int32 SlotIndex = 0; SlotIndex < QuickbarKeys.Num(); ++SlotIndex)
		{
			InputMappingContext->MapKey(QuickbarSlotActions[SlotIndex], QuickbarKeys[SlotIndex]);
		}
	}

	auto AddNegatedMapping = [this](UInputAction* Action, const FKey& Key)
	{
		FEnhancedActionKeyMapping& Mapping = InputMappingContext->MapKey(Action, Key);
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(InputMappingContext));
	};

	AddNegatedMapping(MoveForwardAction, EKeys::S);
	AddNegatedMapping(MoveRightAction, EKeys::A);
}

void AJTSCharacter::EnsureGameplayInputMapping()
{
	InitializeInput();
	RegisterInputMappingContext();
}

void AJTSCharacter::RegisterInputMappingContext()
{
	if (InputMappingContext == nullptr)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (PlayerController == nullptr || !PlayerController->IsLocalController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	if (LocalPlayer == nullptr)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (InputSubsystem == nullptr || RegisteredInputSubsystem.Get() == InputSubsystem)
	{
		return;
	}

	UnregisterInputMappingContext();
	FModifyContextOptions MappingOptions;
	MappingOptions.bIgnoreAllPressedKeysUntilRelease = true;
	InputSubsystem->AddMappingContext(InputMappingContext, 0, MappingOptions);
	RegisteredInputSubsystem = InputSubsystem;

	UE_LOG(LogTemp, Log, TEXT("Jump to Space Enhanced Input mapping context registered."));
}

void AJTSCharacter::UnregisterInputMappingContext()
{
	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = RegisteredInputSubsystem.Get())
	{
		if (InputMappingContext != nullptr)
		{
			InputSubsystem->RemoveMappingContext(InputMappingContext);
		}
	}

	RegisteredInputSubsystem.Reset();
}

void AJTSCharacter::BindGameState()
{
	AJTSGameState* const NewGameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
	if (BoundGameState.Get() == NewGameState)
	{
		return;
	}

	if (AJTSGameState* const PreviousGameState = BoundGameState.Get())
	{
		PreviousGameState->OnGameplayPhaseChanged.RemoveDynamic(this, &AJTSCharacter::HandleGameplayPhaseChanged);
	}

	BoundGameState = NewGameState;
	if (NewGameState != nullptr)
	{
		NewGameState->OnGameplayPhaseChanged.AddDynamic(this, &AJTSCharacter::HandleGameplayPhaseChanged);
	}
}

void AJTSCharacter::UnbindGameState()
{
	if (AJTSGameState* const GameState = BoundGameState.Get())
	{
		GameState->OnGameplayPhaseChanged.RemoveDynamic(this, &AJTSCharacter::HandleGameplayPhaseChanged);
	}
	BoundGameState.Reset();
}

void AJTSCharacter::BindPlayerState()
{
	AJTSPlayerState* const NewPlayerState = GetPlayerState<AJTSPlayerState>();
	if (BoundPlayerState.Get() == NewPlayerState)
	{
		ApplyAvatarColor();
		ApplyProgressionMovementSpeed();
		if (IsValid(InventoryComponent))
		{
			InventoryComponent->RefreshCapacityFromProgression();
		}
		return;
	}

	if (AJTSPlayerState* const PreviousPlayerState = BoundPlayerState.Get())
	{
		PreviousPlayerState->OnNetworkStateChanged.RemoveDynamic(this, &AJTSCharacter::HandlePlayerStateNetworkChanged);
	}

	BoundPlayerState = NewPlayerState;
	if (NewPlayerState != nullptr)
	{
		NewPlayerState->OnNetworkStateChanged.AddDynamic(this, &AJTSCharacter::HandlePlayerStateNetworkChanged);
	}
	ApplyAvatarColor();
	ApplyProgressionMovementSpeed();
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->RefreshCapacityFromProgression();
	}
}

void AJTSCharacter::UnbindPlayerState()
{
	if (AJTSPlayerState* const BoundState = BoundPlayerState.Get())
	{
		BoundState->OnNetworkStateChanged.RemoveDynamic(this, &AJTSCharacter::HandlePlayerStateNetworkChanged);
	}
	BoundPlayerState.Reset();
}

void AJTSCharacter::MoveForward(const FInputActionValue& Value)
{
	if (IsBoarded() || Controller == nullptr)
	{
		return;
	}

	const float MovementValue = Value.Get<float>();
	if (!FMath::IsNearlyZero(MovementValue))
	{
		FVector ForwardDirection;
		FVector RightDirection;
		GetMovementInputDirections(ForwardDirection, RightDirection);
		AddMovementInput(ForwardDirection, MovementValue);
	}
}

void AJTSCharacter::MoveRight(const FInputActionValue& Value)
{
	if (IsBoarded() || Controller == nullptr)
	{
		return;
	}

	const float MovementValue = Value.Get<float>();
	if (!FMath::IsNearlyZero(MovementValue))
	{
		FVector ForwardDirection;
		FVector RightDirection;
		GetMovementInputDirections(ForwardDirection, RightDirection);
		AddMovementInput(RightDirection, MovementValue);
	}
}

void AJTSCharacter::GetMovementInputDirections(FVector& OutForward, FVector& OutRight) const
{
	OutForward = FVector::ForwardVector;
	OutRight = FVector::RightVector;
	if (IsRealPlanetGameplayActive())
	{
		const FVector LocalUp = bPlanetFrameInitialized ? LastPlanetUp : GetDesiredPlanetUp();
		OutForward = GetStablePlanetTangent(LocalUp, GetPlanetCameraForward(LocalUp));
		OutRight = FVector::CrossProduct(LocalUp, OutForward).GetSafeNormal();
		if (OutRight.IsNearlyZero())
		{
			OutRight = GetStablePlanetTangent(LocalUp, FVector::RightVector);
		}
		return;
	}

	if (Controller == nullptr)
	{
		return;
	}

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);
	OutForward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	OutRight = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
}

void AJTSCharacter::LookYaw(const FInputActionValue& Value)
{
	if (IsRealPlanetGameplayActive())
	{
		const FVector LocalUp = bPlanetFrameInitialized ? LastPlanetUp : GetDesiredPlanetUp();
		PlanetCameraTangentForward = GetStablePlanetTangent(LocalUp, PlanetCameraTangentForward);
		const float YawDeltaRadians = FMath::DegreesToRadians(Value.Get<float>() * MouseSensitivityX);
		PlanetCameraTangentForward = FQuat(LocalUp, YawDeltaRadians).RotateVector(PlanetCameraTangentForward).GetSafeNormal();
		UpdatePlanetCameraFrame(LocalUp, 0.0f);
		// The real-planet camera owns an absolute local frame. Apply the configured body behavior in
		// this input path as well, so FaceCamera does not wait for the next actor tick.
		UpdatePlanetBodyOrientation(LocalUp, 0.0f);
		return;
	}

	if (Controller != nullptr)
	{
		AddControllerYawInput(Value.Get<float>() * MouseSensitivityX);
	}
}

void AJTSCharacter::LookPitch(const FInputActionValue& Value)
{
	if (IsRealPlanetGameplayActive())
	{
		const float RequestedPitchMin = bFirstPersonView ? FirstPersonViewPitchMin : ThirdPersonViewPitchMin;
		const float RequestedPitchMax = bFirstPersonView ? FirstPersonViewPitchMax : ThirdPersonViewPitchMax;
		const AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetController());
		const float PitchDirection = PlayerController != nullptr && PlayerController->IsLookYAxisInverted() ? -1.0f : 1.0f;
		// Positive local camera pitch means look up. The default mouse mapping is therefore direct;
		// inversion remains a player-controller preference rather than a hidden engine-side sign flip.
		PlanetCameraPitch = FMath::Clamp(
			PlanetCameraPitch + Value.Get<float>() * MouseSensitivityY * PitchDirection,
			FMath::Min(RequestedPitchMin, RequestedPitchMax),
			FMath::Max(RequestedPitchMin, RequestedPitchMax));
		const FVector LocalUp = bPlanetFrameInitialized ? LastPlanetUp : GetDesiredPlanetUp();
		UpdatePlanetCameraFrame(LocalUp, 0.0f);
		return;
	}

	if (Controller != nullptr)
	{
		const AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetController());
		const float PitchDirection = PlayerController != nullptr && PlayerController->IsLookYAxisInverted() ? -1.0f : 1.0f;
		AddControllerPitchInput(Value.Get<float>() * MouseSensitivityY * PitchDirection);
	}
}

void AJTSCharacter::StartSprint(const FInputActionValue& Value)
{
	bSprintInputActive = !IsBoarded();
	ApplyProgressionMovementSpeed();
}

void AJTSCharacter::StopSprint(const FInputActionValue& Value)
{
	bSprintInputActive = false;
	ApplyProgressionMovementSpeed();
}

void AJTSCharacter::HandleJumpStarted(const FInputActionValue& Value)
{
	if (!IsBoarded())
	{
		// UE 5.8 CharacterMovement applies JumpZVelocity along LocalUp when custom gravity is active.
		// Keep the standard Jump path; do not inject a World-Z LaunchCharacter impulse.
		Jump();
	}
}

void AJTSCharacter::HandleInteractStarted(const FInputActionValue& Value)
{
	static_cast<void>(Value);
	// E is a one-press world action. Check the modal shop first so it also remains
	// the lightweight close shortcut without sharing state with the boarding hold.
	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetController()))
	{
		if (PlayerController->IsSpaceShopOpen())
		{
			PlayerController->CloseSpaceShop();
			return;
		}
		if (PlayerController->IsMoonShopOpen())
		{
			PlayerController->CloseMoonShop();
			return;
		}
	}

	if (IsGameplayInputBlocked())
	{
		return;
	}

	if (InteractionComponent != nullptr)
	{
		InteractionComponent->TryInteract();
	}
}

void AJTSCharacter::HandleBoardStarted(const FInputActionValue& Value)
{
	static_cast<void>(Value);
	if (IsGameplayInputBlocked())
	{
		return;
	}

	bInteractKeyHeld = true;
	if (IsBoarded())
	{
		bInteractKeyHeld = false;
		if (AJTSSpacecraftActor* const Spacecraft = BoardedSpacecraft.Get())
		{
			if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetController()))
			{
				PlayerController->ServerRequestDisembarkSpacecraft(Spacecraft);
			}
		}
		return;
	}

	BeginBoardingHold();
}

void AJTSCharacter::HandleBoardTriggered(const FInputActionValue& Value)
{
	static_cast<void>(Value);
	if (IsGameplayInputBlocked() || IsBoarded())
	{
		return;
	}

	const bool bCanBeginBoarding = (BoundGameState.IsValid() && BoundGameState->IsEarthCollectionActive())
		|| IsSpaceWorldSurfaceGameplayActive();
	if (!bCanBeginBoarding || bBoardingHoldActive)
	{
		return;
	}
	if (bInteractKeyHeld)
	{
		return;
	}

	// A startup overlap can arrive after F begins evaluating. Re-check the same
	// view-gated hold path without turning ordinary E interactions into holds.
	bInteractKeyHeld = true;
	BeginBoardingHold();
}

void AJTSCharacter::HandleBoardCompleted(const FInputActionValue& Value)
{
	static_cast<void>(Value);
	bInteractKeyHeld = false;
	CancelBoardingHold();
}

void AJTSCharacter::HandleBoardCanceled(const FInputActionValue& Value)
{
	static_cast<void>(Value);
	bInteractKeyHeld = false;
	CancelBoardingHold();
}

void AJTSCharacter::HandleAttackStarted(const FInputActionValue& Value)
{
	if (!CanUseNormalGameplayInput())
	{
		return;
	}

	// A ranged click asks the body to catch the screen center. A tool, a melee weapon,
	// and empty hands keep the body on its own facing and swing that way.
	if (IsValid(RangedWeaponComponent) && RangedWeaponComponent->HasActiveRangedWeapon())
	{
		AlignBodyToViewOnAttack();
	}
	if (IsValid(RangedWeaponComponent) && RangedWeaponComponent->HasActiveRangedWeapon())
	{
		RangedWeaponComponent->StartFire();
		return;
	}
	if (IsValid(MeleeComponent))
	{
		MeleeComponent->AttackPressed();
	}
}

bool AJTSCharacter::GetViewTangentForward(FVector& OutForward) const
{
	if (IsRealPlanetGameplayActive())
	{
		const FVector LocalUp = bPlanetFrameInitialized ? LastPlanetUp : GetDesiredPlanetUp();
		OutForward = GetStablePlanetTangent(LocalUp, GetPlanetCameraForward(LocalUp));
		return !OutForward.IsNearlyZero();
	}
	if (Controller == nullptr)
	{
		return false;
	}

	const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	OutForward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	return !OutForward.IsNearlyZero();
}

float AJTSCharacter::GetViewBodyYawDeltaDegrees(const FVector& ViewForward) const
{
	const FVector BodyForward = GetActorForwardVector().GetSafeNormal();
	const FVector Desired = ViewForward.GetSafeNormal();
	if (BodyForward.IsNearlyZero() || Desired.IsNearlyZero())
	{
		return 0.0f;
	}

	const FVector Up = IsRealPlanetGameplayActive()
		? (bPlanetFrameInitialized ? LastPlanetUp : GetDesiredPlanetUp())
		: FVector::UpVector;
	const float Signed = FMath::RadiansToDegrees(FMath::Atan2(
		FVector::DotProduct(FVector::CrossProduct(BodyForward, Desired), Up),
		FVector::DotProduct(BodyForward, Desired)));
	return FRotator::NormalizeAxis(Signed);
}

bool AJTSCharacter::IsMovingOnFoot() const
{
	const UCharacterMovementComponent* const Movement = GetCharacterMovement();
	return IsValid(Movement) && Movement->Velocity.SizeSquared() > FMath::Square(40.0f)
		&& Movement->IsMovingOnGround();
}

bool AJTSCharacter::GetDesiredFeetForward(FVector& OutForward) const
{
	// First person, aim, a jump, and the single frame of a grounded click face the camera.
	// After that click, grounded ordinary third person falls through to the walk
	// direction, so orbiting the camera never starts another foot shuffle.
	if (bWantsViewFacing || WantsContinuousViewFacing())
	{
		if (IsLocallyControlled() && GetViewTangentForward(OutForward))
		{
			return true;
		}
		if (bWantsViewFacing && !PendingViewFacingForward.IsNearlyZero())
		{
			OutForward = PendingViewFacingForward.GetSafeNormal();
			return true;
		}
		if (WantsContinuousViewFacing() && Controller != nullptr)
		{
			const FVector Up = IsRealPlanetGameplayActive()
				? (bPlanetFrameInitialized ? LastPlanetUp : GetDesiredPlanetUp())
				: FVector::UpVector;
			const FVector View = FVector::VectorPlaneProject(Controller->GetControlRotation().Vector(), Up);
			if (!View.IsNearlyZero())
			{
				OutForward = View.GetSafeNormal();
				return true;
			}
		}
	}

	const UCharacterMovementComponent* const Movement = GetCharacterMovement();
	if (!IsValid(Movement))
	{
		return false;
	}

	const FVector Up = IsRealPlanetGameplayActive()
		? (bPlanetFrameInitialized ? LastPlanetUp : GetDesiredPlanetUp())
		: FVector::UpVector;
	const FVector TangentVelocity = FVector::VectorPlaneProject(Movement->Velocity, Up);
	if (TangentVelocity.SizeSquared() <= FMath::Square(40.0f))
	{
		return false;
	}

	OutForward = TangentVelocity.GetSafeNormal();
	return !OutForward.IsNearlyZero();
}

bool AJTSCharacter::WantsContinuousViewFacing() const
{
	const UCharacterMovementComponent* const Movement = GetCharacterMovement();
	const bool bAirborne = (IsValid(Movement) && Movement->IsFalling()) || bPressedJump;
	// Aim and first person always match the camera, on the ground and in the air.
	// A jump does the same from the press, so the body is already on the view when the feet leave.
	return bFirstPersonView
		|| bAirborne
		|| (IsValid(RangedWeaponComponent) && RangedWeaponComponent->IsAiming());
}

void AJTSCharacter::AlignBodyToViewOnAttack()
{
	// Aim, first person, and a jump already keep the whole body on the camera.
	// A click there must not leave a follow that survives after the player lets go.
	if (WantsContinuousViewFacing())
	{
		bWantsViewFacing = false;
		return;
	}

	FVector ViewForward = FVector::ZeroVector;
	if (!GetViewTangentForward(ViewForward))
	{
		return;
	}

	// The gun arm already covers this much yaw on its own. Inside that cone the
	// barrel meets the camera without turning the feet. Past it, the body catches up.
	const float ArmReachDegrees = FMath::Max(10.0f, UpperBodyYawLimitDegrees);
	if (FMath::Abs(GetViewBodyYawDeltaDegrees(ViewForward)) <= ArmReachDegrees)
	{
		bWantsViewFacing = false;
		return;
	}

	// One press, one catch. Holding the button does not keep the legs on the camera.
	bWantsViewFacing = true;
	PendingViewFacingForward = ViewForward;
	if (!HasAuthority())
	{
		ServerRequestViewFacing(ViewForward);
	}
}

void AJTSCharacter::ServerRequestViewFacing_Implementation(FVector_NetQuantizeNormal ViewForward)
{
	if (ViewForward.IsNearlyZero() || bFirstPersonView)
	{
		return;
	}

	bWantsViewFacing = true;
	PendingViewFacingForward = ViewForward.GetSafeNormal();
}

void AJTSCharacter::StepBodyTowardView(const FVector& ViewForward, float DeltaSeconds, bool bUsePlanetFrame)
{
	const float YawDelta = GetViewBodyYawDeltaDegrees(ViewForward);
	const float AbsYaw = FMath::Abs(YawDelta);
	const UCharacterMovementComponent* const Movement = GetCharacterMovement();
	const bool bAirborne = (IsValid(Movement) && Movement->IsFalling()) || bPressedJump;
	const bool bLockedToView = WantsContinuousViewFacing();
	// The click flag is consumed here. Later ticks must not treat it as a held follow.
	const bool bClickAlign = bWantsViewFacing;
	bWantsViewFacing = false;
	// Grounded ordinary third person only shuffles for a walk or that one click.
	// Aim, first person, and a jump rotate the body with the camera and never
	// play the shuffle, so the jump tuck stays one continuous pose.
	const bool bPlayShuffle = !bAirborne && !bLockedToView && (IsMovingOnFoot() || bClickAlign);
	const bool bTurnBody = bPlayShuffle || bLockedToView;
	const float TargetShuffle = bPlayShuffle && AbsYaw > 4.0f ? 1.0f : 0.0f;
	TurnShuffleAlpha = FMath::FInterpTo(TurnShuffleAlpha, TargetShuffle, DeltaSeconds, bPlayShuffle ? 10.0f : 6.0f);
	if (!bTurnBody)
	{
		return;
	}

	// A grounded click catches the whole remaining yaw on that press.
	// Aim, first person, and a jump snap the whole body onto the camera this frame.
	// The upper body is not left twisted behind a slower capsule turn.
	const float DegreesPerSecond = FMath::Max(60.0f, FootShuffleDegreesPerSecond);
	const float StepDegrees = (bClickAlign || bLockedToView)
		? AbsYaw
		: FMath::Min(AbsYaw, DegreesPerSecond * DeltaSeconds);
	if (StepDegrees <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector Up = bUsePlanetFrame
		? (bPlanetFrameInitialized ? LastPlanetUp : GetDesiredPlanetUp())
		: FVector::UpVector;
	const FQuat Turn(Up, FMath::DegreesToRadians(FMath::Sign(YawDelta) * StepDegrees));
	if (bUsePlanetFrame)
	{
		PlanetBodyForward = GetStablePlanetTangent(Up, Turn.RotateVector(PlanetBodyForward));
		SetActorRotation(FRotationMatrix::MakeFromXZ(PlanetBodyForward, Up).ToQuat());
	}
	else
	{
		SetActorRotation(Turn * GetActorQuat());
	}
}

void AJTSCharacter::UpdateFacingPresentation(float DeltaSeconds)
{
	if (IsBoarded())
	{
		TurnShuffleAlpha = FMath::FInterpTo(TurnShuffleAlpha, 0.0f, DeltaSeconds, 8.0f);
		return;
	}

	// A click is consumed inside StepBodyTowardView on this same frame.
	// Nothing here may keep the body following the camera after that swing.
	if (WantsContinuousViewFacing())
	{
		bWantsViewFacing = false;
	}

	FVector FeetForward = FVector::ZeroVector;
	if (!GetDesiredFeetForward(FeetForward))
	{
		TurnShuffleAlpha = FMath::FInterpTo(TurnShuffleAlpha, 0.0f, DeltaSeconds, 8.0f);
		return;
	}

	// Grounded ordinary third person leaves the legs where they are.
	// Aim, first person, a jump, and that one click own the facing instead.
	const bool bUsePlanetFrame = IsRealPlanetGameplayActive();
	if (!bUsePlanetFrame)
	{
		bUseControllerRotationYaw = false;
		if (UCharacterMovementComponent* const Movement = GetCharacterMovement())
		{
			const bool bFeetCatching = IsMovingOnFoot() || bWantsViewFacing || WantsContinuousViewFacing();
			Movement->bOrientRotationToMovement = !bFeetCatching;
		}
	}
	StepBodyTowardView(FeetForward, DeltaSeconds, bUsePlanetFrame);
}

void AJTSCharacter::HandleAttackReleased(const FInputActionValue& Value)
{
	// Release must always be forwarded so a blocked UI or phase transition cannot leave the hold state stuck.
	if (IsValid(RangedWeaponComponent))
	{
		RangedWeaponComponent->StopFire();
	}
	if (IsValid(MeleeComponent))
	{
		MeleeComponent->AttackReleased();
	}
}

void AJTSCharacter::HandleMeleeAttackStarted(EJTSAttackType AttackType)
{
	if (!IsValid(MeleeComponent))
	{
		return;
	}
	if (AttackType != EJTSAttackType::Punch)
	{
		if (IsValid(WeaponVisualComponent))
		{
			WeaponVisualComponent->PlayMeleeSwingPresentation();
		}
		return;
	}

	PlayUnarmedPunchPresentation(
		MeleeComponent->IsCurrentPunchLeft(),
		MeleeComponent->IsContinuingUnarmedCombo());
}

void AJTSCharacter::HandleMeleeAttackFinished(EJTSAttackType AttackType)
{
	if (AttackType == EJTSAttackType::Punch)
	{
		StopUnarmedPunchPresentation();
	}
}

void AJTSCharacter::HandleAimStarted(const FInputActionValue& Value)
{
	static_cast<void>(Value);
	if (!CanUseNormalGameplayInput() || !IsValid(RangedWeaponComponent) || !RangedWeaponComponent->HasActiveRangedWeapon())
	{
		return;
	}
	RangedWeaponComponent->StartAim();
}

void AJTSCharacter::HandleAimReleased(const FInputActionValue& Value)
{
	static_cast<void>(Value);
	if (IsValid(RangedWeaponComponent))
	{
		RangedWeaponComponent->StopAim();
	}
}

void AJTSCharacter::HandleToggleCameraStarted(const FInputActionValue& Value)
{
	if (!CanUseNormalGameplayInput())
	{
		return;
	}

	bFirstPersonView = !bFirstPersonView;
	ApplyCameraView();
}

void AJTSCharacter::HandleCameraZoom(const FInputActionValue& Value)
{
	if (!CanUseNormalGameplayInput())
	{
		return;
	}

	// Scroll changes the third-person camera distance for every held item.
	AdjustThirdPersonCameraDistance(Value.Get<float>());
}

void AJTSCharacter::HandleQuickbarSlotOneStarted(const FInputActionValue& Value)
{
	SelectQuickbarSlotByPage(0);
}

void AJTSCharacter::HandleQuickbarSlotTwoStarted(const FInputActionValue& Value)
{
	SelectQuickbarSlotByPage(1);
}

void AJTSCharacter::HandleQuickbarSlotThreeStarted(const FInputActionValue& Value)
{
	SelectQuickbarSlotByPage(2);
}

void AJTSCharacter::HandleQuickbarSlotFourStarted(const FInputActionValue& Value)
{
	SelectQuickbarSlotByPage(3);
}

void AJTSCharacter::HandleQuickbarSlotFiveStarted(const FInputActionValue& Value)
{
	SelectQuickbarSlotByPage(4);
}

void AJTSCharacter::HandleQuickbarSlotSixStarted(const FInputActionValue& Value)
{
	SelectQuickbarSlotByPage(5);
}

void AJTSCharacter::HandleQuickbarSlotSevenStarted(const FInputActionValue& Value)
{
	SelectQuickbarSlotByPage(6);
}

void AJTSCharacter::HandleQuickbarSlotEightStarted(const FInputActionValue& Value)
{
	SelectQuickbarSlotByPage(7);
}

void AJTSCharacter::HandleQuickbarSlotNineStarted(const FInputActionValue& Value)
{
	SelectQuickbarSlotByPage(8);
}

void AJTSCharacter::HandlePreviousQuickbarPageStarted(const FInputActionValue& Value)
{
	if (CanUseNormalGameplayInput() && IsValid(InventoryComponent))
	{
		InventoryComponent->SelectQuickbarPage(InventoryComponent->GetQuickbarPageIndex() - 1);
	}
}

void AJTSCharacter::HandleNextQuickbarPageStarted(const FInputActionValue& Value)
{
	if (CanUseNormalGameplayInput() && IsValid(InventoryComponent))
	{
		InventoryComponent->SelectQuickbarPage(InventoryComponent->GetQuickbarPageIndex() + 1);
	}
}

void AJTSCharacter::HandleDiscardItemStarted(const FInputActionValue& Value)
{
	BeginItemDiscardHold();
}

void AJTSCharacter::HandleDiscardItemReleased(const FInputActionValue& Value)
{
	EndItemDiscardHold();
}

void AJTSCharacter::SelectQuickbarSlotByPage(int32 SlotIndexInPage)
{
	if (!CanUseNormalGameplayInput() || !IsValid(InventoryComponent)
		|| SlotIndexInPage < 0 || SlotIndexInPage >= UJTSInventoryComponent::MaximumQuickbarSlots)
	{
		return;
	}

	const int32 SlotIndex = InventoryComponent->GetQuickbarPageStart() + SlotIndexInPage;
	if (SlotIndex < InventoryComponent->GetInventoryCapacity())
	{
		InventoryComponent->SelectQuickbarSlot(SlotIndex);
	}
}

void AJTSCharacter::BeginItemDiscardHold()
{
	if (!CanUseNormalGameplayInput() || !IsValid(InventoryComponent))
	{
		return;
	}

	CancelItemDiscardHold();
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	HeldItemDiscardSlotIndex = InventoryComponent->GetSelectedQuickbarSlot();
	if (InventoryComponent->GetItemAtSlot(HeldItemDiscardSlotIndex).IsEmpty())
	{
		HeldItemDiscardSlotIndex = INDEX_NONE;
		return;
	}

	bItemDiscardHoldCompleted = false;
	ItemDiscardHoldStartTime = static_cast<double>(World->GetTimeSeconds());
	World->GetTimerManager().SetTimer(
		ItemDiscardHoldTimerHandle,
		this,
		&AJTSCharacter::CompleteItemDiscardHold,
		FMath::Max(0.1f, ItemDestroyHoldDuration),
		false);
}

void AJTSCharacter::EndItemDiscardHold()
{
	if (HeldItemDiscardSlotIndex == INDEX_NONE)
	{
		return;
	}

	const int32 SlotIndex = HeldItemDiscardSlotIndex;
	const bool bShouldDrop = !bItemDiscardHoldCompleted;
	CancelItemDiscardHold();
	if (!bShouldDrop || !CanUseNormalGameplayInput() || !IsValid(InventoryComponent))
	{
		return;
	}

	const FJTSItemInstance Item = InventoryComponent->GetItemAtSlot(SlotIndex);
	if (Item.IsEmpty())
	{
		return;
	}
	if (Item.StackCount > 1)
	{
		if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetController()))
		{
			PlayerController->OpenInventoryQuantityDialog(SlotIndex, false);
		}
		return;
	}
	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetController()))
	{
		PlayerController->ServerRequestInventoryQuantityAction(SlotIndex, 1, false);
	}
}

void AJTSCharacter::CancelItemDiscardHold()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ItemDiscardHoldTimerHandle);
	}

	HeldItemDiscardSlotIndex = INDEX_NONE;
	ItemDiscardHoldStartTime = 0.0;
	bItemDiscardHoldCompleted = false;
}

void AJTSCharacter::CompleteItemDiscardHold()
{
	if (!CanUseNormalGameplayInput() || !IsValid(InventoryComponent) || HeldItemDiscardSlotIndex == INDEX_NONE)
	{
		CancelItemDiscardHold();
		return;
	}

	const int32 SlotIndex = HeldItemDiscardSlotIndex;
	const FJTSItemInstance Item = InventoryComponent->GetItemAtSlot(SlotIndex);
	bItemDiscardHoldCompleted = true;
	if (Item.StackCount > 1)
	{
		if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetController()))
		{
			PlayerController->OpenInventoryQuantityDialog(SlotIndex, true);
		}
	}
	else if (!Item.IsEmpty())
	{
		if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetController()))
		{
			PlayerController->ServerRequestInventoryQuantityAction(SlotIndex, 1, true);
		}
	}

	HeldItemDiscardSlotIndex = INDEX_NONE;
	ItemDiscardHoldStartTime = 0.0;
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ItemDiscardHoldTimerHandle);
	}
}

bool AJTSCharacter::CanUseNormalGameplayInput() const
{
	return !IsBoarded()
		&& ((BoundGameState.IsValid() && (BoundGameState->IsEarthCollectionActive() || BoundGameState->IsMoonExploration()))
			|| IsSpaceWorldSurfaceGameplayActive())
		&& !IsGameplayInputBlocked();
}

bool AJTSCharacter::IsSpaceWorldSurfaceGameplayActive() const
{
	const AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
	return IsValid(Manager) && Manager->IsSurfaceGameplayReady();
}

bool AJTSCharacter::IsRealPlanetGameplayActive() const
{
	const AJTSPlanetAnchor* const Planet = GameplayPlanet.Get();
	return IsValid(Planet) && IsPlanetGravityEnabled();
}

FVector AJTSCharacter::GetDesiredPlanetUp() const
{
	if (const UCharacterMovementComponent* const MovementComponent = GetCharacterMovement();
		IsValid(MovementComponent))
	{
		const FVector GravityDirection = MovementComponent->GetGravityDirection();
		if (!GravityDirection.IsNearlyZero())
		{
			return -GravityDirection.GetSafeNormal();
		}
	}

	return GetActorUpVector().GetSafeNormal();
}

FVector AJTSCharacter::GetStablePlanetTangent(const FVector& UpVector, const FVector& PreferredDirection) const
{
	const FVector SafeUp = UpVector.GetSafeNormal();
	FVector Tangent = FVector::VectorPlaneProject(PreferredDirection, SafeUp).GetSafeNormal();
	if (!Tangent.IsNearlyZero())
	{
		return Tangent;
	}

	// This is a fallback only; normal movement and camera orientation are parallel-transported from prior frames.
	const FVector ReferenceAxis = FMath::Abs(SafeUp.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
	Tangent = FVector::CrossProduct(ReferenceAxis, SafeUp).GetSafeNormal();
	if (Tangent.IsNearlyZero())
	{
		Tangent = FVector::CrossProduct(FVector::RightVector, SafeUp).GetSafeNormal();
	}
	return Tangent;
}

void AJTSCharacter::UpdatePlanetGameplayFrame(float DeltaSeconds)
{
	const FVector DesiredUp = GetDesiredPlanetUp();
	if (DesiredUp.IsNearlyZero())
	{
		return;
	}

	// Keep the camera frame current before evaluating FaceCamera body yaw. Both frames still use the
	// planet-local up vector, so this does not introduce World-Z orientation into SpaceWorld.
	if (IsLocallyControlled())
	{
		UpdatePlanetCameraFrame(DesiredUp, DeltaSeconds);
	}
	if (HasAuthority() || IsLocallyControlled())
	{
		UpdatePlanetBodyOrientation(DesiredUp, DeltaSeconds);
	}

	if (bDebugPlanetSurface && IsValid(GameplayPlanet))
	{
		const FVector CharacterLocation = GetActorLocation();
		DrawDebugLine(GetWorld(), CharacterLocation, GameplayPlanet->GetPlanetCenter(), FColor::Cyan, false, -1.0f, 0, 1.0f);
		DrawDebugDirectionalArrow(GetWorld(), CharacterLocation, CharacterLocation + LastPlanetUp * 220.0f, 30.0f, FColor::Green, false, -1.0f, 0, 1.5f);
		DrawDebugString(
			GetWorld(),
			CharacterLocation + LastPlanetUp * 150.0f,
			FString::Printf(TEXT("Planet=%s  ApproxAltitude=%.1f"),
				*GameplayPlanet->GetPlanetId().ToString(),
				GameplayPlanet->GetApproximateAltitude(CharacterLocation)),
			nullptr,
			FColor::White,
			0.0f,
			true,
			1.0f);
	}
}

void AJTSCharacter::UpdatePlanetBodyOrientation(const FVector& DesiredUp, float DeltaSeconds)
{
	const FVector TargetUp = DesiredUp.GetSafeNormal();
	if (!bPlanetFrameInitialized)
	{
		LastPlanetUp = TargetUp;
		PlanetBodyForward = GetStablePlanetTangent(TargetUp, GetActorForwardVector());
		bPlanetFrameInitialized = true;
	}
	else
	{
		const FQuat ParallelTransport = FQuat::FindBetweenNormals(LastPlanetUp, TargetUp);
		PlanetBodyForward = GetStablePlanetTangent(TargetUp, ParallelTransport.RotateVector(PlanetBodyForward));
		LastPlanetUp = TargetUp;
	}

	if (PlanetBodyFacingMode == EJTSPlanetBodyFacingMode::FaceCamera)
	{
		// Do not enable bUseControllerRotationYaw here: controller rotation also contains the
		// gravity-relative pitch/roll used by the absolute camera. Only the local tangent yaw belongs
		// to the character body.
		const FVector CameraDirection = !IsLocallyControlled() && Controller != nullptr
			? Controller->GetControlRotation().Vector() : PlanetCameraTangentForward;
		PlanetBodyForward = GetStablePlanetTangent(LastPlanetUp, CameraDirection);
	}
	else
	{
		FVector FeetForward = FVector::ZeroVector;
		if (GetDesiredFeetForward(FeetForward))
		{
			StepBodyTowardView(GetStablePlanetTangent(LastPlanetUp, FeetForward), DeltaSeconds, true);
		}
		else
		{
			TurnShuffleAlpha = FMath::FInterpTo(TurnShuffleAlpha, 0.0f, DeltaSeconds, 8.0f);
		}
	}

	GetCharacterMovement()->bOrientRotationToMovement = false;
	SetActorRotation(FRotationMatrix::MakeFromXZ(PlanetBodyForward, LastPlanetUp).ToQuat());
}

void AJTSCharacter::UpdatePlanetCameraFrame(const FVector& CurrentUp, float DeltaSeconds)
{
	static_cast<void>(DeltaSeconds);

	const FVector SafeUp = CurrentUp.GetSafeNormal();
	if (SafeUp.IsNearlyZero() || !IsValid(CameraBoom))
	{
		return;
	}

	if (!bPlanetCameraFrameInitialized)
	{
		PlanetCameraTangentForward = GetStablePlanetTangent(SafeUp, PlanetBodyForward);
		LastPlanetCameraUp = SafeUp;
		bPlanetCameraFrameInitialized = true;
	}
	else
	{
		const FQuat ParallelTransport = FQuat::FindBetweenNormals(LastPlanetCameraUp, SafeUp);
		PlanetCameraTangentForward = GetStablePlanetTangent(SafeUp, ParallelTransport.RotateVector(PlanetCameraTangentForward));
		LastPlanetCameraUp = SafeUp;
	}

	const float RequestedPitchMin = bFirstPersonView ? FirstPersonViewPitchMin : ThirdPersonViewPitchMin;
	const float RequestedPitchMax = bFirstPersonView ? FirstPersonViewPitchMax : ThirdPersonViewPitchMax;
	PlanetCameraPitch = FMath::Clamp(
		PlanetCameraPitch,
		FMath::Min(RequestedPitchMin, RequestedPitchMax),
		FMath::Max(RequestedPitchMin, RequestedPitchMax));

	const FVector CameraForward = GetPlanetCameraForward(SafeUp);
	const FQuat CameraRotation = FRotationMatrix::MakeFromXZ(CameraForward, SafeUp).ToQuat();
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->SetWorldRotation(CameraRotation);

	if (Controller != nullptr && IsLocallyControlled())
	{
		// Control rotation is output for camera rays and aim only; it never drives capsule/body orientation here.
		Controller->SetControlRotation(CameraRotation.Rotator());
	}

	if (bDebugPlanetCamera)
	{
		const FVector CameraOrigin = CameraPivot != nullptr ? CameraPivot->GetComponentLocation() : GetActorLocation();
		DrawDebugDirectionalArrow(GetWorld(), CameraOrigin, CameraOrigin + CameraForward * 260.0f, 32.0f, FColor::Yellow, false, -1.0f, 0, 1.5f);
		DrawDebugDirectionalArrow(GetWorld(), CameraOrigin, CameraOrigin + SafeUp * 180.0f, 28.0f, FColor::Blue, false, -1.0f, 0, 1.0f);
	}
}

FVector AJTSCharacter::GetPlanetCameraForward(const FVector& CurrentUp) const
{
	const FVector TangentForward = GetStablePlanetTangent(CurrentUp, PlanetCameraTangentForward);
	const FVector CameraRight = FVector::CrossProduct(CurrentUp, TangentForward).GetSafeNormal();
	if (CameraRight.IsNearlyZero())
	{
		return TangentForward;
	}

	// UE's local right axis rotates the forward vector downward for positive angles, hence the sign.
	return FQuat(CameraRight, FMath::DegreesToRadians(-PlanetCameraPitch)).RotateVector(TangentForward).GetSafeNormal();
}

bool AJTSCharacter::IsGameplayInputBlocked() const
{
	const AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetController());
	return !IsValid(PlayerController) || PlayerController->IsMoonShopOpen() || PlayerController->IsSpaceShopOpen() || PlayerController->IsGameMenuOpen();
}

void AJTSCharacter::ApplyCameraPitchLimits()
{
	APlayerController* const PlayerController = Cast<APlayerController>(GetController());
	if (PlayerController == nullptr || PlayerController->PlayerCameraManager == nullptr)
	{
		return;
	}

	const float RequestedPitchMin = bFirstPersonView ? FirstPersonViewPitchMin : ThirdPersonViewPitchMin;
	const float RequestedPitchMax = bFirstPersonView ? FirstPersonViewPitchMax : ThirdPersonViewPitchMax;
	PlayerController->PlayerCameraManager->ViewPitchMin = FMath::Min(RequestedPitchMin, RequestedPitchMax);
	PlayerController->PlayerCameraManager->ViewPitchMax = FMath::Max(RequestedPitchMin, RequestedPitchMax);
}

void AJTSCharacter::InitializeThirdPersonCameraDistance()
{
	if (bThirdPersonCameraDistanceInitialized)
	{
		return;
	}

	const float MinimumArmLength = FMath::Min(ThirdPersonCameraMinArmLength, ThirdPersonCameraMaxArmLength);
	const float MaximumArmLength = FMath::Max(ThirdPersonCameraMinArmLength, ThirdPersonCameraMaxArmLength);
	CurrentThirdPersonCameraArmLength = FMath::Clamp(ThirdPersonArmLength, MinimumArmLength, MaximumArmLength);
	bThirdPersonCameraDistanceInitialized = true;
}

void AJTSCharacter::ApplyCameraView()
{
	ApplyCameraPitchLimits();

	if (CameraPivot == nullptr || CameraBoom == nullptr || FollowCamera == nullptr)
	{
		return;
	}
	InitializeThirdPersonCameraDistance();

	CameraPivot->SetRelativeLocation(FVector(0.0f, 0.0f, CameraPivotHeight));
	CameraBoom->SocketOffset = bFirstPersonView
		? FVector::ZeroVector
		: FVector(ThirdPersonShoulderOffset.X, ThirdPersonShoulderOffset.Y, 0.0f);
	CameraBoom->TargetArmLength = bFirstPersonView ? 0.0f : CurrentThirdPersonCameraArmLength;
	const bool bUseRealPlanetCamera = IsRealPlanetGameplayActive();
	CameraBoom->bUsePawnControlRotation = !bUseRealPlanetCamera;
	CameraBoom->SetUsingAbsoluteRotation(bUseRealPlanetCamera);
	CameraBoom->bDoCollisionTest = !bFirstPersonView;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;
	if (bUseRealPlanetCamera)
	{
		UpdatePlanetCameraFrame(bPlanetFrameInitialized ? LastPlanetUp : GetDesiredPlanetUp(), 0.0f);
	}
	else if (UCharacterMovementComponent* const MovementComponent = GetCharacterMovement())
	{
		// A click is one frame. It must not flip movement orientation onto the camera.
		MovementComponent->bOrientRotationToMovement = !WantsContinuousViewFacing();
		bUseControllerRotationYaw = false;
	}
	FollowCamera->SetFieldOfView(bFirstPersonView ? FirstPersonFOV : ThirdPersonFOV);
	if (GetMesh() != nullptr)
	{
		GetMesh()->SetOwnerNoSee(bFirstPersonView);
	}
	if (DebugVisual != nullptr)
	{
		DebugVisual->SetOwnerNoSee(bFirstPersonView);
	}
}

bool AJTSCharacter::BeginBoardingHold()
{
	const bool bCanBeginBoarding = (BoundGameState.IsValid() && BoundGameState->IsEarthCollectionActive())
		|| IsSpaceWorldSurfaceGameplayActive();
	if (bBoardingHoldActive || IsBoarded() || !bCanBeginBoarding)
	{
		return false;
	}

	AJTSSpacecraftActor* const Spacecraft = GetCurrentBoardingSpacecraft();
	UWorld* const World = GetWorld();
	if (!IsValid(Spacecraft) || World == nullptr || !Spacecraft->IsPawnInBoardingRange(this))
	{
		return false;
	}

	bBoardingHoldActive = true;
	BoardingSpacecraft = Spacecraft;
	BoardingHoldStartTime = static_cast<double>(World->GetTimeSeconds());
	World->GetTimerManager().SetTimer(
		BoardingHoldTimerHandle,
		this,
		&AJTSCharacter::CompleteBoardingHold,
		FMath::Max(0.1f, BoardingHoldDuration),
		false);
	return true;
}

void AJTSCharacter::CancelBoardingHold()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BoardingHoldTimerHandle);
	}

	bBoardingHoldActive = false;
	BoardingHoldStartTime = 0.0;
	BoardingSpacecraft = nullptr;
}

void AJTSCharacter::CompleteBoardingHold()
{
	if (!bBoardingHoldActive || !bInteractKeyHeld || IsBoarded())
	{
		CancelBoardingHold();
		return;
	}

	AJTSSpacecraftActor* const Spacecraft = BoardingSpacecraft.Get();
	const bool bCanCompleteBoarding = (BoundGameState.IsValid() && BoundGameState->IsEarthCollectionActive())
		|| IsSpaceWorldSurfaceGameplayActive();
	if (!bCanCompleteBoarding
		|| !IsValid(Spacecraft)
		|| GetCurrentBoardingSpacecraft() != Spacecraft)
	{
		CancelBoardingHold();
		return;
	}

	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetController()))
	{
		PlayerController->ServerRequestBoardSpacecraft(Spacecraft);
	}
	CancelBoardingHold();
	bInteractKeyHeld = false;
}

AJTSSpacecraftActor* AJTSCharacter::GetCurrentBoardingSpacecraft()
{
	if (!IsValid(InteractionComponent))
	{
		return nullptr;
	}

	InteractionComponent->RefreshInteractable();
	AActor* const CurrentInteractionTarget = InteractionComponent->GetCurrentInteractable();
	if (AJTSSpacecraftActor* const Spacecraft = Cast<AJTSSpacecraftActor>(CurrentInteractionTarget))
	{
		return Spacecraft->IsPawnInBoardingRange(this) ? Spacecraft : nullptr;
	}

	// BoardingTrigger owns the authoritative proximity state. Some Blueprint spacecraft collision
	// setups deliberately do not enter the generic all-object overlap scan, so use that known
	// candidate only after it passes the exact same camera cone and Visibility test as other targets.
	if (!IsValid(CurrentInteractionTarget))
	{
		if (AJTSSpacecraftActor* const NearbyShip = NearbySpacecraft.Get();
			IsValid(NearbyShip)
			&& NearbyShip->IsPawnInBoardingRange(this)
			&& InteractionComponent->IsInteractableInView(NearbyShip))
		{
			return NearbyShip;
		}
	}

	return nullptr;
}

bool AJTSCharacter::RestoreAfterBoarding(AJTSSpacecraftActor* Spacecraft, bool bMoveToExitPoint)
{
	FVector DisembarkLocation = GetActorLocation();
	FJTSPlanetSurfaceFrame DisembarkSurfaceFrame;
	AJTSPlanetAnchor* GroundedPlanet = nullptr;
	bool bOnRealPlanet = false;
	if (bMoveToExitPoint && !FindSafeDisembarkLocation(Spacecraft, DisembarkLocation, &DisembarkSurfaceFrame))
	{
		return false;
	}
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (bMoveToExitPoint)
	{
		GroundedPlanet = IsValid(Spacecraft) ? Spacecraft->GetGroundedPlanet() : nullptr;
		bOnRealPlanet = IsValid(GroundedPlanet) && GroundedPlanet->HasGameplaySurface();
		FQuat DisembarkRotation = GetActorQuat();
		FVector GravityUp = FVector::UpVector;
		FVector SurfaceForward = DisembarkSurfaceFrame.Forward;
		if (bOnRealPlanet)
		{
			GravityUp = GroundedPlanet->GetRadialUpVector(DisembarkLocation).GetSafeNormal();
			SurfaceForward = GetStablePlanetTangent(GravityUp, DisembarkSurfaceFrame.Forward);
			DisembarkRotation = FRotationMatrix::MakeFromXZ(SurfaceForward, GravityUp).ToQuat();
		}
		SetActorLocationAndRotation(DisembarkLocation, DisembarkRotation, false, nullptr, ETeleportType::TeleportPhysics);

		if (bOnRealPlanet)
		{
			SetGameplayPlanet(GroundedPlanet);
			LastPlanetUp = GravityUp;
			PlanetBodyForward = SurfaceForward;
			PlanetCameraTangentForward = SurfaceForward;
			LastPlanetCameraUp = GravityUp;
			bPlanetFrameInitialized = true;
			bPlanetCameraFrameInitialized = true;
		}
	}

	BoardedSpacecraft = nullptr;
	ApplyBoardedPresentation();
	if (bOnRealPlanet && !SnapToPlanetSurface(GroundedPlanet, DisembarkLocation))
	{
		// The first candidate was already collision-validated. Keep it as a safe fallback, but do not
		// force walking when a streamed surface changed between validation and collision restoration.
		if (UCharacterMovementComponent* const MovementComponent = GetCharacterMovement())
		{
			MovementComponent->SetMovementMode(MOVE_Falling);
		}
	}
	if (NearbySpacecraft.Get() == Spacecraft)
	{
		NearbySpacecraft = nullptr;
	}
	ForceNetUpdate();
	return true;
}

bool AJTSCharacter::FindSafeDisembarkLocation(
	AJTSSpacecraftActor* Spacecraft,
	FVector& OutLocation,
	FJTSPlanetSurfaceFrame* OutSurfaceFrame) const
{
	if (!IsValid(Spacecraft) || !IsValid(GetCapsuleComponent()))
	{
		return false;
	}

	if (AJTSPlanetAnchor* const GroundedPlanet = Spacecraft->GetGroundedPlanet())
	{
		// A streamed-out or temporarily missing mesh falls through to the spacecraft-local fallback
		// in FindGroundedSpacecraftDisembarkLocation rather than trapping the player inside.
		return FindGroundedSpacecraftDisembarkLocation(Spacecraft, GroundedPlanet, OutLocation, OutSurfaceFrame);
	}

	return FindLegacySafeDisembarkLocation(Spacecraft, OutLocation);
}

void AJTSCharacter::OnRep_BoardedSpacecraft()
{
	ApplyBoardedPresentation();
}

void AJTSCharacter::OnRep_GameplayPlanet()
{
	bPlanetFrameInitialized = false;
	bPlanetCameraFrameInitialized = false;
	SetGameplayPlanet(GameplayPlanet.Get());
}

void AJTSCharacter::ApplyBoardedPresentation()
{
	const bool bNowBoarded = BoardedSpacecraft != nullptr;
	if (bNowBoarded == bBoardedPresentationApplied)
	{
		return;
	}
	bBoardedPresentationApplied = bNowBoarded;
	if (UCapsuleComponent* const Capsule = GetCapsuleComponent())
	{
		if (bNowBoarded)
		{
			PreviousCapsuleCollisionEnabled = Capsule->GetCollisionEnabled();
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		else
		{
			Capsule->SetCollisionEnabled(PreviousCapsuleCollisionEnabled);
		}
	}
	if (GetMesh() != nullptr)
	{
		if (bNowBoarded) { bPreviousMeshVisible = GetMesh()->IsVisible(); GetMesh()->SetVisibility(false, true); }
		else { GetMesh()->SetVisibility(bPreviousMeshVisible, true); }
	}
	if (DebugVisual != nullptr)
	{
		if (bNowBoarded) { bPreviousDebugVisualVisible = DebugVisual->IsVisible(); DebugVisual->SetVisibility(false, true); }
		else { DebugVisual->SetVisibility(bPreviousDebugVisualVisible, true); }
	}
	if (UCharacterMovementComponent* const MovementComponent = GetCharacterMovement())
	{
		if (bNowBoarded) { MovementComponent->StopMovementImmediately(); MovementComponent->DisableMovement(); }
		else { MovementComponent->SetMovementMode(MOVE_Falling); ApplyProgressionMovementSpeed(); }
	}
}

void AJTSCharacter::UpdateAimCamera(float DeltaSeconds)
{
	const bool bWantsAim = IsValid(RangedWeaponComponent)
		&& RangedWeaponComponent->IsAiming()
		&& RangedWeaponComponent->HasActiveRangedWeapon();
	const float TargetAlpha = bWantsAim ? 1.0f : 0.0f;
	if (!IsRealPlanetGameplayActive() && !WantsContinuousViewFacing())
	{
		// Free look leaves the capsule where the last step put it.
		bUseControllerRotationYaw = false;
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->bOrientRotationToMovement = true;
		}
	}
	AimCameraAlpha = FMath::FInterpTo(AimCameraAlpha, TargetAlpha, DeltaSeconds, FMath::Max(1.0f, AimCameraInterpSpeed));

	if (CameraBoom != nullptr)
	{
		const FVector BaseOffset = FVector(ThirdPersonShoulderOffset.X, ThirdPersonShoulderOffset.Y, 0.0f);
		const FVector AdsOffset = FVector(AimShoulderOffset.X, AimShoulderOffset.Y, 0.0f);
		CameraBoom->SocketOffset = bFirstPersonView ? FVector::ZeroVector : FMath::Lerp(BaseOffset, AdsOffset, AimCameraAlpha);
		CameraBoom->TargetArmLength = bFirstPersonView
			? 0.0f
			: FMath::Lerp(CurrentThirdPersonCameraArmLength, AimThirdPersonArmLength, AimCameraAlpha);
	}
	if (FollowCamera != nullptr)
	{
		const float BaseFOV = bFirstPersonView ? FirstPersonFOV : ThirdPersonFOV;
		const float ActiveAimFOV = IsValid(RangedWeaponComponent) ? RangedWeaponComponent->GetActiveAimFOV() : AimFOV;
		FollowCamera->SetFieldOfView(FMath::Lerp(BaseFOV, ActiveAimFOV, AimCameraAlpha));
	}
	if (IsValid(WeaponVisualComponent))
	{
		WeaponVisualComponent->SetAimAlpha(AimCameraAlpha);
	}
}

void AJTSCharacter::PlayUnarmedPunchPresentation(bool bUseLeftPunch, bool bIsComboContinuation)
{
	UAnimSequenceBase* const SelectedAnimation = bUseLeftPunch
		? UnarmedPunchLeftAnimation.Get()
		: UnarmedPunchRightAnimation.Get();
	if (!IsValid(SelectedAnimation) || GetMesh() == nullptr)
	{
		return;
	}

	UAnimInstance* const AnimInstance = GetMesh()->GetAnimInstance();
	if (!IsValid(AnimInstance))
	{
		return;
	}

	const float BlendInTime = bIsComboContinuation
		? FMath::Min(UnarmedPunchBlendInTime, 0.025f)
		: UnarmedPunchBlendInTime;
	ActiveUnarmedPunchMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
		SelectedAnimation,
		TEXT("UpperBody"),
		BlendInTime,
		UnarmedPunchBlendOutTime,
		FMath::Max(0.1f, UnarmedPunchPlayRate));
}

void AJTSCharacter::StopUnarmedPunchPresentation()
{
	UAnimInstance* const AnimInstance = GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr;
	if (!IsValid(AnimInstance))
	{
		ActiveUnarmedPunchMontage = nullptr;
		return;
	}

	if (IsValid(ActiveUnarmedPunchMontage))
	{
		AnimInstance->Montage_Stop(UnarmedPunchBlendOutTime, ActiveUnarmedPunchMontage);
	}
	ActiveUnarmedPunchMontage = nullptr;
}

void AJTSCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BindPlayerState();
}

void AJTSCharacter::HandlePlayerStateNetworkChanged()
{
	ApplyAvatarColor();
	ApplyProgressionMovementSpeed();
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->RefreshCapacityFromProgression();
	}
}

void AJTSCharacter::ApplyAvatarColor()
{
	const AJTSPlayerState* const State = GetPlayerState<AJTSPlayerState>();
	if (State != nullptr && DebugVisual != nullptr)
	{
		const FLinearColor AvatarColor = State->GetAvatarLinearColor();
		DebugVisual->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(AvatarColor.R, AvatarColor.G, AvatarColor.B));
	}
}

void AJTSCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSCharacter, BoardedSpacecraft);
	DOREPLIFETIME(AJTSCharacter, GameplayPlanet);
	DOREPLIFETIME_CONDITION(AJTSCharacter, ReplicatedAimPitch, COND_SkipOwner);
}

bool AJTSCharacter::FindGroundedSpacecraftDisembarkLocation(
	AJTSSpacecraftActor* Spacecraft,
	AJTSPlanetAnchor* Planet,
	FVector& OutLocation,
	FJTSPlanetSurfaceFrame* OutSurfaceFrame) const
{
	if (!IsValid(Spacecraft) || !IsValid(Planet) || !IsValid(GetCapsuleComponent()))
	{
		return false;
	}

	const FTransform ShipSurfaceTransform = Spacecraft->GetActorTransform();
	const FVector SurfaceUp = ShipSurfaceTransform.GetUnitAxis(EAxis::Z).GetSafeNormal();
	FVector SurfaceForward = FVector::VectorPlaneProject(
		ShipSurfaceTransform.GetUnitAxis(EAxis::X),
		SurfaceUp).GetSafeNormal();
	if (SurfaceForward.IsNearlyZero())
	{
		SurfaceForward = Planet->ProjectDirectionToSurfaceTangent(
			Spacecraft->GetActorForwardVector(),
			Spacecraft->GetActorLocation());
	}
	if (SurfaceUp.IsNearlyZero() || SurfaceForward.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("Grounded spacecraft %s has an invalid local surface frame for disembark."), *Spacecraft->GetName());
		return false;
	}

	FVector SurfaceRight = FVector::CrossProduct(SurfaceUp, SurfaceForward).GetSafeNormal();
	if (SurfaceRight.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("Grounded spacecraft %s could not construct a local surface right vector."), *Spacecraft->GetName());
		return false;
	}
	SurfaceForward = FVector::CrossProduct(SurfaceRight, SurfaceUp).GetSafeNormal();

	const FVector ShipLocation = Spacecraft->GetActorLocation();
	FVector RequestedExitLocation = ShipLocation;
	if (USceneComponent* const ExitPoint = Spacecraft->GetExitPoint())
	{
		RequestedExitLocation = ExitPoint->GetComponentLocation();
	}

	FVector ExitDirection = FVector::VectorPlaneProject(RequestedExitLocation - ShipLocation, SurfaceUp).GetSafeNormal();
	if (ExitDirection.IsNearlyZero())
	{
		// The current ship exit point is authored on its negative local-right side.
		ExitDirection = -SurfaceRight;
	}

	const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector RequestedTangentOffset = FVector::VectorPlaneProject(RequestedExitLocation - ShipLocation, SurfaceUp);
	const float RequestedDistance = FMath::Max(0.0f, FVector::DotProduct(RequestedTangentOffset, ExitDirection));
	const float StartDistance = FMath::Max(
		Spacecraft->GetExteriorHullSupportDistance(ExitDirection) + CapsuleRadius + 32.0f,
		RequestedDistance);
	const float RequestedHeight = FMath::Abs(FVector::DotProduct(RequestedExitLocation - ShipLocation, SurfaceUp));
	const float SurfaceHeightOffset = FMath::Max(
		Spacecraft->GetExteriorHullSupportDistance(SurfaceUp) + CapsuleHalfHeight + 80.0f,
		RequestedHeight + CapsuleHalfHeight + 80.0f);

	TArray<FVector> CandidateDirections;
	auto AddCandidateDirection = [&CandidateDirections](const FVector& Direction)
	{
		const FVector SafeDirection = Direction.GetSafeNormal();
		if (SafeDirection.IsNearlyZero()
			|| CandidateDirections.ContainsByPredicate([&SafeDirection](const FVector& ExistingDirection)
			{
				return FVector::DotProduct(ExistingDirection, SafeDirection) > 0.999f;
			}))
		{
			return;
		}
		CandidateDirections.Add(SafeDirection);
	};

	AddCandidateDirection(ExitDirection);
	AddCandidateDirection(SurfaceRight);
	AddCandidateDirection(-SurfaceRight);
	AddCandidateDirection(SurfaceForward);
	AddCandidateDirection(-SurfaceForward);
	AddCandidateDirection(SurfaceRight + SurfaceForward);
	AddCandidateDirection(SurfaceRight - SurfaceForward);
	AddCandidateDirection(-SurfaceRight + SurfaceForward);
	AddCandidateDirection(-SurfaceRight - SurfaceForward);

	constexpr int32 SearchDistanceRings = 3;
	constexpr float SearchDistanceStep = 140.0f;
	for (int32 DistanceRing = 0; DistanceRing < SearchDistanceRings; ++DistanceRing)
	{
		for (const FVector& CandidateDirection : CandidateDirections)
		{
			const float CandidateDistance = FMath::Max(
				StartDistance + SearchDistanceStep * static_cast<float>(DistanceRing),
				Spacecraft->GetExteriorHullSupportDistance(CandidateDirection) + CapsuleRadius + 32.0f);
			const FVector CandidateReferenceLocation = ShipLocation
				+ CandidateDirection * CandidateDistance
				+ SurfaceUp * SurfaceHeightOffset;

			FJTSPlanetSurfaceFrame CandidateSurfaceFrame;
			if (!Planet->GetSurfaceFrameAt(CandidateReferenceLocation, SurfaceForward, CandidateSurfaceFrame)
				|| CandidateSurfaceFrame.Up.IsNearlyZero())
			{
				continue;
			}

			const FVector GravityUp = Planet->GetRadialUpVector(CandidateSurfaceFrame.Location).GetSafeNormal();
			const float Alignment = FVector::DotProduct(GravityUp, CandidateSurfaceFrame.Up);
			if (Alignment < GetCharacterMovement()->GetWalkableFloorZ()) continue;
			const FQuat CapsuleRotation = FRotationMatrix::MakeFromXZ(SurfaceForward, GravityUp).ToQuat();
			const FVector CandidateLocation = CandidateSurfaceFrame.Location
				+ CandidateSurfaceFrame.Up * (GetCapsuleSupportDistanceAlongDirection(CandidateSurfaceFrame.Up, CapsuleRotation)
					+ PlanetSurfaceSnapClearance);
			if (!IsDisembarkLocationClear(CandidateLocation, CapsuleRotation)) continue;
			OutLocation = CandidateLocation;
			if (OutSurfaceFrame != nullptr)
			{
				*OutSurfaceFrame = CandidateSurfaceFrame;
			}
			return true;
		}
	}

	// Search clear space above each side if every surface candidate is obstructed.
	// Falling settles the capsule; never materialize it inside an obstacle.
	const float FallbackHeight = FMath::Max(
		CapsuleHalfHeight + PlanetSurfaceSnapClearance,
		Spacecraft->GetExteriorHullSupportDistance(SurfaceUp) + CapsuleHalfHeight + 16.0f);
	for (int32 HeightStep = 0; HeightStep < 4; ++HeightStep)
	{
		for (const FVector& Direction : CandidateDirections)
		{
			const float Distance = FMath::Max(StartDistance, Spacecraft->GetExteriorHullSupportDistance(Direction) + CapsuleRadius + 32.0f);
			const FVector Candidate = ShipLocation + Direction * Distance + SurfaceUp * (FallbackHeight + HeightStep * CapsuleHalfHeight);
			const FVector GravityUp = Planet->GetRadialUpVector(Candidate).GetSafeNormal();
			const FQuat Rotation = FRotationMatrix::MakeFromXZ(SurfaceForward, GravityUp).ToQuat();
			if (!IsDisembarkLocationClear(Candidate, Rotation)) continue;
			OutLocation = Candidate;
			if (OutSurfaceFrame != nullptr)
			{
				OutSurfaceFrame->Location = Candidate - GravityUp * CapsuleHalfHeight;
				OutSurfaceFrame->Up = GravityUp;
				OutSurfaceFrame->Forward = Rotation.GetAxisX();
				OutSurfaceFrame->Right = Rotation.GetAxisY();
				OutSurfaceFrame->Transform = FTransform(Rotation, OutSurfaceFrame->Location);
			}
			return true;
		}
	}
	return false;
}

bool AJTSCharacter::IsDisembarkLocationClear(const FVector& Location, const FQuat& Rotation) const
{
	const UWorld* const World = GetWorld();
	const UCapsuleComponent* const Capsule = GetCapsuleComponent();
	if (World == nullptr || Capsule == nullptr) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JTSDisembarkPlacement), false, this);
	return !World->OverlapBlockingTestByChannel(Location, Rotation, Capsule->GetCollisionObjectType(),
		Capsule->GetCollisionShape(), Params, FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
}

bool AJTSCharacter::FindLegacySafeDisembarkLocation(AJTSSpacecraftActor* Spacecraft, FVector& OutLocation) const
{
	if (!IsValid(Spacecraft) || !IsValid(GetCapsuleComponent()))
	{
		return false;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	FVector RequestedExitLocation = Spacecraft->GetActorLocation();
	if (USceneComponent* const ExitPoint = Spacecraft->GetExitPoint())
	{
		RequestedExitLocation = ExitPoint->GetComponentLocation();
	}

	const FVector ShipCenter = Spacecraft->GetActorLocation();

	FVector ExitDirection = RequestedExitLocation - ShipCenter;
	ExitDirection.Z = 0.0f;
	ExitDirection = ExitDirection.GetSafeNormal();
	if (ExitDirection.IsNearlyZero())
	{
		ExitDirection = Spacecraft->GetActorForwardVector();
		ExitDirection.Z = 0.0f;
		ExitDirection = ExitDirection.GetSafeNormal();
	}
	if (ExitDirection.IsNearlyZero())
	{
		ExitDirection = FVector(1.0f, 0.0f, 0.0f);
	}

	const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float MinimumOutsideDistance = Spacecraft->GetExteriorHullSupportDistance(ExitDirection) + CapsuleRadius + 16.0f;
	const FVector RequestedHorizontalOffset = RequestedExitLocation - ShipCenter;
	const float RequestedDistance = FMath::Max(
		0.0f,
		FVector::DotProduct(FVector(RequestedHorizontalOffset.X, RequestedHorizontalOffset.Y, 0.0f), ExitDirection));
	const float StartDistance = FMath::Max(MinimumOutsideDistance, RequestedDistance);

	FCollisionQueryParams GroundTraceParams(SCENE_QUERY_STAT(JTSBoardingExitGroundTrace), false, this);
	GroundTraceParams.AddIgnoredActor(this);
	GroundTraceParams.AddIgnoredActor(Spacecraft);

	FVector FallbackLocation = ShipCenter + ExitDirection * StartDistance
		+ FVector(0.0f, 0.0f, FMath::Max(CapsuleHalfHeight + 4.0f, RequestedExitLocation.Z - ShipCenter.Z));
	constexpr int32 MaxExitSearchAttempts = 16;
	constexpr float ExitSearchStep = 75.0f;
	for (int32 AttemptIndex = 0; AttemptIndex < MaxExitSearchAttempts; ++AttemptIndex)
	{
		const float CandidateDistance = StartDistance + (ExitSearchStep * static_cast<float>(AttemptIndex));
		const FVector CandidateHorizontal = ShipCenter + ExitDirection * CandidateDistance;
		FallbackLocation = CandidateHorizontal
			+ FVector(0.0f, 0.0f, FMath::Max(CapsuleHalfHeight + 4.0f, RequestedExitLocation.Z - ShipCenter.Z));
		const FVector TraceStart = CandidateHorizontal + FVector(0.0f, 0.0f, 1000.0f);
		const FVector TraceEnd = CandidateHorizontal - FVector(0.0f, 0.0f, 2000.0f);

		FHitResult GroundHit;
		if (!World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, GroundTraceParams)
			|| !GroundHit.bBlockingHit)
		{
			continue;
		}

		const FVector CandidateLocation = FVector(
			CandidateHorizontal.X,
			CandidateHorizontal.Y,
			GroundHit.ImpactPoint.Z + CapsuleHalfHeight + 4.0f);
		if (!IsDisembarkLocationClear(CandidateLocation, FQuat::Identity)) continue;
		OutLocation = CandidateLocation;
		return true;
	}

	// No terrain collision (for example, a streaming gap) is still not a reason to keep the player
	// inside. Spawn beside the exit at the spacecraft's local height and let normal falling resolve it.
	OutLocation = FallbackLocation;
	return IsDisembarkLocationClear(OutLocation, FQuat::Identity);
}

void AJTSCharacter::HandleGameplayPhaseChanged(EJTSGameplayPhase NewGameplayPhase)
{
	// Boarding is deliberately available through the same hold-F affordance on Earth and
	// on a ready planetary surface. Do not cancel a just-started hold merely because the
	// replicated SpaceWorld phase arrived a frame after the player did.
	const bool bBoardingSupportedInCurrentWorld = NewGameplayPhase == EJTSGameplayPhase::EarthCollection
		|| IsSpaceWorldSurfaceGameplayActive();
	if (!bBoardingSupportedInCurrentWorld)
	{
		bInteractKeyHeld = false;
		CancelBoardingHold();
	}

	if (NewGameplayPhase != EJTSGameplayPhase::MoonExploration)
	{
		if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetController()))
		{
			PlayerController->CloseMoonShop();
		}
	}
}

void AJTSCharacter::HandleHealthDeath(AController* InstigatorController, AActor* DamageCauser)
{
	static_cast<void>(InstigatorController);
	static_cast<void>(DamageCauser);

	if (HasAuthority())
	{
		if (AJTSPlayerState* const State = GetPlayerState<AJTSPlayerState>())
		{
			State->SetExpeditionStatus(EJTSPlayerExpeditionStatus::Dead);
		}
	}

	if (UWorld* const World = GetWorld())
	{
		if (AJTSSpaceWorldGameMode* const SpaceWorldGameMode = World->GetAuthGameMode<AJTSSpaceWorldGameMode>())
		{
			SpaceWorldGameMode->HandlePlayerCharacterDeath(this);
		}
	}
}
