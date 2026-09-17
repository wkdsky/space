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
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "UObject/UnrealType.h"
#include "space/Components/JTSSpacecraftFlightMovementComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSPlanetAnchor.h"
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
	AJTSCharacter* CameraCharacter = nullptr;
	APlayerController* CameraController = Fixture.AddPlayer(CameraCharacter);
	TestTrue(TEXT("Camera test driver boards"), Fixture.Ship->TryBoardPlayer(CameraCharacter));
	Fixture.Ship->ActivateFlightCameraThirdPerson();
	USpringArmComponent* Boom = Fixture.Ship->GetFlightCameraBoom();
	TestTrue(TEXT("Flight camera uses player control rotation"), Boom->bUsePawnControlRotation);
	TestFalse(TEXT("Flight camera rotation is not welded to the ship"), Boom->IsUsingAbsoluteRotation());
	const FRotator FirstView(-15.0f, 25.0f, 0.0f);
	CameraController->SetControlRotation(FirstView);
	Boom->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	const FVector FirstForward = Fixture.Ship->GetFlightCamera()->GetForwardVector();
	const FRotator SecondView(20.0f, 100.0f, 0.0f);
	CameraController->SetControlRotation(SecondView);
	Boom->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	const FVector SecondForward = Fixture.Ship->GetFlightCamera()->GetForwardVector();
	TestTrue(TEXT("Mouse-driven control rotation changes the flight view"), !FirstForward.Equals(SecondForward, 0.001));
	TestTrue(TEXT("Flight camera follows the requested control rotation"), SecondForward.Equals(SecondView.Vector(), 0.001));
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
