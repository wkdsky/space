// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSCharacter.h"

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
#include "space/Components/JTSCarryComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSMeleeComponent.h"
#include "space/Components/JTSPlayerEquipmentComponent.h"
#include "space/Components/JTSPlanetGravityComponent.h"
#include "space/Interaction/InteractionComponent.h"
#include "space/Player/JTSPlayerController.h"
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
	MovementComponent->AirControl = 0.35f;
	MovementComponent->MaxWalkSpeed = WalkingSpeed;
	MovementComponent->MinAnalogWalkSpeed = 20.0f;
	MovementComponent->BrakingDecelerationWalking = 2000.0f;
	MovementComponent->GravityScale = 1.0f;

	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
	CarryComponent = CreateDefaultSubobject<UJTSCarryComponent>(TEXT("CarryComponent"));
	EquipmentComponent = CreateDefaultSubobject<UJTSPlayerEquipmentComponent>(TEXT("EquipmentComponent"));
	HealthComponent = CreateDefaultSubobject<UJTSHealthComponent>(TEXT("HealthComponent"));
	MeleeComponent = CreateDefaultSubobject<UJTSMeleeComponent>(TEXT("MeleeComponent"));
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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DebugMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DebugMeshAsset.Succeeded())
	{
		DebugVisual->SetStaticMesh(DebugMeshAsset.Object);
	}
}

UJTSCarryComponent* AJTSCharacter::GetCarryComponent() const
{
	return CarryComponent.Get();
}

UJTSPlayerEquipmentComponent* AJTSCharacter::GetEquipmentComponent() const
{
	return EquipmentComponent.Get();
}

UJTSHealthComponent* AJTSCharacter::GetHealthComponent() const
{
	return HealthComponent.Get();
}

