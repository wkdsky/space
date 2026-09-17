// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Ships/JTSSpacecraftActor.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
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
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Math/RotationMatrix.h"
#include "space/Components/JTSCarryComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSPlayerEquipmentComponent.h"
#include "space/Components/JTSSpacecraftFlightMovementComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetLandingManager.h"
#include "space/World/JTSPlanetLandingSite.h"
#include "space/World/JTSPlanetSurfaceAnchor.h"
#include "space/World/JTSSpaceWorldManager.h"
#include "space/World/JTSSurfacePlacementBounds.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
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
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(10.0f);

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
	FlightCameraBoom->TargetArmLength = FlightCameraDefaultArmLength;
	FlightCameraBoom->SocketOffset = FlightCameraSocketOffset;
	FlightCameraBoom->bUsePawnControlRotation = true;
	// The ship is often parked directly on uneven terrain. A spring-arm collision retraction can
	// collapse an otherwise valid exterior view to the cockpit, so driving always keeps its full arm.
	FlightCameraBoom->bDoCollisionTest = false;
	FlightCameraBoom->bEnableCameraLag = true;
	FlightCameraBoom->CameraLagSpeed = 14.0f;
	// Mouse look must not pass through a rotation lag. Location lag is retained only to soften ship motion.
	FlightCameraBoom->bEnableCameraRotationLag = false;
	CameraBoom = FlightCameraBoom;

	FlightCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FlightCamera"));
	FlightCamera->SetupAttachment(FlightCameraBoom, USpringArmComponent::SocketName);
	FlightCamera->bUsePawnControlRotation = false;
	FlightCamera->SetFieldOfView(NormalFlightFOV);

	BoardingTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("BoardingTrigger"));
	BoardingTrigger->SetupAttachment(SceneRoot);
	BoardingTrigger->SetSphereRadius(300.0f);
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

}

void AJTSSpacecraftActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateBoardingTriggerFromSpacecraftMeshBounds();
}

bool AJTSSpacecraftActor::GetPhysicalSpacecraftMeshLocalBounds(FBox& OutLocalBounds) const
{
	OutLocalBounds = FBox(ForceInit);
	if (!IsValid(SpacecraftMesh))
	{
		return false;
	}

	// Use physical mesh bounds for both proximity and target calculations. The local bounds are
	// transformed later, so the selected mesh's relative transform and scale remain authoritative.
	const FBoxSphereBounds LocalBounds = SpacecraftMesh->CalcBounds(FTransform::Identity);
	const float BoundsScale = FMath::Max(FMath::Abs(SpacecraftMesh->BoundsScale), KINDA_SMALL_NUMBER);
	const FVector PhysicalExtent = LocalBounds.BoxExtent.GetAbs() / BoundsScale;
	if (PhysicalExtent.IsNearlyZero())
	{
		return false;
	}

	OutLocalBounds = FBox(LocalBounds.Origin - PhysicalExtent, LocalBounds.Origin + PhysicalExtent);
	return OutLocalBounds.IsValid != 0;
}

void AJTSSpacecraftActor::UpdateBoardingTriggerFromSpacecraftMeshBounds()
{
	if (!bAutoSizeBoardingTriggerFromSpacecraftMesh || !IsValid(BoardingTrigger) || !IsValid(SceneRoot))
	{
		return;
	}

	FBox PhysicalLocalBounds(ForceInit);
	if (!GetPhysicalSpacecraftMeshLocalBounds(PhysicalLocalBounds) || !IsValid(SpacecraftMesh))
	{
		return;
	}

	const FTransform RootTransform = SceneRoot->GetComponentTransform();
	const FTransform MeshTransform = SpacecraftMesh->GetComponentTransform();
	const FVector TriggerCenter = RootTransform.InverseTransformPosition(
		MeshTransform.TransformPosition(PhysicalLocalBounds.GetCenter()));
	float RequiredRadius = 0.0f;
	for (int32 XSign = -1; XSign <= 1; XSign += 2)
	{
		for (int32 YSign = -1; YSign <= 1; YSign += 2)
		{
			for (int32 ZSign = -1; ZSign <= 1; ZSign += 2)
			{
				const FVector MeshLocalCorner(
					XSign > 0 ? PhysicalLocalBounds.Max.X : PhysicalLocalBounds.Min.X,
					YSign > 0 ? PhysicalLocalBounds.Max.Y : PhysicalLocalBounds.Min.Y,
					ZSign > 0 ? PhysicalLocalBounds.Max.Z : PhysicalLocalBounds.Min.Z);
				const FVector CornerInRoot = RootTransform.InverseTransformPosition(
					MeshTransform.TransformPosition(MeshLocalCorner));
				RequiredRadius = FMath::Max(RequiredRadius, FVector::Distance(CornerInRoot, TriggerCenter));
			}
		}
	}

	const FVector TriggerRelativeScale = BoardingTrigger->GetRelativeScale3D().GetAbs();
	const float TriggerShapeScale = FMath::Max3(
		TriggerRelativeScale.X,
		TriggerRelativeScale.Y,
		TriggerRelativeScale.Z);
	const float DesiredScaledRadius = FMath::Max(
		FMath::Max(1.0f, BoardingTriggerMinimumRadius),
		RequiredRadius + FMath::Max(0.0f, BoardingProximityMargin));
	BoardingTrigger->SetRelativeLocation(TriggerCenter, false, nullptr, ETeleportType::TeleportPhysics);
	BoardingTrigger->SetSphereRadius(DesiredScaledRadius / FMath::Max(TriggerShapeScale, KINDA_SMALL_NUMBER), true);
}

void AJTSSpacecraftActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateFlightCamera(DeltaSeconds);
}

