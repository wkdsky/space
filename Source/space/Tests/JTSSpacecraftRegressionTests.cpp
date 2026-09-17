#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "UObject/UnrealType.h"
#include "space/Components/JTSSpacecraftFlightMovementComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetLandingManager.h"
#include "space/World/JTSPlanetLandingSite.h"
#include "space/World/JTSSpaceWorldManager.h"

namespace
{
	struct FShipTestWorld
	{
		UWorld* World;
		AJTSPlanetAnchor* Planet;
		AJTSSpaceWorldManager* Manager;
		AJTSSpacecraftActor* Ship;

		FShipTestWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->InitializeActorsForPlay(FURL());
			Planet = World->SpawnActor<AJTSPlanetAnchor>();
			AStaticMeshActor* Ground = World->SpawnActor<AStaticMeshActor>();
			Ground->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
			Ground->SetActorScale3D(FVector(200.0));
			Ground->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
			FindFProperty<FObjectProperty>(Planet->GetClass(), TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet, Ground);
			FindFProperty<FFloatProperty>(Planet->GetClass(), TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet, 10000.0f);
			Manager = World->SpawnActor<AJTSSpaceWorldManager>();
			Manager->SetCurrentPlanet(Planet);
			Manager->SetSurfaceGameplayReady(true);
			UClass* ShipClass = LoadClass<AJTSSpacecraftActor>(nullptr, TEXT("/Game/Space/Blueprints/Ships/BP_Spacecraft.BP_Spacecraft_C"));
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Ship = World->SpawnActor<AJTSSpacecraftActor>(ShipClass, FVector(0, 0, 12000), FRotator::ZeroRotator, Params);
			Ship->DispatchBeginPlay();
		}

		~FShipTestWorld()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		bool Park(const FVector& Up)
		{
			FJTSPlanetSurfaceFrame Frame;
			return Planet->GetSurfaceFrameAt(Up * 12000.0, FVector::ForwardVector, Frame)
				&& Ship->SnapSpacecraftToSurfaceTransform(Planet, Frame.Transform);
		}