bool AJTSCharacter::IsFirstPersonView() const
{
	return bFirstPersonView;
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

AJTSPlanetAnchor* AJTSCharacter::GetGameplayPlanet() const
{
	return GameplayPlanet.Get();
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
	if (SurfaceUp.IsNearlyZero())
	{
		return false;
	}

	const FVector GravityUp = GetDesiredPlanetUp();
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

	const FVector SurfaceLocation = SurfaceFrame.Location
		+ SurfaceFrame.Up * (GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + PlanetSurfaceSnapClearance);
	FCollisionQueryParams PlacementParams(SCENE_QUERY_STAT(JTSCharacterPlanetSurfacePlacement), false, this);
	PlacementParams.AddIgnoredActor(this);
	if (IsValid(AdditionalIgnoredActor))
	{
		PlacementParams.AddIgnoredActor(AdditionalIgnoredActor);
	}

	if (World->OverlapBlockingTestByChannel(
		SurfaceLocation,
		SurfaceFrame.Transform.GetRotation(),
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

float AJTSCharacter::GetAimPitch() const
{
	return AimPitch;
}

int32 AJTSCharacter::GetEquipmentHoldSlotIndex() const
{
	if (bEquipmentHoldCompleted || !IsValid(EquipmentComponent)
		|| !EquipmentComponent->GetEquipmentSlots().IsValidIndex(HeldEquipmentSlotIndex)
		|| EquipmentComponent->GetEquipmentSlot(HeldEquipmentSlotIndex) == EJTSEquipmentType::None)
	{
		return INDEX_NONE;
	}

	return HeldEquipmentSlotIndex;
}

float AJTSCharacter::GetEquipmentHoldProgress() const
{
	if (GetEquipmentHoldSlotIndex() == INDEX_NONE)
	{
		return 0.0f;
	}

	const UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return 0.0f;
	}

	return FMath::Clamp(
		static_cast<float>((static_cast<double>(World->GetTimeSeconds()) - EquipmentHoldStartTime)
			/ static_cast<double>(FMath::Max(0.1f, EquipmentHoldToDropDuration))),
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
		static_cast<float>((static_cast<double>(World->GetTimeSeconds()) - BoardingHoldStartTime) / static_cast<double>(BoardingHoldDuration)),
		0.0f,
		1.0f);
}

float AJTSCharacter::GetBoardingHoldRemainingTime() const
{
	if (!bBoardingHoldActive)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, BoardingHoldDuration * (1.0f - GetBoardingHoldProgress()));
}

bool AJTSCharacter::IsBoarded() const
{
	return BoardedSpacecraft.IsValid();
}

AJTSSpacecraftActor* AJTSCharacter::GetNearbySpacecraft() const
{
	return NearbySpacecraft.Get();
}

AJTSSpacecraftActor* AJTSCharacter::GetBoardedSpacecraft() const
{
	return BoardedSpacecraft.Get();
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
	if (!IsValid(Spacecraft) || IsBoarded() || !IsValid(GetCapsuleComponent()))
	{
		return false;
	}

	USceneComponent* const BoardingPoint = Spacecraft->GetBoardingPoint();
	if (!IsValid(BoardingPoint))
	{
		return false;
	}

	CancelBoardingHold();
	CancelEquipmentSlotHold();
	bInteractKeyHeld = false;
	NearbySpacecraft = Spacecraft;
	BoardedSpacecraft = Spacecraft;

	PreviousCapsuleCollisionEnabled = GetCapsuleComponent()->GetCollisionEnabled();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (GetMesh() != nullptr)
	{
		bPreviousMeshVisible = GetMesh()->IsVisible();
		GetMesh()->SetVisibility(false, true);
	}
	if (DebugVisual != nullptr)
	{
		bPreviousDebugVisualVisible = DebugVisual->IsVisible();
		DebugVisual->SetVisibility(false, true);
	}

	if (UCharacterMovementComponent* const MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	if (!AttachToComponent(BoardingPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale))
	{
		RestoreAfterBoarding(Spacecraft, false);
		return false;
	}

	SetActorRelativeLocation(FVector::ZeroVector);
	return true;
}

void AJTSCharacter::ExitBoardedState(AJTSSpacecraftActor* Spacecraft)
{
	if (BoardedSpacecraft.Get() != Spacecraft)
	{
		return;
	}

	RestoreAfterBoarding(Spacecraft, true);
}

void AJTSCharacter::HandleSpacecraftInvalidated(AJTSSpacecraftActor* Spacecraft)
{
	if (BoardedSpacecraft.Get() == Spacecraft)
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

	if (IsValid(HealthComponent))
	{
		HealthComponent->SetMaxHealth(PlayerMaxHealth, true);
	}

	ApplyCameraView();
	BindGameState();
	UE_LOG(LogTemp, Log, TEXT("Jump to Space character initialized."));

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

	const float ControllerPitch = bUsingRealPlanetFrame
		? PlanetCameraPitch
		: (Controller != nullptr ? FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch) : 0.0f);
	const float MinimumAimPitch = FMath::Min(AimPitchMin, AimPitchMax);
	const float MaximumAimPitch = FMath::Max(AimPitchMin, AimPitchMax);
	AimPitch = FMath::Clamp(ControllerPitch, MinimumAimPitch, MaximumAimPitch);
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
	CancelEquipmentSlotHold();
	UnbindGameState();
	UnregisterInputMappingContext();

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
	EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &AJTSCharacter::HandleInteractCompleted);
	EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Canceled, this, &AJTSCharacter::HandleInteractCanceled);
	EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AJTSCharacter::HandleAttackStarted);
	EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Completed, this, &AJTSCharacter::HandleAttackReleased);
	EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Canceled, this, &AJTSCharacter::HandleAttackReleased);
	EnhancedInputComponent->BindAction(ToggleCameraAction, ETriggerEvent::Started, this, &AJTSCharacter::HandleToggleCameraStarted);
	if (EquipmentSlotActions.Num() == 4)
	{
		EnhancedInputComponent->BindAction(EquipmentSlotActions[0], ETriggerEvent::Started, this, &AJTSCharacter::HandleEquipmentSlotOneStarted);
		EnhancedInputComponent->BindAction(EquipmentSlotActions[1], ETriggerEvent::Started, this, &AJTSCharacter::HandleEquipmentSlotTwoStarted);
		EnhancedInputComponent->BindAction(EquipmentSlotActions[2], ETriggerEvent::Started, this, &AJTSCharacter::HandleEquipmentSlotThreeStarted);
		EnhancedInputComponent->BindAction(EquipmentSlotActions[3], ETriggerEvent::Started, this, &AJTSCharacter::HandleEquipmentSlotFourStarted);
		EnhancedInputComponent->BindAction(EquipmentSlotActions[0], ETriggerEvent::Completed, this, &AJTSCharacter::HandleEquipmentSlotOneReleased);
		EnhancedInputComponent->BindAction(EquipmentSlotActions[1], ETriggerEvent::Completed, this, &AJTSCharacter::HandleEquipmentSlotTwoReleased);
		EnhancedInputComponent->BindAction(EquipmentSlotActions[2], ETriggerEvent::Completed, this, &AJTSCharacter::HandleEquipmentSlotThreeReleased);
		EnhancedInputComponent->BindAction(EquipmentSlotActions[3], ETriggerEvent::Completed, this, &AJTSCharacter::HandleEquipmentSlotFourReleased);
		EnhancedInputComponent->BindAction(EquipmentSlotActions[0], ETriggerEvent::Canceled, this, &AJTSCharacter::HandleEquipmentSlotOneReleased);
		EnhancedInputComponent->BindAction(EquipmentSlotActions[1], ETriggerEvent::Canceled, this, &AJTSCharacter::HandleEquipmentSlotTwoReleased);
		EnhancedInputComponent->BindAction(EquipmentSlotActions[2], ETriggerEvent::Canceled, this, &AJTSCharacter::HandleEquipmentSlotThreeReleased);
		EnhancedInputComponent->BindAction(EquipmentSlotActions[3], ETriggerEvent::Canceled, this, &AJTSCharacter::HandleEquipmentSlotFourReleased);
	}

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
	AttackAction = NewObject<UInputAction>(this, TEXT("AttackAction"), RF_Transient);
	ToggleCameraAction = NewObject<UInputAction>(this, TEXT("ToggleCameraAction"), RF_Transient);
	EquipmentSlotActions.Reset();
	for (int32 SlotIndex = 0; SlotIndex < 4; ++SlotIndex)
	{
		EquipmentSlotActions.Add(NewObject<UInputAction>(this, *FString::Printf(TEXT("EquipmentSlot%dAction"), SlotIndex + 1), RF_Transient));
	}

	MoveForwardAction->ValueType = EInputActionValueType::Axis1D;
	MoveRightAction->ValueType = EInputActionValueType::Axis1D;
	LookYawAction->ValueType = EInputActionValueType::Axis1D;
	LookPitchAction->ValueType = EInputActionValueType::Axis1D;
	JumpAction->ValueType = EInputActionValueType::Boolean;
	SprintAction->ValueType = EInputActionValueType::Boolean;
	InteractAction->ValueType = EInputActionValueType::Boolean;
	AttackAction->ValueType = EInputActionValueType::Boolean;
	ToggleCameraAction->ValueType = EInputActionValueType::Boolean;
	for (UInputAction* const EquipmentSlotAction : EquipmentSlotActions)
	{
		EquipmentSlotAction->ValueType = EInputActionValueType::Boolean;
	}

	InputMappingContext->MapKey(MoveForwardAction, EKeys::W);
	InputMappingContext->MapKey(MoveRightAction, EKeys::D);
	InputMappingContext->MapKey(LookYawAction, EKeys::MouseX);
	InputMappingContext->MapKey(LookPitchAction, EKeys::MouseY);
	InputMappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	InputMappingContext->MapKey(SprintAction, EKeys::LeftShift);
	InputMappingContext->MapKey(InteractAction, EKeys::E);
	InputMappingContext->MapKey(AttackAction, EKeys::LeftMouseButton);
	InputMappingContext->MapKey(ToggleCameraAction, EKeys::V);
	if (EquipmentSlotActions.Num() == 4)
	{
		InputMappingContext->MapKey(EquipmentSlotActions[0], EKeys::One);
		InputMappingContext->MapKey(EquipmentSlotActions[1], EKeys::Two);
		InputMappingContext->MapKey(EquipmentSlotActions[2], EKeys::Three);
		InputMappingContext->MapKey(EquipmentSlotActions[3], EKeys::Four);
	}

	auto AddNegatedMapping = [this](UInputAction* Action, const FKey& Key)
	{
		FEnhancedActionKeyMapping& Mapping = InputMappingContext->MapKey(Action, Key);
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(InputMappingContext));
	};

	AddNegatedMapping(MoveForwardAction, EKeys::S);
	AddNegatedMapping(MoveRightAction, EKeys::A);
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
	InputSubsystem->AddMappingContext(InputMappingContext, 0);
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
	if (IsBoarded())
	{
		return;
	}

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

	if (!IsBoarded())
	{
		AddControllerYawInput(Value.Get<float>() * MouseSensitivityX);
	}
}

