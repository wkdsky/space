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
		if (UpDot < 0.20f || UpDot > 0.68f) continue;
		SlopeHit = Candidate;
		SlopeUp = Up;
		bFoundSlope = true;
		break;
	}
	if (!TestTrue(TEXT("Real Moon mesh has a 40-95 degree climbable patch"), bFoundSlope)) return false;

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
