// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Ships/JTSSpacecraftActor.h"

#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Math/RotationMatrix.h"
#include "Materials/MaterialInterface.h"
#include "space/Components/JTSCarryComponent.h"
#include "space/Components/JTSSpacecraftFlightMovementComponent.h"
#include "space/Core/JTSGameInstance.h"
#include "space/Components/JTSMoonWrappedActorComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetLandingManager.h"
#include "space/World/JTSPlanetLandingSite.h"
#include "space/World/JTSPlanetSurfaceAnchor.h"
#include "space/World/JTSSpaceWorldManager.h"
#include "TimerManager.h"
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

		case EJTSResourceType::Organic:
			return TEXT("Organic");

		default:
			return TEXT("Unknown");
		}
	}

	bool IsSupportedResourceType(EJTSResourceType ResourceType)
	{
		switch (ResourceType)
		{
		case EJTSResourceType::Fuel:
		case EJTSResourceType::Water:
		case EJTSResourceType::Food:
		case EJTSResourceType::Rock:
		case EJTSResourceType::Ore:
		case EJTSResourceType::Organic:
			return true;

		default:
			return false;
		}
	}
}

AJTSSpacecraftActor::AJTSSpacecraftActor()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	FlightCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("FlightCollision"));
	FlightCollision->SetupAttachment(SceneRoot);
	FlightCollision->InitBoxExtent(FVector(150.0f, 100.0f, 75.0f));
	FlightCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	FlightCollision->SetCollisionObjectType(ECC_Pawn);
	FlightCollision->SetCollisionResponseToAllChannels(ECR_Block);
	FlightCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	FlightCollision->SetGenerateOverlapEvents(false);
	FlightCollision->SetCanEverAffectNavigation(false);

	SpacecraftMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpacecraftMesh"));
	SpacecraftMesh->SetupAttachment(SceneRoot);
	SpacecraftMesh->SetRelativeScale3D(FVector(3.0f, 2.0f, 1.5f));
	SpacecraftMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SpacecraftMesh->SetCollisionResponseToAllChannels(ECR_Block);
	SpacecraftMesh->SetGenerateOverlapEvents(true);
	SpacecraftMesh->SetCanEverAffectNavigation(false);
	SpacecraftMesh->SetSimulatePhysics(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		SpacecraftMesh->SetStaticMesh(CubeMeshAsset.Object);
	}

	FlightMovementComponent = CreateDefaultSubobject<UJTSSpacecraftFlightMovementComponent>(TEXT("FlightMovementComponent"));
	FlightMovementComponent->SetUpdatedComponent(SceneRoot);

	GroundProbeComponent = CreateDefaultSubobject<UJTSSpacecraftGroundProbeComponent>(TEXT("GroundProbeComponent"));

	// Retain the legacy subobject name so existing Blueprint component templates keep their camera tuning.
	FlightCameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	FlightCameraBoom->SetupAttachment(SceneRoot);
	FlightCameraBoom->TargetArmLength = 900.0f;
	FlightCameraBoom->SocketOffset = FVector(0.0f, 0.0f, 120.0f);
	FlightCameraBoom->bUsePawnControlRotation = false;
	// The ship is often parked directly on uneven terrain. A spring-arm collision retraction can
	// collapse an otherwise valid exterior view to the cockpit, so driving always keeps its full arm.
	FlightCameraBoom->bDoCollisionTest = false;
	FlightCameraBoom->bEnableCameraLag = true;
	FlightCameraBoom->CameraLagSpeed = 8.0f;
	FlightCameraBoom->bEnableCameraRotationLag = true;
	FlightCameraBoom->CameraRotationLagSpeed = 10.0f;
	CameraBoom = FlightCameraBoom;

	FlightCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FlightCamera"));
	FlightCamera->SetupAttachment(FlightCameraBoom, USpringArmComponent::SocketName);
	FlightCamera->bUsePawnControlRotation = false;
	FlightCamera->SetFieldOfView(NormalFlightFOV);

	BoardingTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("BoardingTrigger"));
	BoardingTrigger->SetupAttachment(SceneRoot);
	BoardingTrigger->SetSphereRadius(360.0f);
	BoardingTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoardingTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	BoardingTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoardingTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	BoardingTrigger->SetGenerateOverlapEvents(true);
	BoardingTrigger->SetCanEverAffectNavigation(false);
	BoardingTrigger->OnComponentBeginOverlap.AddDynamic(this, &AJTSSpacecraftActor::HandleBoardingTriggerBeginOverlap);
	BoardingTrigger->OnComponentEndOverlap.AddDynamic(this, &AJTSSpacecraftActor::HandleBoardingTriggerEndOverlap);

	BoardingPoint = CreateDefaultSubobject<USceneComponent>(TEXT("BoardingPoint"));
	BoardingPoint->SetupAttachment(SceneRoot);
	BoardingPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 190.0f));

	ExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ExitPoint"));
	ExitPoint->SetupAttachment(SceneRoot);
	ExitPoint->SetRelativeLocation(FVector(0.0f, -380.0f, 105.0f));

	MoonWrappedActorComponent = CreateDefaultSubobject<UJTSMoonWrappedActorComponent>(TEXT("MoonWrappedActorComponent"));
}

void AJTSSpacecraftActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateFlightCamera(DeltaSeconds);
}

void AJTSSpacecraftActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeFlightCameraDistance();
	if (FlightMovementComponent != nullptr
		&& !FlightMovementComponent->OnBoostStateChanged.IsAlreadyBound(this, &AJTSSpacecraftActor::HandleFlightBoostStateChanged))
	{
		FlightMovementComponent->OnBoostStateChanged.AddDynamic(this, &AJTSSpacecraftActor::HandleFlightBoostStateChanged);
	}
	if (FlightMovementComponent != nullptr
		&& !FlightMovementComponent->OnAssistedLandingCompleted.IsBoundToObject(this))
	{
		FlightMovementComponent->OnAssistedLandingCompleted.AddUObject(this, &AJTSSpacecraftActor::HandleAssistedLandingCompleted);
	}
	if (FlightMovementComponent != nullptr
		&& !FlightMovementComponent->OnAssistedLandingFailed.IsBoundToObject(this))
	{
		FlightMovementComponent->OnAssistedLandingFailed.AddUObject(this, &AJTSSpacecraftActor::HandleAssistedLandingFailed);
	}

	if (!IsValid(BoardingTrigger))
	{
		UE_LOG(LogTemp, Warning, TEXT("Spacecraft BoardingTrigger Overlap Not Bound"));
	}
	else if (!BoardingTrigger->OnComponentBeginOverlap.IsAlreadyBound(this, &AJTSSpacecraftActor::HandleBoardingTriggerBeginOverlap))
	{
		UE_LOG(LogTemp, Warning, TEXT("Spacecraft BoardingTrigger Overlap Not Bound"));
		BoardingTrigger->OnComponentBeginOverlap.AddDynamic(this, &AJTSSpacecraftActor::HandleBoardingTriggerBeginOverlap);
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	RestorePersistentStorage();

	if (AJTSGameState* const JTSGameState = World->GetGameState<AJTSGameState>())
	{
		JTSGameState->OnGameplayPhaseChanged.AddDynamic(this, &AJTSSpacecraftActor::HandleGameplayPhaseChanged);
	}

	DepositResourcesFromOverlappingPlayers();
	// A character can be spawned inside this trigger after the spacecraft's BeginPlay pass. Query
	// once more on the next game tick so NearbySpacecraft is correct even when no begin-overlap
	// notification is generated by startup ordering.
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &AJTSSpacecraftActor::ReconcileInitialBoardingOverlaps));
	// Blueprints may retain legacy cockpit cameras. Resolve the default before this actor can ever
	// become a view target, then repeat the same selection during possession/view-target changes.
	ActivateFlightCameraThirdPerson();
}

void AJTSSpacecraftActor::PossessedBy(AController* NewController)
{
	UnregisterFlightInputMappingContext();
	Super::PossessedBy(NewController);
	RegisterFlightInputMappingContext();
	ActivateFlightCameraThirdPerson();
	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(NewController))
	{
		PlayerController->SetSpacecraftCameraViewTarget(this);
	}
}

void AJTSSpacecraftActor::UnPossessed()
{
	UnregisterFlightInputMappingContext();
	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->ClearInput();
	}
	Super::UnPossessed();
}

void AJTSSpacecraftActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFlightInputMappingContext();
	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->OnBoostStateChanged.RemoveDynamic(this, &AJTSSpacecraftActor::HandleFlightBoostStateChanged);
		FlightMovementComponent->OnAssistedLandingCompleted.RemoveAll(this);
		FlightMovementComponent->OnAssistedLandingFailed.RemoveAll(this);
	}
	SavePersistentStorage();

	if (UWorld* const World = GetWorld())
	{
		if (AJTSGameState* const JTSGameState = World->GetGameState<AJTSGameState>())
		{
			JTSGameState->OnGameplayPhaseChanged.RemoveDynamic(this, &AJTSSpacecraftActor::HandleGameplayPhaseChanged);
		}
	}

	AJTSCharacter* const BoardedCharacter = BoardedPlayer.Get();
	AJTSCharacter* const NearbyCharacter = NearbyPlayer.Get();
	if (BoardedCharacter != nullptr)
	{
		BoardedCharacter->HandleSpacecraftInvalidated(this);
	}
	if (NearbyCharacter != nullptr && NearbyCharacter != BoardedCharacter)
	{
		NearbyCharacter->HandleSpacecraftInvalidated(this);
	}

	BoardedPlayer = nullptr;
	NearbyPlayer = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AJTSSpacecraftActor::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	InitializeFlightInput();
	if (BoundFlightInputComponent.Get() == PlayerInputComponent)
	{
		RegisterFlightInputMappingContext();
		return;
	}

	UEnhancedInputComponent* const EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (EnhancedInputComponent == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Jump to Space requires UEnhancedInputComponent for spacecraft flight input."));
		return;
	}

	EnhancedInputComponent->BindAction(FlightForwardAction, ETriggerEvent::Triggered, this, &AJTSSpacecraftActor::FlightMoveForward);
	EnhancedInputComponent->BindAction(FlightForwardAction, ETriggerEvent::Completed, this, &AJTSSpacecraftActor::FlightMoveForward);
	EnhancedInputComponent->BindAction(FlightForwardAction, ETriggerEvent::Canceled, this, &AJTSSpacecraftActor::FlightMoveForward);
	EnhancedInputComponent->BindAction(FlightRightAction, ETriggerEvent::Triggered, this, &AJTSSpacecraftActor::FlightMoveRight);
	EnhancedInputComponent->BindAction(FlightRightAction, ETriggerEvent::Completed, this, &AJTSSpacecraftActor::FlightMoveRight);
	EnhancedInputComponent->BindAction(FlightRightAction, ETriggerEvent::Canceled, this, &AJTSSpacecraftActor::FlightMoveRight);
	EnhancedInputComponent->BindAction(FlightVerticalAction, ETriggerEvent::Triggered, this, &AJTSSpacecraftActor::FlightMoveVertical);
	EnhancedInputComponent->BindAction(FlightVerticalAction, ETriggerEvent::Completed, this, &AJTSSpacecraftActor::FlightMoveVertical);
	EnhancedInputComponent->BindAction(FlightVerticalAction, ETriggerEvent::Canceled, this, &AJTSSpacecraftActor::FlightMoveVertical);
	EnhancedInputComponent->BindAction(FlightRollAction, ETriggerEvent::Triggered, this, &AJTSSpacecraftActor::FlightRoll);
	EnhancedInputComponent->BindAction(FlightRollAction, ETriggerEvent::Completed, this, &AJTSSpacecraftActor::FlightRoll);
	EnhancedInputComponent->BindAction(FlightRollAction, ETriggerEvent::Canceled, this, &AJTSSpacecraftActor::FlightRoll);
	EnhancedInputComponent->BindAction(FlightLookYawAction, ETriggerEvent::Triggered, this, &AJTSSpacecraftActor::FlightLookYaw);
	EnhancedInputComponent->BindAction(FlightLookPitchAction, ETriggerEvent::Triggered, this, &AJTSSpacecraftActor::FlightLookPitch);
	EnhancedInputComponent->BindAction(FlightCameraZoomAction, ETriggerEvent::Triggered, this, &AJTSSpacecraftActor::FlightCameraZoom);
	EnhancedInputComponent->BindAction(FlightBoostAction, ETriggerEvent::Started, this, &AJTSSpacecraftActor::FlightBoostStarted);
	EnhancedInputComponent->BindAction(FlightBoostAction, ETriggerEvent::Completed, this, &AJTSSpacecraftActor::FlightBoostStopped);
	EnhancedInputComponent->BindAction(FlightBoostAction, ETriggerEvent::Canceled, this, &AJTSSpacecraftActor::FlightBoostStopped);
	EnhancedInputComponent->BindAction(FlightBrakeAction, ETriggerEvent::Started, this, &AJTSSpacecraftActor::FlightBrakeStarted);
	EnhancedInputComponent->BindAction(FlightBrakeAction, ETriggerEvent::Completed, this, &AJTSSpacecraftActor::FlightBrakeStopped);
	EnhancedInputComponent->BindAction(FlightBrakeAction, ETriggerEvent::Canceled, this, &AJTSSpacecraftActor::FlightBrakeStopped);
	EnhancedInputComponent->BindAction(FlightLandingAction, ETriggerEvent::Started, this, &AJTSSpacecraftActor::FlightLandingStarted);
	EnhancedInputComponent->BindAction(FlightDisembarkAction, ETriggerEvent::Started, this, &AJTSSpacecraftActor::FlightDisembarkStarted);

	BoundFlightInputComponent = PlayerInputComponent;
	RegisterFlightInputMappingContext();
}

void AJTSSpacecraftActor::InitializeFlightInput()
{
	if (FlightInputMappingContext != nullptr)
	{
		return;
	}

	FlightInputMappingContext = NewObject<UInputMappingContext>(this, TEXT("JTSFlightInputMappingContext"), RF_Transient);
	FlightForwardAction = NewObject<UInputAction>(this, TEXT("FlightForwardAction"), RF_Transient);
	FlightRightAction = NewObject<UInputAction>(this, TEXT("FlightRightAction"), RF_Transient);
	FlightVerticalAction = NewObject<UInputAction>(this, TEXT("FlightVerticalAction"), RF_Transient);
	FlightRollAction = NewObject<UInputAction>(this, TEXT("FlightRollAction"), RF_Transient);
	FlightLookYawAction = NewObject<UInputAction>(this, TEXT("FlightLookYawAction"), RF_Transient);
	FlightLookPitchAction = NewObject<UInputAction>(this, TEXT("FlightLookPitchAction"), RF_Transient);
	FlightCameraZoomAction = NewObject<UInputAction>(this, TEXT("FlightCameraZoomAction"), RF_Transient);
	FlightBoostAction = NewObject<UInputAction>(this, TEXT("FlightBoostAction"), RF_Transient);
	FlightBrakeAction = NewObject<UInputAction>(this, TEXT("FlightBrakeAction"), RF_Transient);
	FlightLandingAction = NewObject<UInputAction>(this, TEXT("FlightLandingAction"), RF_Transient);
	FlightDisembarkAction = NewObject<UInputAction>(this, TEXT("FlightDisembarkAction"), RF_Transient);

	FlightForwardAction->ValueType = EInputActionValueType::Axis1D;
	FlightRightAction->ValueType = EInputActionValueType::Axis1D;
	FlightVerticalAction->ValueType = EInputActionValueType::Axis1D;
	FlightRollAction->ValueType = EInputActionValueType::Axis1D;
	FlightLookYawAction->ValueType = EInputActionValueType::Axis1D;
	FlightLookPitchAction->ValueType = EInputActionValueType::Axis1D;
	FlightCameraZoomAction->ValueType = EInputActionValueType::Axis1D;
	FlightBoostAction->ValueType = EInputActionValueType::Boolean;
	FlightBrakeAction->ValueType = EInputActionValueType::Boolean;
	FlightLandingAction->ValueType = EInputActionValueType::Boolean;
	FlightDisembarkAction->ValueType = EInputActionValueType::Boolean;

	FlightInputMappingContext->MapKey(FlightForwardAction, EKeys::W);
	FlightInputMappingContext->MapKey(FlightRightAction, EKeys::D);
	FlightInputMappingContext->MapKey(FlightVerticalAction, EKeys::SpaceBar);
	FlightInputMappingContext->MapKey(FlightVerticalAction, EKeys::R);
	FlightInputMappingContext->MapKey(FlightRollAction, EKeys::E);
	FlightInputMappingContext->MapKey(FlightLookYawAction, EKeys::MouseX);
	FlightInputMappingContext->MapKey(FlightLookPitchAction, EKeys::MouseY);
	FlightInputMappingContext->MapKey(FlightCameraZoomAction, EKeys::MouseWheelAxis);
	FlightInputMappingContext->MapKey(FlightBoostAction, EKeys::LeftShift);
	FlightInputMappingContext->MapKey(FlightBrakeAction, EKeys::C);
	FlightInputMappingContext->MapKey(FlightLandingAction, EKeys::L);
	FlightInputMappingContext->MapKey(FlightDisembarkAction, EKeys::F);

	auto AddNegatedMapping = [this](UInputAction* Action, const FKey& Key)
	{
		FEnhancedActionKeyMapping& Mapping = FlightInputMappingContext->MapKey(Action, Key);
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(FlightInputMappingContext));
	};
	AddNegatedMapping(FlightForwardAction, EKeys::S);
	AddNegatedMapping(FlightRightAction, EKeys::A);
	AddNegatedMapping(FlightVerticalAction, EKeys::LeftControl);
	AddNegatedMapping(FlightRollAction, EKeys::Q);
}