void AJTSCharacter::LookPitch(const FInputActionValue& Value)
{
	if (IsBoarded())
	{
		return;
	}

	if (IsRealPlanetGameplayActive())
	{
		const float RequestedPitchMin = bFirstPersonView ? FirstPersonViewPitchMin : ThirdPersonViewPitchMin;
		const float RequestedPitchMax = bFirstPersonView ? FirstPersonViewPitchMax : ThirdPersonViewPitchMax;
		// Retain the established mouse-Y direction while keeping pitch in the local gravity-relative frame.
		PlanetCameraPitch = FMath::Clamp(
			PlanetCameraPitch - Value.Get<float>() * MouseSensitivityY,
			FMath::Min(RequestedPitchMin, RequestedPitchMax),
			FMath::Max(RequestedPitchMin, RequestedPitchMax));
		const FVector LocalUp = bPlanetFrameInitialized ? LastPlanetUp : GetDesiredPlanetUp();
		UpdatePlanetCameraFrame(LocalUp, 0.0f);
		return;
	}

	if (!IsBoarded())
	{
		// MouseY is not negated in the Enhanced Input mapping, so retain the established pitch direction here.
		AddControllerPitchInput(-Value.Get<float>() * MouseSensitivityY);
	}
}

void AJTSCharacter::StartSprint(const FInputActionValue& Value)
{
	if (!IsBoarded())
	{
		GetCharacterMovement()->MaxWalkSpeed = SprintingSpeed;
	}
}

