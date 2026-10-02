#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/UnrealType.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSRangedWeaponComponent.h"
#include "space/Components/JTSStaminaComponent.h"
#include "space/Components/JTSWallClimbComponent.h"
#include "space/Components/JTSWeaponVisualComponent.h"
#include "space/Animation/JTSAnimInstance.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Player/JTSCharacter.h"
#include "space/World/JTSPlanetAnchor.h"

namespace
{
	struct FClimbTestWorld
	{
		UWorld* World = nullptr;
		AJTSCharacter* Character = nullptr;
		AStaticMeshActor* Wall = nullptr;

		explicit FClimbTestWorld(UClass* CharacterClass = nullptr)
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->InitializeActorsForPlay(FURL());

			AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>();
			Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Floor->GetStaticMeshComponent()->SetStaticMesh(
				LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
			Floor->SetActorLocation(FVector(0.0f, 0.0f, -50.0f));
			Floor->SetActorScale3D(FVector(10.0f, 10.0f, 1.0f));

			Wall = World->SpawnActor<AStaticMeshActor>();
			Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Wall->GetStaticMeshComponent()->SetStaticMesh(
				LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
			Wall->SetActorLocation(FVector(120.0f, 0.0f, 140.0f));
			Wall->SetActorScale3D(FVector(0.5f, 4.0f, 4.0f));

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Character = World->SpawnActor<AJTSCharacter>(
				CharacterClass != nullptr ? CharacterClass : AJTSCharacter::StaticClass(),
				FVector(0.0f, 0.0f, 110.0f), FRotator::ZeroRotator, Params);
		}

		~FClimbTestWorld()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		void Advance(float Seconds)
		{
			for (float Elapsed = 0.0f; Elapsed < Seconds; Elapsed += 0.05f)
			{
				World->Tick(LEVELTICK_All, 0.05f);
				if (UJTSWallClimbComponent* Climb = Character->FindComponentByClass<UJTSWallClimbComponent>())
				{
					static_cast<UActorComponent*>(Climb)->TickComponent(0.05f, LEVELTICK_All, nullptr);
				}
				if (UJTSStaminaComponent* Stamina = Character->GetStaminaComponent())
				{
					Stamina->TickComponent(0.05f, LEVELTICK_All, nullptr);
				}
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSFreeHandClimbRegression, "JTS.Character.FreeHandClimbAndStamina",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSFreeHandClimbRegression::RunTest(const FString& Parameters)
{
	FClimbTestWorld Fixture;
	if (!TestNotNull(TEXT("Character spawns"), Fixture.Character)) return false;
	UJTSWallClimbComponent* Climb = Fixture.Character->FindComponentByClass<UJTSWallClimbComponent>();
	UJTSStaminaComponent* Stamina = Fixture.Character->GetStaminaComponent();
	if (!TestNotNull(TEXT("Wall climb component exists"), Climb)
		|| !TestNotNull(TEXT("Shared stamina component exists"), Stamina)) return false;
	FHitResult GripHit;
	FCollisionQueryParams GripParams(SCENE_QUERY_STAT(JTSClimbTestGrip), false, Fixture.Character);
	const FVector GripStart = Fixture.Character->GetActorLocation()
		+ FVector::UpVector * (Fixture.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.15f);
	if (!TestTrue(TEXT("Test wall has visibility collision in front of the character"),
		Fixture.World->LineTraceSingleByChannel(GripHit, GripStart, GripStart + FVector::ForwardVector * 105.0f,
			ECC_Visibility, GripParams))) return false;
	TestTrue(TEXT("Wall face has a climbable angle"),
		FMath::Abs(FVector::DotProduct(GripHit.ImpactNormal, FVector::UpVector)) < 0.01f);
	TestTrue(TEXT("Character is facing the wall"),
		FVector::DotProduct(Fixture.Character->GetActorForwardVector(), FVector::ForwardVector) > 0.99f);
	TestTrue(TEXT("Stamina permits climbing"), Stamina->CanClimb());

	Climb->ToggleAttach();
	if (!TestTrue(TEXT("A vertical wall accepts free-hand climbing"), Climb->IsClimbing())) return false;
	const float BeforeStaticCling = Stamina->GetCurrentStamina();
	static_cast<UActorComponent*>(Climb)->TickComponent(1.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("One second of static grip now costs four stamina"),
		FMath::IsNearlyEqual(BeforeStaticCling - Stamina->GetCurrentStamina(), 4.0f, 0.05f));
	const float InitialStamina = Stamina->GetCurrentStamina();
	const FVector StartLocation = Fixture.Character->GetActorLocation();
	Climb->SubmitClimbIntent(FVector::RightVector);
	TestTrue(TEXT("Lateral intent pays the step cost immediately"), Stamina->GetCurrentStamina() <= InitialStamina - 1.4f);
	TestTrue(TEXT("Lateral intent keeps the character attached"), Climb->IsClimbing());
	TestTrue(TEXT("Lateral intent is aligned with the wall"),
		FVector::DotProduct(Climb->GetStepDirection(), FVector::RightVector) > 0.9f);
	Fixture.Advance(0.05f);
	TestTrue(TEXT("First climb tick preserves the attachment"), Climb->IsClimbing());
	Fixture.Advance(0.45f);
	TestTrue(FString::Printf(TEXT("A wall step moves laterally from %.1f to %.1f"),
		StartLocation.Y, Fixture.Character->GetActorLocation().Y),
		Fixture.Character->GetActorLocation().Y > StartLocation.Y + 55.0f);
	TestTrue(TEXT("Clinging and stepping consume stamina"), Stamina->GetCurrentStamina() < InitialStamina - 3.0f);

	const float BeforeLeapZ = Fixture.Character->GetActorLocation().Z;
	const float BeforeLeapStamina = Stamina->GetCurrentStamina();
	Climb->SubmitClimbLeap(FVector::UpVector);
	Fixture.Advance(0.55f);
	TestTrue(TEXT("Leap reaches a substantially higher grip"),
		Fixture.Character->GetActorLocation().Z > BeforeLeapZ + 95.0f);
	TestTrue(TEXT("Wall leap charges the shared stamina pool"),
		Stamina->GetCurrentStamina() < BeforeLeapStamina - 9.0f);
	const float BeforeSideLeapY = Fixture.Character->GetActorLocation().Y;
	Climb->SubmitClimbLeap(FVector::RightVector);
	Fixture.Advance(0.55f);
	TestTrue(TEXT("Side leap reaches a substantially farther grip"),
		Fixture.Character->GetActorLocation().Y > BeforeSideLeapY + 95.0f);
	TestTrue(TEXT("Side leap keeps the character on the wall"), Climb->IsClimbing());
	const float BeforeBufferedLeapY = Fixture.Character->GetActorLocation().Y;
	Climb->SubmitClimbIntent(-FVector::RightVector);
	Fixture.Advance(0.1f);
	Climb->SubmitClimbLeap(-FVector::RightVector);
	Fixture.Advance(0.3f);
	TestTrue(TEXT("Space pressed during a regular step starts a wall leap after that step"), Climb->IsLeaping());
	Fixture.Advance(0.55f);
	TestTrue(TEXT("Buffered side leap crosses the wall instead of being discarded"),
		Fixture.Character->GetActorLocation().Y < BeforeBufferedLeapY - 120.0f);
	TestTrue(TEXT("Buffered leap remains attached"), Climb->IsClimbing());

	Climb->ToggleAttach();
	TestFalse(TEXT("Climb shortcut detaches"), Climb->IsClimbing());
	Fixture.Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Fixture.Character->GetCharacterMovement()->Velocity = FVector(300.0f, 0.0f, 0.0f);
	const float BeforeSprint = Stamina->GetCurrentStamina();
	Stamina->SetSprintRequested(true);
	Stamina->TickComponent(0.5f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Sprinting draws from the same pool"), Stamina->GetCurrentStamina() < BeforeSprint);
	Stamina->SetSprintRequested(false);
	Fixture.Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	const float BeforeRecovery = Stamina->GetCurrentStamina();
	Stamina->TickComponent(1.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Walking or standing restores stamina"), Stamina->GetCurrentStamina() > BeforeRecovery);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSClimbDiagonalLeapAndDropRegression,
	"JTS.Character.ClimbDiagonalLeapAndDrop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSClimbDiagonalLeapAndDropRegression::RunTest(const FString& Parameters)
{
	FClimbTestWorld Fixture;
	if (!TestNotNull(TEXT("Character spawns"), Fixture.Character)) return false;
	UJTSWallClimbComponent* Climb = Fixture.Character->FindComponentByClass<UJTSWallClimbComponent>();
	UJTSStaminaComponent* Stamina = Fixture.Character->GetStaminaComponent();
	if (!TestNotNull(TEXT("Climb exists"), Climb) || !TestNotNull(TEXT("Stamina exists"), Stamina)) return false;
	Climb->TryAutoAttach();
	if (!TestTrue(TEXT("Character grabs the wall"), Climb->IsClimbing())) return false;
	const FVector Start = Fixture.Character->GetActorLocation();
	const float BeforeLeapStamina = Stamina->GetCurrentStamina();
	Climb->SubmitClimbLeap((FVector::UpVector + FVector::RightVector).GetSafeNormal());
	TestTrue(TEXT("Diagonal leap begins while gripping"), Climb->IsLeaping());
	Fixture.Advance(0.55f);
	TestTrue(TEXT("Diagonal leap rises"), Fixture.Character->GetActorLocation().Z > Start.Z + 65.0f);
	TestTrue(TEXT("Diagonal leap traverses sideways"), Fixture.Character->GetActorLocation().Y > Start.Y + 65.0f);
	TestTrue(TEXT("Diagonal leap retains its new grip"), Climb->IsClimbing());
	TestTrue(TEXT("Diagonal leap spends stamina"), Stamina->GetCurrentStamina() < BeforeLeapStamina - 9.0f);
	Climb->SubmitClimbDrop();
	TestFalse(TEXT("Down plus Space actively releases the grip"), Climb->IsClimbing());
	TestEqual(TEXT("Drop resumes falling under planet gravity"),
		Fixture.Character->GetCharacterMovement()->MovementMode, MOVE_Falling);
	TestTrue(TEXT("Drop restores movement gravity"), Fixture.Character->GetCharacterMovement()->GravityScale > 0.0f);
	Climb->TryAutoAttach();
	TestFalse(TEXT("Drop cannot immediately auto-grab the same wall"), Climb->IsClimbing());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSFastClimbDescentRegression,
	"JTS.Character.ClimbFastDescentAndBottomOut", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSFastClimbDescentRegression::RunTest(const FString& Parameters)
{
	FClimbTestWorld Fixture;
	if (!TestNotNull(TEXT("Character spawns"), Fixture.Character)) return false;
	Fixture.Character->SetActorLocation(FVector(0.0f, 0.0f, 280.0f));
	UJTSWallClimbComponent* Climb = Fixture.Character->FindComponentByClass<UJTSWallClimbComponent>();
	if (!TestNotNull(TEXT("Climb component exists"), Climb)) return false;
	Climb->TryAutoAttach();
	if (!TestTrue(TEXT("High wall accepts a grip"), Climb->IsClimbing())) return false;
	const float StartZ = Fixture.Character->GetActorLocation().Z;
	Climb->SubmitClimbIntent(-FVector::UpVector);
	Fixture.Advance(0.05f);
	TestTrue(TEXT("Hands and feet prepare before the quick drop"),
		Fixture.Character->GetActorLocation().Z > StartZ - 15.0f);
	Fixture.Advance(0.35f);
	TestTrue(TEXT("S descends farther than the normal 70 cm step"),
		Fixture.Character->GetActorLocation().Z < StartZ - 75.0f);
	if (!TestTrue(TEXT("Quick descent catches a new grip"), Climb->IsClimbing())) return false;
	Climb->SubmitClimbIntent(-FVector::UpVector);
	Fixture.Advance(0.4f);
	TestFalse(TEXT("Reaching walkable ground leaves the wall"), Climb->IsClimbing());
	TestEqual(TEXT("Ground dismount resumes walking"),
		Fixture.Character->GetCharacterMovement()->MovementMode, MOVE_Walking);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSJumpLandingClimbRegression,
	"JTS.Character.JumpLandingClimb", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSJumpLandingClimbRegression::RunTest(const FString& Parameters)
{
	FClimbTestWorld Fixture;
	Fixture.Character->SetActorLocation(FVector(35.0f, 0.0f, 280.0f));
	UJTSWallClimbComponent* Climb = Fixture.Character->FindComponentByClass<UJTSWallClimbComponent>();
	UCharacterMovementComponent* Movement = Fixture.Character->GetCharacterMovement();
	if (!TestNotNull(TEXT("Climb component exists"), Climb)
		|| !TestNotNull(TEXT("Movement component exists"), Movement)) return false;
	Movement->SetMovementMode(MOVE_Falling);
	Movement->Velocity = FVector(0.0f, 0.0f, 300.0f);
	Climb->ArmJumpGrab();
	static_cast<UActorComponent*>(Climb)->TickComponent(0.05f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Jump ascent near a wall does not grab early"), Climb->IsClimbing());
	Movement->Velocity = FVector(-300.0f, 0.0f, -300.0f);
	static_cast<UActorComponent*>(Climb)->TickComponent(0.05f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Jumping away from a wall does not auto-grab"), Climb->IsClimbing());
	Movement->Velocity = FVector(0.0f, 0.0f, -300.0f);
	static_cast<UActorComponent*>(Climb)->TickComponent(0.05f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Jump descent close to a climbable slope grabs automatically"), Climb->IsClimbing());

	FClimbTestWorld SlopeFixture;
	SlopeFixture.Wall->SetActorRotation(FRotationMatrix::MakeFromX(
		FVector(0.8660254f, 0.0f, -0.5f)).Rotator());
	SlopeFixture.Character->SetActorLocation(FVector(-20.0f, 0.0f, 110.0f));
	FHitResult LandingHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JTSJumpLandingSlopeTest), false, SlopeFixture.Character);
	const FVector ProbeStart = SlopeFixture.Character->GetActorLocation() - FVector::UpVector * 35.0f;
	if (!TestTrue(TEXT("Sloped wall yields a real collision hit"),
		SlopeFixture.World->LineTraceSingleByChannel(LandingHit, ProbeStart,
			ProbeStart + FVector::ForwardVector * 140.0f, ECC_Visibility, Params))) return false;
	UJTSWallClimbComponent* SlopeClimb = SlopeFixture.Character->FindComponentByClass<UJTSWallClimbComponent>();
	if (!TestNotNull(TEXT("Slope climb component exists"), SlopeClimb)) return false;
	SlopeClimb->ArmJumpGrab();
	FHitResult WalkableLandingHit = LandingHit;
	WalkableLandingHit.ImpactNormal = FVector::UpVector;
	SlopeClimb->TryJumpLandingGrip(WalkableLandingHit);
	SlopeClimb->TryJumpLandingGrip(LandingHit);
	TestFalse(TEXT("A normal landing ends that jump's grab request"), SlopeClimb->IsClimbing());
	SlopeClimb->ArmJumpGrab();
	SlopeFixture.World->Tick(LEVELTICK_TimeOnly, 3.0f);
	SlopeClimb->TryJumpLandingGrip(LandingHit);
	TestTrue(TEXT("A long low-gravity jump still grips a climbable landing slope"), SlopeClimb->IsClimbing());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSFarJumpSlopeGrip,
	"JTS.Character.FarJumpSlopeGrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSFarJumpSlopeGrip::RunTest(const FString& Parameters)
{
	FClimbTestWorld Fixture;
	Fixture.Wall->SetActorRotation(FRotationMatrix::MakeFromX(
		FVector(0.8660254f, 0.0f, -0.5f)).Rotator());
	Fixture.Character->SetActorLocation(FVector(-120.0f, 0.0f, 110.0f));
	UJTSWallClimbComponent* Climb = Fixture.Character->FindComponentByClass<UJTSWallClimbComponent>();
	UCharacterMovementComponent* Movement = Fixture.Character->GetCharacterMovement();
	if (!TestNotNull(TEXT("Climb component exists"), Climb)
		|| !TestNotNull(TEXT("Movement component exists"), Movement)) return false;
	FHitResult DistantSlope;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JTSFarJumpSlope), false, Fixture.Character);
	const FVector SightStart = Fixture.Character->GetActorLocation();
	if (!TestTrue(TEXT("The approach has real slope collision beyond normal grip range"),
		Fixture.World->LineTraceSingleByChannel(DistantSlope, SightStart,
			SightStart + FVector::ForwardVector * 400.0f, ECC_Visibility, Params)
			&& DistantSlope.Distance > 150.0f)) return false;
	Movement->bRunPhysicsWithNoController = true;
	Movement->SetMovementMode(MOVE_Falling);
	Movement->Velocity = FVector(800.0f, 0.0f, 420.0f);
	Climb->ArmJumpGrab();
	bool bCaughtWhileRising = false;
	float LargestFrameTravel = 0.0f;
	for (int32 Frame = 0; Frame < 15 && !Climb->IsClimbing(); ++Frame)
	{
		const bool bRising = Movement->Velocity.Z > 0.0f;
		const FVector BeforeFrame = Fixture.Character->GetActorLocation();
		Movement->TickComponent(0.05f, LEVELTICK_All, nullptr);
		Fixture.Advance(0.05f);
		LargestFrameTravel = FMath::Max(LargestFrameTravel,
			FVector::Distance(BeforeFrame, Fixture.Character->GetActorLocation()));
		bCaughtWhileRising = Climb->IsClimbing() && bRising;
	}
	TestTrue(TEXT("A distant jump catches the steep slope on approach or first impact"), Climb->IsClimbing());
	TestTrue(TEXT("The slope can be gripped before the jump apex"), bCaughtWhileRising);
	TestTrue(FString::Printf(TEXT("The grip has no large one-frame position pop: %.1f cm"),
		LargestFrameTravel), LargestFrameTravel < 80.0f);
	if (Climb->IsClimbing())
	{
		Fixture.Advance(0.20f);
		TestTrue(TEXT("The new grip stays attached instead of sliding down"), Climb->IsClimbing());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSTopOutMantleRegression,
	"JTS.Character.TopOutMantle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSTopOutMantleRegression::RunTest(const FString& Parameters)
{
	FClimbTestWorld Fixture;
	AStaticMeshActor* Shelf = Fixture.World->SpawnActor<AStaticMeshActor>();
	Shelf->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Shelf->GetStaticMeshComponent()->SetStaticMesh(
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Shelf->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Shelf->SetActorLocation(FVector(250.0f, 0.0f, 290.0f));
	Shelf->SetActorScale3D(FVector(3.0f, 4.0f, 1.0f));
	Fixture.Character->SetActorLocation(FVector(0.0f, 0.0f, 250.0f));
	UJTSWallClimbComponent* Climb = Fixture.Character->FindComponentByClass<UJTSWallClimbComponent>();
	if (!TestNotNull(TEXT("Climb component exists"), Climb)) return false;
	Climb->TryAutoAttach();
	if (!TestTrue(TEXT("Ledge wall accepts the grip"), Climb->IsClimbing())) return false;
	const FVector Grip = Fixture.Character->GetActorLocation();
	Climb->SubmitClimbIntent(FVector::UpVector);
	if (!TestTrue(TEXT("Reachable walkable lip begins a mantle"), Climb->IsMantling())) return false;
	Fixture.Advance(0.10f);
	TestTrue(TEXT("Mantle lifts the capsule before crossing the lip"),
		Climb->IsMantling() && Fixture.Character->GetActorLocation().Z > Grip.Z + 15.0f);
	Fixture.Advance(0.65f);
	TestFalse(TEXT("Mantle ends the wall grip"), Climb->IsClimbing());
	TestEqual(TEXT("Mantle resumes walking"),
		Fixture.Character->GetCharacterMovement()->MovementMode, MOVE_Walking);
	TestTrue(TEXT("Mantle lands beyond the wall with floor clearance"),
		Fixture.Character->GetActorLocation().X > 150.0f
			&& Fixture.Character->GetActorLocation().Z > 425.0f);

	FClimbTestWorld SlopeFixture;
	AStaticMeshActor* SlopeShelf = SlopeFixture.World->SpawnActor<AStaticMeshActor>();
	SlopeShelf->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	SlopeShelf->GetStaticMeshComponent()->SetStaticMesh(
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	SlopeShelf->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	SlopeShelf->SetActorLocation(FVector(250.0f, 0.0f, 312.0f));
	SlopeShelf->SetActorRotation(FRotationMatrix::MakeFromXZ(
		FVector(0.9396926f, 0.0f, 0.3420201f), FVector(-0.3420201f, 0.0f, 0.9396926f)).Rotator());
	SlopeShelf->SetActorScale3D(FVector(3.0f, 4.0f, 1.0f));
	SlopeFixture.Character->SetActorLocation(FVector(0.0f, 0.0f, 250.0f));
	UJTSWallClimbComponent* SlopeClimb = SlopeFixture.Character->FindComponentByClass<UJTSWallClimbComponent>();
	if (!TestNotNull(TEXT("Slope climb component exists"), SlopeClimb)) return false;
	SlopeClimb->TryAutoAttach();
	if (!TestTrue(TEXT("Steep wall below a gentle ramp accepts a grip"), SlopeClimb->IsClimbing())) return false;
	SlopeClimb->SubmitClimbIntent(FVector::UpVector);
	if (!TestTrue(TEXT("Gentle ramp begins a mantle"), SlopeClimb->IsMantling())) return false;
	SlopeFixture.Advance(0.80f);
	TestFalse(TEXT("Gentle-ramp mantle releases the grip"), SlopeClimb->IsClimbing());
	TestEqual(TEXT("Gentle-ramp mantle resumes walking"),
		SlopeFixture.Character->GetCharacterMovement()->MovementMode, MOVE_Walking);

	FClimbTestWorld CurvedFixture;
	CurvedFixture.Wall->GetStaticMeshComponent()->SetStaticMesh(
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	CurvedFixture.Wall->SetActorLocation(FVector(350.0f, 0.0f, 0.0f));
	CurvedFixture.Wall->SetActorRotation(FRotator::ZeroRotator);
	CurvedFixture.Wall->SetActorScale3D(FVector(6.0f));
	AJTSPlanetAnchor* CurvedPlanet = CurvedFixture.World->SpawnActor<AJTSPlanetAnchor>(
		AJTSPlanetAnchor::StaticClass(), FVector(0.0f, 0.0f, -100000.0f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Curved transition planet anchor exists"), CurvedPlanet)) return false;
	FObjectPropertyBase* CurvedSurfaceProperty = FindFProperty<FObjectPropertyBase>(
		AJTSPlanetAnchor::StaticClass(), TEXT("GameplaySurfaceActor"));
	if (!TestNotNull(TEXT("Curved transition surface property exists"), CurvedSurfaceProperty)) return false;
	CurvedSurfaceProperty->SetObjectPropertyValue_InContainer(CurvedPlanet, CurvedFixture.Wall);
	CurvedFixture.Character->SetActorLocation(FVector(43.0f, 0.0f, 230.0f));
	CurvedFixture.Character->SetGameplayPlanet(CurvedPlanet);
	UJTSWallClimbComponent* CurvedClimb = CurvedFixture.Character->FindComponentByClass<UJTSWallClimbComponent>();
	if (!TestNotNull(TEXT("Curved transition climb component exists"), CurvedClimb)) return false;
	CurvedClimb->TryAutoAttach();
	if (!TestTrue(TEXT("Steep side of a continuous curved surface accepts a grip"),
		CurvedClimb->IsClimbing())) return false;
	CurvedClimb->SubmitClimbIntent(FVector::UpVector);
	if (!TestTrue(TEXT("Curved surface rolls into a walkable mantle"), CurvedClimb->IsMantling())) return false;
	CurvedFixture.Advance(0.80f);
	TestFalse(TEXT("Curved mantle releases the grip"), CurvedClimb->IsClimbing());
	TestEqual(TEXT("Curved mantle resumes walking"),
		CurvedFixture.Character->GetCharacterMovement()->MovementMode, MOVE_Walking);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSHeadLampFollowsClimbRegression,
	"JTS.Character.HeadLampFollowsClimb", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSHeadLampFollowsClimbRegression::RunTest(const FString& Parameters)
{
	UClass* PlayerBlueprint = LoadClass<AJTSCharacter>(nullptr,
		TEXT("/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2.BP_JTSPlayer_Casual_2_C"));
	if (!TestNotNull(TEXT("Playable character Blueprint loads"), PlayerBlueprint)) return false;
	FClimbTestWorld Fixture(PlayerBlueprint);
	UJTSInventoryComponent* Inventory = Fixture.Character->GetInventoryComponent();
	UJTSWeaponVisualComponent* Visual = Fixture.Character->FindComponentByClass<UJTSWeaponVisualComponent>();
	UJTSWallClimbComponent* Climb = Fixture.Character->FindComponentByClass<UJTSWallClimbComponent>();
	if (!TestNotNull(TEXT("Inventory exists"), Inventory)
		|| !TestNotNull(TEXT("Visual component exists"), Visual)
		|| !TestNotNull(TEXT("Climb component exists"), Climb)) return false;
	if (!TestTrue(TEXT("Head lamp can be added"), Inventory->TryAddItemById(EJTSItemId::WaistLamp))) return false;
	int32 LampSlot = INDEX_NONE;
	for (int32 Index = 0; Index < Inventory->GetItemSlots().Num(); ++Index)
	{
		if (Inventory->GetItemAtSlot(Index).ItemId == EJTSItemId::WaistLamp) LampSlot = Index;
	}
	if (!TestTrue(TEXT("Head lamp occupies a slot"), LampSlot != INDEX_NONE)) return false;
	Inventory->SelectQuickbarSlot(LampSlot);
	Inventory->RequestToggleWaistLamp();
	if (!TestTrue(TEXT("Head lamp is lit"), Inventory->IsWaistLampEquipped())) return false;
	Visual->RefreshWeaponVisual();
	UStaticMeshComponent* LampBody = nullptr;
	TArray<UStaticMeshComponent*> MeshPieces;
	Fixture.Character->GetComponents<UStaticMeshComponent>(MeshPieces);
	for (UStaticMeshComponent* Piece : MeshPieces)
	{
		if (Piece->GetFName() == TEXT("WaistLampBody")) LampBody = Piece;
	}
	if (!TestNotNull(TEXT("Head lamp housing exists"), LampBody)) return false;
	Climb->TryAutoAttach();
	if (!TestTrue(TEXT("Lit character attaches to the wall"), Climb->IsClimbing())) return false;
	TestTrue(TEXT("Lamp pose follow keeps ticking while hands are stowed"), Visual->IsComponentTickEnabled());
	Fixture.Advance(0.05f);
	const FVector BeforeLamp = LampBody->GetComponentLocation();
	Climb->SubmitClimbIntent(FVector::UpVector);
	Fixture.Advance(0.45f);
	TestTrue(TEXT("Head lamp pose follow stays scheduled after the step"), Visual->IsComponentTickEnabled());
	static_cast<UActorComponent*>(Visual)->TickComponent(0.05f, LEVELTICK_All, nullptr);
	const FVector AfterLamp = LampBody->GetComponentLocation();
	const FVector AfterHead = Fixture.Character->GetMesh()->GetBoneLocation(TEXT("Head"));
	TestTrue(FString::Printf(TEXT("Head lamp moves with the climbing character: %.1f to %.1f"),
		BeforeLamp.Z, AfterLamp.Z), AfterLamp.Z > BeforeLamp.Z + 35.0f);
	TestTrue(FString::Printf(TEXT("Lamp housing remains by the animated head: distance %.1f"),
		FVector::Distance(AfterLamp, AfterHead)), FVector::Distance(AfterLamp, AfterHead) < 35.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSClimbPoseKinematics,
	"JTS.Character.ClimbPoseKinematics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSClimbPoseKinematics::RunTest(const FString& Parameters)
{
	UClass* PlayerBlueprint = LoadClass<AJTSCharacter>(nullptr,
		TEXT("/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2.BP_JTSPlayer_Casual_2_C"));
	if (!TestNotNull(TEXT("Playable Casual_2 Blueprint loads"), PlayerBlueprint)) return false;
	FClimbTestWorld Fixture(PlayerBlueprint);
	if (!TestNotNull(TEXT("Playable character spawns"), Fixture.Character)) return false;
	Fixture.Wall->SetActorScale3D(FVector(0.5f, 4.0f, 8.0f));
	Fixture.Character->SetActorLocation(FVector(0.0f, 0.0f, 280.0f));
	USkeletalMeshComponent* Mesh = Fixture.Character->GetMesh();
	UJTSWallClimbComponent* Climb = Fixture.Character->FindComponentByClass<UJTSWallClimbComponent>();
	if (!TestNotNull(TEXT("Playable mesh exists"), Mesh)
		|| !TestNotNull(TEXT("Climb component exists"), Climb)) return false;
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh->TickAnimation(0.05f, false);
	Mesh->RefreshBoneTransforms();
	const float StandingHipToThigh = FVector::Distance(
		Mesh->GetBoneLocation(TEXT("Hips")), Mesh->GetBoneLocation(TEXT("UpperLeg_L")));
	auto AdvancePose = [&](float Seconds)
	{
		for (float Elapsed = 0.0f; Elapsed < Seconds; Elapsed += 0.05f)
		{
			Fixture.Advance(0.05f);
			// Offscreen automation worlds do not submit skeletal work to a viewport.
			Mesh->TickAnimation(0.05f, false);
			Mesh->RefreshBoneTransforms();
		}
	};
	Climb->TryAutoAttach();
	if (!TestTrue(TEXT("Playable mesh grabs the tall wall"), Climb->IsClimbing())) return false;
	AdvancePose(0.25f);
	UJTSAnimInstance* Pose = Cast<UJTSAnimInstance>(Mesh->GetAnimInstance());
	if (!TestNotNull(TEXT("Playable climb animation instance updates"), Pose)) return false;
	TestTrue(TEXT("Grip pose blends fully in"), Pose->IsClimbPoseActive());
	const float GrippingHipToThigh = FVector::Distance(
		Mesh->GetBoneLocation(TEXT("Hips")), Mesh->GetBoneLocation(TEXT("UpperLeg_L")));
	TestTrue(FString::Printf(TEXT("Pelvis and thigh roots stay together: %.1f to %.1f"),
		StandingHipToThigh, GrippingHipToThigh),
		FMath::Abs(GrippingHipToThigh - StandingHipToThigh) < 6.0f);
	const float HeldHandHeight = FVector::DotProduct(
		Mesh->GetBoneLocation(TEXT("Wrist_L")) - Mesh->GetBoneLocation(TEXT("Hips")), FVector::UpVector);
	Climb->SubmitClimbLeap(FVector::UpVector);
	if (!TestTrue(TEXT("Upward leap starts"), Climb->IsLeaping())) return false;
	const FName LeadWrist = Climb->IsLeadHandLeft() ? TEXT("Wrist_L") : TEXT("Wrist_R");
	const float HeldLeadHeight = FVector::DotProduct(
		Mesh->GetBoneLocation(LeadWrist) - Mesh->GetBoneLocation(TEXT("Hips")), FVector::UpVector);
	AdvancePose(0.10f);
	const float LoadedHandHeight = FVector::DotProduct(
		Mesh->GetBoneLocation(TEXT("Wrist_L")) - Mesh->GetBoneLocation(TEXT("Hips")), FVector::UpVector);
	TestTrue(FString::Printf(TEXT("Load hangs the pelvis below the planted hand: %.1f to %.1f"),
		HeldHandHeight, LoadedHandHeight), LoadedHandHeight > HeldHandHeight + 2.0f);
	AdvancePose(0.05f);
	TestTrue(TEXT("Lead hand is sampled before the grip settles"), Climb->IsLeaping());
	const float ReachingHandHeight = FVector::DotProduct(
		Mesh->GetBoneLocation(LeadWrist) - Mesh->GetBoneLocation(TEXT("Hips")), FVector::UpVector);
	TestTrue(FString::Printf(TEXT("Lead hand reaches above its idle grip: %.1f to %.1f"),
		HeldLeadHeight, ReachingHandHeight), ReachingHandHeight > HeldLeadHeight + 5.0f);
	AdvancePose(0.30f);
	TestTrue(TEXT("Catch remains attached"), Climb->IsClimbing());
	Climb->SubmitClimbDrop();
	AdvancePose(0.05f);
	TestTrue(TEXT("Release keeps a brief follow-through pose"), Pose->IsClimbPoseActive());
	AdvancePose(0.25f);
	TestFalse(TEXT("Release pose finishes before normal airborne animation"), Pose->IsClimbPoseActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSClimbContactWeightTransfer,
	"JTS.Character.ClimbContactWeightTransfer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSClimbContactWeightTransfer::RunTest(const FString& Parameters)
{
	UClass* PlayerBlueprint = LoadClass<AJTSCharacter>(nullptr,
		TEXT("/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2.BP_JTSPlayer_Casual_2_C"));
	if (!TestNotNull(TEXT("Playable Casual_2 Blueprint loads"), PlayerBlueprint)) return false;
	FClimbTestWorld Fixture(PlayerBlueprint);
	Fixture.Wall->SetActorScale3D(FVector(0.5f, 4.0f, 8.0f));
	Fixture.Character->SetActorLocation(FVector(0.0f, 0.0f, 260.0f));
	USkeletalMeshComponent* Mesh = Fixture.Character->GetMesh();
	UJTSWallClimbComponent* Climb = Fixture.Character->FindComponentByClass<UJTSWallClimbComponent>();
	if (!TestNotNull(TEXT("Playable mesh exists"), Mesh)
		|| !TestNotNull(TEXT("Climb component exists"), Climb)) return false;
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh->TickAnimation(0.016f, false);
	Mesh->RefreshBoneTransforms();
	const FQuat RestShoes[2] = {
		Mesh->GetBoneQuaternion(TEXT("Foot_L")), Mesh->GetBoneQuaternion(TEXT("Foot_R"))
	};
	auto DescribeLegs = [&](const TCHAR* Phase)
	{
		const FVector Normal = Climb->GetSurfaceNormal().GetSafeNormal();
		const FVector Up = Fixture.Character->GetActorUpVector();
		for (const TCHAR* Suffix : { TEXT("_L"), TEXT("_R") })
		{
			const FVector Hip = Mesh->GetBoneLocation(*FString::Printf(TEXT("UpperLeg%s"), Suffix));
			const FVector Knee = Mesh->GetBoneLocation(*FString::Printf(TEXT("LowerLeg%s"), Suffix));
			const FVector Ankle = Mesh->GetBoneLocation(*FString::Printf(TEXT("LowerLeg%s_end"), Suffix));
			AddInfo(FString::Printf(TEXT("%s%s hip %s knee %s ankle %s knee outward %.1f knee drop %.1f ankle drop %.1f"),
				Phase, Suffix, *Hip.ToCompactString(), *Knee.ToCompactString(), *Ankle.ToCompactString(),
				FVector::DotProduct(Knee - Hip, Normal), FVector::DotProduct(Hip - Knee, Up),
				FVector::DotProduct(Hip - Ankle, Up)));
			TestTrue(FString::Printf(TEXT("%s%s knee bends toward the wall"), Phase, Suffix),
				FVector::DotProduct(Knee - Hip, Normal) < -4.0f);
			TestTrue(FString::Printf(TEXT("%s%s knee does not pass through the foothold plane"), Phase, Suffix),
				FVector::DotProduct(Knee - Ankle, Normal) > -10.0f);
			TestTrue(FString::Printf(TEXT("%s%s thigh keeps descending from the pelvis"), Phase, Suffix),
				FVector::DotProduct(Hip - Knee, Up) > 15.0f);
			TestTrue(FString::Printf(TEXT("%s%s foot stays below the pelvis"), Phase, Suffix),
				FVector::DotProduct(Hip - Ankle, Up) > 63.0f);
		}
	};
	auto AdvancePose = [&](int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Fixture.Advance(0.05f);
			Mesh->TickAnimation(0.05f, false);
			Mesh->RefreshBoneTransforms();
		}
	};
	Climb->TryAutoAttach();
	if (!TestTrue(TEXT("Playable mesh attaches before a weight-transfer step"), Climb->IsClimbing())) return false;
	AdvancePose(5);
	DescribeLegs(TEXT("Hold"));
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FName Foot = Index == 0 ? TEXT("Foot_L") : TEXT("Foot_R");
		const FName Ankle = Index == 0 ? TEXT("LowerLeg_L_end") : TEXT("LowerLeg_R_end");
		const float ShoeRoll = FMath::RadiansToDegrees(RestShoes[Index].AngularDistance(Mesh->GetBoneQuaternion(Foot)));
		TestTrue(FString::Printf(TEXT("%s does not roll onto its edge on first grip: %.1f deg"),
			*Foot.ToString(), ShoeRoll), ShoeRoll < 50.0f);
		TestTrue(FString::Printf(TEXT("%s stays by its ankle on first grip"), *Foot.ToString()),
			FVector::Distance(Mesh->GetBoneLocation(Foot), Mesh->GetBoneLocation(Ankle)) < 27.0f);
	}
	Climb->SubmitClimbIntent(FVector::UpVector);
	const FName LeadWrist = Climb->IsLeadHandLeft() ? TEXT("Wrist_L") : TEXT("Wrist_R");
	const FName SupportWrist = Climb->IsLeadHandLeft() ? TEXT("Wrist_R") : TEXT("Wrist_L");
	const FName SupportAnkle = Climb->IsLeadHandLeft() ? TEXT("LowerLeg_L_end") : TEXT("LowerLeg_R_end");
	const FVector LeadStart = Mesh->GetBoneLocation(LeadWrist);
	const FVector SupportStart = Mesh->GetBoneLocation(SupportWrist);
	const FVector AnkleStart = Mesh->GetBoneLocation(SupportAnkle);
	const FVector LeftKneeStart = Mesh->GetBoneLocation(TEXT("LowerLeg_L"));
	const FVector LeftAnkleStart = Mesh->GetBoneLocation(TEXT("LowerLeg_L_end"));
	const FVector CapsuleStart = Fixture.Character->GetActorLocation();
	AddInfo(FString::Printf(TEXT("Rest contact: actor X %.1f, lead wrist X %.1f, support wrist X %.1f, left knee X %.1f, ankle X %.1f"),
		CapsuleStart.X, LeadStart.X, SupportStart.X, LeftKneeStart.X, LeftAnkleStart.X));
	AdvancePose(2);
	const float LeadTravel = FVector::Distance(LeadStart, Mesh->GetBoneLocation(LeadWrist));
	const float SupportSlip = FVector::Distance(SupportStart, Mesh->GetBoneLocation(SupportWrist));
	const float AnkleSlip = FVector::Distance(AnkleStart, Mesh->GetBoneLocation(SupportAnkle));
	const float CapsuleTravel = FVector::Distance(CapsuleStart, Fixture.Character->GetActorLocation());
	AddInfo(FString::Printf(TEXT("Early transfer: lead %.1f cm, support %.1f cm, ankle %.1f cm, capsule %.1f cm"),
		LeadTravel, SupportSlip, AnkleSlip, CapsuleTravel));
	TestTrue(FString::Printf(TEXT("Searching hand moves before the body pull: %.1f cm"), LeadTravel),
		LeadTravel > 5.0f);
	TestTrue(FString::Printf(TEXT("Supporting hand remains on its hold: %.1f cm slip"), SupportSlip),
		SupportSlip < 9.0f);
	TestTrue(FString::Printf(TEXT("Pressing foot remains on its hold: %.1f cm slip"), AnkleSlip),
		AnkleSlip < 9.0f);
	TestTrue(FString::Printf(TEXT("Capsule waits for a foothold before traveling: %.1f cm"), CapsuleTravel),
		CapsuleTravel < 15.0f);
	AdvancePose(3);
	DescribeLegs(TEXT("Pull"));
	const float WallFaceX = Fixture.Wall->GetActorLocation().X
		- Fixture.Wall->GetStaticMeshComponent()->Bounds.BoxExtent.X;
	const float LeadWallGap = WallFaceX - Mesh->GetBoneLocation(LeadWrist).X;
	const float SupportWallGap = WallFaceX - Mesh->GetBoneLocation(SupportWrist).X;
	const float AnkleWallGap = WallFaceX - Mesh->GetBoneLocation(SupportAnkle).X;
	const float AnkleRise = Mesh->GetBoneLocation(SupportAnkle).Z - AnkleStart.Z;
	AddInfo(FString::Printf(TEXT("Mid pull: capsule Z %.1f, lead wall gap %.1f, support wall gap %.1f, ankle wall gap %.1f, ankle rise %.1f"),
		Fixture.Character->GetActorLocation().Z, LeadWallGap, SupportWallGap, AnkleWallGap, AnkleRise));
	TestTrue(TEXT("Reaching hand stays close to the rock through the pull"), LeadWallGap < 25.0f);
	TestTrue(TEXT("Following hand stays close to the rock through the pull"), SupportWallGap < 25.0f);
	TestTrue(TEXT("Following ankle clears the wall without peeling far away"), AnkleWallGap < 18.0f);
	AdvancePose(3);
	TestTrue(TEXT("Body completes the pull after the contact sequence"),
		Fixture.Character->GetActorLocation().Z > CapsuleStart.Z + 55.0f);
	Climb->SubmitClimbLeap(FVector::UpVector);
	if (!TestTrue(TEXT("Upward leap starts after the planted step"), Climb->IsLeaping())) return false;
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		AdvancePose(1);
		if (Frame == 3 || Frame == 6)
		{
			DescribeLegs(Frame == 3 ? TEXT("LeapEarly") : TEXT("LeapLate"));
			const float LeftGap = WallFaceX - Mesh->GetBoneLocation(TEXT("Wrist_L")).X;
			const float RightGap = WallFaceX - Mesh->GetBoneLocation(TEXT("Wrist_R")).X;
			TestTrue(FString::Printf(TEXT("Leap wrists stay near the wall at frame %d: %.1f / %.1f cm"),
				Frame, LeftGap, RightGap), LeftGap < 30.0f && RightGap < 30.0f);
			for (int32 Index = 0; Index < 2; ++Index)
			{
				const FName Foot = Index == 0 ? TEXT("Foot_L") : TEXT("Foot_R");
				const float ShoeRoll = FMath::RadiansToDegrees(
					RestShoes[Index].AngularDistance(Mesh->GetBoneQuaternion(Foot)));
				TestTrue(FString::Printf(TEXT("%s stays level during the wall leap: %.1f deg"),
					*Foot.ToString(), ShoeRoll), ShoeRoll < 50.0f);
			}
		}
	}
	TestTrue(TEXT("Leap completes on a higher hold"),
		Fixture.Character->GetActorLocation().Z > CapsuleStart.Z + 170.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSJumpShoeClearance,
	"JTS.Character.JumpShoeClearance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSJumpShoeClearance::RunTest(const FString& Parameters)
{
	UClass* PlayerBlueprint = LoadClass<AJTSCharacter>(nullptr,
		TEXT("/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2.BP_JTSPlayer_Casual_2_C"));
	if (!TestNotNull(TEXT("Playable Casual_2 Blueprint loads"), PlayerBlueprint)) return false;
	FClimbTestWorld Fixture(PlayerBlueprint);
	USkeletalMeshComponent* Mesh = Fixture.Character->GetMesh();
	if (!TestNotNull(TEXT("Playable mesh exists"), Mesh)) return false;
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh->TickAnimation(0.016f, false);
	Mesh->RefreshBoneTransforms();
	const FQuat RestShoes[2] = {
		Mesh->GetBoneQuaternion(TEXT("Foot_L")), Mesh->GetBoneQuaternion(TEXT("Foot_R"))
	};
	Fixture.Character->bPressedJump = true;
	for (int32 Frame = 0; Frame < 5; ++Frame)
	{
		Fixture.Advance(0.05f);
		Mesh->TickAnimation(0.05f, false);
		Mesh->RefreshBoneTransforms();
	}
	const FVector Up = Fixture.Character->GetActorUpVector();
	const FVector Hips = Mesh->GetBoneLocation(TEXT("Hips"));
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FName FootName = Index == 0 ? TEXT("Foot_L") : TEXT("Foot_R");
		const FName KneeName = Index == 0 ? TEXT("LowerLeg_L") : TEXT("LowerLeg_R");
		const FName AnkleName = Index == 0 ? TEXT("LowerLeg_L_end") : TEXT("LowerLeg_R_end");
		const float Clearance = FVector::DotProduct(Hips - Mesh->GetBoneLocation(FootName), Up);
		const float KneeDrop = FVector::DotProduct(Hips - Mesh->GetBoneLocation(KneeName), Up);
		const float ShinDrop = FVector::DotProduct(
			Mesh->GetBoneLocation(KneeName) - Mesh->GetBoneLocation(AnkleName), Up);
		const float ShoeRoll = FMath::RadiansToDegrees(
			RestShoes[Index].AngularDistance(Mesh->GetBoneQuaternion(FootName)));
		TestTrue(FString::Printf(TEXT("%s hangs below the hips in the jump: %.1f cm"),
			*FootName.ToString(), Clearance), Clearance > 35.0f);
		TestTrue(FString::Printf(TEXT("%s knee stays below the pelvis: %.1f cm"),
			*FootName.ToString(), KneeDrop), KneeDrop > 18.0f);
		TestTrue(FString::Printf(TEXT("%s shin points down from the knee: %.1f cm"),
			*FootName.ToString(), ShinDrop), ShinDrop > 10.0f);
		TestTrue(FString::Printf(TEXT("%s shoe avoids the upright sole: %.1f deg"),
			*FootName.ToString(), ShoeRoll), ShoeRoll < 50.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSGroundTurnResponsiveness,
	"JTS.Character.GroundTurnResponsiveness", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSGroundTurnResponsiveness::RunTest(const FString& Parameters)
{
	FClimbTestWorld Fixture;
	UCharacterMovementComponent* Movement = Fixture.Character->GetCharacterMovement();
	if (!TestNotNull(TEXT("Character movement exists"), Movement)) return false;
	Movement->SetMovementMode(MOVE_Walking);
	Fixture.Character->SetActorRotation(FRotator::ZeroRotator);
	Movement->Velocity = FVector::RightVector * 500.0f;
	static_cast<AActor*>(Fixture.Character)->Tick(1.0f / 60.0f);
	const float FirstFrameRight = FVector::DotProduct(
		Fixture.Character->GetActorForwardVector(), FVector::RightVector);
	TestTrue(TEXT("90 degree turn begins without a one-frame body snap"),
		FirstFrameRight > 0.01f && FirstFrameRight < 0.25f);
	for (int32 Frame = 1; Frame < 6; ++Frame)
	{
		static_cast<AActor*>(Fixture.Character)->Tick(1.0f / 60.0f);
	}
	const float FacingRight = FVector::DotProduct(
		Fixture.Character->GetActorForwardVector(), FVector::RightVector);
	TestTrue(FString::Printf(TEXT("90 degree travel turn reacts within 0.1 seconds: dot %.2f"), FacingRight),
		FacingRight > 0.8f);
	TestTrue(TEXT("Ground turn still moves through intermediate orientations"), FacingRight < 0.999f);
	for (int32 Frame = 0; Frame < 8; ++Frame)
	{
		static_cast<AActor*>(Fixture.Character)->Tick(1.0f / 60.0f);
	}
	TestTrue(TEXT("Quick body turn settles within a quarter second"),
		FVector::DotProduct(Fixture.Character->GetActorForwardVector(), FVector::RightVector) > 0.995f);
	const FVector BeforeReverse = Fixture.Character->GetActorForwardVector();
	Movement->Velocity = -BeforeReverse * 500.0f;
	static_cast<AActor*>(Fixture.Character)->Tick(1.0f / 60.0f);
	TestTrue(TEXT("180 degree reversal keeps a visible first-frame turn"),
		FVector::DotProduct(Fixture.Character->GetActorForwardVector(), BeforeReverse) > 0.95f);
	for (int32 Frame = 0; Frame < 18; ++Frame)
	{
		static_cast<AActor*>(Fixture.Character)->Tick(1.0f / 60.0f);
	}
	TestTrue(TEXT("180 degree reversal reaches its heading promptly"),
		FVector::DotProduct(Fixture.Character->GetActorForwardVector(), -BeforeReverse) > 0.98f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSPlanetSlopeClimbRegression, "JTS.Character.PlanetSlopeClimbWithHeldItem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSPlanetSlopeClimbRegression::RunTest(const FString& Parameters)
{
	FClimbTestWorld Fixture;
	if (!TestNotNull(TEXT("Character spawns"), Fixture.Character)) return false;
	Fixture.Wall->SetActorRotation(FRotationMatrix::MakeFromX(FVector(0.8660254f, 0.0f, -0.5f)).Rotator());
	Fixture.Wall->GetStaticMeshComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Fixture.Character->SetActorLocation(FVector(-20.0f, 0.0f, 110.0f));
	AJTSPlanetAnchor* Planet = Fixture.World->SpawnActor<AJTSPlanetAnchor>(AJTSPlanetAnchor::StaticClass(),
		FVector(0.0f, 0.0f, -100000.0f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Planet anchor spawns"), Planet)) return false;
	FObjectPropertyBase* SurfaceActorProperty = FindFProperty<FObjectPropertyBase>(
		AJTSPlanetAnchor::StaticClass(), TEXT("GameplaySurfaceActor"));
	if (!TestNotNull(TEXT("Planet exposes its configured surface"), SurfaceActorProperty)) return false;
	SurfaceActorProperty->SetObjectPropertyValue_InContainer(Planet, Fixture.Wall);
	Fixture.Character->SetGameplayPlanet(Planet);

	const FVector Up = Planet->GetRadialUpVector(Fixture.Character->GetActorLocation());
	const FVector ProbeStart = Fixture.Character->GetActorLocation()
		- Up * Fixture.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.45f;
	FHitResult SurfaceHit;
	if (!TestTrue(TEXT("Configured planet mesh answers a direct slope trace"),
		Planet->TraceGameplaySurfaceSegment(ProbeStart, ProbeStart + FVector::ForwardVector * 140.0f, SurfaceHit))) return false;
	const float UpDot = FVector::DotProduct(SurfaceHit.ImpactNormal, Up);
	TestTrue(TEXT("The real mesh slope is between 40 and 95 degrees"), UpDot > 0.3f && UpDot < 0.7f);
	FHitResult VisibilityHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JTSPlanetSlopeVisibilityTest), false, Fixture.Character);
	TestFalse(TEXT("Planet mesh does not block the old Visibility probe"),
		Fixture.World->LineTraceSingleByChannel(VisibilityHit, ProbeStart,
			ProbeStart + FVector::ForwardVector * 140.0f, ECC_Visibility, Params));

	UJTSInventoryComponent* Inventory = Fixture.Character->FindComponentByClass<UJTSInventoryComponent>();
	if (!TestNotNull(TEXT("Inventory exists"), Inventory)) return false;
	if (!TestTrue(TEXT("Character can hold dual pistols"), Inventory->TryAddItemById(EJTSItemId::IceAxe))) return false;
	Inventory->SelectQuickbarSlot(0);
	TestEqual(TEXT("Dual pistols are held before climbing"), Inventory->GetActiveItemId(), EJTSItemId::IceAxe);
	UJTSWeaponVisualComponent* Visual = Fixture.Character->FindComponentByClass<UJTSWeaponVisualComponent>();
	if (!TestNotNull(TEXT("Held item visual exists"), Visual)) return false;
	Visual->RefreshWeaponVisual();
	UJTSRangedWeaponComponent* Ranged = Fixture.Character->FindComponentByClass<UJTSRangedWeaponComponent>();
	if (!TestNotNull(TEXT("Ranged weapon component exists"), Ranged)) return false;
	Ranged->StartAim();
	TestTrue(TEXT("Held pistols can aim before climbing"), Ranged->IsAiming());
	TArray<UStaticMeshComponent*> MeshComponents;
	Fixture.Character->GetComponents<UStaticMeshComponent>(MeshComponents);
	UStaticMeshComponent* Grip = nullptr;
	TArray<UStaticMeshComponent*> HeldPieces;
	for (UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		if (MeshComponent->GetFName() == TEXT("WeaponVisualGrip")) Grip = MeshComponent;
		if (MeshComponent->GetName().StartsWith(TEXT("WeaponVisual"))
			|| MeshComponent->GetName().StartsWith(TEXT("LeftPistol"))) HeldPieces.Add(MeshComponent);
	}
	if (!TestNotNull(TEXT("Held item grip exists"), Grip)) return false;
	TestTrue(TEXT("Held item is visible before climbing"), Grip->IsVisible());
	UJTSWallClimbComponent* Climb = Fixture.Character->FindComponentByClass<UJTSWallClimbComponent>();
	if (!TestNotNull(TEXT("Climb component exists"), Climb)) return false;
	Climb->TryAutoAttach();
	if (!TestTrue(TEXT("Forward auto-grab attaches to the configured planet slope while holding an item"), Climb->IsClimbing())) return false;
	TestFalse(TEXT("Wall grip cancels the old weapon ADS state"), Ranged->IsAiming());
	for (UStaticMeshComponent* Piece : HeldPieces)
	{
		TestFalse(FString::Printf(TEXT("%s is stowed immediately on climb"), *Piece->GetName()), Piece->IsVisible());
		TestTrue(FString::Printf(TEXT("%s is hidden in game on climb"), *Piece->GetName()), Piece->bHiddenInGame);
	}
	Visual->RefreshWeaponVisual();
	TestFalse(TEXT("Inventory visual refresh cannot redraw a pistol while climbing"), Grip->IsVisible());
	Visual->RestoreAfterCharacterMeshShown();
	TestFalse(TEXT("Mesh restoration cannot redraw a held item during climbing"), Grip->IsVisible());
	Fixture.Advance(0.1f);
	TestTrue(TEXT("Planet surface contact remains stable"), Climb->IsClimbing());
	const float BeforeStepY = Fixture.Character->GetActorLocation().Y;
	Climb->SubmitClimbIntent(FVector::RightVector);
	Fixture.Advance(0.45f);
	TestTrue(TEXT("Lateral movement follows the planet surface"),
		Fixture.Character->GetActorLocation().Y > BeforeStepY + 20.0f);
	TestTrue(TEXT("Slope climb remains attached after moving"), Climb->IsClimbing());
	Climb->ToggleAttach();
	TestEqual(TEXT("Held item is still selected after climbing"), Inventory->GetActiveItemId(), EJTSItemId::IceAxe);
	Visual->RefreshWeaponVisual();
	TestTrue(TEXT("Held item returns after climbing"), Grip->IsVisible());
	TestFalse(TEXT("Old ADS input does not reactivate on exit"), Ranged->IsAiming());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSClimbContactToleranceRegression,
	"JTS.Character.ClimbContactTolerance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSClimbContactToleranceRegression::RunTest(const FString& Parameters)
{
	FClimbTestWorld Fixture;
	UJTSWallClimbComponent* Climb = Fixture.Character->FindComponentByClass<UJTSWallClimbComponent>();
	if (!TestNotNull(TEXT("Climb component exists"), Climb)) return false;
	Fixture.Character->bUseControllerRotationYaw = true;
	Fixture.Character->GetCharacterMovement()->bUseControllerDesiredRotation = true;
	Climb->TryAutoAttach();
	if (!TestTrue(TEXT("Forward auto-grab attaches"), Climb->IsClimbing())) return false;
	TestFalse(TEXT("Grabbing locks controller-driven body yaw"), Fixture.Character->bUseControllerRotationYaw);
	TestFalse(TEXT("Grabbing locks movement controller rotation"),
		Fixture.Character->GetCharacterMovement()->bUseControllerDesiredRotation);
	Fixture.Character->bUseControllerRotationYaw = true;
	static_cast<APawn*>(Fixture.Character)->FaceRotation(FRotator(0.0f, 90.0f, 0.0f), 0.016f);
	TestTrue(TEXT("Camera yaw cannot rotate the gripping body away from the wall"),
		FVector::DotProduct(Fixture.Character->GetActorForwardVector(), FVector::ForwardVector) > 0.9f);
	Fixture.Advance(0.05f);
	TestFalse(TEXT("Climbing reapplies the facing lock after a Blueprint override"),
		Fixture.Character->bUseControllerRotationYaw);

	const FVector WallLocation = Fixture.Wall->GetActorLocation();
	Fixture.Wall->SetActorLocation(WallLocation + FVector(900.0f, 0.0f, 0.0f));
	Fixture.Advance(0.1f);
	TestTrue(TEXT("A brief missing surface does not release the grip"), Climb->IsClimbing());
	Fixture.Wall->SetActorLocation(WallLocation);
	Fixture.Advance(0.1f);
	if (!TestTrue(TEXT("Contact recovery keeps climbing active"), Climb->IsClimbing())) return false;

	AStaticMeshActor* Blocker = Fixture.World->SpawnActor<AStaticMeshActor>();
	Blocker->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Blocker->GetStaticMeshComponent()->SetStaticMesh(
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Blocker->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Blocker->GetStaticMeshComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Blocker->SetActorLocation(Fixture.Character->GetActorLocation() + FVector(0.0f, 72.0f, 0.0f));
	Blocker->SetActorScale3D(FVector(0.4f, 0.1f, 1.5f));
	Climb->SubmitClimbIntent(FVector::RightVector);
	Fixture.Advance(0.45f);
	TestTrue(TEXT("A blocked sideways step stays attached instead of jumping into the air"), Climb->IsClimbing());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSRealPlanetMeshViewOrbitRegression,
	"JTS.Character.RealPlanetMeshViewOrbit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSRealPlanetMeshViewOrbitRegression::RunTest(const FString& Parameters)
{
	UClass* PlayerBlueprint = LoadClass<AJTSCharacter>(nullptr,
		TEXT("/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2.BP_JTSPlayer_Casual_2_C"));
	if (!TestNotNull(TEXT("The playable Casual_2 Blueprint loads"), PlayerBlueprint)) return false;
	FClimbTestWorld Fixture(PlayerBlueprint);
	if (!TestNotNull(TEXT("The playable Blueprint spawns"), Fixture.Character)) return false;
	UStaticMesh* PlanetMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/ThirdParty/Environment/UltimateSpace/Meshes/Planet_3.Planet_3"));
	if (!TestNotNull(TEXT("SpaceWorld Moon's real mesh loads"), PlanetMesh)) return false;
	Fixture.Wall->GetStaticMeshComponent()->SetStaticMesh(PlanetMesh);
	Fixture.Wall->SetActorLocation(FVector::ZeroVector);
	Fixture.Wall->SetActorRotation(FRotator(-34.0f, 0.0f, 0.0f));
	Fixture.Wall->SetActorScale3D(FVector(82.0f));
	if (!TestTrue(TEXT("Moon asset provides collision triangles"),
		PlanetMesh->ContainsPhysicsTriMeshData(true))) return false;
	if (UBodySetup* Body = PlanetMesh->GetBodySetup()) Body->CreatePhysicsMeshes();
	Fixture.Wall->GetStaticMeshComponent()->RecreatePhysicsState();

	AJTSPlanetAnchor* Planet = Fixture.World->SpawnActor<AJTSPlanetAnchor>(AJTSPlanetAnchor::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Planet anchor exists"), Planet)) return false;
	FObjectPropertyBase* SurfaceActorProperty = FindFProperty<FObjectPropertyBase>(
		AJTSPlanetAnchor::StaticClass(), TEXT("GameplaySurfaceActor"));
	if (!TestNotNull(TEXT("Planet surface property exists"), SurfaceActorProperty)) return false;
	SurfaceActorProperty->SetObjectPropertyValue_InContainer(Planet, Fixture.Wall);
	UJTSWallClimbComponent* Climb = Fixture.Character->FindComponentByClass<UJTSWallClimbComponent>();
	if (!TestNotNull(TEXT("Wall climb component exists"), Climb)) return false;

	FHitResult SlopeHit;
	FVector SlopeUp = FVector::UpVector;
	bool bFoundSlope = false;
	float LowestUpDot = 1.0f;
	int32 SurfaceSamples = 0;
	constexpr int32 Samples = 1024;
	for (int32 Index = 0; Index < Samples; ++Index)
	{
		const float Z = 1.0f - 2.0f * (Index + 0.5f) / Samples;
		const float Angle = Index * 2.39996323f;
		const float Ring = FMath::Sqrt(FMath::Max(0.0f, 1.0f - Z * Z));
		const FVector Ray(Ring * FMath::Cos(Angle), Ring * FMath::Sin(Angle), Z);
		FHitResult Candidate;
		if (!Planet->TraceGameplaySurfaceSegment(Ray * 25000.0f, FVector::ZeroVector, Candidate)) continue;
		const FVector Up = Candidate.ImpactPoint.GetSafeNormal();
		const float UpDot = FVector::DotProduct(Candidate.ImpactNormal.GetSafeNormal(), Up);
		LowestUpDot = FMath::Min(LowestUpDot, UpDot);
		++SurfaceSamples;
		if (UpDot < 0.20f || UpDot > 0.68f) continue;
		SlopeHit = Candidate;
		SlopeUp = Up;
		bFoundSlope = true;
		break;
	}
	if (!TestTrue(FString::Printf(TEXT("Real Moon mesh has a climbable patch: %d hits, lowest up dot %.3f"),
		SurfaceSamples, LowestUpDot), bFoundSlope)) return false;

	const FVector Normal = SlopeHit.ImpactNormal.GetSafeNormal();
	const UCapsuleComponent* Capsule = Fixture.Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float Height = Capsule->GetScaledCapsuleHalfHeight();
	const float Reach = Radius + FMath::Max(0.0f, Height - Radius)
		* FMath::Abs(FVector::DotProduct(Normal, SlopeUp));
	const FVector IntoWall = FVector::VectorPlaneProject(-Normal, SlopeUp).GetSafeNormal();
	Fixture.Character->SetActorLocation(SlopeHit.ImpactPoint + Normal * (Reach + 14.0f), false);
	Fixture.Character->SetGameplayPlanet(Planet);
	Fixture.Character->SetActorRotation(FRotationMatrix::MakeFromXZ(IntoWall, SlopeUp).Rotator());
	UJTSInventoryComponent* Inventory = Fixture.Character->GetInventoryComponent();
	UJTSWeaponVisualComponent* Visual = Fixture.Character->FindComponentByClass<UJTSWeaponVisualComponent>();
	UJTSRangedWeaponComponent* Ranged = Fixture.Character->FindComponentByClass<UJTSRangedWeaponComponent>();
	if (!TestNotNull(TEXT("Playable inventory exists"), Inventory)
		|| !TestNotNull(TEXT("Playable held-item visual exists"), Visual)
		|| !TestNotNull(TEXT("Playable ranged weapon exists"), Ranged)) return false;
	if (!TestTrue(TEXT("Playable character equips the dual pistols"),
		Inventory->TryAddItemById(EJTSItemId::IceAxe))) return false;
	int32 PistolSlot = INDEX_NONE;
	for (int32 Index = 0; Index < Inventory->GetItemSlots().Num(); ++Index)
	{
		if (Inventory->GetItemAtSlot(Index).ItemId == EJTSItemId::IceAxe)
		{
			PistolSlot = Index;
			break;
		}
	}
	if (!TestTrue(TEXT("Dual pistols occupy a selectable quickbar slot"), PistolSlot != INDEX_NONE)) return false;
	Inventory->SelectQuickbarSlot(PistolSlot);
	if (!TestEqual(TEXT("Playable character is actually holding dual pistols"),
		Inventory->GetActiveItemId(), EJTSItemId::IceAxe)) return false;
	Visual->RefreshWeaponVisual();
	Ranged->StartAim();
	TArray<UStaticMeshComponent*> HeldPieces;
	TArray<UStaticMeshComponent*> AllMeshes;
	Fixture.Character->GetComponents<UStaticMeshComponent>(AllMeshes);
	for (UStaticMeshComponent* Piece : AllMeshes)
	{
		if (Piece->GetName().StartsWith(TEXT("WeaponVisual"))
			|| Piece->GetName().StartsWith(TEXT("LeftPistol"))) HeldPieces.Add(Piece);
	}
	if (!TestTrue(TEXT("Playable Blueprint has visible held meshes"),
		HeldPieces.ContainsByPredicate([](const UStaticMeshComponent* Piece) { return Piece->IsVisible(); }))) return false;
	Climb->TryAutoAttach();
	if (!TestTrue(TEXT("Real spherical terrain accepts the grip"), Climb->IsClimbing())) return false;
	USkeletalMeshComponent* PoseMesh = Fixture.Character->GetMesh();
	if (!TestNotNull(TEXT("Real-sphere playable pose mesh exists"), PoseMesh)) return false;
	PoseMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	for (int32 Frame = 0; Frame < 5; ++Frame)
	{
		PoseMesh->TickAnimation(0.05f, false);
		PoseMesh->RefreshBoneTransforms();
	}
	for (const TCHAR* Suffix : { TEXT("_L"), TEXT("_R") })
	{
		const FVector Hip = PoseMesh->GetBoneLocation(*FString::Printf(TEXT("UpperLeg%s"), Suffix));
		const FVector Knee = PoseMesh->GetBoneLocation(*FString::Printf(TEXT("LowerLeg%s"), Suffix));
		const FVector Ankle = PoseMesh->GetBoneLocation(*FString::Printf(TEXT("LowerLeg%s_end"), Suffix));
		FVector FootSurface;
		FVector FootNormal;
		const bool bFoundFoothold = Climb->FindPoseContact(Ankle, FootSurface, FootNormal);
		AddInfo(FString::Printf(TEXT("RealSlope%s knee outward %.1f beyond ankle %.1f knee drop %.1f ankle drop %.1f contact %d gap %.1f"), Suffix,
			FVector::DotProduct(Knee - Hip, Climb->GetSurfaceNormal()),
			FVector::DotProduct(Knee - Ankle, Climb->GetSurfaceNormal()),
			FVector::DotProduct(Hip - Knee, SlopeUp), FVector::DotProduct(Hip - Ankle, SlopeUp),
			bFoundFoothold, bFoundFoothold ? FVector::DotProduct(Ankle - FootSurface, FootNormal) : -999.0f));
		TestTrue(FString::Printf(TEXT("RealSlope%s knee bends toward the slope"), Suffix),
			FVector::DotProduct(Knee - Hip, Climb->GetSurfaceNormal()) < -5.0f);
		TestTrue(FString::Printf(TEXT("RealSlope%s foot stays below the pelvis"), Suffix),
			FVector::DotProduct(Hip - Ankle, SlopeUp) > 60.0f);
		TestTrue(FString::Printf(TEXT("RealSlope%s shoe finds the actual mesh"), Suffix), bFoundFoothold);
		if (bFoundFoothold)
		{
			const float ContactGap = FVector::DotProduct(Ankle - FootSurface, FootNormal);
			TestTrue(FString::Printf(TEXT("RealSlope%s shoe stays at its foothold"), Suffix),
				ContactGap > 4.0f && ContactGap < 17.0f);
		}
	}
	TestFalse(TEXT("Real sphere grip cancels weapon aiming"), Ranged->IsAiming());
	for (const UStaticMeshComponent* Piece : HeldPieces)
	{
		if (!TestFalse(FString::Printf(TEXT("Real-sphere grip stows %s immediately"), *Piece->GetName()),
			Piece->IsVisible())) return false;
	}
	const FVector GripLocation = Fixture.Character->GetActorLocation();
	const FVector GripForward = Fixture.Character->GetActorForwardVector();
	USpringArmComponent* const CameraBoom = Fixture.Character->FindComponentByClass<USpringArmComponent>();
	if (!TestNotNull(TEXT("Playable camera spring arm exists"), CameraBoom)) return false;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->bDoCollisionTest = true;
	const float StaminaBeforeOrbit = Fixture.Character->GetStaminaComponent()->GetCurrentStamina();
	for (int32 Index = 0; Index < 24; ++Index)
	{
		const FVector ViewForward = FQuat(SlopeUp, FMath::DegreesToRadians(Index * 15.0f)).RotateVector(GripForward);
		CameraBoom->SetWorldRotation(FRotationMatrix::MakeFromXZ(ViewForward, SlopeUp).Rotator());
		CameraBoom->TargetArmLength = Index % 2 == 0 ? 35.0f : 400.0f;
		static_cast<APawn*>(Fixture.Character)->FaceRotation(FRotator(0.0f, Index * 15.0f, 0.0f), 0.05f);
		Fixture.Advance(0.05f);
		if (!TestTrue(TEXT("Camera orbit keeps real-sphere grip active"), Climb->IsClimbing())) return false;
		for (const UStaticMeshComponent* Piece : HeldPieces)
		{
			if (!TestFalse(FString::Printf(TEXT("Orbit keeps %s hidden"), *Piece->GetName()),
				Piece->IsVisible())) return false;
		}
	}
	TestTrue(TEXT("Camera orbit cannot move the held capsule"),
		FVector::DistSquared(Fixture.Character->GetActorLocation(), GripLocation) < FMath::Square(12.0f));
	TestTrue(TEXT("Camera orbit cannot turn body away from the real surface"),
		FVector::DotProduct(Fixture.Character->GetActorForwardVector(), GripForward) > 0.95f);
	TestTrue(TEXT("Static climbing drains stamina on the actual player Blueprint"),
		Fixture.Character->GetStaminaComponent()->GetCurrentStamina() < StaminaBeforeOrbit - 4.0f);
	const FVector WallRight = FVector::CrossProduct(Climb->GetSurfaceNormal(), SlopeUp).GetSafeNormal();
	const FVector BeforeSphereStep = Fixture.Character->GetActorLocation();
	Climb->SubmitClimbIntent(WallRight);
	Fixture.Advance(0.45f);
	float StepTravel = FVector::DotProduct(Fixture.Character->GetActorLocation() - BeforeSphereStep, WallRight);
	float StepSign = 1.0f;
	if (StepTravel < 35.0f)
	{
		Climb->SubmitClimbIntent(-WallRight);
		Fixture.Advance(0.45f);
		StepTravel = FVector::DotProduct(Fixture.Character->GetActorLocation() - BeforeSphereStep, -WallRight);
		StepSign = -1.0f;
	}
	TestTrue(FString::Printf(TEXT("A 70 cm side step traverses the real planet mesh (%.1f cm)"), StepTravel),
		StepTravel > 45.0f);
	TestTrue(TEXT("Real planet mesh keeps the grip after the longer stride"), Climb->IsClimbing());
	const FVector BeforeSphereLeap = Fixture.Character->GetActorLocation();
	Climb->SubmitClimbLeap(WallRight * -StepSign);
	Fixture.Advance(0.55f);
	const float LeapTravel = FVector::DotProduct(Fixture.Character->GetActorLocation() - BeforeSphereLeap,
		WallRight * -StepSign);
	TestTrue(FString::Printf(TEXT("A sideways leap reaches another real planet grip (%.1f cm)"), LeapTravel),
		LeapTravel > 75.0f);
	TestTrue(TEXT("Real planet mesh keeps the grip after the sideways leap"), Climb->IsClimbing());
	Climb->ToggleAttach();
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		PoseMesh->TickAnimation(0.05f, false);
		PoseMesh->RefreshBoneTransforms();
	}
	// This isolated world advances components manually; refresh the actor's presentation after its pose blend.
	Fixture.Character->RefreshClimbEquipmentPresentation();
	TestTrue(TEXT("Pistols return after the real-sphere climb ends"),
		HeldPieces.ContainsByPredicate([](const UStaticMeshComponent* Piece) { return Piece->IsVisible(); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSStaminaCriticalClimbGateRegression,
	"JTS.Character.StaminaCriticalClimbGate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSStaminaCriticalClimbGateRegression::RunTest(const FString& Parameters)
{
	FClimbTestWorld Fixture;
	if (!TestNotNull(TEXT("Character spawns"), Fixture.Character)) return false;
	UJTSStaminaComponent* Stamina = Fixture.Character->GetStaminaComponent();
	UJTSWallClimbComponent* Climb = Fixture.Character->FindComponentByClass<UJTSWallClimbComponent>();
	if (!TestNotNull(TEXT("Stamina exists"), Stamina) || !TestNotNull(TEXT("Climb exists"), Climb)) return false;
	if (!TestTrue(TEXT("Reduce stamina to yellow"), Stamina->Spend(74.0f))) return false;
	TestTrue(TEXT("Above the red threshold permits starting a climb"), Stamina->CanClimb());
	if (!TestTrue(TEXT("Reduce stamina to the exact red threshold"), Stamina->Spend(1.0f))) return false;
	TestFalse(TEXT("Red stamina prevents a new climb"), Stamina->CanClimb());
	Climb->TryAutoAttach();
	TestFalse(TEXT("Forward auto-grab refuses a red stamina bar"), Climb->IsClimbing());
	Climb->ToggleAttach();
	TestFalse(TEXT("Climb shortcut refuses a red stamina bar"), Climb->IsClimbing());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSDualPistolsNoClimbCapabilityRegression,
	"JTS.Items.DualPistolsRangedOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSDualPistolsNoClimbCapabilityRegression::RunTest(const FString& Parameters)
{
	const UJTSItemDefinition* Definition = UJTSItemDefinitionLibrary::GetItemDefinition(nullptr, EJTSItemId::IceAxe);
	if (!TestNotNull(TEXT("Dual pistols data asset loads"), Definition)) return false;
	TestTrue(TEXT("Dual pistols fire as a ranged weapon"), Definition->IsRangedWeapon());
	TestFalse(TEXT("Dual pistols cannot mine"), Definition->HasCapability(EJTSItemCapability::Mining));
	TestFalse(TEXT("Dual pistols have no melee override"), Definition->HasCapability(EJTSItemCapability::MeleeOverride));
	TestTrue(TEXT("Dual pistols deal ranged damage"), Definition->RangedDamage > 0.0f);
	return true;
}

#endif