void AJTSSpacecraftActor::RegisterFlightInputMappingContext()
{
	if (FlightInputMappingContext == nullptr)
	{
		return;
	}

	APlayerController* const PlayerController = Cast<APlayerController>(GetController());
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	ULocalPlayer* const LocalPlayer = PlayerController->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* const InputSubsystem = LocalPlayer != nullptr
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
		: nullptr;
	if (InputSubsystem == nullptr || RegisteredFlightInputSubsystem.Get() == InputSubsystem)
	{
		return;
	}

	UnregisterFlightInputMappingContext();
	InputSubsystem->AddMappingContext(FlightInputMappingContext, 1);
	RegisteredFlightInputSubsystem = InputSubsystem;
}

void AJTSSpacecraftActor::UnregisterFlightInputMappingContext()
{
	if (UEnhancedInputLocalPlayerSubsystem* const InputSubsystem = RegisteredFlightInputSubsystem.Get())
	{
		if (FlightInputMappingContext != nullptr)
		{
			InputSubsystem->RemoveMappingContext(FlightInputMappingContext);
		}
	}
	RegisteredFlightInputSubsystem.Reset();
}

void AJTSSpacecraftActor::FlightMoveForward(const FInputActionValue& Value)
{
	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->SetForwardInput(Value.Get<float>());
	}
}

void AJTSSpacecraftActor::FlightMoveRight(const FInputActionValue& Value)
{
	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->SetStrafeInput(Value.Get<float>());
	}
}

void AJTSSpacecraftActor::FlightMoveVertical(const FInputActionValue& Value)
{
	if (Value.Get<float>() > KINDA_SMALL_NUMBER && IsSpaceWorldSurfaceActive())
	{
		BeginSurfaceTakeoff();
	}

	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->SetVerticalInput(Value.Get<float>());
	}
}

void AJTSSpacecraftActor::FlightRoll(const FInputActionValue& Value)
{
	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->SetRollInput(Value.Get<float>());
	}
}

void AJTSSpacecraftActor::FlightLookYaw(const FInputActionValue& Value)
{
	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->AddYawInput(Value.Get<float>());
	}
}

void AJTSSpacecraftActor::FlightLookPitch(const FInputActionValue& Value)
{
	if (FlightMovementComponent != nullptr)
	{
		const AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetController());
		const float PitchDirection = PlayerController != nullptr && PlayerController->IsLookYAxisInverted() ? -1.0f : 1.0f;
		FlightMovementComponent->AddPitchInput(Value.Get<float>() * PitchDirection);
	}
}

void AJTSSpacecraftActor::FlightCameraZoom(const FInputActionValue& Value)
{
	AdjustFlightCameraDistance(Value.Get<float>());
}

void AJTSSpacecraftActor::FlightBoostStarted(const FInputActionValue& Value)
{
	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->SetBoosting(Value.Get<bool>());
	}
}

void AJTSSpacecraftActor::FlightBoostStopped(const FInputActionValue& Value)
{
	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->SetBoosting(false);
	}
}

void AJTSSpacecraftActor::FlightBrakeStarted(const FInputActionValue& Value)
{
	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->SetBraking(Value.Get<bool>());
	}
}

void AJTSSpacecraftActor::FlightBrakeStopped(const FInputActionValue& Value)
{
	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->SetBraking(false);
	}
}

void AJTSSpacecraftActor::FlightLandingStarted(const FInputActionValue& Value)
{
	if (Value.Get<bool>())
	{
		RequestLanding();
	}
}

void AJTSSpacecraftActor::FlightDisembarkStarted(const FInputActionValue& Value)
{
	if (!Value.Get<bool>() || !IsGroundedOnPlanet())
	{
		return;
	}

	if (AJTSCharacter* const Character = BoardedPlayer.Get())
	{
		TryDisembarkPlayer(Character);
	}
}

void AJTSSpacecraftActor::InitializeFlightCameraDistance()
{
	if (bFlightCameraDistanceInitialized || FlightCameraBoom == nullptr)
	{
		return;
	}

	const float MinimumArmLength = FMath::Min(FlightCameraMinArmLength, FlightCameraMaxArmLength);
	const float MaximumArmLength = FMath::Max(FlightCameraMinArmLength, FlightCameraMaxArmLength);
	CurrentFlightCameraArmLength = FMath::Clamp(FlightCameraBoom->TargetArmLength, MinimumArmLength, MaximumArmLength);
	FlightCameraBoom->TargetArmLength = CurrentFlightCameraArmLength;
	bFlightCameraDistanceInitialized = true;
}

void AJTSSpacecraftActor::UpdateFlightCamera(float DeltaSeconds)
{
	if (FlightCamera == nullptr)
	{
		return;
	}

	const bool bShouldBoostFOV = FlightMovementComponent != nullptr && FlightMovementComponent->IsBoosting();
	const float TargetFOV = bShouldBoostFOV ? BoostFlightFOV : NormalFlightFOV;
	FlightCamera->SetFieldOfView(FMath::FInterpTo(
		FlightCamera->FieldOfView,
		TargetFOV,
		DeltaSeconds,
		FMath::Max(0.1f, FlightFOVInterpolationSpeed)));
}

void AJTSSpacecraftActor::HandleFlightBoostStateChanged(bool bIsBoosting)
{
	OnBoostStateChanged.Broadcast(bIsBoosting);
}

bool AJTSSpacecraftActor::TryDepositResourcesFromPawn(APawn* InteractingPawn)
{
	return DepositPlayerResources(Cast<AJTSCharacter>(InteractingPawn));
}

bool AJTSSpacecraftActor::TryBoardPlayer(APawn* InteractingPawn)
{
	AJTSCharacter* const Character = Cast<AJTSCharacter>(InteractingPawn);
	const bool bEarthCollectionActive = IsEarthCollectionActive();
	const bool bSpaceWorldSurfaceActive = IsSpaceWorldSurfaceActive();
	if ((!bEarthCollectionActive && !bSpaceWorldSurfaceActive)
		|| !IsValid(Character)
		|| HasBoardedPlayer()
		|| !IsPawnInBoardingRange(Character))
	{
		return false;
	}

	APlayerController* const PlayerController = bSpaceWorldSurfaceActive
		? Cast<APlayerController>(Character->GetController())
		: nullptr;
	if (bSpaceWorldSurfaceActive && !IsValid(PlayerController))
	{
		return false;
	}

	if (!Character->EnterBoardedState(this))
	{
		return false;
	}

	BoardedPlayer = Character;
	NearbyPlayer = Character;
	if (!bSpaceWorldSurfaceActive)
	{
		// Preserve Earth collection's existing hold-to-board state. Earth launch flow owns its
		// transition and intentionally does not hand direct spacecraft control to the player.
		return true;
	}

	PlayerController->Possess(this);
	if (GetController() != PlayerController)
	{
		BoardedPlayer = nullptr;
		NearbyPlayer = nullptr;
		Character->ExitBoardedState(this);
		if (PlayerController->GetPawn() != Character)
		{
			PlayerController->Possess(Character);
		}
		return false;
	}
	if (PlayerController->IsLocalController())
	{
		// Do not carry the E hold that completed boarding into the newly active flight input context.
		// The player must release it, then explicitly press a flight control such as Space to take off.
		PlayerController->FlushPressedKeys();
	}
	if (AJTSPlayerController* const JTSPlayerController = Cast<AJTSPlayerController>(PlayerController))
	{
		JTSPlayerController->SetSpacecraftCameraViewTarget(this);
	}
	else
	{
		PlayerController->SetViewTargetWithBlend(this, 0.35f);
	}

	return true;
}