void AJTSCharacter::StopSprint(const FInputActionValue& Value)
{
	GetCharacterMovement()->MaxWalkSpeed = WalkingSpeed;
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
	if (IsGameplayInputBlocked())
	{
		return;
	}

	// TODO(FakeMoon): Re-evaluate camera-ray versus flat-world trajectory for future long-range hitscan/projectiles.
	bInteractKeyHeld = true;
	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetController()); PlayerController != nullptr && PlayerController->IsMoonShopOpen())
	{
		bInteractKeyHeld = false;
		PlayerController->CloseMoonShop();
		return;
	}

	if (IsBoarded())
	{
		bInteractKeyHeld = false;
		if (AJTSSpacecraftActor* const Spacecraft = BoardedSpacecraft.Get())
		{
			Spacecraft->TryDisembarkPlayer(this);
		}
		return;
	}

	const bool bEarthCollectionActive = BoundGameState.IsValid() && BoundGameState->IsEarthCollectionActive();
	const bool bMoonExplorationActive = BoundGameState.IsValid() && BoundGameState->IsMoonExploration();
	const bool bSpaceWorldSurfaceActive = IsSpaceWorldSurfaceGameplayActive();
	AJTSSpacecraftActor* const Spacecraft = NearbySpacecraft.Get();
	if ((bEarthCollectionActive || bSpaceWorldSurfaceActive)
		&& IsValid(Spacecraft)
		&& Spacecraft->IsPawnInBoardingRange(this))
	{
		BeginBoardingHold();
		return;
	}
	if (bMoonExplorationActive && !bSpaceWorldSurfaceActive)
	{
		bInteractKeyHeld = false;
		if (InteractionComponent != nullptr)
		{
			if (AActor* const InteractionTarget = InteractionComponent->GetCurrentInteractable())
			{
				if (!InteractionTarget->IsA<AJTSSpacecraftActor>())
				{
					InteractionComponent->TryInteract();
					return;
				}
			}
		}

		if (IsValid(Spacecraft) && Spacecraft->IsPawnInBoardingRange(this))
		{
			if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetController()))
			{
				PlayerController->OpenMoonShop(this);
			}
		}
		return;
	}

	bInteractKeyHeld = false;
	if (InteractionComponent != nullptr)
	{
		InteractionComponent->TryInteract();
	}
}

void AJTSCharacter::HandleInteractCompleted(const FInputActionValue& Value)
{
	bInteractKeyHeld = false;
	CancelBoardingHold();
}

