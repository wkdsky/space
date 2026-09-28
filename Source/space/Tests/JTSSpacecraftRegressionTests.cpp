#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
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
#include "space/Components/JTSInventoryComponent.h"
#include "UObject/UnrealType.h"
#include "space/Components/JTSSpacecraftFlightMovementComponent.h"
#include "space/Items/JTSItemTypes.h"
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

		AJTSPlanetAnchor* AddFlightPlanet(const FName PlanetId, const FVector& PlanetCenter)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AJTSPlanetAnchor* const AdditionalPlanet = World->SpawnActor<AJTSPlanetAnchor>(
				AJTSPlanetAnchor::StaticClass(),
				PlanetCenter,
				FRotator::ZeroRotator,
				SpawnParameters);
			FindFProperty<FNameProperty>(AdditionalPlanet->GetClass(), TEXT("PlanetId"))
				->SetPropertyValue_InContainer(AdditionalPlanet, PlanetId);
			FindFProperty<FFloatProperty>(AdditionalPlanet->GetClass(), TEXT("ApproximateRadius"))
				->SetPropertyValue_InContainer(AdditionalPlanet, 10000.0f);
			FindFProperty<FFloatProperty>(AdditionalPlanet->GetClass(), TEXT("GravityInfluenceRange"))
				->SetPropertyValue_InContainer(AdditionalPlanet, 12000.0f);
			AStaticMeshActor* const AdditionalGround = World->SpawnActor<AStaticMeshActor>(
				AStaticMeshActor::StaticClass(),
				PlanetCenter,
				FRotator::ZeroRotator,
				SpawnParameters);
			AdditionalGround->GetStaticMeshComponent()->SetStaticMesh(
				LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
			AdditionalGround->SetActorScale3D(FVector(200.0));
			AdditionalGround->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
			FindFProperty<FObjectProperty>(AdditionalPlanet->GetClass(), TEXT("GameplaySurfaceActor"))
				->SetObjectPropertyValue_InContainer(AdditionalPlanet, AdditionalGround);
			Manager->RegisterPlanet(AdditionalPlanet);
			return AdditionalPlanet;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSInventorySelectedQuickbarRegression, "JTS.Inventory.SelectedQuickbarReceivesHoldable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSInventorySelectedQuickbarRegression::RunTest(const FString& Parameters)
{
	FShipTestWorld Fixture;
	AJTSCharacter* Character = nullptr;
	Fixture.AddPlayer(Character);
	UJTSInventoryComponent* const Inventory = IsValid(Character) ? Character->GetInventoryComponent() : nullptr;
	if (!TestNotNull(TEXT("Character owns an inventory"), Inventory))
	{
		return false;
	}

	Inventory->RestoreItems({}, 1);
	TestEqual(TEXT("Selected quickbar slot remains the player's choice"), Inventory->GetSelectedQuickbarSlot(), 1);
	TestTrue(TEXT("Selected quickbar slot starts empty"), Inventory->GetItemAtSlot(1).IsEmpty());
	TestTrue(TEXT("Holdable can enter the inventory"), Inventory->TryAddItemById(EJTSItemId::Pistol));
	TestTrue(TEXT("Holdable is placed in the selected empty quickbar slot"), Inventory->GetItemAtSlot(1).ItemId == EJTSItemId::Pistol);
	TestTrue(TEXT("Placed holdable becomes the active item"), Inventory->GetActiveItemId() == EJTSItemId::Pistol);
	TestTrue(TEXT("Unselected empty quickbar slot remains untouched"), Inventory->GetItemAtSlot(0).IsEmpty());
	return true;
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
		TestTrue(TEXT("Character movement accepts the configured 60 degree walkable slope"),
			Character->GetCharacterMovement()->GetWalkableFloorAngle() >= 59.99f);
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
	TestTrue(TEXT("Driver snaps to the real surface in walking mode"), Characters[0]->GetCharacterMovement()->MovementMode == MOVE_Walking);
	TestTrue(TEXT("Exit capsule follows local radial up"), FVector::DotProduct(Characters[0]->GetActorUpVector(),
		Fixture.Planet->GetRadialUpVector(Characters[0]->GetActorLocation())) > 0.999);
	FJTSPlanetSurfaceFrame DriverExitSurfaceFrame;
	if (TestTrue(TEXT("Driver exit resolves a real terrain surface"), Fixture.Planet->GetSurfaceFrameAt(
		Characters[0]->GetActorLocation(), Characters[0]->GetActorForwardVector(), DriverExitSurfaceFrame)))
	{
		const UCapsuleComponent* const DriverCapsule = Characters[0]->GetCapsuleComponent();
		const float CapsuleRadius = DriverCapsule->GetScaledCapsuleRadius();
		const float CapsuleCylinderHalfHeight = FMath::Max(0.0f, DriverCapsule->GetScaledCapsuleHalfHeight() - CapsuleRadius);
		const float CapsuleSupport = CapsuleRadius + CapsuleCylinderHalfHeight * FMath::Abs(FVector::DotProduct(
			Characters[0]->GetActorUpVector(), DriverExitSurfaceFrame.Up));
		const float SurfaceClearance = FVector::DotProduct(
			Characters[0]->GetActorLocation() - DriverExitSurfaceFrame.Location,
			DriverExitSurfaceFrame.Up);
		TestTrue(TEXT("Driver exit capsule is above, rather than embedded in, the terrain"),
			SurfaceClearance >= CapsuleSupport + 1.0f);
	}
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
	UJTSSpacecraftFlightMovementComponent* const Movement = Fixture.Ship->GetFlightMovementComponent();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	Fixture.Ship->AddActorWorldOffset(InitialUp * 2000.0f);
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);

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
	const bool bBothControlKeysMapToDescent = Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
		[&Fixture](const FEnhancedActionKeyMapping& Mapping)
		{
			return Mapping.Action == Fixture.Ship->FlightVerticalAction
				&& Mapping.Key == EKeys::LeftControl;
		})
		&& Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
			[&Fixture](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == Fixture.Ship->FlightVerticalAction
					&& Mapping.Key == EKeys::RightControl;
			});
	TestTrue(TEXT("Both Ctrl keys submit the shared radial descent / auto-land input"), bBothControlKeysMapToDescent);
	const bool bArrowKeysSteerTheHull = Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
		[&Fixture](const FEnhancedActionKeyMapping& Mapping)
		{
			return Mapping.Action == Fixture.Ship->FlightSteerYawAction && Mapping.Key == EKeys::Right;
		})
		&& Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
			[&Fixture](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == Fixture.Ship->FlightSteerPitchAction && Mapping.Key == EKeys::Up;
			});
	TestTrue(TEXT("Arrow keys steer the hull while the mouse stays on the camera"), bArrowKeysSteerTheHull);

	const FQuat RotationBeforeMouseLook = Fixture.Ship->GetActorQuat();
	const float QuarterTurnMouseInput = 90.0f / FMath::Max(Fixture.Ship->FlightCameraLookSensitivity, KINDA_SMALL_NUMBER);
	Fixture.Ship->FlightLookYaw(FInputActionValue(QuarterTurnMouseInput));
	TestTrue(TEXT("Orbiting the camera does not steer the spacecraft"),
		Fixture.Ship->GetActorQuat().Equals(RotationBeforeMouseLook, 0.0001f));

	const bool bYawKeysTurnTheHull = Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
		[&Fixture](const FEnhancedActionKeyMapping& Mapping)
		{
			return Mapping.Action == Fixture.Ship->FlightSteerYawAction && Mapping.Key == EKeys::D;
		})
		&& Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
			[&Fixture](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == Fixture.Ship->FlightSteerYawAction && Mapping.Key == EKeys::A;
			})
		&& Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
			[&Fixture](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == Fixture.Ship->FlightTurnAroundAction && Mapping.Key == EKeys::S;
			});
	TestTrue(TEXT("A and D yaw the hull and S starts an in-place turnaround"), bYawKeysTurnTheHull);

	Fixture.Ship->FlightMoveForward(FInputActionValue(0.0f));
	Fixture.Ship->FlightSteerYaw(FInputActionValue(0.0f));
	Fixture.Ship->FlightSteerPitch(FInputActionValue(0.0f));
	Fixture.Ship->FlightTurnAround(FInputActionValue(false));
	Movement->StopMovementImmediately();
	Fixture.Ship->FlightMoveForward(FInputActionValue(1.0f));
	{
		const FVector ReferenceUp = Fixture.Ship->GetFlightReferenceUp();
		const FVector HullForward = FVector::VectorPlaneProject(
			Fixture.Ship->GetActorForwardVector(),
			ReferenceUp).GetSafeNormal();
		const FVector StartLocation = Fixture.Ship->GetActorLocation();
		Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
		const FVector Displacement = Fixture.Ship->GetActorLocation() - StartLocation;
		const FVector PlanarDisplacement = FVector::VectorPlaneProject(Displacement, ReferenceUp).GetSafeNormal();
		TestTrue(TEXT("W moves along the hull-forward tangent"),
			FVector::DotProduct(PlanarDisplacement, HullForward) > 0.995f);
		TestTrue(TEXT("Planar flight stays tangent to the current planet"),
			FMath::Abs(FVector::DotProduct(Displacement.GetSafeNormal(), ReferenceUp)) < 0.02f);
	}
	Fixture.Ship->FlightMoveForward(FInputActionValue(0.0f));

	FJTSSpacecraftFlightStats FacingStats = Movement->GetEffectiveStats();
	FacingStats.MaxMoveSpeed = 0.0f;
	Movement->SetEffectiveStats(FacingStats);
	Fixture.Ship->FlightMoveForward(FInputActionValue(0.0f));
	Fixture.Ship->FlightTurnAround(FInputActionValue(false));
	Fixture.Ship->FlightSteerYaw(FInputActionValue(1.0f));
	Movement->StopMovementImmediately();
	const FQuat YawStartRotation = Fixture.Ship->GetActorQuat();
	const FVector FacingUp = Fixture.Ship->GetFlightReferenceUp();
	const FVector YawStartLocation = Fixture.Ship->GetActorLocation();
	for (int32 TurnStep = 0; TurnStep < 8; ++TurnStep)
	{
		Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	}
	const FVector YawedForward = Fixture.Ship->GetActorForwardVector();
	const FVector StartForward = YawStartRotation.GetForwardVector();
	const float YawedRight = FVector::DotProduct(
		FVector::CrossProduct(FacingUp, StartForward).GetSafeNormal(),
		YawedForward);
	TestTrue(TEXT("D yaws the hull to its own right without using the camera"), YawedRight > 0.2f);
	TestTrue(TEXT("Key yaw keeps the spacecraft aligned to radial up"),
		FVector::DotProduct(Fixture.Ship->GetActorUpVector(), FacingUp) > 0.95f);
	TestTrue(TEXT("Yaw does not translate the hull"),
		FVector::DistSquared(Fixture.Ship->GetActorLocation(), YawStartLocation) < FMath::Square(5.0f));

	Fixture.Ship->FlightSteerYaw(FInputActionValue(-1.0f));
	const FQuat LeftStartRotation = Fixture.Ship->GetActorQuat();
	for (int32 TurnStep = 0; TurnStep < 8; ++TurnStep)
	{
		Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	}
	const float YawedLeft = FVector::DotProduct(
		FVector::CrossProduct(FacingUp, LeftStartRotation.GetForwardVector()).GetSafeNormal(),
		Fixture.Ship->GetActorForwardVector());
	TestTrue(TEXT("A yaws the hull to its own left"), YawedLeft < -0.2f);
	Fixture.Ship->FlightSteerYaw(FInputActionValue(0.0f));

	FJTSSpacecraftFlightStats ReverseStats = Movement->GetEffectiveStats();
	ReverseStats.MaxMoveSpeed = 1000.0f;
	ReverseStats.FacingTurnRate = 3600.0f;
	ReverseStats.FacingTurnAcceleration = 36000.0f;
	Movement->SetEffectiveStats(ReverseStats);
	Movement->StopMovementImmediately();
	const FVector TurnAroundDeckUp = Fixture.Ship->GetActorUpVector().GetSafeNormal();
	const FVector TurnAroundFacing = FVector::VectorPlaneProject(
		Fixture.Ship->GetActorForwardVector(),
		TurnAroundDeckUp).GetSafeNormal();
	const FVector TurnAroundStart = Fixture.Ship->GetActorLocation();
	Fixture.Ship->FlightTurnAround(FInputActionValue(true));
	for (int32 TurnStep = 0; TurnStep < 20; ++TurnStep)
	{
		Movement->TickComponent(0.05f, LEVELTICK_All, nullptr);
		TestTrue(TEXT("S keeps the deck plane fixed while the nose swings"),
			FVector::DotProduct(Fixture.Ship->GetActorUpVector(), TurnAroundDeckUp) > 0.999f);
	}
	const FVector TurnedInDeck = FVector::VectorPlaneProject(
		Fixture.Ship->GetActorForwardVector(),
		TurnAroundDeckUp).GetSafeNormal();
	TestTrue(TEXT("S turns the nose around in place until it faces the old tail"),
		FVector::DotProduct(TurnedInDeck, -TurnAroundFacing) > 0.95f);
	TestTrue(TEXT("The turnaround leaves the ship's deck plane unchanged"),
		FVector::DotProduct(Fixture.Ship->GetActorUpVector(), TurnAroundDeckUp) > 0.999f);
	TestTrue(TEXT("The turnaround does not translate the hull"),
		FVector::DistSquared(Fixture.Ship->GetActorLocation(), TurnAroundStart) < FMath::Square(8.0f));
	const FQuat HeldTurnRotation = Fixture.Ship->GetActorQuat();
	for (int32 HoldStep = 0; HoldStep < 10; ++HoldStep)
	{
		Movement->TickComponent(0.05f, LEVELTICK_All, nullptr);
	}
	TestTrue(TEXT("Holding S after the swap does not keep spinning"),
		Fixture.Ship->GetActorQuat().AngularDistance(HeldTurnRotation) < 0.05f);
	Fixture.Ship->FlightTurnAround(FInputActionValue(false));

	// A level start hides a turnaround that flattens onto radial up. Pitch the hull first, then S
	// must yaw inside that tilted deck instead of rolling the belly over.
	Fixture.Ship->FlightSteerPitch(FInputActionValue(1.0f));
	for (int32 PitchStep = 0; PitchStep < 16; ++PitchStep)
	{
		Movement->TickComponent(0.05f, LEVELTICK_All, nullptr);
	}
	Fixture.Ship->FlightSteerPitch(FInputActionValue(0.0f));
	Movement->StopMovementImmediately();
	const FVector PitchedDeckUp = Fixture.Ship->GetActorUpVector().GetSafeNormal();
	const FVector PitchedForward = FVector::VectorPlaneProject(
		Fixture.Ship->GetActorForwardVector(),
		PitchedDeckUp).GetSafeNormal();
	TestTrue(TEXT("Pitch leaves the hull off the radial horizon before the turnaround"),
		FMath::Abs(FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), FacingUp)) > 0.25f);
	Fixture.Ship->FlightTurnAround(FInputActionValue(true));
	for (int32 TurnStep = 0; TurnStep < 20; ++TurnStep)
	{
		Movement->TickComponent(0.05f, LEVELTICK_All, nullptr);
	}
	TestTrue(TEXT("A pitched hull turns around inside its own deck plane"),
		FVector::DotProduct(
			FVector::VectorPlaneProject(Fixture.Ship->GetActorForwardVector(), PitchedDeckUp).GetSafeNormal(),
			-PitchedForward) > 0.95f
		&& FVector::DotProduct(Fixture.Ship->GetActorUpVector(), PitchedDeckUp) > 0.999f);
	Fixture.Ship->FlightTurnAround(FInputActionValue(false));

	Fixture.Ship->FlightMoveForward(FInputActionValue(0.0f));
	Movement->StopMovementImmediately();
	const float PitchUpMouseInput = 30.0f / FMath::Max(Fixture.Ship->FlightCameraLookSensitivity, KINDA_SMALL_NUMBER);
	const FQuat AttitudeBeforeCameraPitch = Fixture.Ship->GetActorQuat();
	Fixture.Ship->FlightLookPitch(FInputActionValue(PitchUpMouseInput));
	Fixture.Ship->FlightMoveForward(FInputActionValue(1.0f));
	const FVector ClimbUp = Fixture.Ship->GetFlightReferenceUp();
	const FVector ClimbCameraForward = Fixture.Ship->GetFlightCameraForward(ClimbUp).GetSafeNormal();
	TestTrue(TEXT("Pitching the camera up can look back at the ship and sky"),
		FVector::DotProduct(ClimbCameraForward, ClimbUp) > 0.45f);
	const FVector ClimbStart = Fixture.Ship->GetActorLocation();
	const FVector HullBeforeClimb = Fixture.Ship->GetActorForwardVector();
	Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	const FVector ClimbDirection = (Fixture.Ship->GetActorLocation() - ClimbStart).GetSafeNormal();
	TestTrue(TEXT("W follows the hull after the camera has looked up"),
		FVector::DotProduct(ClimbDirection, HullBeforeClimb) > 0.95f);
	TestTrue(TEXT("Looking up does not pitch the hull"),
		FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), AttitudeBeforeCameraPitch.GetForwardVector()) > 0.995f);

	Movement->StopMovementImmediately();
	Fixture.Ship->FlightMoveForward(FInputActionValue(0.0f));
	Fixture.Ship->FlightSteerPitch(FInputActionValue(-1.0f));
	const FVector DiveUp = Fixture.Ship->GetFlightReferenceUp();
	const FVector DiveStartForward = Fixture.Ship->GetActorForwardVector();
	const FVector DiveTangent = FVector::VectorPlaneProject(DiveStartForward, DiveUp).GetSafeNormal();
	for (int32 DiveStep = 0; DiveStep < 12; ++DiveStep)
	{
		Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	}
	const float HullDiveComponent = FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), DiveUp);
	AddInfo(FString::Printf(
		TEXT("Surface dive envelope: assist=%.4f hullVertical=%.4f"),
		Movement->GetSurfaceFlightAssistAlpha(),
		HullDiveComponent));
	TestTrue(TEXT("A held nose-down key is limited to the shallow surface envelope"),
		HullDiveComponent < -0.02f
		&& HullDiveComponent > -0.35f
		&& FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), DiveTangent) > 0.85f);
	const float PitchDownMouseInput = -60.0f / FMath::Max(Fixture.Ship->FlightCameraLookSensitivity, KINDA_SMALL_NUMBER);
	Fixture.Ship->FlightLookPitch(FInputActionValue(PitchDownMouseInput));
	const FVector DiveCameraForward = Fixture.Ship->GetFlightCameraForward(DiveUp);
	TestTrue(TEXT("The camera can look past the limited hull dive"),
		FVector::DotProduct(DiveCameraForward, DiveUp) < HullDiveComponent - 0.15f);

	Fixture.Ship->FlightMoveForward(FInputActionValue(0.0f));
	Fixture.Ship->FlightSteerYaw(FInputActionValue(0.0f));
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

	const float DescentStartClearance = Fixture.Ship->GetLandingCollisionClearance(InitialUp) + 500.0f;
	Fixture.Ship->SetActorLocation(
		Fixture.Planet->GetPlanetCenter() + InitialUp * (Fixture.Planet->GetApproximateRadius() + DescentStartClearance));
	Movement->StopMovementImmediately();
	Movement->SetVerticalInput(-1.0f);
	for (int32 DescentStep = 0; DescentStep < 240; ++DescentStep)
	{
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	}
	float ProtectedSurfaceAltitude = 0.0f;
	const float ProtectedHullClearance = Fixture.Ship->GetLandingCollisionClearance(InitialUp);
	TestTrue(TEXT("Holding Ctrl outside a landing capture cannot drive the physical hull into terrain"),
		Movement->GetResolvedSurfaceAltitude(Fixture.Planet, ProtectedSurfaceAltitude)
		&& ProtectedSurfaceAltitude >= ProtectedHullClearance + 90.0f);

	Movement->SetVerticalInput(0.0f);
	Movement->StopMovementImmediately();
	const FVector DepartureUp = Movement->GetReferenceUp();
	FVector DepartureTangent = FVector::VectorPlaneProject(Fixture.Ship->GetActorForwardVector(), DepartureUp).GetSafeNormal();
	if (DepartureTangent.IsNearlyZero())
	{
		FVector DepartureRight;
		DepartureUp.FindBestAxisVectors(DepartureTangent, DepartureRight);
	}
	Fixture.Ship->SetActorRotation(FRotationMatrix::MakeFromXZ(
		(DepartureTangent + DepartureUp).GetSafeNormal(),
		DepartureUp).ToQuat());
	Movement->SetSteeringInput(FVector2D(0.0f, 1.0f));
	Movement->SetMoveInput(FVector2D(0.0f, 1.0f));
	const FVector DepartureStart = Fixture.Ship->GetActorLocation();
	for (int32 DepartureStep = 0; DepartureStep < 30; ++DepartureStep)
	{
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	}
	TestTrue(TEXT("A close-surface nose-up command lifts before rotating the long hull into terrain"),
		FVector::DotProduct(Fixture.Ship->GetActorLocation() - DepartureStart, DepartureUp) > 150.0f
		&& FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), DepartureUp) > 0.25f);

	Fixture.Ship->FlightMoveVertical(FInputActionValue(0.0f));
	Movement->StopMovementImmediately();
	Fixture.Ship->FlightMoveForward(FInputActionValue(1.0f));
	const FVector RecoveryStart = Fixture.Ship->GetActorLocation();
	Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Terrain protection leaves hull-forward flight available instead of wedging the craft"),
		FVector::DistSquared(Fixture.Ship->GetActorLocation(), RecoveryStart) > FMath::Square(10.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSSpaceFlightFrameRegression, "JTS.Spacecraft.PlanetFrameHandoff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSSpaceFlightFrameRegression::RunTest(const FString& Parameters)
{
	FShipTestWorld Fixture;
	if (!TestTrue(TEXT("Planet-frame handoff test parks on the source planet"), Fixture.Park(FVector::RightVector)))
	{
		return false;
	}

	UJTSSpacecraftFlightMovementComponent* const Movement = Fixture.Ship->GetFlightMovementComponent();
	if (!TestTrue(TEXT("Planet-frame handoff has movement"), IsValid(Movement))
		|| !TestTrue(TEXT("Planet-frame handoff begins takeoff"), Fixture.Ship->BeginSurfaceTakeoff()))
	{
		return false;
	}

	const FVector SourceUp = Fixture.Planet->GetRadialUpVector(Fixture.Ship->GetActorLocation()).GetSafeNormal();
	const float SourceRadius = Fixture.Planet->GetApproximateRadius();
	const float SurfaceFrameAltitude = Fixture.Planet->GetTakeoffTransitionAltitude();
	const float SpaceFlightAltitude = Fixture.Planet->GetSpaceFlightAltitude();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	const FVector RecoveryUp = FVector::UpVector;
	Fixture.Ship->SetActorLocation(Fixture.Planet->GetPlanetCenter()
		+ RecoveryUp * (SourceRadius + SurfaceFrameAltitude * 0.5f));
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Surface recovery uses the temporary local radial frame"),
		FVector::DotProduct(Fixture.Ship->GetFlightReferenceUp(), RecoveryUp) > 0.999f);
	Fixture.Ship->SetActorLocation(Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (SourceRadius + SurfaceFrameAltitude + 100.0f));
	Fixture.Ship->SetActorRotation(FRotationMatrix::MakeFromXZ(FVector::ForwardVector, FVector::UpVector).ToQuat());
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Leaving the shallow surface band releases the radial movement frame"),
		Movement->IsUsingPlanetSurfaceFlightFrame());
	TestEqual(TEXT("The wider space-flight threshold retains the takeoff state briefly"),
		Fixture.Manager->GetCurrentTravelState(), EJTSSpaceTravelState::Takeoff);
	TestTrue(TEXT("Free-flight camera preserves the departure horizon without snapping to World-Z"),
		FVector::DotProduct(Fixture.Ship->GetFlightReferenceUp(), SourceUp) > 0.999f);
	TestTrue(TEXT("Surface recovery does not overwrite the departure inertial horizon"),
		FVector::DotProduct(Fixture.Ship->GetFlightReferenceUp(), SourceUp) > 0.999f);

	Fixture.Ship->SetActorLocation(Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (SourceRadius + SpaceFlightAltitude + 100.0f));
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Takeoff transitions to the shared free-space travel state"),
		Fixture.Manager->GetCurrentTravelState(), EJTSSpaceTravelState::SpaceFlight);
	TestNull(TEXT("Free space clears the departed planet flight target"), Fixture.Ship->GetFlightPlanet());

	Fixture.Ship->SetActorLocation(Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (SourceRadius + Fixture.Planet->GetGravityInfluenceRange() + 100.0f));
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestNull(TEXT("Deep space clears the stale source planet world reference"), Fixture.Manager->GetCurrentPlanet());

	// A held pitch key turns the hull and then holds that attitude. The camera is not part of the
	// command, so looking around while thrusting cannot start a vertical rotation loop.
	FJTSSpacecraftFlightStats DeepSpaceStats = Movement->GetEffectiveStats();
	DeepSpaceStats.MaxMoveSpeed = 1000.0f;
	DeepSpaceStats.Acceleration = 100000.0f;
	DeepSpaceStats.Deceleration = 100000.0f;
	DeepSpaceStats.FacingTurnRate = 3600.0f;
	DeepSpaceStats.FacingTurnAcceleration = 36000.0f;
	Movement->SetEffectiveStats(DeepSpaceStats);
	Fixture.Ship->SetActorRotation(FQuat::Identity);
	Movement->StopMovementImmediately();
	Fixture.Ship->bFlightCameraFrameInitialized = false;
	const float PitchInput = 30.0f / FMath::Max(Fixture.Ship->FlightCameraLookSensitivity, KINDA_SMALL_NUMBER);
	Fixture.Ship->FlightLookPitch(FInputActionValue(PitchInput));
	Fixture.Ship->FlightSteerPitch(FInputActionValue(1.0f));
	Fixture.Ship->FlightMoveForward(FInputActionValue(1.0f));
	for (int32 Step = 0; Step < 8; ++Step)
	{
		Movement->TickComponent(0.05f, LEVELTICK_All, nullptr);
		Fixture.Ship->Tick(0.05f);
	}
	TestTrue(TEXT("Deep-space pitch keys raise the nose while the camera stays independent"),
		FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), FVector::UpVector) > 0.35f);
	Fixture.Ship->FlightSteerPitch(FInputActionValue(0.0f));
	const FQuat SettledForwardFlightRotation = Fixture.Ship->GetActorQuat();
	for (int32 Step = 0; Step < 30; ++Step)
	{
		Movement->TickComponent(0.05f, LEVELTICK_All, nullptr);
		Fixture.Ship->Tick(0.05f);
	}
	TestTrue(TEXT("Releasing the pitch key holds the hull and does not loop"),
		Fixture.Ship->GetActorQuat().AngularDistance(SettledForwardFlightRotation) < 0.001f);

	Movement->StopMovementImmediately();
	const FQuat ReverseStartRotation = Fixture.Ship->GetActorQuat();
	const FVector ReverseStartLocation = Fixture.Ship->GetActorLocation();
	const FVector DeckUp = ReverseStartRotation.GetUpVector().GetSafeNormal();
	const FVector DeckForward = FVector::VectorPlaneProject(
		ReverseStartRotation.GetForwardVector(),
		DeckUp).GetSafeNormal();
	Fixture.Ship->FlightLookPitch(FInputActionValue(-60.0f / FMath::Max(Fixture.Ship->FlightCameraLookSensitivity, KINDA_SMALL_NUMBER)));
	Fixture.Ship->FlightMoveForward(FInputActionValue(0.0f));
	Fixture.Ship->FlightTurnAround(FInputActionValue(true));
	for (int32 TurnStep = 0; TurnStep < 20; ++TurnStep)
	{
		Movement->TickComponent(0.05f, LEVELTICK_All, nullptr);
		Fixture.Ship->Tick(0.05f);
		TestTrue(TEXT("Deep-space S keeps the captured deck plane"),
			FVector::DotProduct(Fixture.Ship->GetActorUpVector(), DeckUp) > 0.999f);
	}
	const FVector TurnedDeckForward = FVector::VectorPlaneProject(
		Fixture.Ship->GetActorForwardVector(),
		DeckUp).GetSafeNormal();
	TestTrue(TEXT("Deep-space S turns the nose onto the old tail inside the ship's plane"),
		FVector::DotProduct(TurnedDeckForward, -DeckForward) > 0.95f);
	TestTrue(TEXT("Deep-space turnaround does not roll the deck onto another up axis"),
		FVector::DotProduct(Fixture.Ship->GetActorUpVector(), DeckUp) > 0.999f);
	TestTrue(TEXT("Deep-space turnaround does not translate"),
		FVector::DistSquared(Fixture.Ship->GetActorLocation(), ReverseStartLocation) < FMath::Square(8.0f));
	Fixture.Ship->FlightTurnAround(FInputActionValue(false));
	Movement->StopMovementImmediately();

	AJTSPlanetAnchor* const DestinationPlanet = Fixture.AddFlightPlanet(TEXT("Destination"), FVector(100000.0f, 0.0f, 0.0f));
	const FVector DestinationUp = FVector::ForwardVector;
	constexpr float DestinationSurfaceRadius = 10000.0f;
	FindFProperty<FFloatProperty>(DestinationPlanet->GetClass(), TEXT("ApproximateRadius"))
		->SetPropertyValue_InContainer(DestinationPlanet, 13000.0f);
	Fixture.Ship->SetActorLocation(DestinationPlanet->GetPlanetCenter()
		+ DestinationUp * (DestinationSurfaceRadius + 7000.0f));
	Fixture.Ship->SetActorRotation(FRotationMatrix::MakeFromXZ(FVector::ForwardVector, FVector::UpVector).ToQuat());
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Entering another influence range changes the current planet"),
		Fixture.Manager->GetCurrentPlanet(), DestinationPlanet);
	TestEqual(TEXT("Entering another influence range binds that planet as the arrival target"),
		Fixture.Ship->GetFlightPlanet(), DestinationPlanet);
	TestFalse(TEXT("Arrival target remains free flight above its shallow surface band"),
		Movement->IsUsingPlanetSurfaceFlightFrame());
	TestTrue(TEXT("Arrival camera keeps the departure inertial frame at high altitude"),
		FVector::DotProduct(Fixture.Ship->GetFlightReferenceUp(), SourceUp) > 0.999f
		&& FMath::Abs(FVector::DotProduct(Fixture.Ship->GetFlightReferenceUp(), DestinationUp)) < 0.01f);
	float ResolvedDestinationAltitude = 0.0f;
	TestTrue(TEXT("Arrival altitude resolves from the real mesh despite a mismatched authored radius"),
		DestinationPlanet->GetAltitudeAboveSurface(Fixture.Ship->GetActorLocation(), ResolvedDestinationAltitude)
		&& FMath::IsNearlyEqual(ResolvedDestinationAltitude, 7000.0f, 5.0f));

	Fixture.Ship->SetActorLocation(DestinationPlanet->GetPlanetCenter()
		+ DestinationUp * (DestinationSurfaceRadius + SurfaceFrameAltitude - 300.0f));
	Movement->SetVerticalInput(-1.0f);
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	Movement->SetVerticalInput(0.0f);
	TestEqual(TEXT("Descending into the destination begins approach"),
		Fixture.Manager->GetCurrentTravelState(), EJTSSpaceTravelState::Approach);
	TestTrue(TEXT("Destination surface assist enters continuously inside its transition band"),
		Movement->GetSurfaceFlightAssistAlpha() > 0.0f
		&& Movement->GetSurfaceFlightAssistAlpha() < 1.0f);
	TestTrue(TEXT("Arrival reference starts blending toward destination radial Up"),
		FVector::DotProduct(Fixture.Ship->GetFlightReferenceUp(), DestinationUp) > 0.0f
		&& FVector::DotProduct(Fixture.Ship->GetFlightReferenceUp(), DestinationUp) < 0.999f);

	Fixture.Ship->SetActorLocation(DestinationPlanet->GetPlanetCenter()
		+ DestinationUp * (DestinationSurfaceRadius + SurfaceFrameAltitude * 0.5f));
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Low destination flight reaches the full destination surface frame"),
		Movement->GetSurfaceFlightAssistAlpha() > 0.999f
		&& FVector::DotProduct(Fixture.Ship->GetFlightReferenceUp(), DestinationUp) > 0.999f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSInterplanetaryCruiseRegression, "JTS.Spacecraft.InterplanetaryCruise",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSInterplanetaryCruiseRegression::RunTest(const FString& Parameters)
{
	FShipTestWorld Fixture;
	if (!TestTrue(TEXT("Free-flight test parks on the source planet"), Fixture.Park(FVector::RightVector)))
	{
		return false;
	}

	UJTSSpacecraftFlightMovementComponent* const Movement = Fixture.Ship->GetFlightMovementComponent();
	if (!TestTrue(TEXT("Free-flight test has movement"), IsValid(Movement))
		|| !TestTrue(TEXT("Free-flight test begins takeoff"), Fixture.Ship->BeginSurfaceTakeoff()))
	{
		return false;
	}

	AJTSPlanetAnchor* const Mars = Fixture.AddFlightPlanet(TEXT("Mars"), FVector(180000.0f, 0.0f, 0.0f));
	AJTSPlanetAnchor* const Deimos = Fixture.AddFlightPlanet(TEXT("Deimos"), FVector(210000.0f, 0.0f, 0.0f));
	AStaticMeshActor* const MarsSurface = Cast<AStaticMeshActor>(Mars->GetGameplaySurfaceActor());
	if (!TestTrue(TEXT("Free-flight test places a Mars surface"), IsValid(MarsSurface)))
	{
		return false;
	}

	FindFProperty<FFloatProperty>(Fixture.Planet->GetClass(), TEXT("HeliocentricDistanceKilometers"))
		->SetPropertyValue_InContainer(Fixture.Planet, 384400.0f);
	FindFProperty<FFloatProperty>(Mars->GetClass(), TEXT("HeliocentricDistanceKilometers"))
		->SetPropertyValue_InContainer(Mars, 227900000.0f);
	FindFProperty<FFloatProperty>(Deimos->GetClass(), TEXT("HeliocentricDistanceKilometers"))
		->SetPropertyValue_InContainer(Deimos, 227923400.0f);
	FindFProperty<FNameProperty>(Deimos->GetClass(), TEXT("ParentPlanetId"))
		->SetPropertyValue_InContainer(Deimos, FName(TEXT("Mars")));

	const FVector SourceUp = Fixture.Planet->GetRadialUpVector(Fixture.Ship->GetActorLocation()).GetSafeNormal();
	const FVector OutsideLocation = Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (Fixture.Planet->GetVisualRadius() + Fixture.Planet->GetGravityInfluenceRange() + 20000.0f);
	Fixture.Ship->SetActorLocation(OutsideLocation);
	const FVector TowardMars = (Mars->GetPlanetCenter() - Fixture.Ship->GetActorLocation()).GetSafeNormal();
	Fixture.Ship->SetActorRotation(FRotationMatrix::MakeFromX(TowardMars).ToQuat());
	Movement->StopMovementImmediately();
	Movement->SetMoveInput(FVector2D(0.0f, 1.0f));
	Movement->SetSteeringInput(FVector2D::ZeroVector);
	Movement->TickComponent(1.0f, LEVELTICK_All, nullptr);
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);

	TestNull(TEXT("Leaving every influence range keeps deep space unbound"), Fixture.Manager->GetCurrentPlanet());
	const FVector Traveled = Fixture.Ship->GetActorLocation() - OutsideLocation;
	TestTrue(TEXT("Forward flight translates toward the aimed planet"),
		FVector::DotProduct(Traveled.GetSafeNormal(), TowardMars) > 0.95f
		&& Traveled.SizeSquared() > FMath::Square(500.0f));
	TestTrue(TEXT("A real interplanetary route is millions of kilometres"),
		Fixture.Manager->GetRouteKilometers(Fixture.Planet, Mars) > 1000000.0f);
	TestTrue(TEXT("A parent and child stay a local transfer"),
		Fixture.Manager->SharesLocalSky(Mars, Deimos));
	const float ApparentAtDeparture = Fixture.Manager->GetApparentRangeCentimeters(Fixture.Ship, Mars);
	TestTrue(TEXT("Just after departure the destination still reads as interplanetary"),
		ApparentAtDeparture > 1000000.0f * 100000.0f);
	TestFalse(TEXT("The departure body stays visible beside the ship"), Fixture.Planet->IsHidden());
	float ShrinkProbeAltitude = 0.0f;
	TestTrue(TEXT("Shrink probe measures altitude from the real surface"),
		Fixture.Planet->GetAltitudeAboveSurface(OutsideLocation, ShrinkProbeAltitude));
	const FVector ShrinkProbeLocation = Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (FVector::Distance(OutsideLocation, Fixture.Planet->GetPlanetCenter()) - ShrinkProbeAltitude + 70000.0f);
	Fixture.Ship->SetActorLocation(ShrinkProbeLocation);
	Movement->SetMoveInput(FVector2D::ZeroVector);
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	const float ScaleJustPastRelease = Fixture.Planet->GetGameplaySurfaceActor()->GetActorScale3D().GetAbsMax();
	TestTrue(TEXT("Just past 500 m the departure body has started shrinking and is still a world"),
		ScaleJustPastRelease < 200.0f * 0.9f && ScaleJustPastRelease > 200.0f * 0.05f);
	TestFalse(TEXT("Just past 500 m the departure body is still on screen"), Fixture.Planet->IsHidden());
	Fixture.Ship->SetActorLocation(OutsideLocation);
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("The distant destination is hidden while its disk is smaller than a pixel"), Mars->IsHidden());
	const FVector MarsSurfaceLocation = MarsSurface->GetActorLocation();

	const FVector ApproachLocation = Mars->GetPlanetCenter()
		+ FVector::UpVector * (Mars->GetApproximateRadius() + Mars->GetGravityInfluenceRange() - 500.0f);
	Fixture.Ship->SetActorLocation(ApproachLocation);
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	const float ApparentOnApproach = Fixture.Manager->GetApparentRangeCentimeters(Fixture.Ship, Fixture.Planet);
	TestTrue(TEXT("Arriving leaves the departure body at an interplanetary range"),
		ApparentOnApproach > 1000000.0f * 100000.0f);
	TestFalse(TEXT("The destination is visible again inside its own neighbourhood"), Mars->IsHidden());
	TestTrue(TEXT("Scaling a distant body leaves its surface where it was authored"),
		MarsSurface->GetActorLocation().Equals(MarsSurfaceLocation, 1.0f));
	const FVector DepartureScaleAfterCruise = Fixture.Planet->GetGameplaySurfaceActor()->GetActorScale3D();
	TestTrue(TEXT("Leaving the 500 m band lets the departure body shrink"),
		DepartureScaleAfterCruise.GetAbsMax() < 200.0f * 0.5f);

	FActorSpawnParameters MarkerSpawnParameters;
	MarkerSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AJTSPlanetLandingSite* const DepartureMarker = Fixture.World->SpawnActor<AJTSPlanetLandingSite>(
		AJTSPlanetLandingSite::StaticClass(),
		Fixture.Planet->GetPlanetCenter() + SourceUp * Fixture.Planet->GetApproximateRadius(),
		FRotator::ZeroRotator,
		MarkerSpawnParameters);
	FindFProperty<FObjectProperty>(DepartureMarker->GetClass(), TEXT("PlanetAnchor"))
		->SetObjectPropertyValue_InContainer(DepartureMarker, Fixture.Planet);
	DepartureMarker->DispatchBeginPlay();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("A landing marker left behind a shrunken body is hidden"), DepartureMarker->IsHidden());
	TestFalse(TEXT("A landing marker left behind a shrunken body has no collision"), DepartureMarker->GetActorEnableCollision());
	TestEqual(TEXT("Flying into the destination influence reacquires that planet"),
		Fixture.Manager->GetCurrentPlanet(), Mars);

	const FVector AuthoredBandLocation = Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (Fixture.Planet->GetVisualRadius() + 45000.0f);
	Fixture.Ship->SetActorLocation(AuthoredBandLocation);
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Within 500 m of the surface the body keeps its authored scale"),
		Fixture.Planet->GetGameplaySurfaceActor()->GetActorScale3D().Equals(FVector(200.0), 0.1));
	TestFalse(TEXT("Within 500 m of the surface the landing marker is shown"), DepartureMarker->IsHidden());

	const FVector ReturnLocation = Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (Fixture.Planet->GetApproximateRadius() + Fixture.Planet->GetGravityInfluenceRange() * 0.5f);
	Fixture.Ship->SetActorLocation(ReturnLocation);
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Turning back makes the departure body visible again"), Fixture.Planet->IsHidden());
	TestTrue(TEXT("Returning inside 500 m restores the authored surface scale"),
		Fixture.Planet->GetGameplaySurfaceActor()->GetActorScale3D().Equals(FVector(200.0), 0.1));
	TestFalse(TEXT("Returning inside 500 m shows the landing marker again"), DepartureMarker->IsHidden());
	TestEqual(TEXT("Flying back into the departure influence reacquires that planet"),
		Fixture.Manager->GetCurrentPlanet(), Fixture.Planet);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMarIILandingMapRegression, "JTS.Spacecraft.MarIILandingMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSMarIILandingMapRegression::RunTest(const FString& Parameters)
{
	// This is deliberately a map-data regression rather than a synthetic sphere test. The actor
	// transforms, static mesh, collision profile, and LandingSite Blueprint all come from the
	// shipped SpaceWorld map, but are exercised inside an isolated game world.
	UWorld* const SourceMap = LoadObject<UWorld>(nullptr, TEXT("/Game/Space/Maps/L_SpaceWorld.L_SpaceWorld"));
	if (!TestNotNull(TEXT("SpaceWorld map asset loads"), SourceMap)
		|| !TestNotNull(TEXT("SpaceWorld persistent level exists"), SourceMap->PersistentLevel.Get()))
	{
		return false;
	}

	AJTSPlanetAnchor* SourceMarII = nullptr;
	AJTSPlanetLandingSite* SourceLandingSite = nullptr;
	for (AActor* const Actor : SourceMap->PersistentLevel->Actors)
	{
		if (AJTSPlanetAnchor* const CandidateAnchor = Cast<AJTSPlanetAnchor>(Actor);
			IsValid(CandidateAnchor) && CandidateAnchor->GetPlanetId() == TEXT("MarII"))
		{
			SourceMarII = CandidateAnchor;
		}
	}
	for (AActor* const Actor : SourceMap->PersistentLevel->Actors)
	{
		if (AJTSPlanetLandingSite* const CandidateSite = Cast<AJTSPlanetLandingSite>(Actor);
			IsValid(CandidateSite) && CandidateSite->GetPlanetAnchor() == SourceMarII)
		{
			SourceLandingSite = CandidateSite;
			break;
		}
	}
	if (!TestNotNull(TEXT("MarII PlanetAnchor exists in SpaceWorld"), SourceMarII)
		|| !TestNotNull(TEXT("MarII LandingSite exists and is directly bound"), SourceLandingSite))
	{
		return false;
	}

	AActor* const SourceSurfaceActor = SourceMarII->GetGameplaySurfaceActor();
	UStaticMeshComponent* const SourceSurfaceMesh = IsValid(SourceSurfaceActor)
		? SourceSurfaceActor->FindComponentByClass<UStaticMeshComponent>()
		: nullptr;
	if (!TestNotNull(TEXT("MarII has a configured gameplay surface mesh"), SourceSurfaceMesh)
		|| !TestNotNull(TEXT("MarII gameplay surface has a static mesh"), SourceSurfaceMesh->GetStaticMesh().Get()))
	{
		return false;
	}
	UStaticMesh* const SourceStaticMesh = SourceSurfaceMesh->GetStaticMesh().Get();
	AddInfo(FString::Printf(TEXT("MarII source geometry: Triangles=%d HasComplexData=%d"),
		SourceStaticMesh->GetNumTriangles(0),
		static_cast<int32>(SourceStaticMesh->ContainsPhysicsTriMeshData(true))));

	FShipTestWorld Fixture;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AJTSPlanetAnchor* const MarII = Fixture.World->SpawnActor<AJTSPlanetAnchor>(
		AJTSPlanetAnchor::StaticClass(), SourceMarII->GetActorTransform(), SpawnParameters);
	AStaticMeshActor* const MarIISurface = Fixture.World->SpawnActor<AStaticMeshActor>(
		AStaticMeshActor::StaticClass(), SourceSurfaceActor->GetActorTransform(), SpawnParameters);
	if (!TestNotNull(TEXT("Isolated MarII anchor spawns"), MarII)
		|| !TestNotNull(TEXT("Isolated MarII surface spawns"), MarIISurface))
	{
		return false;
	}

	UStaticMeshComponent* const MarIISurfaceMesh = MarIISurface->GetStaticMeshComponent();
	MarIISurfaceMesh->SetStaticMesh(SourceStaticMesh);
	MarIISurfaceMesh->SetCollisionEnabled(SourceSurfaceMesh->GetCollisionEnabled());
	MarIISurfaceMesh->SetCollisionProfileName(SourceSurfaceMesh->GetCollisionProfileName());
	// The test world is created before this component receives its mesh. Refresh the registered
	// collision state explicitly so a map asset using complex-as-simple collision is queryable too.
	MarIISurfaceMesh->SetCollisionObjectType(SourceSurfaceMesh->GetCollisionObjectType());
	MarIISurfaceMesh->SetCollisionResponseToChannel(ECC_Pawn, SourceSurfaceMesh->GetCollisionResponseToChannel(ECC_Pawn));
	MarIISurfaceMesh->SetCollisionResponseToChannel(ECC_Visibility, SourceSurfaceMesh->GetCollisionResponseToChannel(ECC_Visibility));
	MarIISurfaceMesh->UpdateBounds();
	MarIISurfaceMesh->RecreatePhysicsState();
	MarIISurface->SetActorTransform(SourceSurfaceActor->GetActorTransform(), false, nullptr, ETeleportType::TeleportPhysics);
	FindFProperty<FNameProperty>(MarII->GetClass(), TEXT("PlanetId"))
		->SetPropertyValue_InContainer(MarII, SourceMarII->GetPlanetId());
	FindFProperty<FFloatProperty>(MarII->GetClass(), TEXT("ApproximateRadius"))
		->SetPropertyValue_InContainer(MarII, SourceMarII->GetApproximateRadius());
	FindFProperty<FObjectProperty>(MarII->GetClass(), TEXT("GameplaySurfaceActor"))
		->SetObjectPropertyValue_InContainer(MarII, MarIISurface);
	Fixture.Manager->RegisterPlanet(MarII);

	AJTSPlanetLandingSite* const LandingSite = Fixture.World->SpawnActor<AJTSPlanetLandingSite>(
		SourceLandingSite->GetClass(), SourceLandingSite->GetActorTransform(), SpawnParameters);
	AJTSPlanetLandingManager* const LandingManager = Fixture.World->SpawnActor<AJTSPlanetLandingManager>();
	if (!TestNotNull(TEXT("Isolated MarII LandingSite spawns from its authored Blueprint"), LandingSite)
		|| !TestNotNull(TEXT("Isolated LandingManager spawns"), LandingManager))
	{
		return false;
	}
	FindFProperty<FObjectProperty>(LandingSite->GetClass(), TEXT("PlanetAnchor"))
		->SetObjectPropertyValue_InContainer(LandingSite, MarII);
	UBoxComponent* const SourceVolume = SourceLandingSite->FindComponentByClass<UBoxComponent>();
	UBoxComponent* const LandingVolume = LandingSite->FindComponentByClass<UBoxComponent>();
	if (!TestNotNull(TEXT("MarII LandingSite source has a LandingVolume"), SourceVolume)
		|| !TestNotNull(TEXT("Isolated MarII LandingSite has a LandingVolume"), LandingVolume))
	{
		return false;
	}
	LandingVolume->SetRelativeTransform(SourceVolume->GetRelativeTransform());
	LandingVolume->SetBoxExtent(SourceVolume->GetUnscaledBoxExtent(), true);
	TestTrue(TEXT("MarII LandingSite registers"), LandingManager->RegisterLandingSite(LandingSite));

	FJTSPlanetSurfaceFrame LandingFrame;
	AddInfo(FString::Printf(TEXT("MarII surface setup: SourceMesh=%s Collision=%d Visibility=%d Bounds=%s"),
		*GetNameSafe(SourceSurfaceMesh->GetStaticMesh().Get()),
		static_cast<int32>(MarIISurfaceMesh->GetCollisionEnabled()),
		static_cast<int32>(MarIISurfaceMesh->GetCollisionResponseToChannel(ECC_Visibility)),
		*MarIISurfaceMesh->Bounds.GetBox().ToString()));
	if (!TestTrue(TEXT("MarII authored landing frame resolves against its actual mesh collision"),
		MarII->GetSurfaceFrameAt(LandingSite->GetActorLocation(), FVector::ForwardVector, LandingFrame)))
	{
		return false;
	}
	TestTrue(TEXT("MarII real mesh point at the cyan marker belongs to the legal LandingSite footprint"),
		LandingSite->IsLocationInsideLandingArea(LandingFrame.Location));

	const FQuat SurfaceRotation = FRotationMatrix::MakeFromXZ(LandingFrame.Forward, LandingFrame.Up).ToQuat();
	Fixture.Ship->SetActorLocationAndRotation(
		LandingFrame.Location + LandingFrame.Up * 900.0f,
		SurfaceRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	Fixture.Ship->SetFlightTargetPlanet(MarII);
	Fixture.Ship->GetFlightMovementComponent()->StopMovementImmediately();
	const bool bAccepted = Fixture.Ship->RequestLanding();
	AddInfo(FString::Printf(TEXT("MarII landing request: Accepted=%d Failure=%d Ground=%s"),
		bAccepted,
		static_cast<int32>(Fixture.Ship->GetLastLandingFailure()),
		*Fixture.Ship->GetGroundInfo().GroundLocation.ToCompactString()));
	if (!TestTrue(TEXT("MarII accepts a stationary spacecraft centred over the real landing marker"), bAccepted))
	{
		return false;
	}

	UJTSSpacecraftFlightMovementComponent* const Movement = Fixture.Ship->GetFlightMovementComponent();
	for (int32 Step = 0; Step < 1800 && !Fixture.Ship->IsLanded(); ++Step)
	{
		Fixture.Ship->Tick(1.0f / 60.0f);
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	}
	AddInfo(FString::Printf(TEXT("MarII landing result: State=%d Phase=%d Failure=%d Height=%.2f"),
		static_cast<int32>(Fixture.Ship->GetFlightState()),
		static_cast<int32>(Fixture.Ship->GetLandingAssistPhase()),
		static_cast<int32>(Fixture.Ship->GetLastLandingFailure()),
		Fixture.Ship->GetGroundInfo().DockingHeight));
	if (!TestTrue(TEXT("MarII controlled landing reaches the landed state"), Fixture.Ship->IsLanded()))
	{
		return false;
	}

	AJTSCharacter* DisembarkedCharacter = nullptr;
	APlayerController* const DisembarkController = Fixture.AddPlayer(DisembarkedCharacter);
	if (!TestTrue(TEXT("MarII driver boards the landed spacecraft"), Fixture.Ship->TryBoardPlayer(DisembarkedCharacter))
		|| !TestTrue(TEXT("MarII driver exits to the real mesh surface"), Fixture.Ship->TryDisembarkPlayerForController(DisembarkController)))
	{
		return false;
	}
	TestEqual(TEXT("MarII disembark assigns the landed planet"), DisembarkedCharacter->GetGameplayPlanet(), MarII);
	TestTrue(TEXT("MarII disembark enters walking mode instead of settling from inside terrain"),
		DisembarkedCharacter->GetCharacterMovement()->MovementMode == MOVE_Walking);
	FJTSPlanetSurfaceFrame DisembarkSurfaceFrame;
	if (TestTrue(TEXT("MarII disembark resolves the actual mesh beneath the capsule"), MarII->GetSurfaceFrameAt(
		DisembarkedCharacter->GetActorLocation(), DisembarkedCharacter->GetActorForwardVector(), DisembarkSurfaceFrame)))
	{
		const UCapsuleComponent* const DisembarkCapsule = DisembarkedCharacter->GetCapsuleComponent();
		const float CapsuleRadius = DisembarkCapsule->GetScaledCapsuleRadius();
		const float CapsuleCylinderHalfHeight = FMath::Max(0.0f, DisembarkCapsule->GetScaledCapsuleHalfHeight() - CapsuleRadius);
		const float CapsuleSupport = CapsuleRadius + CapsuleCylinderHalfHeight * FMath::Abs(FVector::DotProduct(
			DisembarkedCharacter->GetActorUpVector(), DisembarkSurfaceFrame.Up));
		const float SurfaceClearance = FVector::DotProduct(
			DisembarkedCharacter->GetActorLocation() - DisembarkSurfaceFrame.Location,
			DisembarkSurfaceFrame.Up);
		TestTrue(TEXT("MarII disembark capsule clears the terrain instead of embedding to knee height"),
			SurfaceClearance >= CapsuleSupport + 1.0f);
	}
	return true;
}

#endif
