// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Ships/JTSSpacecraftActor.h"

#include "Camera/CameraComponent.h"
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
#include "space/World/JTSMoonSurfaceController.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetSurfaceAnchor.h"
#include "space/World/JTSSpaceWorldManager.h"
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

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(SceneRoot);
	CameraBoom->TargetArmLength = 900.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 120.0f);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 8.0f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 10.0f;

	FlightCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FlightCamera"));
	FlightCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
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
	if (FlightMovementComponent != nullptr
		&& !FlightMovementComponent->OnBoostStateChanged.IsAlreadyBound(this, &AJTSSpacecraftActor::HandleFlightBoostStateChanged))
	{
		FlightMovementComponent->OnBoostStateChanged.AddDynamic(this, &AJTSSpacecraftActor::HandleFlightBoostStateChanged);
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
}

void AJTSSpacecraftActor::PossessedBy(AController* NewController)
{
	UnregisterFlightInputMappingContext();
	Super::PossessedBy(NewController);
	RegisterFlightInputMappingContext();
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
	EnhancedInputComponent->BindAction(FlightBoostAction, ETriggerEvent::Started, this, &AJTSSpacecraftActor::FlightBoostStarted);
	EnhancedInputComponent->BindAction(FlightBoostAction, ETriggerEvent::Completed, this, &AJTSSpacecraftActor::FlightBoostStopped);
	EnhancedInputComponent->BindAction(FlightBoostAction, ETriggerEvent::Canceled, this, &AJTSSpacecraftActor::FlightBoostStopped);
	EnhancedInputComponent->BindAction(FlightBrakeAction, ETriggerEvent::Started, this, &AJTSSpacecraftActor::FlightBrakeStarted);
	EnhancedInputComponent->BindAction(FlightBrakeAction, ETriggerEvent::Completed, this, &AJTSSpacecraftActor::FlightBrakeStopped);
	EnhancedInputComponent->BindAction(FlightBrakeAction, ETriggerEvent::Canceled, this, &AJTSSpacecraftActor::FlightBrakeStopped);
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
	FlightBoostAction = NewObject<UInputAction>(this, TEXT("FlightBoostAction"), RF_Transient);
	FlightBrakeAction = NewObject<UInputAction>(this, TEXT("FlightBrakeAction"), RF_Transient);
	FlightDisembarkAction = NewObject<UInputAction>(this, TEXT("FlightDisembarkAction"), RF_Transient);

	FlightForwardAction->ValueType = EInputActionValueType::Axis1D;
	FlightRightAction->ValueType = EInputActionValueType::Axis1D;
	FlightVerticalAction->ValueType = EInputActionValueType::Axis1D;
	FlightRollAction->ValueType = EInputActionValueType::Axis1D;
	FlightLookYawAction->ValueType = EInputActionValueType::Axis1D;
	FlightLookPitchAction->ValueType = EInputActionValueType::Axis1D;
	FlightBoostAction->ValueType = EInputActionValueType::Boolean;
	FlightBrakeAction->ValueType = EInputActionValueType::Boolean;
	FlightDisembarkAction->ValueType = EInputActionValueType::Boolean;

	FlightInputMappingContext->MapKey(FlightForwardAction, EKeys::W);
	FlightInputMappingContext->MapKey(FlightRightAction, EKeys::D);
	FlightInputMappingContext->MapKey(FlightVerticalAction, EKeys::SpaceBar);
	FlightInputMappingContext->MapKey(FlightRollAction, EKeys::E);
	FlightInputMappingContext->MapKey(FlightLookYawAction, EKeys::MouseX);
	FlightInputMappingContext->MapKey(FlightLookPitchAction, EKeys::MouseY);
	FlightInputMappingContext->MapKey(FlightBoostAction, EKeys::LeftShift);
	FlightInputMappingContext->MapKey(FlightBrakeAction, EKeys::C);
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
		FlightMovementComponent->AddPitchInput(Value.Get<float>());
	}
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
	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->SetTargetPlanet(Planet);
	}
}

bool AJTSSpacecraftActor::BeginAssistedLanding(const FTransform& LandingTransform, float DurationSeconds)
{
	ClearGroundedPlanet();
	if (FlightMovementComponent != nullptr)
	{
		return FlightMovementComponent->BeginAssistedLanding(LandingTransform, DurationSeconds);
	}
	return false;
}

void AJTSSpacecraftActor::SetGroundedPlanet(AJTSPlanetAnchor* InPlanetAnchor)
{
	GroundedPlanet = InPlanetAnchor;
	bIsGroundedOnPlanet = IsValid(InPlanetAnchor);
	if (!bIsGroundedOnPlanet)
	{
		return;
	}

	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->SetTargetPlanet(InPlanetAnchor);
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

	AJTSPlanetAnchor* const Planet = GroundedPlanet.Get();
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
	if (FlightMovementComponent != nullptr && !FlightMovementComponent->IsActive())
	{
		FlightMovementComponent->Activate(true);
	}
}

AJTSPlanetAnchor* AJTSSpacecraftActor::GetGroundedPlanet() const
{
	return bIsGroundedOnPlanet ? GroundedPlanet.Get() : nullptr;
}

bool AJTSSpacecraftActor::IsGroundedOnPlanet() const
{
	return bIsGroundedOnPlanet && IsValid(GroundedPlanet);
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

	const FQuat SurfaceRotation = FRotationMatrix::MakeFromXZ(SurfaceForward, SurfaceUp).ToQuat();
	const FVector GroundedLocation = SurfaceTransform.GetLocation()
		+ SurfaceUp * FMath::Max(0.0f, ShipGroundClearance);
	SetActorLocationAndRotation(
		GroundedLocation,
		SurfaceRotation,
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
		&& (IsEarthCollectionActive() || IsMoonExplorationActive())
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