		APlayerController* AddPlayer(AJTSCharacter*& Character, bool bLocalController = false)
		{
			APlayerController* Controller = bLocalController
				? World->SpawnActor<AJTSPlayerController>()
				: World->SpawnActor<APlayerController>();
			if (bLocalController)
			{
				Controller->SetAsLocalPlayerController();
			}
			AJTSPlayerState* State = World->SpawnActor<AJTSPlayerState>();
			State->SetOwner(Controller);
			Controller->SetPlayerState(State);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Character = World->SpawnActor<AJTSCharacter>(AJTSCharacter::StaticClass(),
				Ship->GetActorLocation() + Ship->GetActorRightVector() * 600.0, Ship->GetActorRotation(), Params);
			Character->SetGameplayPlanet(Planet);
			Controller->Possess(Character);
			return Controller;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSBoardingRegression, "JTS.Spacecraft.PossessionAndDisembark",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSBoardingRegression::RunTest(const FString& Parameters)
{
	FShipTestWorld Fixture;
	if (!TestTrue(TEXT("Actual Blueprint parks on spherical terrain"), Fixture.Park(FVector(0.3, 0.4, 0.8660254)))) return false;
	TArray<APlayerController*> Controllers;
	TArray<AJTSCharacter*> Characters;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		AJTSCharacter* Character = nullptr;
		Controllers.Add(Fixture.AddPlayer(Character, Index == 0));
		Characters.Add(Character);
		if (Index == 0)
		{
			const UEnhancedInputComponent* const CharacterInput = Cast<UEnhancedInputComponent>(Character->InputComponent);
			TestNotNull(TEXT("Local character starts with an enhanced input component"), CharacterInput);
			if (CharacterInput != nullptr)
			{
				TestTrue(TEXT("Local character input actions are bound"), CharacterInput->GetActionEventBindings().Num() > 0);
			}
		}
		TestTrue(TEXT("Four occupants board"), Fixture.Ship->TryBoardPlayer(Character));
	}
	const UInputComponent* const BoardedCharacterInput = Characters[0]->InputComponent;
	TestNull(TEXT("Boarding destroys the no-longer-possessed character input component"), BoardedCharacterInput);
	TestNotNull(TEXT("Possessed spacecraft owns the active input component"), Fixture.Ship->InputComponent.Get());
	TestTrue(TEXT("Engine clears driver character PlayerState after possession transfer"), Characters[0]->GetPlayerState() == nullptr);
	TestTrue(TEXT("Driver remains associated with a seat"), Fixture.Ship->CanDisembarkPlayer(Characters[0]));
	AJTSCharacter* Stranger = nullptr;
	APlayerController* StrangerController = Fixture.AddPlayer(Stranger);
	TestFalse(TEXT("Fifth player cannot board"), Fixture.Ship->TryBoardPlayer(Stranger));
	TestFalse(TEXT("Non-occupant cannot eject another player"), Fixture.Ship->TryDisembarkPlayerForController(StrangerController));
	AActor* Blocker = Fixture.World->SpawnActor<AActor>();
	UBoxComponent* BlockerBox = NewObject<UBoxComponent>(Blocker);
	Blocker->SetRootComponent(BlockerBox);
	BlockerBox->SetBoxExtent(FVector(4000));
	BlockerBox->SetCollisionProfileName(TEXT("BlockAll"));
	BlockerBox->RegisterComponent();
	Blocker->SetActorLocation(Fixture.Ship->GetActorLocation());
	AddExpectedError(TEXT("Disembark blocked"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Blocked exit fails without ejecting into geometry"), Fixture.Ship->TryDisembarkPlayerForController(Controllers[0]));
	TestTrue(TEXT("Blocked exit preserves driver possession and seat"), Controllers[0]->GetPawn() == Fixture.Ship && Characters[0]->IsBoarded());
	Blocker->Destroy();
	Fixture.Ship->FlightDisembarkStarted(FInputActionValue(true));
	TestTrue(TEXT("Held boarding key cannot immediately disembark"), Controllers[0]->GetPawn() == Fixture.Ship && Characters[0]->IsBoarded());
	Fixture.Ship->FlightDisembarkReleased(FInputActionValue(false));
	Fixture.Ship->FlightDisembarkStarted(FInputActionValue(true));
	TestTrue(TEXT("Disembark waits until the flight input callback has completed"), Controllers[0]->GetPawn() == Fixture.Ship);
	Fixture.World->Tick(LEVELTICK_All, 1.0f / 60.0f);
	TestTrue(TEXT("Driver F RPC restores original character possession"), Controllers[0]->GetPawn() == Characters[0]);
	const UEnhancedInputComponent* const RestoredCharacterInput = Cast<UEnhancedInputComponent>(Characters[0]->InputComponent);
	TestNotNull(TEXT("Disembark rebuilds the character enhanced input component"), RestoredCharacterInput);
	if (RestoredCharacterInput != nullptr)
	{
		TestTrue(TEXT("Disembarked character input actions are rebound"), RestoredCharacterInput->GetActionEventBindings().Num() > 0);
	}
	TestFalse(TEXT("Disembarked controller accepts movement input"), Controllers[0]->IsMoveInputIgnored());
	TestFalse(TEXT("Disembarked controller accepts look input"), Controllers[0]->IsLookInputIgnored());
	TestTrue(TEXT("Disembarked character is the camera view target"), Controllers[0]->GetViewTarget() == Characters[0]);
	TestFalse(TEXT("Driver is no longer attached/boarded"), Characters[0]->IsBoarded());
	TestTrue(TEXT("Driver capsule collision restored"), Characters[0]->GetCapsuleComponent()->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Driver settles using gravity"), Characters[0]->GetCharacterMovement()->MovementMode == MOVE_Falling);
	TestTrue(TEXT("Exit capsule follows local radial up"), FVector::DotProduct(Characters[0]->GetActorUpVector(),
		Fixture.Planet->GetRadialUpVector(Characters[0]->GetActorLocation())) > 0.999);
	TestTrue(TEXT("Driver can board again"), Fixture.Ship->TryBoardPlayer(Characters[0]));
	const UEnhancedInputComponent* const ReboardedShipInput = Cast<UEnhancedInputComponent>(Fixture.Ship->InputComponent);
	TestNotNull(TEXT("Reboarded spacecraft rebuilds enhanced input"), ReboardedShipInput);
	if (ReboardedShipInput != nullptr)
	{
		TestTrue(TEXT("Reboarded spacecraft flight actions are rebound"), ReboardedShipInput->GetActionEventBindings().Num() > 0);
	}
	TestTrue(TEXT("Repeated driver exit succeeds"), Fixture.Ship->TryDisembarkPlayerForController(Controllers[0]));
	const UEnhancedInputComponent* const RepeatedCharacterInput = Cast<UEnhancedInputComponent>(Characters[0]->InputComponent);
	TestNotNull(TEXT("Repeated disembark rebuilds character input"), RepeatedCharacterInput);
	if (RepeatedCharacterInput != nullptr)
	{
		TestTrue(TEXT("Repeated disembark rebinds character actions"), RepeatedCharacterInput->GetActionEventBindings().Num() > 0);
	}
	for (int32 Index = 1; Index < 4; ++Index)
	{
		TestTrue(TEXT("Passenger exits through owner controller"), Fixture.Ship->TryDisembarkPlayerForController(Controllers[Index]));
		TestTrue(TEXT("Passenger retains character possession"), Controllers[Index]->GetPawn() == Characters[Index]);
	}
	TestFalse(TEXT("All seats empty"), Fixture.Ship->HasBoardedPlayer());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSHullCameraRegression, "JTS.Spacecraft.HullClearanceAndCamera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSHullCameraRegression::RunTest(const FString& Parameters)
{
	FShipTestWorld Fixture;
	UBoxComponent* Hull = Fixture.Ship->FindComponentByClass<UBoxComponent>();
	TestTrue(TEXT("Blueprint flight hull includes the 200cm visual half-height"), Hull->GetScaledBoxExtent().Z >= 199.9);
	const FVector TestUps[] = {FVector::UpVector, FVector::RightVector, -FVector::UpVector, FVector(0.3, 0.4, 0.8660254)};
	for (const FVector& Up : TestUps)
	{
		TestTrue(TEXT("Parks at pole/equator/underside/oblique surface"), Fixture.Park(Up));
		TestTrue(TEXT("Landed state agrees with ground state"), Fixture.Ship->IsLanded());
		TestTrue(TEXT("Hull is clear of spherical terrain"), Fixture.Ship->CanOccupyLandingTransform(Fixture.Ship->GetActorTransform()));
		TestTrue(TEXT("Landing support includes mesh bottom"), Fixture.Ship->GetLandingCollisionClearance() >= 201.9f);
	}

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Fixture.Ship);
	for (const UPrimitiveComponent* const PrimitiveComponent : PrimitiveComponents)
	{
		TestEqual(
			*FString::Printf(TEXT("Spacecraft primitive %s ignores character camera probes"), *GetNameSafe(PrimitiveComponent)),
			PrimitiveComponent->GetCollisionResponseToChannel(ECC_Camera),
			ECR_Ignore);
	}

	AJTSCharacter* CameraCharacter = nullptr;
	APlayerController* CameraController = Fixture.AddPlayer(CameraCharacter);
	const FVector ReferenceUp = Fixture.Planet->GetRadialUpVector(Fixture.Ship->GetActorLocation()).GetSafeNormal();
	const FVector EntryTangent = FVector::VectorPlaneProject(Fixture.Ship->GetActorForwardVector(), ReferenceUp).GetSafeNormal();
	const FVector EntryRight = FVector::CrossProduct(ReferenceUp, EntryTangent).GetSafeNormal();
	const FVector EntryForward = FQuat(EntryRight, FMath::DegreesToRadians(18.0f)).RotateVector(EntryTangent).GetSafeNormal();
	CameraController->SetControlRotation(EntryForward.Rotation());
	TestTrue(TEXT("Camera test driver boards"), Fixture.Ship->TryBoardPlayer(CameraCharacter));
	Fixture.Ship->ActivateFlightCameraThirdPerson();
	USpringArmComponent* Boom = Fixture.Ship->GetFlightCameraBoom();
	TestFalse(TEXT("Flight camera is independent from pawn control rotation"), Boom->bUsePawnControlRotation);
	TestTrue(TEXT("Flight camera keeps a world-space planet-relative orbit"), Boom->IsUsingAbsoluteRotation());
	TestFalse(TEXT("Flight camera never retracts into the spacecraft hull"), Boom->bDoCollisionTest);
	TestTrue(TEXT("Flight camera smooths orbit rotation"), Boom->bEnableCameraRotationLag);
	TestTrue(TEXT("Flight camera lag uses frame-rate-independent substepping"), Boom->bUseCameraLagSubstepping);
	TestTrue(TEXT("Flight camera retains a proper exterior arm length"), Boom->TargetArmLength >= Fixture.Ship->FlightCameraMinArmLength);
	const FVector FirstForward = Fixture.Ship->GetFlightCamera()->GetForwardVector();
	TestTrue(TEXT("Boarding inherits the player's existing sight line"), FVector::DotProduct(FirstForward, EntryForward) > 0.999f);

	const FQuat ShipRotationBeforeLook = Fixture.Ship->GetActorQuat();
	Fixture.Ship->FlightLookYaw(FInputActionValue(120.0f));
	Fixture.Ship->FlightLookPitch(FInputActionValue(-35.0f));
	for (int32 CameraStep = 0; CameraStep < 12; ++CameraStep)
	{
		Boom->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	}
	const FVector SecondForward = Fixture.Ship->GetFlightCamera()->GetForwardVector();
	TestTrue(TEXT("Mouse input rotates the independent orbit camera"), !FirstForward.Equals(SecondForward, 0.001));
	TestTrue(TEXT("Mouse look never directly rotates the spacecraft"), Fixture.Ship->GetActorQuat().Equals(ShipRotationBeforeLook, 0.0001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSThirdPersonFlightRegression, "JTS.Spacecraft.ThirdPersonFlightControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSThirdPersonFlightRegression::RunTest(const FString& Parameters)
{
	FShipTestWorld Fixture;
	if (!TestTrue(TEXT("Flight-control test parks on an oblique spherical surface"),
		Fixture.Park(FVector(0.3f, 0.4f, 0.8660254f))))
	{
		return false;
	}

	AJTSCharacter* DriverCharacter = nullptr;
	APlayerController* DriverController = Fixture.AddPlayer(DriverCharacter);
	const FVector InitialUp = Fixture.Planet->GetRadialUpVector(Fixture.Ship->GetActorLocation()).GetSafeNormal();
	const FVector InitialViewForward = FVector::VectorPlaneProject(FVector::ForwardVector, InitialUp).GetSafeNormal();
	DriverController->SetControlRotation(InitialViewForward.Rotation());
	TestTrue(TEXT("Flight-control test driver boards"), Fixture.Ship->TryBoardPlayer(DriverCharacter));
	TestTrue(TEXT("Surface takeoff enables authoritative flight movement"), Fixture.Ship->BeginSurfaceTakeoff());
	Fixture.Ship->AddActorWorldOffset(InitialUp * 2000.0f);

	UJTSSpacecraftFlightMovementComponent* const Movement = Fixture.Ship->GetFlightMovementComponent();
	const FJTSSpacecraftFlightStats BlueprintStats = Movement->GetBaseStats();
	TestTrue(TEXT("BP_Spacecraft keeps usable third-person flight defaults"),
		BlueprintStats.MaxMoveSpeed > 0.0f
		&& BlueprintStats.LiftSpeed >= 2400.0f
		&& BlueprintStats.Acceleration > 0.0f
		&& BlueprintStats.Deceleration > 0.0f
		&& BlueprintStats.FacingTurnRate > 0.0f
		&& BlueprintStats.FacingTurnAcceleration > 0.0f);
	FJTSSpacecraftFlightStats TestStats = Movement->GetEffectiveStats();
	TestStats.MaxMoveSpeed = 1000.0f;
	TestStats.LiftSpeed = 900.0f;
	TestStats.Acceleration = 100000.0f;
	TestStats.Deceleration = 100000.0f;
	TestStats.FacingTurnRate = 3600.0f;
	TestStats.FacingTurnAcceleration = 36000.0f;
	Movement->SetEffectiveStats(TestStats);
	TestEqual(TEXT("Test flight stats apply the requested facing rate"),
		Movement->GetEffectiveStats().FacingTurnRate,
		3600.0f);
	TestTrue(TEXT("Planetary low flight uses the radial surface frame"),
		Movement->IsUsingPlanetSurfaceFlightFrame());
	Fixture.Ship->EnsureFlightInputMapping();
	const bool bLegacyLandingKeyMapped = Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
		[](const FEnhancedActionKeyMapping& Mapping)
		{
			return Mapping.Key == EKeys::L;
		});
	TestFalse(TEXT("L is no longer mapped to landing"), bLegacyLandingKeyMapped);
	const bool bDedicatedSpaceActionMapped = Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
		[&Fixture](const FEnhancedActionKeyMapping& Mapping)
		{
			return Mapping.Action == Fixture.Ship->FlightAscendAction && Mapping.Key == EKeys::SpaceBar;
		});
	TestTrue(TEXT("Space has a dedicated takeoff and landing-abort action"), bDedicatedSpaceActionMapped);

	const FQuat RotationBeforeMouseLook = Fixture.Ship->GetActorQuat();
	const float QuarterTurnMouseInput = 90.0f / FMath::Max(Fixture.Ship->FlightCameraLookSensitivity, KINDA_SMALL_NUMBER);
	Fixture.Ship->FlightLookYaw(FInputActionValue(QuarterTurnMouseInput));
	TestTrue(TEXT("Orbiting the camera does not steer the spacecraft"),
		Fixture.Ship->GetActorQuat().Equals(RotationBeforeMouseLook, 0.0001f));

	auto TestPlanarDirection = [this, &Fixture, Movement](
		const TCHAR* Description,
		float ForwardInput,
		float RightInput,
		bool bExpectRightAxis)
	{
		Fixture.Ship->FlightMoveForward(FInputActionValue(0.0f));
		Fixture.Ship->FlightMoveRight(FInputActionValue(0.0f));
		Movement->StopMovementImmediately();
		Fixture.Ship->FlightMoveForward(FInputActionValue(ForwardInput));
		Fixture.Ship->FlightMoveRight(FInputActionValue(RightInput));

		const FVector ReferenceUp = Fixture.Ship->GetFlightReferenceUp();
		const FVector ViewForward = FVector::VectorPlaneProject(
			Fixture.Ship->LocalFlightInput.ViewForward,
			ReferenceUp).GetSafeNormal();
		const FVector ViewRight = FVector::CrossProduct(ReferenceUp, ViewForward).GetSafeNormal();
		const FVector ExpectedDirection = bExpectRightAxis
			? ViewRight * FMath::Sign(RightInput)
			: ViewForward * FMath::Sign(ForwardInput);
		const FVector StartLocation = Fixture.Ship->GetActorLocation();
		Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
		const FVector Displacement = Fixture.Ship->GetActorLocation() - StartLocation;
		const FVector PlanarDisplacement = FVector::VectorPlaneProject(Displacement, ReferenceUp).GetSafeNormal();
		TestTrue(Description, FVector::DotProduct(PlanarDisplacement, ExpectedDirection) > 0.995f);
		TestTrue(TEXT("Planar flight stays tangent to the current planet"),
			FMath::Abs(FVector::DotProduct(Displacement.GetSafeNormal(), ReferenceUp)) < 0.02f);
	};

	TestPlanarDirection(TEXT("W moves along the camera-forward tangent"), 1.0f, 0.0f, false);
	TestPlanarDirection(TEXT("S moves opposite the camera-forward tangent"), -1.0f, 0.0f, false);
	TestPlanarDirection(TEXT("D moves along the camera-right tangent"), 0.0f, 1.0f, true);
	TestPlanarDirection(TEXT("A moves opposite the camera-right tangent"), 0.0f, -1.0f, true);

	FJTSSpacecraftFlightStats FacingStats = Movement->GetEffectiveStats();
	FacingStats.MaxMoveSpeed = 0.0f;
	Movement->SetEffectiveStats(FacingStats);
	Fixture.Ship->FlightMoveForward(FInputActionValue(1.0f));
	Fixture.Ship->FlightMoveRight(FInputActionValue(0.0f));
	Movement->StopMovementImmediately();
	const FVector FacingUp = Fixture.Ship->GetFlightReferenceUp();
	const FVector DesiredFacing = FVector::VectorPlaneProject(
		Fixture.Ship->LocalFlightInput.ViewForward,
		FacingUp).GetSafeNormal();
	for (int32 TurnStep = 0; TurnStep < 3; ++TurnStep)
	{
		Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	}
	const FVector ActualFacing = Fixture.Ship->GetActorForwardVector();
	const float FacingDot = FVector::DotProduct(ActualFacing, DesiredFacing);
	AddInfo(FString::Printf(TEXT("Third-person facing: dot=%.6f actual=%s desired=%s up=%s"),
		FacingDot,
		*ActualFacing.ToCompactString(),
		*DesiredFacing.ToCompactString(),
		*FacingUp.ToCompactString()));
	TestTrue(TEXT("Spacecraft automatically faces the camera heading"),
		FacingDot > 0.995f);
	TestTrue(TEXT("Automatic facing keeps the spacecraft aligned to radial up"),
		FVector::DotProduct(Fixture.Ship->GetActorUpVector(), Fixture.Ship->GetFlightReferenceUp()) > 0.995f);

	FJTSSpacecraftFlightStats ReverseStats = Movement->GetEffectiveStats();
	ReverseStats.MaxMoveSpeed = 1000.0f;
	Movement->SetEffectiveStats(ReverseStats);
	Fixture.Ship->FlightMoveForward(FInputActionValue(-1.0f));
	Fixture.Ship->FlightMoveRight(FInputActionValue(0.0f));
	Movement->StopMovementImmediately();
	const FVector ReverseFacing = Fixture.Ship->GetActorForwardVector();
	const FVector ReverseStart = Fixture.Ship->GetActorLocation();
	Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	const FVector ReverseDirection = (Fixture.Ship->GetActorLocation() - ReverseStart).GetSafeNormal();
	TestTrue(TEXT("S applies direct reverse thrust"), FVector::DotProduct(ReverseDirection, ReverseFacing) < -0.995f);
	TestTrue(TEXT("S does not turn the spacecraft around"),
		FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), ReverseFacing) > 0.995f);

	Fixture.Ship->FlightMoveForward(FInputActionValue(0.0f));
	Movement->StopMovementImmediately();
	const float PitchUpMouseInput = 30.0f / FMath::Max(Fixture.Ship->FlightCameraLookSensitivity, KINDA_SMALL_NUMBER);
	Fixture.Ship->FlightLookPitch(FInputActionValue(PitchUpMouseInput));
	Fixture.Ship->FlightMoveForward(FInputActionValue(1.0f));
	const FVector ClimbUp = Fixture.Ship->GetFlightReferenceUp();
	const FVector ClimbForward = Fixture.Ship->LocalFlightInput.ViewForward.GetSafeNormal();
	TestTrue(TEXT("Pitching up remains available for observing the planet and spacecraft"),
		FVector::DotProduct(ClimbForward, ClimbUp) > 0.45f);
	const FVector ClimbTangent = FVector::VectorPlaneProject(ClimbForward, ClimbUp).GetSafeNormal();
	const FVector ClimbStart = Fixture.Ship->GetActorLocation();
	Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	const FVector ClimbDirection = (Fixture.Ship->GetActorLocation() - ClimbStart).GetSafeNormal();
	TestTrue(TEXT("Camera pitch does not make W leave the planet tangent"),
		FVector::DotProduct(ClimbDirection, ClimbTangent) > 0.995f
		&& FMath::Abs(FVector::DotProduct(ClimbDirection, ClimbUp)) < 0.02f);
	TestTrue(TEXT("Spacecraft remains radially level while the camera looks up"),
		FVector::DotProduct(Fixture.Ship->GetActorUpVector(), ClimbUp) > 0.995f
		&& FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), ClimbTangent) > 0.995f);

	Movement->StopMovementImmediately();
	const float PitchDownMouseInput = -60.0f / FMath::Max(Fixture.Ship->FlightCameraLookSensitivity, KINDA_SMALL_NUMBER);
	Fixture.Ship->FlightLookPitch(FInputActionValue(PitchDownMouseInput));
	Fixture.Ship->FlightMoveForward(FInputActionValue(1.0f));
	const FVector DiveUp = Fixture.Ship->GetFlightReferenceUp();
	const FVector DiveForward = Fixture.Ship->LocalFlightInput.ViewForward.GetSafeNormal();
	TestTrue(TEXT("Pitching down remains available for inspecting the surface"),
		FVector::DotProduct(DiveForward, DiveUp) < -0.45f);
	const FVector DiveTangent = FVector::VectorPlaneProject(DiveForward, DiveUp).GetSafeNormal();
	const FVector DiveStart = Fixture.Ship->GetActorLocation();
	Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	const FVector DiveDirection = (Fixture.Ship->GetActorLocation() - DiveStart).GetSafeNormal();
	TestTrue(TEXT("Camera pitch does not make W dive toward the surface"),
		FVector::DotProduct(DiveDirection, DiveTangent) > 0.995f
		&& FMath::Abs(FVector::DotProduct(DiveDirection, DiveUp)) < 0.02f);
	TestTrue(TEXT("Spacecraft remains radially level while the camera looks down"),
		FVector::DotProduct(Fixture.Ship->GetActorUpVector(), DiveUp) > 0.995f
		&& FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), DiveTangent) > 0.995f);

	Fixture.Ship->FlightMoveForward(FInputActionValue(0.0f));
	Fixture.Ship->FlightMoveRight(FInputActionValue(0.0f));
	Movement->StopMovementImmediately();
	Fixture.Ship->FlightMoveVertical(FInputActionValue(1.0f));
	const FVector LiftUp = Fixture.Ship->GetFlightReferenceUp();
	const FVector LiftStart = Fixture.Ship->GetActorLocation();
	Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	const FVector LiftDisplacement = Fixture.Ship->GetActorLocation() - LiftStart;
	const FVector LiftDirection = LiftDisplacement.GetSafeNormal();
	TestTrue(TEXT("Space raises the spacecraft along radial up"), FVector::DotProduct(LiftDirection, LiftUp) > 0.995f);
	TestTrue(TEXT("Radial lift has useful immediate authority"), LiftDisplacement.Size() >= 85.0f);
	Fixture.Ship->FlightMoveVertical(FInputActionValue(-1.0f));
	TestEqual(TEXT("Ctrl submits radial descent input"), Fixture.Ship->LocalFlightInput.Lift, -1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSAutomaticLandingRegression, "JTS.Spacecraft.AutomaticLandingControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSAutomaticLandingRegression::RunTest(const FString& Parameters)
{
	FShipTestWorld Fixture;
	if (!TestTrue(TEXT("Automatic-landing test parks on the spherical surface"), Fixture.Park(FVector::UpVector)))
	{
		return false;
	}

	FJTSPlanetSurfaceFrame LandingFrame;
	if (!TestTrue(TEXT("Landing site resolves a real surface frame"), Fixture.Planet->GetSurfaceFrameAt(
		Fixture.Ship->GetActorLocation(),
		Fixture.Ship->GetActorForwardVector(),
		LandingFrame)))
	{
		return false;
	}

	FActorSpawnParameters SiteSpawnParameters;
	SiteSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AJTSPlanetLandingManager* const PlanetLandingManager = Fixture.World->SpawnActor<AJTSPlanetLandingManager>();
	AJTSPlanetLandingSite* const LandingSite = Fixture.World->SpawnActor<AJTSPlanetLandingSite>(
		AJTSPlanetLandingSite::StaticClass(),
		LandingFrame.Transform,
		SiteSpawnParameters);
	FindFProperty<FObjectProperty>(LandingSite->GetClass(), TEXT("PlanetAnchor"))
		->SetObjectPropertyValue_InContainer(LandingSite, Fixture.Planet);
	TestTrue(TEXT("Landing site registers with the landing manager"),
		PlanetLandingManager->RegisterLandingSite(LandingSite));
	TestTrue(TEXT("Landing site contains the spacecraft surface point"),
		LandingSite->IsLocationInsideLandingArea(LandingFrame.Location));

	AJTSCharacter* DriverCharacter = nullptr;
	Fixture.AddPlayer(DriverCharacter);
	TestTrue(TEXT("Automatic-landing driver boards"), Fixture.Ship->TryBoardPlayer(DriverCharacter));
	TestTrue(TEXT("Automatic-landing test takes off"), Fixture.Ship->BeginSurfaceTakeoff());
	Fixture.Ship->AddActorWorldOffset(FVector::UpVector * 1800.0f);

	UJTSSpacecraftFlightMovementComponent* const Movement = Fixture.Ship->GetFlightMovementComponent();
	Fixture.Ship->FlightMoveVertical(FInputActionValue(-1.0f));
	for (int32 Step = 0; Step < 240
		&& Fixture.Ship->GetFlightState() != EJTSSpacecraftFlightState::LandingAssist;
		++Step)
	{
		Fixture.Ship->Tick(1.0f / 60.0f);
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	}
	TestEqual(TEXT("Holding Ctrl inside the landing site triggers automatic takeover"),
		Fixture.Ship->GetFlightState(),
		EJTSSpacecraftFlightState::LandingAssist);
	TestTrue(TEXT("Automatic takeover uses the existing landing movement"), Movement->IsAssistedLanding());

	Fixture.Ship->FlightMoveVertical(FInputActionValue(0.0f));
	Fixture.Ship->FlightMoveForward(FInputActionValue(1.0f));
	Fixture.Ship->Tick(1.0f / 60.0f);
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Releasing Ctrl and pressing movement keys does not cancel automatic landing"),
		Fixture.Ship->GetFlightState(),
		EJTSSpacecraftFlightState::LandingAssist);

	Fixture.Ship->FlightMoveVertical(FInputActionValue(-1.0f));
	Fixture.Ship->FlightAscendStarted(FInputActionValue(true));
	TestEqual(TEXT("Space aborts automatic landing"),
		Fixture.Ship->GetFlightState(),
		EJTSSpacecraftFlightState::Flying);
	TestFalse(TEXT("Space stops the assisted-landing movement state"), Movement->IsAssistedLanding());
	Fixture.Ship->Tick(0.2f);
	TestEqual(TEXT("Holding Ctrl cannot immediately re-engage after a Space abort"),
		Fixture.Ship->GetFlightState(),
		EJTSSpacecraftFlightState::Flying);

	Fixture.Ship->FlightMoveForward(FInputActionValue(0.0f));
	Fixture.Ship->FlightMoveVertical(FInputActionValue(0.0f));
	Fixture.Ship->FlightMoveVertical(FInputActionValue(-1.0f));
	for (int32 Step = 0; Step < 120
		&& Fixture.Ship->GetFlightState() != EJTSSpacecraftFlightState::LandingAssist;
		++Step)
	{
		Fixture.Ship->Tick(1.0f / 60.0f);
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	}
	TestEqual(TEXT("Ctrl can engage automatic landing again after an abort"),
		Fixture.Ship->GetFlightState(),
		EJTSSpacecraftFlightState::LandingAssist);
	Fixture.Ship->FlightMoveVertical(FInputActionValue(0.0f));
	for (int32 Step = 0; Step < 1200 && !Fixture.Ship->IsLanded(); ++Step)
	{
		Fixture.Ship->Tick(1.0f / 60.0f);
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	}
	TestTrue(TEXT("Automatic landing completes after Ctrl is released"), Fixture.Ship->IsLanded());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSLandingCycleRegression, "JTS.Spacecraft.TakeoffLandingCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSLandingCycleRegression::RunTest(const FString& Parameters)
{
	FShipTestWorld Fixture;
	if (!TestTrue(TEXT("Initial parking succeeds"), Fixture.Park(FVector::UpVector))) return false;
	AJTSCharacter* Character = nullptr;
	APlayerController* Controller = Fixture.AddPlayer(Character);
	TestTrue(TEXT("Driver boards"), Fixture.Ship->TryBoardPlayer(Character));
	TestTrue(TEXT("Takeoff clears landed state"), Fixture.Ship->BeginSurfaceTakeoff());
	TestFalse(TEXT("Cannot disembark in flight"), Fixture.Ship->TryDisembarkPlayerForController(Controller));
	UJTSSpacecraftFlightMovementComponent* Movement = Fixture.Ship->GetFlightMovementComponent();
	Fixture.Ship->AddActorWorldOffset(FVector(0, 0, 1200));
	Fixture.Ship->RefreshGroundInfo(Fixture.Planet);
	const FTransform LandingFrame = Fixture.Ship->GetGroundInfo().SurfaceTransform;
	TestTrue(TEXT("Assisted landing begins"), Fixture.Ship->BeginAssistedLanding(LandingFrame, 2.0f));
	for (int32 Step = 0; Step < 1200 && !Fixture.Ship->IsLanded(); ++Step)
	{
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	}
	AddInfo(FString::Printf(TEXT("Landing end: state=%d phase=%d height=%.2f speed=%.2f location=%s assisted=%d bound=%d begun=%d alignment=%.4f"),
		static_cast<int32>(Fixture.Ship->GetFlightState()), static_cast<int32>(Fixture.Ship->GetLandingAssistPhase()),
		Fixture.Ship->GetGroundInfo().DockingHeight, Fixture.Ship->GetCurrentSpeed(), *Fixture.Ship->GetActorLocation().ToCompactString(),
		Movement->IsAssistedLanding(), Movement->OnAssistedLandingCompleted.IsBound(), Fixture.Ship->HasActorBegunPlay(),
		FVector::DotProduct(Fixture.Ship->GetActorUpVector(), Fixture.Ship->GetGroundInfo().SurfaceNormal)));
	TestTrue(TEXT("Assisted landing completes"), Fixture.Ship->IsLanded());
	TestTrue(TEXT("Surface gameplay restored after landing"), Fixture.Manager->IsSurfaceGameplayReady());
	TestTrue(TEXT("Driver exits after landing"), Fixture.Ship->TryDisembarkPlayerForController(Controller));
	TestTrue(TEXT("Driver can reboard after landing"), Fixture.Ship->TryBoardPlayer(Character));
	TestTrue(TEXT("Second takeoff works"), Fixture.Ship->BeginSurfaceTakeoff());
	return true;
}

#endif