bool AJTSSpacecraftActor::TryDisembarkPlayer(APawn* InteractingPawn)
{
	AJTSCharacter* const Character = Cast<AJTSCharacter>(InteractingPawn);
	APlayerController* const PlayerController = Cast<APlayerController>(GetController());
	const bool bPlayerIsDriving = IsValid(PlayerController) && PlayerController->GetPawn() == this;
	const bool bSpaceWorldSurfaceActive = IsSpaceWorldSurfaceActive();
	if ((!IsEarthCollectionActive() && !IsMoonExplorationActive() && !bSpaceWorldSurfaceActive)
		|| !IsValid(Character)
		|| BoardedPlayer.Get() != Character)
	{
		return false;
	}
	if (bPlayerIsDriving && (!bSpaceWorldSurfaceActive || !IsGroundedOnPlanet()))
	{
		// Airborne ejection is intentionally outside this first surface-flight slice.
		return false;
	}

	BoardedPlayer = nullptr;
	NearbyPlayer = nullptr;
	if (bPlayerIsDriving)
	{
		PlayerController->UnPossess();
	}
	Character->ExitBoardedState(this);
	if (bPlayerIsDriving && IsValid(PlayerController))
	{
		PlayerController->Possess(Character);
		if (AJTSPlayerController* const JTSPlayerController = Cast<AJTSPlayerController>(PlayerController))
		{
			JTSPlayerController->RestoreCharacterCameraViewTarget(Character);
		}
		else
		{
			PlayerController->SetViewTargetWithBlend(Character, 0.35f);
		}
	}
	return true;
}

bool AJTSSpacecraftActor::IsPlayerBoarded(const APawn* InteractingPawn) const
{
	return IsValid(InteractingPawn) && BoardedPlayer.Get() == InteractingPawn;
}

bool AJTSSpacecraftActor::HasBoardedPlayer() const
{
	return IsValid(BoardedPlayer);
}

AJTSCharacter* AJTSSpacecraftActor::GetBoardedPlayer() const
{
	return BoardedPlayer.Get();
}

bool AJTSSpacecraftActor::IsPawnInBoardingRange(const APawn* InteractingPawn) const
{
	if (!IsValid(InteractingPawn) || !IsValid(BoardingTrigger))
	{
		return false;
	}

	return BoardingTrigger->IsOverlappingActor(InteractingPawn)
		|| FVector::DistSquared(BoardingTrigger->GetComponentLocation(), InteractingPawn->GetActorLocation())
			<= FMath::Square(BoardingTrigger->GetScaledSphereRadius());
}

USceneComponent* AJTSSpacecraftActor::GetBoardingPoint() const
{
	return BoardingPoint.Get();
}

USceneComponent* AJTSSpacecraftActor::GetExitPoint() const
{
	return ExitPoint.Get();
}

UJTSSpacecraftFlightMovementComponent* AJTSSpacecraftActor::GetFlightMovementComponent() const
{
	return FlightMovementComponent.Get();
}

UJTSSpacecraftGroundProbeComponent* AJTSSpacecraftActor::GetGroundProbeComponent() const
{
	return GroundProbeComponent.Get();
}

FJTSSpacecraftGroundInfo AJTSSpacecraftActor::GetGroundInfo() const
{
	return GroundProbeComponent != nullptr
		? GroundProbeComponent->GetGroundInfo()
		: FJTSSpacecraftGroundInfo();
}

bool AJTSSpacecraftActor::RefreshGroundInfo(AJTSPlanetAnchor* Planet, float MaxProbeDistance)
{
	return GroundProbeComponent != nullptr
		&& GroundProbeComponent->ProbeGround(Planet, GetActorForwardVector(), MaxProbeDistance);
}

USpringArmComponent* AJTSSpacecraftActor::GetFlightCameraBoom() const
{
	return FlightCameraBoom.Get();
}

UCameraComponent* AJTSSpacecraftActor::GetFlightCamera() const
{
	return FlightCamera.Get();
}

void AJTSSpacecraftActor::ActivateFlightCameraThirdPerson()
{
	bFindCameraComponentWhenViewTarget = true;
	InitializeFlightCameraDistance();
	if (FlightCameraBoom != nullptr)
	{
		const float MinimumArmLength = FMath::Min(FlightCameraMinArmLength, FlightCameraMaxArmLength);
		const float MaximumArmLength = FMath::Max(FlightCameraMinArmLength, FlightCameraMaxArmLength);
		CurrentFlightCameraArmLength = FMath::Clamp(CurrentFlightCameraArmLength, MinimumArmLength, MaximumArmLength);
		FlightCameraBoom->TargetArmLength = CurrentFlightCameraArmLength;
		FlightCameraBoom->bUsePawnControlRotation = false;
		FlightCameraBoom->SetUsingAbsoluteRotation(false);
		FlightCameraBoom->bDoCollisionTest = false;
	}

	TArray<UCameraComponent*> CameraComponents;
	GetComponents<UCameraComponent>(CameraComponents);
	for (UCameraComponent* const CameraComponent : CameraComponents)
	{
		if (IsValid(CameraComponent))
		{
			CameraComponent->SetActive(CameraComponent == FlightCamera.Get());
		}
	}

	if (FlightCamera != nullptr)
	{
		FlightCamera->bUsePawnControlRotation = false;
		FlightCamera->SetActive(true);
	}
}

void AJTSSpacecraftActor::AdjustFlightCameraDistance(float ScrollAmount)
{
	if (FlightCameraBoom == nullptr || FMath::IsNearlyZero(ScrollAmount))
	{
		return;
	}

	InitializeFlightCameraDistance();
	const float MinimumArmLength = FMath::Min(FlightCameraMinArmLength, FlightCameraMaxArmLength);
	const float MaximumArmLength = FMath::Max(FlightCameraMinArmLength, FlightCameraMaxArmLength);
	CurrentFlightCameraArmLength = FMath::Clamp(
		CurrentFlightCameraArmLength - ScrollAmount * FMath::Max(1.0f, FlightCameraZoomStep),
		MinimumArmLength,
		MaximumArmLength);
	FlightCameraBoom->TargetArmLength = CurrentFlightCameraArmLength;
}

bool AJTSSpacecraftActor::IsBoosting() const
{
	return FlightMovementComponent != nullptr && FlightMovementComponent->IsBoosting();
}

float AJTSSpacecraftActor::GetCurrentSpeed() const
{
	return FlightMovementComponent != nullptr ? FlightMovementComponent->GetCurrentSpeed() : 0.0f;
}

float AJTSSpacecraftActor::GetSpeedNormalized() const
{
	return FlightMovementComponent != nullptr ? FlightMovementComponent->GetSpeedNormalized() : 0.0f;
}

float AJTSSpacecraftActor::GetThrottleNormalized() const
{
	return FlightMovementComponent != nullptr ? FlightMovementComponent->GetThrottleNormalized() : 0.0f;
}

void AJTSSpacecraftActor::SetFlightTargetPlanet(AJTSPlanetAnchor* Planet)
{
	FlightPlanet = Planet;
	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->SetTargetPlanet(Planet);
	}
}

void AJTSSpacecraftActor::InitializeForPlanetArrival(AJTSPlanetAnchor* Planet)
{
	if (!IsValid(Planet))
	{
		return;
	}

	SetFlightTargetPlanet(Planet);
	GroundedPlanet = nullptr;
	bIsGroundedOnPlanet = false;
	ActiveLandingSite = nullptr;
	PendingLandingSite = nullptr;
	PendingLandingTransform = FTransform::Identity;
	PendingLandingClearance = 0.0f;
	FlightState = EJTSSpacecraftFlightState::Flying;
	LastLandingFailure = EJTSLandingValidationFailure::None;

	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->CancelAssistedLanding();
		FlightMovementComponent->ClearInput();
		FlightMovementComponent->StopMovementImmediately();
		FlightMovementComponent->Activate(true);
	}
}

