// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSCharacter.h"

#include "Camera/CameraComponent.h"
#include "CollisionShape.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
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
#include "space/Components/JTSPlanetGravityComponent.h"
#include "space/Interaction/InteractionComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "UObject/ConstructorHelpers.h"

AJTSCharacter::AJTSCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

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
	MovementComponent->SetGravityDirection(FVector::DownVector);

	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
	CarryComponent = CreateDefaultSubobject<UJTSCarryComponent>(TEXT("CarryComponent"));
	PlanetGravityComponent = CreateDefaultSubobject<UJTSPlanetGravityComponent>(TEXT("PlanetGravityComponent"));
	MovementComponent->AddTickPrerequisiteComponent(PlanetGravityComponent);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->TargetArmLength = 420.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 60.0f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;

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

	BindGameState();
	UE_LOG(LogTemp, Log, TEXT("Jump to Space character initialized."));

	RegisterInputMappingContext();
}

void AJTSCharacter::PossessedBy(AController* NewController)
{
	UnregisterInputMappingContext();
	Super::PossessedBy(NewController);

	RegisterInputMappingContext();
}

void AJTSCharacter::OnRep_Controller()
{
	UnregisterInputMappingContext();
	Super::OnRep_Controller();

	RegisterInputMappingContext();
}

void AJTSCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bInteractKeyHeld = false;
	CancelBoardingHold();
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

	MoveForwardAction->ValueType = EInputActionValueType::Axis1D;
	MoveRightAction->ValueType = EInputActionValueType::Axis1D;
	LookYawAction->ValueType = EInputActionValueType::Axis1D;
	LookPitchAction->ValueType = EInputActionValueType::Axis1D;
	JumpAction->ValueType = EInputActionValueType::Boolean;
	SprintAction->ValueType = EInputActionValueType::Boolean;
	InteractAction->ValueType = EInputActionValueType::Boolean;

	InputMappingContext->MapKey(MoveForwardAction, EKeys::W);
	InputMappingContext->MapKey(MoveRightAction, EKeys::D);
	InputMappingContext->MapKey(LookYawAction, EKeys::MouseX);
	InputMappingContext->MapKey(LookPitchAction, EKeys::MouseY);
	InputMappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	InputMappingContext->MapKey(SprintAction, EKeys::LeftShift);
	InputMappingContext->MapKey(InteractAction, EKeys::E);

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
	if (!IsBoarded())
	{
		AddControllerYawInput(Value.Get<float>());
	}
}

void AJTSCharacter::LookPitch(const FInputActionValue& Value)
{
	if (!IsBoarded())
	{
		AddControllerPitchInput(Value.Get<float>());
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
		Jump();
	}
}

void AJTSCharacter::HandleInteractStarted(const FInputActionValue& Value)
{
	// TODO(FakeMoon): Re-evaluate camera-ray versus flat-world trajectory for future long-range hitscan/projectiles.
	bInteractKeyHeld = true;

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
	AJTSSpacecraftActor* const Spacecraft = NearbySpacecraft.Get();
	if (bEarthCollectionActive && IsValid(Spacecraft) && Spacecraft->IsPawnInBoardingRange(this))
	{
		BeginBoardingHold();
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

void AJTSCharacter::BeginBoardingHold()
{
	if (bBoardingHoldActive || IsBoarded() || !BoundGameState.IsValid() || !BoundGameState->IsEarthCollectionActive())
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
	if (!BoundGameState.IsValid() || !BoundGameState->IsEarthCollectionActive() || !IsValid(Spacecraft) || !Spacecraft->IsPawnInBoardingRange(this))
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
		if (FindSafeDisembarkLocation(Spacecraft, DisembarkLocation))
		{
			SetActorLocation(DisembarkLocation, false, nullptr, ETeleportType::TeleportPhysics);
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

bool AJTSCharacter::FindSafeDisembarkLocation(AJTSSpacecraftActor* Spacecraft, FVector& OutLocation) const
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
}