void AJTSCharacter::HandleInteractCanceled(const FInputActionValue& Value)
{
	bInteractKeyHeld = false;
	CancelBoardingHold();
}

void AJTSCharacter::HandleAttackStarted(const FInputActionValue& Value)
{
	if (!CanUseNormalGameplayInput() || !IsValid(MeleeComponent))
	{
		return;
	}
	
	MeleeComponent->AttackPressed();
}

void AJTSCharacter::HandleAttackReleased(const FInputActionValue& Value)
{
	// Release must always be forwarded so a blocked UI or phase transition cannot leave the hold state stuck.
	if (IsValid(MeleeComponent))
	{
		MeleeComponent->AttackReleased();
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

void AJTSCharacter::HandleEquipmentSlotOneStarted(const FInputActionValue& Value)
{
	BeginEquipmentSlotHold(0);
}

void AJTSCharacter::HandleEquipmentSlotTwoStarted(const FInputActionValue& Value)
{
	BeginEquipmentSlotHold(1);
}

void AJTSCharacter::HandleEquipmentSlotThreeStarted(const FInputActionValue& Value)
{
	BeginEquipmentSlotHold(2);
}

void AJTSCharacter::HandleEquipmentSlotFourStarted(const FInputActionValue& Value)
{
	BeginEquipmentSlotHold(3);
}

void AJTSCharacter::HandleEquipmentSlotOneReleased(const FInputActionValue& Value)
{
	EndEquipmentSlotHold(0);
}

void AJTSCharacter::HandleEquipmentSlotTwoReleased(const FInputActionValue& Value)
{
	EndEquipmentSlotHold(1);
}

void AJTSCharacter::HandleEquipmentSlotThreeReleased(const FInputActionValue& Value)
{
	EndEquipmentSlotHold(2);
}

void AJTSCharacter::HandleEquipmentSlotFourReleased(const FInputActionValue& Value)
{
	EndEquipmentSlotHold(3);
}

void AJTSCharacter::BeginEquipmentSlotHold(int32 SlotIndex)
{
	if (!CanUseNormalGameplayInput() || !IsValid(EquipmentComponent)
		|| SlotIndex < 0 || SlotIndex >= EquipmentComponent->GetEquipmentCapacity())
	{
		return;
	}

	CancelEquipmentSlotHold();
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	HeldEquipmentSlotIndex = SlotIndex;
	bEquipmentHoldCompleted = false;
	EquipmentHoldStartTime = static_cast<double>(World->GetTimeSeconds());
	if (EquipmentComponent->GetEquipmentSlot(SlotIndex) == EJTSEquipmentType::None)
	{
		return;
	}
	World->GetTimerManager().SetTimer(
		EquipmentHoldTimerHandle,
		this,
		&AJTSCharacter::CompleteEquipmentSlotHold,
		FMath::Max(0.1f, EquipmentHoldToDropDuration),
		false);
}

void AJTSCharacter::EndEquipmentSlotHold(int32 SlotIndex)
{
	if (HeldEquipmentSlotIndex != SlotIndex)
	{
		return;
	}

	const bool bShouldSelectSlot = !bEquipmentHoldCompleted;
	CancelEquipmentSlotHold();
	if (bShouldSelectSlot && CanUseNormalGameplayInput() && IsValid(EquipmentComponent))
	{
		EquipmentComponent->SelectEquipmentSlot(SlotIndex);
	}
}

void AJTSCharacter::CancelEquipmentSlotHold()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EquipmentHoldTimerHandle);
	}

	HeldEquipmentSlotIndex = INDEX_NONE;
	EquipmentHoldStartTime = 0.0;
	bEquipmentHoldCompleted = false;
}