void AJTSSpacecraftActor::BeginPlay()
{
	Super::BeginPlay();
	// Blueprints select the visual mesh, so size this after their component defaults have been applied.
	UpdateBoardingTriggerFromSpacecraftMeshBounds();
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
	if (FlightMovementComponent != nullptr
		&& !FlightMovementComponent->OnAssistedLandingPhaseChanged.IsBoundToObject(this))
	{
		FlightMovementComponent->OnAssistedLandingPhaseChanged.AddUObject(this, &AJTSSpacecraftActor::HandleAssistedLandingPhaseChanged);
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
	if (HasAuthority())
	{
		SyncReplicatedStorage();
	}

	if (AJTSGameState* const JTSGameState = World->GetGameState<AJTSGameState>())
	{
		if (HasAuthority() && JTSGameState->GetActiveSpacecraft() == nullptr)
		{
			JTSGameState->SetActiveSpacecraft(this);
		}
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
		FlightMovementComponent->OnAssistedLandingPhaseChanged.RemoveAll(this);
	}
	SavePersistentStorage();

	if (UWorld* const World = GetWorld())
	{
		if (AJTSGameState* const JTSGameState = World->GetGameState<AJTSGameState>())
		{
			JTSGameState->OnGameplayPhaseChanged.RemoveDynamic(this, &AJTSSpacecraftActor::HandleGameplayPhaseChanged);
		}
	}

	AJTSCharacter* const NearbyCharacter = NearbyPlayer.Get();
	for (const FJTSSpacecraftOccupantState& Occupant : Occupants)
	{
		if (HasAuthority() && Occupant.PlayerState != nullptr)
		{
			Occupant.PlayerState->SetBoarded(false);
		}
		if (AJTSCharacter* const BoardedCharacter = FindBoardedCharacterForPlayerState(Occupant.PlayerState))
		{
			BoardedCharacter->HandleSpacecraftInvalidated(this);
		}
	}
	if (NearbyCharacter != nullptr && !NearbyCharacter->IsBoarded())
	{
		NearbyCharacter->HandleSpacecraftInvalidated(this);
	}

	BoardedPlayer = nullptr;
	Occupants.Reset();
	DriverPlayerState = nullptr;
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
	LocalFlightInput.Throttle = Value.Get<float>();
	SubmitFlightInput();
}

void AJTSSpacecraftActor::FlightMoveRight(const FInputActionValue& Value)
{
	LocalFlightInput.Strafe = Value.Get<float>();
	SubmitFlightInput();
}

void AJTSSpacecraftActor::FlightMoveVertical(const FInputActionValue& Value)
{
	if (Value.Get<float>() > KINDA_SMALL_NUMBER && IsSpaceWorldSurfaceActive())
	{
		if (HasAuthority()) ServerRequestSurfaceTakeoff_Implementation(); else ServerRequestSurfaceTakeoff();
	}
	LocalFlightInput.Vertical = Value.Get<float>();
	SubmitFlightInput();
}

void AJTSSpacecraftActor::FlightRoll(const FInputActionValue& Value)
{
	LocalFlightInput.Roll = Value.Get<float>();
	SubmitFlightInput();
}

void AJTSSpacecraftActor::FlightLookYaw(const FInputActionValue& Value)
{
	const float LookValue = Value.Get<float>();
	AddControllerYawInput(LookValue * FlightCameraLookSensitivity);
	LocalFlightInput.Yaw = LookValue;
	SubmitFlightInput();
	// Mouse look is a delta, unlike throttle/strafe. Do not resend the last delta with a later key input.
	LocalFlightInput.Yaw = 0.0f;
}

void AJTSSpacecraftActor::FlightLookPitch(const FInputActionValue& Value)
{
	const AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(GetController());
	const float PitchDirection = PlayerController != nullptr && PlayerController->IsLookYAxisInverted() ? -1.0f : 1.0f;
	const float LookValue = Value.Get<float>() * PitchDirection;
	AddControllerPitchInput(LookValue * FlightCameraLookSensitivity);
	LocalFlightInput.Pitch = LookValue;
	SubmitFlightInput();
	// Mouse look is a delta, unlike throttle/strafe. Do not resend the last delta with a later key input.
	LocalFlightInput.Pitch = 0.0f;
}

void AJTSSpacecraftActor::FlightCameraZoom(const FInputActionValue& Value)
{
	AdjustFlightCameraDistance(Value.Get<float>());
}

void AJTSSpacecraftActor::FlightBoostStarted(const FInputActionValue& Value)
{
	LocalFlightInput.bBoosting = Value.Get<bool>();
	SubmitFlightInput();
}

void AJTSSpacecraftActor::FlightBoostStopped(const FInputActionValue& Value)
{
	LocalFlightInput.bBoosting = false;
	SubmitFlightInput();
}

void AJTSSpacecraftActor::FlightBrakeStarted(const FInputActionValue& Value)
{
	LocalFlightInput.bBraking = Value.Get<bool>();
	SubmitFlightInput();
}

void AJTSSpacecraftActor::FlightBrakeStopped(const FInputActionValue& Value)
{
	LocalFlightInput.bBraking = false;
	SubmitFlightInput();
}

void AJTSSpacecraftActor::FlightLandingStarted(const FInputActionValue& Value)
{
	if (Value.Get<bool>())
	{
		if (HasAuthority()) ServerRequestLanding_Implementation(); else ServerRequestLanding();
	}
}

void AJTSSpacecraftActor::FlightDisembarkStarted(const FInputActionValue& Value)
{
	if (!Value.Get<bool>())
	{
		return;
	}

	// The client can be one replication update behind the authoritative landed state. Always submit
	// the driver's intent through the possessed ship; ServerRequestDisembark performs the real
	// occupancy and landed checks on the server.
	if (HasAuthority())
	{
		ServerRequestDisembark_Implementation();
	}
	else
	{
		ServerRequestDisembark();
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
	const float ConfiguredDefaultArmLength = FMath::Clamp(
		FlightCameraDefaultArmLength,
		MinimumArmLength,
		MaximumArmLength);
	// Old Blueprints may still serialize the former 900 cm boom value. Never let that legacy value
	// pull the exterior driving view back into the spacecraft.
	CurrentFlightCameraArmLength = FMath::Clamp(
		FMath::Max(FlightCameraBoom->TargetArmLength, ConfiguredDefaultArmLength),
		MinimumArmLength,
		MaximumArmLength);
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
	if (!HasAuthority())
	{
		return false;
	}
	return DepositPlayerResources(Cast<AJTSCharacter>(InteractingPawn));
}

EJTSShopPurchaseResult AJTSSpacecraftActor::TryPurchase(AJTSCharacter* Player, EJTSItemId ItemId)
{
	// The workshop belongs exclusively to the active SpaceWorld surface phase. This validation
	// mirrors the prompt gate so a client cannot open/purchase from it during Earth collection.
	if (!HasAuthority() || !IsSpaceWorldSurfaceActive() || !IsValid(Player) || !IsPawnInBoardingRange(Player))
	{
		return EJTSShopPurchaseResult::DeliveryFailed;
	}

	TMap<EJTSResourceType, int32> Costs;
	if (!BuildShopCosts(ItemId, Costs))
	{
		return EJTSShopPurchaseResult::InvalidItem;
	}
	if (!TryConsumeResourceAmounts(Costs))
	{
		return EJTSShopPurchaseResult::InsufficientResources;
	}

	bool bDropped = false;
	if (!DeliverShopPurchase(Player, UJTSItemDefinitionLibrary::MakeInstance(ItemId), bDropped))
	{
		// A transaction either yields the requested item or restores every material.
		DepositResourceAmounts(Costs);
		return EJTSShopPurchaseResult::DeliveryFailed;
	}

	return bDropped ? EJTSShopPurchaseResult::SucceededDropped : EJTSShopPurchaseResult::Succeeded;
}

bool AJTSSpacecraftActor::TryBoardPlayer(APawn* InteractingPawn)
{
	if (!HasAuthority())
	{
		return false;
	}
	AJTSCharacter* const InteractingCharacter = Cast<AJTSCharacter>(InteractingPawn);
	AJTSPlayerState* const OccupantPlayerState = IsValid(InteractingCharacter) ? InteractingCharacter->GetPlayerState<AJTSPlayerState>() : nullptr;
	const bool bEarthCollectionActive = IsEarthCollectionActive();
	const bool bSpaceWorldSurfaceActive = IsSpaceWorldSurfaceActive();
	if ((!bEarthCollectionActive && !bSpaceWorldSurfaceActive)
		|| !IsValid(InteractingCharacter)
		|| !IsValid(OccupantPlayerState)
		|| IsPlayerStateOccupying(OccupantPlayerState)
		|| Occupants.Num() >= FMath::Clamp(MaximumOccupants, 1, 4)
		|| !IsPawnInBoardingRange(InteractingCharacter))
	{
		return false;
	}

	APlayerController* const PlayerController = bSpaceWorldSurfaceActive
		? Cast<APlayerController>(InteractingCharacter->GetController())
		: nullptr;
	if (bSpaceWorldSurfaceActive && !IsValid(PlayerController))
	{
		return false;
	}

	if (!InteractingCharacter->EnterBoardedState(this))
	{
		return false;
	}
	OccupantPlayerState->SetBoarded(true);

	NearbyPlayer = InteractingCharacter;
	FJTSSpacecraftOccupantState& Occupant = Occupants.AddDefaulted_GetRef();
	Occupant.PlayerState = OccupantPlayerState;
	Occupant.SeatRole = EJTSSpacecraftSeatRole::Passenger;
	if (!bSpaceWorldSurfaceActive)
	{
		OnRep_Occupants();
		return true;
	}

	if (DriverPlayerState == nullptr)
	{
		DriverPlayerState = OccupantPlayerState;
		Occupant.SeatRole = EJTSSpacecraftSeatRole::Driver;
		BoardedPlayer = InteractingCharacter;
		PlayerController->Possess(this);
		if (GetController() != PlayerController)
		{
			RemoveOccupant(OccupantPlayerState);
			OccupantPlayerState->SetBoarded(false);
			DriverPlayerState = nullptr;
			BoardedPlayer = nullptr;
			InteractingCharacter->ExitBoardedState(this);
			if (PlayerController->GetPawn() != InteractingCharacter)
			{
				PlayerController->Possess(InteractingCharacter);
			}
			return false;
		}
		if (PlayerController->IsLocalController())
		{
			PlayerController->FlushPressedKeys();
		}
		if (AJTSPlayerController* const JTSPlayerController = Cast<AJTSPlayerController>(PlayerController))
		{
			JTSPlayerController->SetSpacecraftCameraViewTarget(this);
		}
	}
	OnRep_Occupants();
	return true;
}

bool AJTSSpacecraftActor::TryDisembarkPlayer(APawn* InteractingPawn)
{
	if (!HasAuthority())
	{
		return false;
	}
	AJTSCharacter* BoardedCharacter = Cast<AJTSCharacter>(InteractingPawn);
	APlayerController* const PlayerController = Cast<APlayerController>(GetController());
	if (BoardedCharacter == nullptr && InteractingPawn == this && PlayerController != nullptr)
	{
		BoardedCharacter = FindBoardedCharacterForPlayerState(PlayerController->GetPlayerState<AJTSPlayerState>());
	}
	AJTSPlayerState* const OccupantPlayerState = BoardedCharacter != nullptr ? BoardedCharacter->GetPlayerState<AJTSPlayerState>() : nullptr;
	const bool bPlayerIsDriving = IsValid(PlayerController) && OccupantPlayerState != nullptr && DriverPlayerState == OccupantPlayerState;
	if (!IsValid(BoardedCharacter)
		|| !IsValid(OccupantPlayerState)
		|| !IsPlayerStateOccupying(OccupantPlayerState)
		|| !CanDisembarkPlayer(BoardedCharacter))
	{
		return false;
	}

	RemoveOccupant(OccupantPlayerState);
	OccupantPlayerState->SetBoarded(false);
	if (bPlayerIsDriving)
	{
		DriverPlayerState = nullptr;
		BoardedPlayer = nullptr;
	}
	if (bPlayerIsDriving)
	{
		PlayerController->UnPossess();
	}
	BoardedCharacter->ExitBoardedState(this);
	if (bPlayerIsDriving && IsValid(PlayerController))
	{
		PlayerController->Possess(BoardedCharacter);
		if (AJTSPlayerController* const JTSPlayerController = Cast<AJTSPlayerController>(PlayerController))
		{
			JTSPlayerController->RestoreCharacterCameraViewTarget(BoardedCharacter);
		}
		else
		{
			PlayerController->SetViewTargetWithBlend(BoardedCharacter, 0.35f);
		}
	}
	OnRep_Occupants();
	return true;
}

bool AJTSSpacecraftActor::TryDisembarkPlayerForController(APlayerController* PlayerController)
{
	if (!HasAuthority() || PlayerController == nullptr)
	{
		return false;
	}
	AJTSPlayerState* const ControllerPlayerState = PlayerController->GetPlayerState<AJTSPlayerState>();
	AJTSCharacter* BoardedCharacter = BoardedPlayer.Get();
	if (!IsValid(BoardedCharacter) || BoardedCharacter->GetPlayerState<AJTSPlayerState>() != ControllerPlayerState)
	{
		BoardedCharacter = FindBoardedCharacterForPlayerState(ControllerPlayerState);
	}
	return TryDisembarkPlayer(BoardedCharacter);
}

bool AJTSSpacecraftActor::IsPlayerBoarded(const APawn* InteractingPawn) const
{
	const AJTSCharacter* const InteractingCharacter = Cast<AJTSCharacter>(InteractingPawn);
	return IsValid(InteractingCharacter) && IsPlayerStateOccupying(InteractingCharacter->GetPlayerState<AJTSPlayerState>());
}

bool AJTSSpacecraftActor::CanDisembarkPlayer(const APawn* InteractingPawn) const
{
	return IsPlayerBoarded(InteractingPawn)
		&& (IsEarthCollectionActive() || IsLanded());
}

bool AJTSSpacecraftActor::HasBoardedPlayer() const
{
	return !Occupants.IsEmpty();
}

AJTSCharacter* AJTSSpacecraftActor::GetBoardedPlayer() const
{
	return IsValid(BoardedPlayer) ? BoardedPlayer.Get() : FindBoardedCharacterForPlayerState(DriverPlayerState);
}

bool AJTSSpacecraftActor::IsPawnInBoardingRange(const APawn* InteractingPawn) const
{
	if (!IsValid(InteractingPawn) || !IsValid(BoardingTrigger))
	{
		return false;
	}

	return BoardingTrigger->IsOverlappingActor(InteractingPawn)
		|| FVector::DistSquared(GetBoardingInteractionCenter(), InteractingPawn->GetActorLocation())
			<= FMath::Square(GetBoardingInteractionRadius());
}

FVector AJTSSpacecraftActor::GetBoardingInteractionCenter() const
{
	return IsValid(BoardingTrigger) ? BoardingTrigger->GetComponentLocation() : GetActorLocation();
}

float AJTSSpacecraftActor::GetBoardingInteractionRadius() const
{
	return IsValid(BoardingTrigger) ? FMath::Max(0.0f, BoardingTrigger->GetScaledSphereRadius()) : 0.0f;
}

FVector AJTSSpacecraftActor::GetBoardingInteractionTargetWorldLocation(const FVector& ReferenceLocation) const
{
	FBox PhysicalLocalBounds(ForceInit);
	if (GetPhysicalSpacecraftMeshLocalBounds(PhysicalLocalBounds) && IsValid(SpacecraftMesh))
	{
		const FTransform MeshTransform = SpacecraftMesh->GetComponentTransform();
		const FVector LocalReferenceLocation = MeshTransform.InverseTransformPosition(ReferenceLocation);
		const FVector ClosestLocalPoint(
			FMath::Clamp(LocalReferenceLocation.X, PhysicalLocalBounds.Min.X, PhysicalLocalBounds.Max.X),
			FMath::Clamp(LocalReferenceLocation.Y, PhysicalLocalBounds.Min.Y, PhysicalLocalBounds.Max.Y),
			FMath::Clamp(LocalReferenceLocation.Z, PhysicalLocalBounds.Min.Z, PhysicalLocalBounds.Max.Z));
		return MeshTransform.TransformPosition(ClosestLocalPoint);
	}

	return IsValid(BoardingTrigger) ? BoardingTrigger->GetComponentLocation() : GetActorLocation();
}

float AJTSSpacecraftActor::GetExteriorHullSupportDistance(const FVector& WorldDirection) const
{
	const FVector SafeDirection = WorldDirection.GetSafeNormal();
	if (SafeDirection.IsNearlyZero())
	{
		return 0.0f;
	}

	// The Blueprint mesh is the visual hull the player must clear. Its physical local bounds retain
	// authored component scale, rotation, and offset, unlike FlightCollision's deliberately compact
	// movement proxy.
	FJTSSurfaceVisualProjectionBounds VisualBounds;
	if (IsValid(SpacecraftMesh)
		&& SpacecraftMesh->IsRegistered()
		&& JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(
			SpacecraftMesh,
			GetActorLocation(),
			SafeDirection,
			VisualBounds)
		&& VisualBounds.bIsValid)
	{
		return FMath::Max(0.0f, VisualBounds.HighestProjectionFromRoot);
	}

	if (IsValid(FlightCollision))
	{
		const FVector HullExtent = FlightCollision->GetScaledBoxExtent();
		const FTransform CollisionTransform = FlightCollision->GetComponentTransform();
		const float HullSupport = FMath::Abs(FVector::DotProduct(CollisionTransform.GetUnitAxis(EAxis::X), SafeDirection)) * HullExtent.X
			+ FMath::Abs(FVector::DotProduct(CollisionTransform.GetUnitAxis(EAxis::Y), SafeDirection)) * HullExtent.Y
			+ FMath::Abs(FVector::DotProduct(CollisionTransform.GetUnitAxis(EAxis::Z), SafeDirection)) * HullExtent.Z;
		const float CenterProjection = FVector::DotProduct(
			CollisionTransform.GetLocation() - GetActorLocation(),
			SafeDirection);
		return FMath::Max(0.0f, CenterProjection + HullSupport);
	}

	return 0.0f;
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
		FlightCameraBoom->SocketOffset = FlightCameraSocketOffset;
		// A driving camera orbits from controller rotation. It is not welded to the ship's transform.
		FlightCameraBoom->bUsePawnControlRotation = true;
		FlightCameraBoom->SetUsingAbsoluteRotation(false);
		FlightCameraBoom->SetRelativeRotation(FRotator::ZeroRotator);
		FlightCameraBoom->bDoCollisionTest = false;
		FlightCameraBoom->bEnableCameraLag = true;
		FlightCameraBoom->CameraLagSpeed = 14.0f;
		FlightCameraBoom->bEnableCameraRotationLag = false;
		FlightCameraBoom->bInheritPitch = true;
		FlightCameraBoom->bInheritYaw = true;
		FlightCameraBoom->bInheritRoll = false;
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
		FlightCamera->SetRelativeRotation(FRotator::ZeroRotator);
		FlightCamera->SetActive(true);
	}

	if (APlayerController* const PlayerController = Cast<APlayerController>(GetController());
		IsValid(PlayerController) && PlayerController->IsLocalController())
	{
		FRotator InitialCameraRotation = GetActorRotation();
		InitialCameraRotation.Roll = 0.0f;
		PlayerController->SetControlRotation(InitialCameraRotation);
		if (APlayerCameraManager* const CameraManager = PlayerController->PlayerCameraManager)
		{
			CameraManager->ViewPitchMin = FMath::Min(FlightCameraPitchMin, FlightCameraPitchMax);
			CameraManager->ViewPitchMax = FMath::Max(FlightCameraPitchMin, FlightCameraPitchMax);
		}
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
	if (!HasAuthority())
	{
		return;
	}
	FlightPlanet = Planet;
	if (FlightMovementComponent != nullptr)
	{
		FlightMovementComponent->SetTargetPlanet(Planet);
	}
}

void AJTSSpacecraftActor::InitializeForPlanetArrival(AJTSPlanetAnchor* Planet)
{
	if (!HasAuthority() || !IsValid(Planet))
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
	LandingAssistPhase = EJTSSpacecraftLandingAssistPhase::None;

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
	if (!HasAuthority())
	{
		return false;
	}
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
	if (!HasAuthority())
	{
		return false;
	}
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
	if (!HasAuthority())
	{
		return;
	}
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
	if (FlightState != EJTSSpacecraftFlightState::Landed)
	{
		LandingAssistPhase = EJTSSpacecraftLandingAssistPhase::None;
	}
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

EJTSSpacecraftLandingAssistPhase AJTSSpacecraftActor::GetLandingAssistPhase() const
{
	return LandingAssistPhase;
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
	if (!HasAuthority())
	{
		return false;
	}
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
	if (!HasAuthority())
	{
		return;
	}
	SetFlightTargetPlanet(InPlanetAnchor);
	GroundedPlanet = InPlanetAnchor;
	bIsGroundedOnPlanet = IsValid(InPlanetAnchor);
	if (!bIsGroundedOnPlanet)
	{
		FlightState = EJTSSpacecraftFlightState::Flying;
		LandingAssistPhase = EJTSSpacecraftLandingAssistPhase::None;
		return;
	}
	FlightState = EJTSSpacecraftFlightState::Landed;
	LandingAssistPhase = EJTSSpacecraftLandingAssistPhase::Touchdown;
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
	if (!HasAuthority())
	{
		return false;
	}
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
	LandingAssistPhase = EJTSSpacecraftLandingAssistPhase::None;
	if (AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
	{
		Manager->SetTravelState(EJTSSpaceTravelState::Takeoff);
	}

	UE_LOG(LogTemp, Log, TEXT("Spacecraft %s began surface takeoff from %s."), *GetName(), *Planet->GetPlanetId().ToString());
	return true;
}

void AJTSSpacecraftActor::ClearGroundedPlanet()
{
	if (!HasAuthority())
	{
		return;
	}
	GroundedPlanet = nullptr;
	bIsGroundedOnPlanet = false;
	ActiveLandingSite = nullptr;
	PendingLandingSite = nullptr;
	PendingLandingTransform = FTransform::Identity;
	PendingLandingClearance = 0.0f;
	LandingAssistPhase = EJTSSpacecraftLandingAssistPhase::None;
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
	if (!HasAuthority())
	{
		return false;
	}
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

void AJTSSpacecraftActor::HandleAssistedLandingPhaseChanged(EJTSSpacecraftLandingAssistPhase NewPhase)
{
	if (HasAuthority())
	{
		LandingAssistPhase = NewPhase;
	}
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
	if (const AJTSPlanetAnchor* const Planet = GetGroundedPlanet())
	{
		FVector NavigationUp = Planet->GetRadialUpVector(GetActorLocation()).GetSafeNormal();
		if (NavigationUp.IsNearlyZero())
		{
			NavigationUp = FVector::UpVector;
		}

		if (IsValid(SpacecraftMesh) && SpacecraftMesh->IsRegistered())
		{
			FBox PhysicalLocalBounds(ForceInit);
			FJTSSurfaceVisualProjectionBounds VisualBounds;
			if (GetPhysicalSpacecraftMeshLocalBounds(PhysicalLocalBounds)
				&& JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(
					SpacecraftMesh,
					GetActorLocation(),
					NavigationUp,
					VisualBounds))
			{
				// HighestPoint is an extreme bounds corner. It is useful for collision clearance, but
				// using it as a HUD anchor makes a long or tilted hull's prompt appear at one side of
				// the ship. Keep the bounds center in the tangent plane and lift it to the same
				// local-surface top projection instead.
				const FVector VisualCenter = SpacecraftMesh->GetComponentTransform().TransformPosition(
					PhysicalLocalBounds.GetCenter());
				const float CenterProjection = FVector::DotProduct(VisualCenter - GetActorLocation(), NavigationUp);
				const float CenterToTopOffset = VisualBounds.HighestProjectionFromRoot - CenterProjection;
				return VisualCenter + NavigationUp * (CenterToTopOffset + NavigationMarkerHeightOffset);
			}
		}

		return GetActorLocation() + NavigationUp * NavigationMarkerHeightOffset;
	}

	// Earth and other non-planet contexts retain World-Z marker behavior.
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
	if (!IsValid(InteractingPawn))
	{
		return false;
	}

	if (IsPlayerBoarded(InteractingPawn))
	{
		return CanDisembarkPlayer(InteractingPawn);
	}

	return (IsEarthCollectionActive() || IsSpaceWorldSurfaceActive())
		&& IsPawnInBoardingRange(InteractingPawn);
}

FText AJTSSpacecraftActor::GetInteractionPrompt_Implementation(APawn* InteractingPawn) const
{
	if (!CanInteract_Implementation(InteractingPawn))
	{
		return FText::GetEmpty();
	}

	if (!IsPlayerBoarded(InteractingPawn))
	{
		if (IsEarthCollectionActive())
		{
			return FText::FromString(TEXT("HOLD [F] BOARD"));
		}

		return FText::FromString(TEXT("[E] SUPPLY\nHOLD [F] BOARD"));
	}

	return CanDisembarkPlayer(InteractingPawn)
		? FText::FromString(TEXT("[F] DISEMBARK"))
		: FText::GetEmpty();
}

void AJTSSpacecraftActor::Interact_Implementation(APawn* InteractingPawn)
{
	// Earth exposes the ship as an interaction target solely so its hold-F boarding path and prompt
	// use the shared interaction validation. The supply screen remains a SpaceWorld-only feature.
	if (!HasAuthority()
		|| !IsSpaceWorldSurfaceActive()
		|| !CanInteract_Implementation(InteractingPawn)
		|| IsPlayerBoarded(InteractingPawn))
	{
		return;
	}

	if (AJTSPlayerController* const PlayerController = Cast<AJTSPlayerController>(InteractingPawn->GetController()))
	{
		PlayerController->ClientOpenSpaceShop(this);
	}
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
	if (!HasAuthority() || ResourceAmounts.IsEmpty())
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

	SyncReplicatedStorage();
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
	if (!HasAuthority() || ResourceAmounts.IsEmpty())
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

	SyncReplicatedStorage();
	SavePersistentStorage();
	OnShipResourcesChanged.Broadcast(GetFuelCount(), GetWaterCount(), GetFoodCount());
	return true;
}

const TMap<EJTSResourceType, int32>& AJTSSpacecraftActor::GetStorage() const
{
	return Storage;
}

void AJTSSpacecraftActor::SyncReplicatedStorage()
{
	if (!HasAuthority()) return;
	ReplicatedStorage.Reset();
	for (const TPair<EJTSResourceType, int32>& Entry : Storage)
	{
		if (Entry.Value > 0)
		{
			FJTSResourceAmount& ReplicatedEntry = ReplicatedStorage.AddDefaulted_GetRef();
			ReplicatedEntry.ResourceType = Entry.Key;
			ReplicatedEntry.Amount = Entry.Value;
		}
	}
	OnRep_Storage();
}

void AJTSSpacecraftActor::RebuildStorageFromReplicatedArray()
{
	Storage.Reset();
	for (const FJTSResourceAmount& Entry : ReplicatedStorage)
	{
		if (IsSupportedResourceType(Entry.ResourceType) && Entry.Amount > 0)
		{
			Storage.FindOrAdd(Entry.ResourceType) += Entry.Amount;
		}
	}
}

void AJTSSpacecraftActor::OnRep_Storage()
{
	RebuildStorageFromReplicatedArray();
	OnShipResourcesChanged.Broadcast(GetFuelCount(), GetWaterCount(), GetFoodCount());
}

void AJTSSpacecraftActor::OnRep_Occupants()
{
	// Character attachment and its replicated BoardedSpacecraft property drive visual presentation.
	// This callback exists for UI/Blueprint observers of seat changes.
}

void AJTSSpacecraftActor::OnRep_FlightState()
{
	if (FlightMovementComponent == nullptr)
	{
		return;
	}

	FlightMovementComponent->SetTargetPlanet(FlightPlanet);
	if (FlightState == EJTSSpacecraftFlightState::Landed || bIsGroundedOnPlanet)
	{
		FlightMovementComponent->ClearInput();
		FlightMovementComponent->Deactivate();
	}
	else if (!FlightMovementComponent->IsActive())
	{
		FlightMovementComponent->Activate(true);
	}
}

bool AJTSSpacecraftActor::IsPlayerStateOccupying(const AJTSPlayerState* InPlayerState) const
{
	return InPlayerState != nullptr && Occupants.ContainsByPredicate([InPlayerState](const FJTSSpacecraftOccupantState& Occupant)
	{
		return Occupant.PlayerState == InPlayerState;
	});
}

AJTSCharacter* AJTSSpacecraftActor::FindBoardedCharacterForPlayerState(const AJTSPlayerState* InPlayerState) const
{
	if (InPlayerState == nullptr || GetWorld() == nullptr) return nullptr;
	for (TActorIterator<AJTSCharacter> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->GetPlayerState<AJTSPlayerState>() == InPlayerState && It->GetBoardedSpacecraft() == this)
		{
			return *It;
		}
	}
	return nullptr;
}

void AJTSSpacecraftActor::RemoveOccupant(const AJTSPlayerState* InPlayerState)
{
	if (!HasAuthority() || InPlayerState == nullptr) return;
	Occupants.RemoveAll([InPlayerState](const FJTSSpacecraftOccupantState& Occupant)
	{
		return Occupant.PlayerState == InPlayerState;
	});
}

void AJTSSpacecraftActor::SubmitFlightInput()
{
	if (HasAuthority())
	{
		ApplyFlightInputOnServer(LocalFlightInput);
	}
	else
	{
		ServerSetFlightInput(LocalFlightInput);
	}
}

void AJTSSpacecraftActor::ApplyFlightInputOnServer(const FJTSSpacecraftInputState& InputState)
{
	if (!HasAuthority() || FlightMovementComponent == nullptr) return;
	FlightMovementComponent->SetForwardInput(FMath::Clamp(InputState.Throttle, -1.0f, 1.0f));
	FlightMovementComponent->SetStrafeInput(FMath::Clamp(InputState.Strafe, -1.0f, 1.0f));
	FlightMovementComponent->SetVerticalInput(FMath::Clamp(InputState.Vertical, -1.0f, 1.0f));
	FlightMovementComponent->SetRollInput(FMath::Clamp(InputState.Roll, -1.0f, 1.0f));
	const float MaximumLookInput = FMath::Max(1.0f, MaxFlightLookInputPerSample);
	FlightMovementComponent->AddYawInput(FMath::Clamp(InputState.Yaw, -MaximumLookInput, MaximumLookInput));
	FlightMovementComponent->AddPitchInput(FMath::Clamp(InputState.Pitch, -MaximumLookInput, MaximumLookInput));
	FlightMovementComponent->SetBoosting(InputState.bBoosting);
	FlightMovementComponent->SetBraking(InputState.bBraking);
}

void AJTSSpacecraftActor::ServerSetFlightInput_Implementation(const FJTSSpacecraftInputState& InputState)
{
	const APlayerController* const DriverController = Cast<APlayerController>(GetController());
	if (DriverController != nullptr && DriverController->GetPlayerState<AJTSPlayerState>() == DriverPlayerState)
	{
		ApplyFlightInputOnServer(InputState);
	}
}

void AJTSSpacecraftActor::ServerRequestLanding_Implementation()
{
	if (DriverPlayerState != nullptr) RequestLanding();
}

void AJTSSpacecraftActor::ServerRequestSurfaceTakeoff_Implementation()
{
	if (DriverPlayerState != nullptr) BeginSurfaceTakeoff();
}

void AJTSSpacecraftActor::ServerRequestDisembark_Implementation()
{
	APlayerController* const DriverController = Cast<APlayerController>(GetController());
	if (!IsValid(DriverController)
		|| DriverController->GetPlayerState<AJTSPlayerState>() != DriverPlayerState)
	{
		return;
	}

	TryDisembarkPlayerForController(DriverController);
}

void AJTSSpacecraftActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSSpacecraftActor, ReplicatedStorage);
	DOREPLIFETIME(AJTSSpacecraftActor, Occupants);
	DOREPLIFETIME(AJTSSpacecraftActor, DriverPlayerState);
	DOREPLIFETIME(AJTSSpacecraftActor, GroundedPlanet);
	DOREPLIFETIME(AJTSSpacecraftActor, bIsGroundedOnPlanet);
	DOREPLIFETIME(AJTSSpacecraftActor, FlightPlanet);
	DOREPLIFETIME(AJTSSpacecraftActor, ActiveLandingSite);
	DOREPLIFETIME(AJTSSpacecraftActor, FlightState);
	DOREPLIFETIME(AJTSSpacecraftActor, LastLandingFailure);
	DOREPLIFETIME(AJTSSpacecraftActor, LandingAssistPhase);
}

void AJTSSpacecraftActor::RestoreStorageFromExpedition(const TMap<EJTSResourceType, int32>& NewStorage)
{
	if (!HasAuthority())
	{
		return;
	}
	Storage.Reset();
	for (const TPair<EJTSResourceType, int32>& Entry : NewStorage)
	{
		if (IsSupportedResourceType(Entry.Key) && Entry.Value > 0)
		{
			Storage.Add(Entry.Key, Entry.Value);
		}
	}
	SyncReplicatedStorage();
	OnShipResourcesChanged.Broadcast(GetFuelCount(), GetWaterCount(), GetFoodCount());
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
	// Rock and Ore enter the ship wallet immediately on overlap. There is deliberately no separate
	// deposit interaction: gathering near the craft is enough to fund the shared supply screen.
	if (IsSpaceWorldRuntimeActive())
	{
		return TryDepositPlayerMaterials(Player);
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
		if (Resource.Key == EJTSResourceType::MoonAntCorpse)
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

bool AJTSSpacecraftActor::TryDepositPlayerMaterials(AJTSCharacter* Player)
{
	if (!HasAuthority() || !IsValid(Player) || !IsPawnInBoardingRange(Player))
	{
		return false;
	}

	UJTSInventoryComponent* const Inventory = Player->GetInventoryComponent();
	if (!IsValid(Inventory))
	{
		return false;
	}

	const int32 RockCount = Inventory->GetItemCount(EJTSItemId::Rock);
	const int32 OreCount = Inventory->GetItemCount(EJTSItemId::Ore);
	TMap<EJTSResourceType, int32> Materials;
	if (RockCount > 0)
	{
		Materials.Add(EJTSResourceType::Rock, RockCount);
	}
	if (OreCount > 0)
	{
		Materials.Add(EJTSResourceType::Ore, OreCount);
	}
	if (Materials.IsEmpty())
	{
		return false;
	}

	const bool bRemovedRock = RockCount <= 0 || Inventory->TryRemoveItem(EJTSItemId::Rock, RockCount);
	const bool bRemovedOre = bRemovedRock && (OreCount <= 0 || Inventory->TryRemoveItem(EJTSItemId::Ore, OreCount));
	if (!bRemovedRock || !bRemovedOre)
	{
		if (bRemovedRock && RockCount > 0)
		{
			Inventory->TryAddItemById(EJTSItemId::Rock, RockCount);
		}
		if (bRemovedOre && OreCount > 0)
		{
			Inventory->TryAddItemById(EJTSItemId::Ore, OreCount);
		}
		return false;
	}

	if (!DepositResourceAmounts(Materials))
	{
		if (RockCount > 0)
		{
			Inventory->TryAddItemById(EJTSItemId::Rock, RockCount);
		}
		if (OreCount > 0)
		{
			Inventory->TryAddItemById(EJTSItemId::Ore, OreCount);
		}
		return false;
	}

	return true;
}

bool AJTSSpacecraftActor::BuildShopCosts(EJTSItemId ItemId, TMap<EJTSResourceType, int32>& OutCosts) const
{
	OutCosts.Reset();
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
	if (!IsValid(Definition) || !Definition->IsShopPurchasable())
	{
		return false;
	}

	for (const FJTSItemCost& Cost : Definition->ShopCosts)
	{
		if (Cost.Amount > 0)
		{
			OutCosts.FindOrAdd(Cost.ResourceType) += Cost.Amount;
		}
	}
	return !OutCosts.IsEmpty();
}

bool AJTSSpacecraftActor::DeliverShopPurchase(AJTSCharacter* Player, const FJTSItemInstance& Item, bool& bOutDropped)
{
	bOutDropped = false;
	if (!IsValid(Player) || Item.IsEmpty())
	{
		return false;
	}

	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, Item.ItemId);
	if (!IsValid(Definition))
	{
		return false;
	}

	bool bDelivered = false;
	if (Definition->IsWearable())
	{
		if (UJTSPlayerEquipmentComponent* const Equipment = Player->GetEquipmentComponent())
		{
			bDelivered = Equipment->TryEquipItem(Item);
		}
	}
	else if (UJTSInventoryComponent* const Inventory = Player->GetInventoryComponent())
	{
		if (Inventory->CanAddItem(Item.ItemId, Item.StackCount))
		{
			int32 Remaining = Item.StackCount;
			bDelivered = Inventory->TryAddItem(Item, Remaining) && Remaining == 0;
		}
	}

	if (bDelivered)
	{
		return true;
	}

	const FVector DropOrigin = IsValid(ExitPoint)
		? ExitPoint->GetComponentLocation()
		: GetActorLocation() + GetActorRightVector() * FMath::Max(150.0f, GetBoardingInteractionRadius() * 0.55f);
	AJTSWorldPickupActor* const Pickup = AJTSWorldPickupActor::SpawnGameplayDrop(
		GetWorld(), Item, DropOrigin, Player, this, Player->GetActorForwardVector());
	bOutDropped = IsValid(Pickup);
	return bOutDropped;
}

void AJTSSpacecraftActor::DepositResourcesFromOverlappingPlayers()
{
	if (!HasAuthority() || !IsValid(BoardingTrigger))
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	BoardingTrigger->GetOverlappingActors(OverlappingActors, AJTSCharacter::StaticClass());
	for (AActor* const OverlappingActor : OverlappingActors)
	{
		AJTSCharacter* const OverlappingCharacter = Cast<AJTSCharacter>(OverlappingActor);
		if (!IsValid(OverlappingCharacter))
		{
			continue;
		}

		NearbyPlayer = OverlappingCharacter;
		DepositPlayerResources(OverlappingCharacter);
		OverlappingCharacter->NotifySpacecraftEntered(this);
	}
}

void AJTSSpacecraftActor::ReconcileInitialBoardingOverlaps()
{
	DepositResourcesFromOverlappingPlayers();
}

void AJTSSpacecraftActor::RestorePersistentStorage()
{
	UWorld* const World = GetWorld();
	if (!HasAuthority() || World == nullptr || (!IsSpaceWorldRuntimeActive() && !IsMoonSurfaceRuntimeActive()))
	{
		return;
	}

	if (!bPersistedStorageRestoreAttempted)
	{
		bPersistedStorageRestoreAttempted = true;
		if (UJTSExpeditionSubsystem* const Expedition = World->GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>())
		{
			Expedition->RestoreSpacecraft(this);
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

}

void AJTSSpacecraftActor::RestoreStorageForMoonTravel()
{
	RestorePersistentStorage();
}

void AJTSSpacecraftActor::SavePersistentStorage() const
{
	UWorld* const World = GetWorld();
	if (!HasAuthority() || World == nullptr || (!IsSpaceWorldRuntimeActive() && !IsMoonSurfaceRuntimeActive()))
	{
		return;
	}

	if (UJTSExpeditionSubsystem* const Expedition = World->GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>())
	{
		Expedition->SetSpacecraftSnapshot(GetClass(), Storage);
		Expedition->RequestSave();
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
	if (!HasAuthority())
	{
		return;
	}
	AJTSCharacter* const OverlappingCharacter = Cast<AJTSCharacter>(OtherActor);
	if (!IsValid(OverlappingCharacter))
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Spacecraft BoardingTrigger Enter Player=%s"), *OverlappingCharacter->GetName());

	NearbyPlayer = OverlappingCharacter;
	DepositPlayerResources(OverlappingCharacter);
	OverlappingCharacter->NotifySpacecraftEntered(this);
}

void AJTSSpacecraftActor::HandleBoardingTriggerEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (!HasAuthority())
	{
		return;
	}
	AJTSCharacter* const OverlappingCharacter = Cast<AJTSCharacter>(OtherActor);
	if (!IsValid(OverlappingCharacter) || NearbyPlayer.Get() != OverlappingCharacter)
	{
		return;
	}

	NearbyPlayer = nullptr;
	OverlappingCharacter->NotifySpacecraftExited(this);
}