bool AJTSSpacecraftActor::RequestLanding()
{
	if (FlightState != EJTSSpacecraftFlightState::Flying)
	{
		UE_LOG(LogTemp, Warning, TEXT("Landing Request ignored: Spacecraft=%s State=%d"),
			*GetName(), static_cast<int32>(FlightState));
		return false;
	}

	FlightState = EJTSSpacecraftFlightState::LandingRequest;
	LastLandingFailure = EJTSLandingValidationFailure::None;
	AJTSPlanetLandingManager* const LandingManager = AJTSPlanetLandingManager::FindPlanetLandingManager(this);
	if (!IsValid(LandingManager))
	{
		CancelLandingRequest(EJTSLandingValidationFailure::NoPlanet);
		return false;
	}

	FJTSPlanetLandingValidationResult ValidationResult;
	return LandingManager->RequestLanding(this, ValidationResult);
}

bool AJTSSpacecraftActor::BeginLandingAssist(
	const FJTSPlanetLandingValidationResult& ValidationResult,
	float DurationSeconds)
{
	if (FlightState != EJTSSpacecraftFlightState::LandingRequest
		|| !ValidationResult.bIsValid
		|| !IsValid(ValidationResult.LandingSite)
		|| FlightMovementComponent == nullptr)
	{
		return false;
	}

	if (!IsValid(FlightPlanet))
	{
		SetFlightTargetPlanet(ValidationResult.LandingSite->GetPlanetAnchor());
	}
	if (!IsValid(FlightPlanet))
	{
		return false;
	}

	GroundedPlanet = nullptr;
	bIsGroundedOnPlanet = false;
	ActiveLandingSite = ValidationResult.LandingSite;
	PendingLandingSite = ValidationResult.LandingSite;
	PendingLandingTransform = ValidationResult.LandingTransform;
	PendingLandingClearance = ValidationResult.LandingClearance;
	if (!FlightMovementComponent->BeginAssistedLanding(FlightPlanet.Get(), PendingLandingClearance, DurationSeconds))
	{
		PendingLandingSite = nullptr;
		ActiveLandingSite = nullptr;
		PendingLandingClearance = 0.0f;
		return false;
	}

	FlightState = EJTSSpacecraftFlightState::LandingAssist;
	return true;
}

void AJTSSpacecraftActor::CancelLandingRequest(EJTSLandingValidationFailure Failure)
{
	if (FlightState == EJTSSpacecraftFlightState::LandingAssist && FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->CancelAssistedLanding();
	}

	if (FlightState != EJTSSpacecraftFlightState::Landed)
	{
		FlightState = EJTSSpacecraftFlightState::Flying;
		ActiveLandingSite = nullptr;
		PendingLandingSite = nullptr;
		PendingLandingTransform = FTransform::Identity;
		PendingLandingClearance = 0.0f;
	}
	LastLandingFailure = Failure;
}

EJTSSpacecraftFlightState AJTSSpacecraftActor::GetFlightState() const
{
	return FlightState;
}

bool AJTSSpacecraftActor::IsLanded() const
{
	return FlightState == EJTSSpacecraftFlightState::Landed
		&& bIsGroundedOnPlanet
		&& IsValid(GroundedPlanet);
}

EJTSLandingValidationFailure AJTSSpacecraftActor::GetLastLandingFailure() const
{
	return LastLandingFailure;
}

AJTSPlanetAnchor* AJTSSpacecraftActor::GetLandedPlanet() const
{
	return IsLanded() ? GroundedPlanet.Get() : nullptr;
}

AJTSPlanetAnchor* AJTSSpacecraftActor::GetFlightPlanet() const
{
	return FlightPlanet.Get();
}

bool AJTSSpacecraftActor::BeginAssistedLanding(const FTransform& LandingTransform, float DurationSeconds)
{
	ClearGroundedPlanet();
	AJTSPlanetAnchor* const Planet = FlightPlanet.Get();
	if (FlightMovementComponent != nullptr && IsValid(Planet))
	{
		const FVector SurfaceUp = LandingTransform.GetUnitAxis(EAxis::Z).GetSafeNormal();
		const FQuat LandingRotation = LandingTransform.GetRotation();
		PendingLandingSite = nullptr;
		PendingLandingTransform = LandingTransform;
		PendingLandingClearance = GetLandingCollisionClearanceForRotation(
			LandingRotation,
			SurfaceUp.IsNearlyZero() ? Planet->GetRadialUpVector(GetActorLocation()) : SurfaceUp);
		LastLandingFailure = EJTSLandingValidationFailure::None;
		if (FlightMovementComponent->BeginAssistedLanding(Planet, PendingLandingClearance, DurationSeconds))
		{
			FlightState = EJTSSpacecraftFlightState::LandingAssist;
			return true;
		}
	}
	FlightState = EJTSSpacecraftFlightState::Flying;
	return false;
}

void AJTSSpacecraftActor::SetGroundedPlanet(AJTSPlanetAnchor* InPlanetAnchor)
{
	SetFlightTargetPlanet(InPlanetAnchor);
	GroundedPlanet = InPlanetAnchor;
	bIsGroundedOnPlanet = IsValid(InPlanetAnchor);
	if (!bIsGroundedOnPlanet)
	{
		FlightState = EJTSSpacecraftFlightState::Flying;
		return;
	}
	FlightState = EJTSSpacecraftFlightState::Landed;
	LastLandingFailure = EJTSLandingValidationFailure::None;

	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->ClearInput();
		FlightMovementComponent->StopMovementImmediately();
		FlightMovementComponent->Deactivate();
	}
}