void AJTSCharacter::CompleteEquipmentSlotHold()
{
	if (!CanUseNormalGameplayInput() || !IsValid(EquipmentComponent) || HeldEquipmentSlotIndex == INDEX_NONE)
	{
		CancelEquipmentSlotHold();
		return;
	}

	const int32 SlotIndex = HeldEquipmentSlotIndex;
	bEquipmentHoldCompleted = EquipmentComponent->DropEquippedItemAtSlot(SlotIndex);
	HeldEquipmentSlotIndex = INDEX_NONE;
	EquipmentHoldStartTime = 0.0;
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EquipmentHoldTimerHandle);
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
	const AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
	return IsValid(Planet) && IsValid(Manager) && Manager->IsPlanetGameplayActive(Planet);
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
	UpdatePlanetCameraFrame(DesiredUp, DeltaSeconds);
	UpdatePlanetBodyOrientation(DesiredUp, DeltaSeconds);

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
		PlanetBodyForward = GetStablePlanetTangent(LastPlanetUp, PlanetCameraTangentForward);
	}
	else if (const UCharacterMovementComponent* const MovementComponent = GetCharacterMovement())
	{
		const FVector TangentVelocity = FVector::VectorPlaneProject(MovementComponent->Velocity, LastPlanetUp);
		if (TangentVelocity.SizeSquared() > FMath::Square(5.0f))
		{
			const FVector DesiredForward = TangentVelocity.GetSafeNormal();
			const FQuat BodyTurn = FQuat::FindBetweenNormals(PlanetBodyForward, DesiredForward);
			const float TurnAlpha = FMath::Clamp(DeltaSeconds * PlanetBodyTurnInterpolationSpeed, 0.0f, 1.0f);
			PlanetBodyForward = GetStablePlanetTangent(
				LastPlanetUp,
				FQuat::Slerp(FQuat::Identity, BodyTurn, TurnAlpha).RotateVector(PlanetBodyForward));
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

	if (Controller != nullptr)
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
	return !IsValid(PlayerController) || PlayerController->IsMoonShopOpen() || PlayerController->IsGameMenuOpen();
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

void AJTSCharacter::ApplyCameraView()
{
	ApplyCameraPitchLimits();

	if (CameraPivot == nullptr || CameraBoom == nullptr || FollowCamera == nullptr)
	{
		return;
	}

	CameraPivot->SetRelativeLocation(FVector(0.0f, 0.0f, CameraPivotHeight));
	CameraBoom->SocketOffset = bFirstPersonView
		? FVector::ZeroVector
		: FVector(ThirdPersonShoulderOffset.X, ThirdPersonShoulderOffset.Y, 0.0f);
	CameraBoom->TargetArmLength = bFirstPersonView ? 0.0f : ThirdPersonArmLength;
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
		MovementComponent->bOrientRotationToMovement = true;
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

void AJTSCharacter::BeginBoardingHold()
{
	const bool bCanBeginBoarding = (BoundGameState.IsValid() && BoundGameState->IsEarthCollectionActive())
		|| IsSpaceWorldSurfaceGameplayActive();
	if (bBoardingHoldActive || IsBoarded() || !bCanBeginBoarding)
	{
		return;
	}

	AJTSSpacecraftActor* const Spacecraft = NearbySpacecraft.Get();
	UWorld* const World = GetWorld();
	if (!IsValid(Spacecraft) || World == nullptr || !Spacecraft->IsPawnInBoardingRange(this))
	{
		return;
	}

	bBoardingHoldActive = true;
	BoardingHoldStartTime = static_cast<double>(World->GetTimeSeconds());
	World->GetTimerManager().SetTimer(
		BoardingHoldTimerHandle,
		this,
		&AJTSCharacter::CompleteBoardingHold,
		BoardingHoldDuration,
		false);
}

void AJTSCharacter::CancelBoardingHold()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BoardingHoldTimerHandle);
	}

	bBoardingHoldActive = false;
	BoardingHoldStartTime = 0.0;
}

void AJTSCharacter::CompleteBoardingHold()
{
	if (!bBoardingHoldActive || !bInteractKeyHeld || IsBoarded())
	{
		CancelBoardingHold();
		return;
	}

	AJTSSpacecraftActor* const Spacecraft = NearbySpacecraft.Get();
	const bool bCanCompleteBoarding = (BoundGameState.IsValid() && BoundGameState->IsEarthCollectionActive())
		|| IsSpaceWorldSurfaceGameplayActive();
	if (!bCanCompleteBoarding
		|| !IsValid(Spacecraft)
		|| !Spacecraft->IsPawnInBoardingRange(this))
	{
		CancelBoardingHold();
		return;
	}

	const bool bBoarded = Spacecraft->TryBoardPlayer(this);
	CancelBoardingHold();
	if (!bBoarded)
	{
		bInteractKeyHeld = false;
	}
}

void AJTSCharacter::RestoreAfterBoarding(AJTSSpacecraftActor* Spacecraft, bool bMoveToExitPoint)
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	GetCapsuleComponent()->SetCollisionEnabled(PreviousCapsuleCollisionEnabled);

	if (bMoveToExitPoint)
	{
		FVector DisembarkLocation;
		FJTSPlanetSurfaceFrame DisembarkSurfaceFrame;
		if (FindSafeDisembarkLocation(Spacecraft, DisembarkLocation, &DisembarkSurfaceFrame))
		{
			AJTSPlanetAnchor* const GroundedPlanet = IsValid(Spacecraft) ? Spacecraft->GetGroundedPlanet() : nullptr;
			const bool bOnRealPlanet = IsValid(GroundedPlanet) && GroundedPlanet->HasGameplaySurface();
			FQuat DisembarkRotation = GetActorQuat();
			FVector GravityUp = FVector::UpVector;
			FVector SurfaceForward = DisembarkSurfaceFrame.Forward;
			if (bOnRealPlanet)
			{
				SetGameplayPlanet(GroundedPlanet);
				GravityUp = GetDesiredPlanetUp();
				SurfaceForward = GetStablePlanetTangent(GravityUp, DisembarkSurfaceFrame.Forward);
				DisembarkRotation = FRotationMatrix::MakeFromXZ(SurfaceForward, GravityUp).ToQuat();
			}
			SetActorLocationAndRotation(
				DisembarkLocation,
				DisembarkRotation,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);

			if (bOnRealPlanet)
			{
				LastPlanetUp = GravityUp;
				PlanetBodyForward = SurfaceForward;
				PlanetCameraTangentForward = SurfaceForward;
				LastPlanetCameraUp = GravityUp;
				bPlanetFrameInitialized = true;
				bPlanetCameraFrameInitialized = true;
			}
		}
	}

	if (GetMesh() != nullptr)
	{
		GetMesh()->SetVisibility(bPreviousMeshVisible, true);
	}
	if (DebugVisual != nullptr)
	{
		DebugVisual->SetVisibility(bPreviousDebugVisualVisible, true);
	}
	if (UCharacterMovementComponent* const MovementComponent = GetCharacterMovement())
	{
		MovementComponent->SetMovementMode(MOVE_Walking);
		MovementComponent->MaxWalkSpeed = WalkingSpeed;
	}

	BoardedSpacecraft = nullptr;
	if (NearbySpacecraft.Get() == Spacecraft)
	{
		NearbySpacecraft = nullptr;
	}
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
		if (!GroundedPlanet->HasGameplaySurface())
		{
			UE_LOG(LogTemp, Warning, TEXT("Grounded spacecraft %s has an invalid gameplay planet for disembark."), *Spacecraft->GetName());
			return false;
		}

		return FindGroundedSpacecraftDisembarkLocation(Spacecraft, GroundedPlanet, OutLocation, OutSurfaceFrame);
	}

	return FindLegacySafeDisembarkLocation(Spacecraft, OutLocation);
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

	FVector ShipExtent(180.0f, 180.0f, 90.0f);
	if (const UBoxComponent* const FlightCollision = Spacecraft->FindComponentByClass<UBoxComponent>())
	{
		ShipExtent = FlightCollision->GetScaledBoxExtent();
	}

	auto GetProjectedShipExtent = [&SurfaceForward, &SurfaceRight, &SurfaceUp, &ShipExtent](const FVector& Direction)
	{
		return FMath::Abs(FVector::DotProduct(Direction, SurfaceForward)) * ShipExtent.X
			+ FMath::Abs(FVector::DotProduct(Direction, SurfaceRight)) * ShipExtent.Y
			+ FMath::Abs(FVector::DotProduct(Direction, SurfaceUp)) * ShipExtent.Z;
	};

	const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector RequestedTangentOffset = FVector::VectorPlaneProject(RequestedExitLocation - ShipLocation, SurfaceUp);
	const float RequestedDistance = FMath::Max(0.0f, FVector::DotProduct(RequestedTangentOffset, ExitDirection));
	const float StartDistance = FMath::Max(
		GetProjectedShipExtent(ExitDirection) + CapsuleRadius + 32.0f,
		RequestedDistance);
	const float RequestedHeight = FMath::Abs(FVector::DotProduct(RequestedExitLocation - ShipLocation, SurfaceUp));
	const float SurfaceHeightOffset = FMath::Max(
		ShipExtent.Z + CapsuleHalfHeight + 80.0f,
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
				GetProjectedShipExtent(CandidateDirection) + CapsuleRadius + 32.0f);
			const FVector CandidateReferenceLocation = ShipLocation
				+ CandidateDirection * CandidateDistance
				+ SurfaceUp * SurfaceHeightOffset;

			FVector CandidateLocation;
			FJTSPlanetSurfaceFrame CandidateSurfaceFrame;
			if (!FindSafeCharacterSurfaceLocation(
				Planet,
				CandidateReferenceLocation,
				SurfaceForward,
				nullptr,
				CandidateLocation,
				&CandidateSurfaceFrame))
			{
				continue;
			}

			OutLocation = CandidateLocation;
			if (OutSurfaceFrame != nullptr)
			{
				*OutSurfaceFrame = CandidateSurfaceFrame;
			}
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Grounded spacecraft disembark candidates exhausted for %s on planet %s."),
		*Spacecraft->GetName(), *Planet->GetPlanetId().ToString());
	return false;
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

	FVector ShipCenter = Spacecraft->GetActorLocation();
	FVector ShipExtent(180.0f, 180.0f, 90.0f);
	if (const UStaticMeshComponent* const ShipMesh = Spacecraft->FindComponentByClass<UStaticMeshComponent>())
	{
		const FBoxSphereBounds MeshBounds = ShipMesh->Bounds;
		ShipCenter = MeshBounds.Origin;
		ShipExtent = MeshBounds.BoxExtent;
	}

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

	const float ProjectedShipExtent = FMath::Abs(ExitDirection.X) * ShipExtent.X
		+ FMath::Abs(ExitDirection.Y) * ShipExtent.Y;
	const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float MinimumOutsideDistance = ProjectedShipExtent + CapsuleRadius + 16.0f;
	const FVector RequestedHorizontalOffset = RequestedExitLocation - ShipCenter;
	const float RequestedDistance = FMath::Max(
		0.0f,
		FVector::DotProduct(FVector(RequestedHorizontalOffset.X, RequestedHorizontalOffset.Y, 0.0f), ExitDirection));
	const float StartDistance = FMath::Max(MinimumOutsideDistance, RequestedDistance);

	FCollisionQueryParams GroundTraceParams(SCENE_QUERY_STAT(JTSBoardingExitGroundTrace), false, this);
	GroundTraceParams.AddIgnoredActor(this);
	GroundTraceParams.AddIgnoredActor(Spacecraft);

	FCollisionQueryParams PlacementParams(SCENE_QUERY_STAT(JTSBoardingExitPlacement), false, this);
	PlacementParams.AddIgnoredActor(this);
	const FCollisionShape CharacterShape = GetCapsuleComponent()->GetCollisionShape();
	const FQuat CharacterRotation = GetCapsuleComponent()->GetComponentQuat();

	FVector FarthestGroundLocation = FVector::ZeroVector;
	bool bHasGroundLocation = false;
	constexpr int32 MaxExitSearchAttempts = 16;
	constexpr float ExitSearchStep = 75.0f;
	for (int32 AttemptIndex = 0; AttemptIndex < MaxExitSearchAttempts; ++AttemptIndex)
	{
		const float CandidateDistance = StartDistance + (ExitSearchStep * static_cast<float>(AttemptIndex));
		const FVector CandidateHorizontal = ShipCenter + ExitDirection * CandidateDistance;
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
		FarthestGroundLocation = CandidateLocation;
		bHasGroundLocation = true;
		if (World->OverlapBlockingTestByChannel(
			CandidateLocation,
			CharacterRotation,
			ECC_Pawn,
			CharacterShape,
			PlacementParams))
		{
			continue;
		}

		OutLocation = CandidateLocation;
		return true;
	}

	if (bHasGroundLocation)
	{
		OutLocation = FarthestGroundLocation;
		return true;
	}

	return false;
}

void AJTSCharacter::HandleGameplayPhaseChanged(EJTSGameplayPhase NewGameplayPhase)
{
	if (NewGameplayPhase != EJTSGameplayPhase::EarthCollection)
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