bool AJTSSpacecraftActor::BeginSurfaceTakeoff()
{
	if (!IsSpaceWorldSurfaceActive() || !IsGroundedOnPlanet())
	{
		return false;
	}

	AJTSPlanetAnchor* const Planet = GetLandedPlanet();
	if (!IsValid(Planet))
	{
		return false;
	}

	SetFlightTargetPlanet(Planet);
	ClearGroundedPlanet();
	if (AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
	{
		Manager->SetTravelState(EJTSSpaceTravelState::Takeoff);
	}

	UE_LOG(LogTemp, Log, TEXT("Spacecraft %s began surface takeoff from %s."), *GetName(), *Planet->GetPlanetId().ToString());
	return true;
}

void AJTSSpacecraftActor::ClearGroundedPlanet()
{
	GroundedPlanet = nullptr;
	bIsGroundedOnPlanet = false;
	ActiveLandingSite = nullptr;
	PendingLandingSite = nullptr;
	PendingLandingTransform = FTransform::Identity;
	PendingLandingClearance = 0.0f;
	if (FlightState == EJTSSpacecraftFlightState::Landed)
	{
		FlightState = EJTSSpacecraftFlightState::Flying;
	}
	if (FlightMovementComponent != nullptr && !FlightMovementComponent->IsActive())
	{
		FlightMovementComponent->Activate(true);
	}
}

AJTSPlanetAnchor* AJTSSpacecraftActor::GetGroundedPlanet() const
{
	return GetLandedPlanet();
}

bool AJTSSpacecraftActor::IsGroundedOnPlanet() const
{
	return IsLanded();
}

bool AJTSSpacecraftActor::SnapSpacecraftToSurfaceTransform(
	AJTSPlanetAnchor* InPlanetAnchor,
	const FTransform& SurfaceTransform)
{
	if (!IsValid(InPlanetAnchor) || !InPlanetAnchor->HasGameplaySurface())
	{
		UE_LOG(LogTemp, Warning, TEXT("Grounded spacecraft %s has no valid real gameplay planet."), *GetName());
		return false;
	}

	const FVector SurfaceUp = SurfaceTransform.GetUnitAxis(EAxis::Z).GetSafeNormal();
	FVector SurfaceForward = FVector::VectorPlaneProject(SurfaceTransform.GetUnitAxis(EAxis::X), SurfaceUp).GetSafeNormal();
	if (SurfaceUp.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("Grounded spacecraft %s received an invalid surface frame on %s."),
			*GetName(), *InPlanetAnchor->GetPlanetId().ToString());
		return false;
	}
	if (SurfaceForward.IsNearlyZero())
	{
		SurfaceForward = InPlanetAnchor->ProjectDirectionToSurfaceTangent(GetActorForwardVector(), SurfaceTransform.GetLocation());
	}
	if (SurfaceForward.IsNearlyZero())
	{
		FVector SurfaceRight;
		SurfaceUp.FindBestAxisVectors(SurfaceForward, SurfaceRight);
	}

	const FQuat SurfaceRotation = FRotationMatrix::MakeFromXZ(SurfaceForward, SurfaceUp).ToQuat();
	const FVector GroundedLocation = SurfaceTransform.GetLocation()
		+ SurfaceUp * GetLandingCollisionClearanceForRotation(SurfaceRotation, SurfaceUp);
	FTransform GroundedTransform(SurfaceRotation, GroundedLocation);
	if (!CanOccupyLandingTransform(GroundedTransform))
	{
		// A Blueprint can offset the visual/collision hull. Search only outward from the actual mesh
		// surface; never leave the arrival craft in flight just because its first contact test is close.
		constexpr int32 MaxOutwardPlacementAttempts = 16;
		constexpr float OutwardPlacementStep = 25.0f;
		bool bFoundClearTransform = false;
		for (int32 AttemptIndex = 1; AttemptIndex <= MaxOutwardPlacementAttempts; ++AttemptIndex)
		{
			FTransform CandidateTransform = GroundedTransform;
			CandidateTransform.AddToTranslation(SurfaceUp * OutwardPlacementStep * AttemptIndex);
			if (CanOccupyLandingTransform(CandidateTransform))
			{
				GroundedTransform = CandidateTransform;
				bFoundClearTransform = true;
				break;
			}
		}

		if (!bFoundClearTransform)
		{
			UE_LOG(LogTemp, Warning, TEXT("Grounded spacecraft %s could not obtain an overlap-free surface placement on %s; using its collision-supported surface transform."),
				*GetName(), *InPlanetAnchor->GetPlanetId().ToString());
		}
	}
	SetActorLocationAndRotation(
		GroundedTransform.GetLocation(),
		GroundedTransform.GetRotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	SetGroundedPlanet(InPlanetAnchor);
	return true;
}

bool AJTSSpacecraftActor::SnapSpacecraftToSurfaceAnchor(AJTSPlanetSurfaceAnchor* SurfaceAnchor)
{
	if (!IsValid(SurfaceAnchor))
	{
		UE_LOG(LogTemp, Warning, TEXT("Grounded spacecraft %s received an invalid landing anchor."), *GetName());
		return false;
	}

	AJTSPlanetAnchor* const Planet = SurfaceAnchor->GetPlanetAnchor();
	FTransform SurfaceTransform;
	if (!IsValid(Planet) || !SurfaceAnchor->GetSurfaceTransform(SurfaceTransform))
	{
		UE_LOG(LogTemp, Warning, TEXT("Grounded spacecraft %s could not resolve landing anchor %s."),
			*GetName(), *SurfaceAnchor->GetName());
		return false;
	}

	return SnapSpacecraftToSurfaceTransform(Planet, SurfaceTransform);
}

bool AJTSSpacecraftActor::CanOccupyLandingTransform(const FTransform& LandingTransform) const
{
	UWorld* const World = GetWorld();
	if (World == nullptr || !IsValid(FlightCollision))
	{
		return false;
	}

	FCollisionQueryParams QueryParameters(SCENE_QUERY_STAT(JTSSpacecraftLandingClearance), false, this);
	QueryParameters.AddIgnoredActor(this);
	const FTransform CollisionTransform = GetFlightCollisionTransformForSpacecraftTransform(LandingTransform);
	return !World->OverlapBlockingTestByChannel(
		CollisionTransform.GetLocation(),
		CollisionTransform.GetRotation(),
		ECC_Visibility,
		FCollisionShape::MakeBox(FlightCollision->GetScaledBoxExtent()),
		QueryParameters);
}

float AJTSSpacecraftActor::GetLandingCollisionClearance(const FVector& SurfaceUp) const
{
	return GetLandingCollisionClearanceForRotation(GetActorQuat(), SurfaceUp);
}

float AJTSSpacecraftActor::GetLandingCollisionClearanceForRotation(
	const FQuat& ShipRotation,
	const FVector& SurfaceUp) const
{
	if (!IsValid(FlightCollision))
	{
		return FMath::Max(0.0f, ShipGroundClearance);
	}

	const FVector SafeSurfaceUp = SurfaceUp.GetSafeNormal();
	if (SafeSurfaceUp.IsNearlyZero())
	{
		return FMath::Max(0.0f, ShipGroundClearance);
	}

	const FVector HullExtent = FlightCollision->GetScaledBoxExtent();
	const FQuat CollisionRotation = (ShipRotation * FlightCollision->GetRelativeRotation().Quaternion()).GetNormalized();
	const float HullSupportDistance = FMath::Abs(FVector::DotProduct(CollisionRotation.GetAxisX(), SafeSurfaceUp)) * HullExtent.X
		+ FMath::Abs(FVector::DotProduct(CollisionRotation.GetAxisY(), SafeSurfaceUp)) * HullExtent.Y
		+ FMath::Abs(FVector::DotProduct(CollisionRotation.GetAxisZ(), SafeSurfaceUp)) * HullExtent.Z;
	const FVector CollisionCenterOffset = ShipRotation.RotateVector(
		FlightCollision->GetRelativeLocation() * GetActorScale3D().GetAbs());
	return FMath::Max(
		0.0f,
		HullSupportDistance - FVector::DotProduct(CollisionCenterOffset, SafeSurfaceUp)
			+ FMath::Max(0.0f, ShipGroundClearance));
}

FTransform AJTSSpacecraftActor::GetFlightCollisionTransformForSpacecraftTransform(const FTransform& SpacecraftTransform) const
{
	if (!IsValid(FlightCollision))
	{
		return SpacecraftTransform;
	}

	const FVector CollisionCenterOffset = SpacecraftTransform.GetRotation().RotateVector(
		FlightCollision->GetRelativeLocation() * GetActorScale3D().GetAbs());
	const FQuat CollisionRotation = (SpacecraftTransform.GetRotation()
		* FlightCollision->GetRelativeRotation().Quaternion()).GetNormalized();
	return FTransform(CollisionRotation, SpacecraftTransform.GetLocation() + CollisionCenterOffset);
}

float AJTSSpacecraftActor::GetLandingCollisionClearance() const
{
	return GetLandingCollisionClearance(GetActorUpVector());
}

bool AJTSSpacecraftActor::GetPlayerRespawnTransform(FJTSPlayerRespawnTransformResult& OutResult) const
{
	OutResult = FJTSPlayerRespawnTransformResult();
	if (!IsLanded())
	{
		return false;
	}

	if (AJTSPlanetLandingManager* const LandingManager = AJTSPlanetLandingManager::FindPlanetLandingManager(this))
	{
		if (LandingManager->FindPlayerRespawnTransform(this, OutResult) && OutResult.bIsValid)
		{
			return true;
		}
	}

	if (GetTopRespawnTransform(OutResult.Transform))
	{
		OutResult.bIsValid = true;
		OutResult.Source = EJTSRespawnTransformSource::SpacecraftTop;
		return true;
	}
	if (GetExitRespawnTransform(OutResult.Transform))
	{
		OutResult.bIsValid = true;
		OutResult.Source = EJTSRespawnTransformSource::SpacecraftExit;
		return true;
	}
	return false;
}

bool AJTSSpacecraftActor::GetTopRespawnTransform(FTransform& OutTransform) const
{
	AJTSPlanetAnchor* const Planet = GetLandedPlanet();
	if (!IsValid(Planet))
	{
		return false;
	}

	const FVector SurfaceUp = Planet->GetRadialUpVector(GetActorLocation()).GetSafeNormal();
	if (SurfaceUp.IsNearlyZero())
	{
		return false;
	}

	FVector Forward = FVector::VectorPlaneProject(GetActorForwardVector(), SurfaceUp).GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		FVector FallbackRight;
		SurfaceUp.FindBestAxisVectors(Forward, FallbackRight);
	}
	const float HullSupportDistance = FMath::Max(0.0f, GetLandingCollisionClearance(SurfaceUp) - ShipGroundClearance);
	OutTransform = FTransform(
		FRotationMatrix::MakeFromXZ(Forward, SurfaceUp).ToQuat(),
		GetActorLocation() + SurfaceUp * (HullSupportDistance + PlayerRespawnCapsuleHalfHeight + PlayerRespawnClearance));
	return true;
}

bool AJTSSpacecraftActor::GetExitRespawnTransform(FTransform& OutTransform) const
{
	if (IsValid(ExitPoint))
	{
		OutTransform = ExitPoint->GetComponentTransform();
		return true;
	}
	if (IsValid(BoardingPoint))
	{
		OutTransform = BoardingPoint->GetComponentTransform();
		return true;
	}
	return false;
}

float AJTSSpacecraftActor::GetPlayerRespawnSearchRadius() const
{
	return PlayerRespawnSearchRadius;
}

float AJTSSpacecraftActor::GetPlayerRespawnCapsuleRadius() const
{
	return PlayerRespawnCapsuleRadius;
}

float AJTSSpacecraftActor::GetPlayerRespawnCapsuleHalfHeight() const
{
	return PlayerRespawnCapsuleHalfHeight;
}

float AJTSSpacecraftActor::GetPlayerRespawnClearance() const
{
	return PlayerRespawnClearance;
}

void AJTSSpacecraftActor::HandleAssistedLandingCompleted()
{
	if (FlightState != EJTSSpacecraftFlightState::LandingAssist)
	{
		return;
	}

	AJTSPlanetAnchor* const Planet = FlightPlanet.Get();
	if (!IsValid(Planet) || !RefreshGroundInfo(Planet))
	{
		CancelLandingRequest(EJTSLandingValidationFailure::NoSurface);
		return;
	}

	const FJTSSpacecraftGroundInfo GroundInfo = GetGroundInfo();
	const float AllowedHeightError = FMath::Max(12.0f, PendingLandingClearance * 0.05f);
	const bool bAtResolvedSurfaceHeight = GroundInfo.bHasGround
		&& FMath::Abs(GroundInfo.DockingHeight - PendingLandingClearance) <= AllowedHeightError;
	const bool bSurfaceAligned = GroundInfo.bHasGround
		&& FVector::DotProduct(GetActorUpVector().GetSafeNormal(), GroundInfo.SurfaceNormal.GetSafeNormal())
			>= FMath::Cos(FMath::DegreesToRadians(8.0f));
	if (!bAtResolvedSurfaceHeight || !bSurfaceAligned)
	{
		UE_LOG(LogTemp, Warning, TEXT("Landing Assist failed final surface check: Spacecraft=%s Height=%.1f Clearance=%.1f Aligned=%s"),
			*GetName(),
			GroundInfo.DockingHeight,
			PendingLandingClearance,
			bSurfaceAligned ? TEXT("TRUE") : TEXT("FALSE"));
		CancelLandingRequest(EJTSLandingValidationFailure::CollisionBlocked);
		return;
	}

	SetGroundedPlanet(Planet);
	PendingLandingSite = nullptr;
	PendingLandingTransform = FTransform::Identity;
	PendingLandingClearance = 0.0f;

	if (AJTSSpaceWorldManager* const SpaceWorldManager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
	{
		if (SpaceWorldManager->GetCurrentPlanet() == Planet)
		{
			SpaceWorldManager->SetTravelState(EJTSSpaceTravelState::Surface);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Landing Complete: Spacecraft=%s Planet=%s Site=%s"),
		*GetName(),
		*Planet->GetPlanetId().ToString(),
		*GetNameSafe(ActiveLandingSite));
}

void AJTSSpacecraftActor::HandleAssistedLandingFailed(EJTSLandingValidationFailure Failure)
{
	if (FlightState != EJTSSpacecraftFlightState::LandingAssist)
	{
		return;
	}

	CancelLandingRequest(Failure);
	if (AJTSSpaceWorldManager* const SpaceWorldManager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
	{
		if (SpaceWorldManager->GetCurrentPlanet() == FlightPlanet.Get()
			&& SpaceWorldManager->GetCurrentTravelState() == EJTSSpaceTravelState::Landing)
		{
			SpaceWorldManager->SetTravelState(EJTSSpaceTravelState::Approach);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Landing Assist cancelled: Spacecraft=%s Reason=%d"),
		*GetName(), static_cast<int32>(Failure));
}

FBox AJTSSpacecraftActor::GetResourceExclusionBounds() const
{
	if (IsValid(SpacecraftMesh) && SpacecraftMesh->IsRegistered())
	{
		const float BoundsScale = FMath::Max(FMath::Abs(SpacecraftMesh->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = SpacecraftMesh->Bounds.BoxExtent.GetAbs() / BoundsScale;
		return FBox(
			SpacecraftMesh->Bounds.Origin - PhysicalExtent,
			SpacecraftMesh->Bounds.Origin + PhysicalExtent);
	}

	return GetComponentsBoundingBox(true);
}

FVector AJTSSpacecraftActor::GetNavigationMarkerWorldLocation() const
{
	if (IsValid(SpacecraftMesh) && SpacecraftMesh->IsRegistered())
	{
		const float BoundsScale = FMath::Max(FMath::Abs(SpacecraftMesh->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = SpacecraftMesh->Bounds.BoxExtent.GetAbs() / BoundsScale;
		return SpacecraftMesh->Bounds.Origin + FVector(0.0f, 0.0f, PhysicalExtent.Z + NavigationMarkerHeightOffset);
	}

	const FBox ActorBounds = GetComponentsBoundingBox(true);
	return ActorBounds.IsValid
		? FVector(ActorBounds.GetCenter().X, ActorBounds.GetCenter().Y, ActorBounds.Max.Z + NavigationMarkerHeightOffset)
		: GetActorLocation() + FVector(0.0f, 0.0f, NavigationMarkerHeightOffset);
}

bool AJTSSpacecraftActor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return IsValid(InteractingPawn)
		&& (IsEarthCollectionActive() || IsMoonExplorationActive() || IsSpaceWorldSurfaceActive())
		&& IsPawnInBoardingRange(InteractingPawn);
}

FText AJTSSpacecraftActor::GetInteractionPrompt_Implementation(APawn* InteractingPawn) const
{
	if (!CanInteract_Implementation(InteractingPawn))
	{
		return FText::GetEmpty();
	}

	if (IsPlayerBoarded(InteractingPawn))
	{
		return FText::FromString(TEXT("[E] EXIT"));
	}

	if (IsSpaceWorldSurfaceActive() || IsEarthCollectionActive())
	{
		return FText::FromString(TEXT("HOLD [E] BOARD"));
	}

	return IsMoonExplorationActive()
		? FText::FromString(TEXT("[E] WORKSHOP"))
		: FText::GetEmpty();
}

void AJTSSpacecraftActor::Interact_Implementation(APawn* /*InteractingPawn*/)
{
}

int32 AJTSSpacecraftActor::GetResourceAmount(EJTSResourceType ResourceType) const
{
	const int32* const ResourceAmount = Storage.Find(ResourceType);
	return ResourceAmount != nullptr ? FMath::Max(0, *ResourceAmount) : 0;
}

bool AJTSSpacecraftActor::HasResource(EJTSResourceType ResourceType, int32 ResourceAmount) const
{
	return IsSupportedResourceType(ResourceType)
		&& ResourceAmount > 0
		&& GetResourceAmount(ResourceType) >= ResourceAmount;
}

bool AJTSSpacecraftActor::TryConsumeResource(EJTSResourceType ResourceType, int32 ResourceAmount)
{
	TMap<EJTSResourceType, int32> ResourceAmounts;
	ResourceAmounts.Add(ResourceType, ResourceAmount);
	return TryConsumeResourceAmounts(ResourceAmounts);
}

bool AJTSSpacecraftActor::TryConsumeResourceAmounts(const TMap<EJTSResourceType, int32>& ResourceAmounts)
{
	if (ResourceAmounts.IsEmpty())
	{
		return false;
	}

	for (const TPair<EJTSResourceType, int32>& Resource : ResourceAmounts)
	{
		if (!HasResource(Resource.Key, Resource.Value))
		{
			return false;
		}
	}

	for (const TPair<EJTSResourceType, int32>& Resource : ResourceAmounts)
	{
		int32* const StoredAmount = Storage.Find(Resource.Key);
		if (StoredAmount == nullptr)
		{
			return false;
		}

		*StoredAmount -= Resource.Value;
		if (*StoredAmount == 0)
		{
			Storage.Remove(Resource.Key);
		}
	}

	SavePersistentStorage();
	OnShipResourcesChanged.Broadcast(GetFuelCount(), GetWaterCount(), GetFoodCount());
	return true;
}

int32 AJTSSpacecraftActor::GetFuelCount() const
{
	return GetResourceAmount(EJTSResourceType::Fuel);
}

int32 AJTSSpacecraftActor::GetWaterCount() const
{
	return GetResourceAmount(EJTSResourceType::Water);
}

int32 AJTSSpacecraftActor::GetFoodCount() const
{
	return GetResourceAmount(EJTSResourceType::Food);
}

int32 AJTSSpacecraftActor::GetTotalResourceCount() const
{
	int32 TotalResourceCount = 0;
	for (const TPair<EJTSResourceType, int32>& Resource : Storage)
	{
		TotalResourceCount += FMath::Max(0, Resource.Value);
	}

	return TotalResourceCount;
}

bool AJTSSpacecraftActor::DepositResources(const TArray<EJTSResourceType>& Resources)
{
	if (Resources.IsEmpty())
	{
		return false;
	}

	TMap<EJTSResourceType, int32> ResourceAmounts;
	for (const EJTSResourceType ResourceType : Resources)
	{
		++ResourceAmounts.FindOrAdd(ResourceType);
	}

	return DepositResourceAmounts(ResourceAmounts);
}
bool AJTSSpacecraftActor::DepositResourceAmounts(const TMap<EJTSResourceType, int32>& ResourceAmounts)
{
	if (ResourceAmounts.IsEmpty())
	{
		return false;
	}

	for (const TPair<EJTSResourceType, int32>& Resource : ResourceAmounts)
	{
		if (!IsSupportedResourceType(Resource.Key) || Resource.Value <= 0)
		{
			return false;
		}
	}

	for (const TPair<EJTSResourceType, int32>& Resource : ResourceAmounts)
	{
		Storage.FindOrAdd(Resource.Key) += Resource.Value;
		UE_LOG(LogTemp, Log, TEXT("Spacecraft Deposit: Type=%s Amount=%d"), GetResourceTypeName(Resource.Key), Resource.Value);
	}

	SavePersistentStorage();
	OnShipResourcesChanged.Broadcast(GetFuelCount(), GetWaterCount(), GetFoodCount());
	return true;
}

const TMap<EJTSResourceType, int32>& AJTSSpacecraftActor::GetStorage() const
{
	return Storage;
}

bool AJTSSpacecraftActor::IsEarthCollectionActive() const
{
	UWorld* const World = GetWorld();
	const AJTSGameState* const JTSGameState = World != nullptr ? World->GetGameState<AJTSGameState>() : nullptr;
	return IsValid(JTSGameState) && JTSGameState->IsEarthCollectionActive();
}

bool AJTSSpacecraftActor::IsMoonExplorationActive() const
{
	UWorld* const World = GetWorld();
	const AJTSGameState* const JTSGameState = World != nullptr ? World->GetGameState<AJTSGameState>() : nullptr;
	return IsValid(JTSGameState) && JTSGameState->IsMoonExploration();
}

bool AJTSSpacecraftActor::IsSpaceWorldSurfaceActive() const
{
	const AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
	return IsValid(Manager) && Manager->IsSurfaceGameplayReady();
}

bool AJTSSpacecraftActor::IsSpaceWorldRuntimeActive() const
{
	return IsValid(AJTSSpaceWorldManager::FindSpaceWorldManager(this));
}

bool AJTSSpacecraftActor::IsMoonSurfaceRuntimeActive() const
{
	const UWorld* const World = GetWorld();
	if (World != nullptr && World->GetAuthGameMode<AJTSMoonGameMode>() != nullptr)
	{
		return true;
	}

	if (const AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this))
	{
		return SurfaceController->OwnsSurfaceActor(this);
	}

	return false;
}

bool AJTSSpacecraftActor::DepositPlayerResources(AJTSCharacter* Player)
{
	if (!IsValid(Player))
	{
		return false;
	}

	UJTSCarryComponent* const CarryComponent = Player->FindComponentByClass<UJTSCarryComponent>();
	if (!IsValid(CarryComponent))
	{
		return false;
	}

	TMap<EJTSResourceType, int32> ResourcesToDeposit;
	if (!CarryComponent->TryTakeAllResources(ResourcesToDeposit) || ResourcesToDeposit.IsEmpty())
	{
		return false;
	}

	TMap<EJTSResourceType, int32> ShipResourceAmounts;
	for (const TPair<EJTSResourceType, int32>& Resource : ResourcesToDeposit)
	{
		if (Resource.Key == EJTSResourceType::AntCorpse)
		{
			// Corpses deliberately remain ordinary carried items until they are submitted to the ship.
			ShipResourceAmounts.FindOrAdd(EJTSResourceType::Organic) += Resource.Value;
		}
		else
		{
			ShipResourceAmounts.FindOrAdd(Resource.Key) += Resource.Value;
		}
	}

	if (!DepositResourceAmounts(ShipResourceAmounts))
	{
		for (const TPair<EJTSResourceType, int32>& Resource : ResourcesToDeposit)
		{
			CarryComponent->TryAddResources(Resource.Key, Resource.Value);
		}
		return false;
	}

	return true;
}

void AJTSSpacecraftActor::DepositResourcesFromOverlappingPlayers()
{
	if (!IsValid(BoardingTrigger))
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	BoardingTrigger->GetOverlappingActors(OverlappingActors, AJTSCharacter::StaticClass());
	for (AActor* const OverlappingActor : OverlappingActors)
	{
		AJTSCharacter* const Character = Cast<AJTSCharacter>(OverlappingActor);
		if (!IsValid(Character))
		{
			continue;
		}

		NearbyPlayer = Character;
		DepositPlayerResources(Character);
		Character->NotifySpacecraftEntered(this);
	}
}

void AJTSSpacecraftActor::ReconcileInitialBoardingOverlaps()
{
	DepositResourcesFromOverlappingPlayers();
}

void AJTSSpacecraftActor::RestorePersistentStorage()
{
	UWorld* const World = GetWorld();
	if (World == nullptr || (!IsSpaceWorldRuntimeActive() && !IsMoonSurfaceRuntimeActive()))
	{
		return;
	}

	if (!bPersistedStorageRestoreAttempted)
	{
		bPersistedStorageRestoreAttempted = true;
		UJTSGameInstance* const GameInstance = World->GetGameInstance<UJTSGameInstance>();
		if (IsValid(GameInstance) && GameInstance->HasPersistedSpacecraftStorage())
		{
			Storage = GameInstance->GetPersistedSpacecraftStorage();
			UE_LOG(
				LogTemp,
				Log,
				TEXT("JumpToSpace Persistent Spacecraft Storage Restored: Fuel=%d Water=%.1f Food=%.1f Rock=%d Ore=%d Organic=%d"),
				GetResourceAmount(EJTSResourceType::Fuel),
				static_cast<float>(GetResourceAmount(EJTSResourceType::Water)),
				static_cast<float>(GetResourceAmount(EJTSResourceType::Food)),
				GetResourceAmount(EJTSResourceType::Rock),
				GetResourceAmount(EJTSResourceType::Ore),
				GetResourceAmount(EJTSResourceType::Organic));
		}
	}

	if (MoonWrappedActorComponent != nullptr && FakeMoonBendMaterial != nullptr)
	{
		MoonWrappedActorComponent->SetFakeMoonBendMaterial(FakeMoonBendMaterial);
	}
}

void AJTSSpacecraftActor::RestoreStorageForMoonTravel()
{
	RestorePersistentStorage();
}

void AJTSSpacecraftActor::SavePersistentStorage() const
{
	UWorld* const World = GetWorld();
	if (World == nullptr || (!IsSpaceWorldRuntimeActive() && !IsMoonSurfaceRuntimeActive()))
	{
		return;
	}

	if (UJTSGameInstance* const GameInstance = World->GetGameInstance<UJTSGameInstance>())
	{
		GameInstance->SetPersistedSpacecraftStorage(Storage);
	}
}

void AJTSSpacecraftActor::HandleGameplayPhaseChanged(EJTSGameplayPhase NewGameplayPhase)
{
	if (NewGameplayPhase == EJTSGameplayPhase::EarthCollection
		|| NewGameplayPhase == EJTSGameplayPhase::MoonExploration)
	{
		DepositResourcesFromOverlappingPlayers();
	}
}

void AJTSSpacecraftActor::HandleBoardingTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	AJTSCharacter* const Character = Cast<AJTSCharacter>(OtherActor);
	if (!IsValid(Character))
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Spacecraft BoardingTrigger Enter Player=%s"), *Character->GetName());

	NearbyPlayer = Character;
	DepositPlayerResources(Character);
	Character->NotifySpacecraftEntered(this);
}

void AJTSSpacecraftActor::HandleBoardingTriggerEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	AJTSCharacter* const Character = Cast<AJTSCharacter>(OtherActor);
	if (!IsValid(Character) || NearbyPlayer.Get() != Character)
	{
		return;
	}

	NearbyPlayer = nullptr;
	Character->NotifySpacecraftExited(this);
}
