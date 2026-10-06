#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "space/Components/JTSCriticalDamageType.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSMeleeComponent.h"
#include "space/Components/JTSRangedWeaponComponent.h"
#include "space/Components/JTSWeaponVisualComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Systems/JTSPlanetEnemySubsystem.h"
#include "space/World/JTSMoonCubeEnemy.h"
#include "space/World/JTSPlanetAnchor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSMoonCubeCombatRegression, "JTS.Moon.CubeCombatAndCriticalAbility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSMoonCubeCombatRegression::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->InitializeActorsForPlay(FURL());

	AJTSPlayerState* PlayerState = World->SpawnActor<AJTSPlayerState>();
	TestTrue(TEXT("Debug levels grant ability points on the server"), PlayerState->GrantDebugLevels());
	FJTSAbilityAllocation Allocation;
	Allocation.CriticalChanceRanks = 3;
	TestTrue(TEXT("Server accepts a valid critical chance allocation"), PlayerState->CommitAbilityAllocation(Allocation));
	TestEqual(TEXT("Three ranks give a 15 percent critical chance"), PlayerState->GetCriticalChancePercent(), 15);
	Allocation.CriticalChanceRanks = 3;
	TestFalse(TEXT("Server rejects ranks above the cap"), PlayerState->CommitAbilityAllocation(Allocation));

	AJTSMoonCubeEnemy* Cube = World->SpawnActor<AJTSMoonCubeEnemy>();
	TestNotNull(TEXT("Cube enemy spawns"), Cube);
	if (Cube != nullptr)
	{
		Cube->DispatchBeginPlay();
		UJTSHealthComponent* Health = Cube->GetHealthComponent();
		TestNotNull(TEXT("Cube uses the shared authoritative health component"), Health);
		if (Health != nullptr)
		{
			TestEqual(TEXT("Cube starts with 1000 health"), Health->GetMaxHealth(), 1000.0f);
		}
		UBoxComponent* WeakPoint = nullptr;
		TArray<UBoxComponent*> Colliders;
		Cube->GetComponents<UBoxComponent>(Colliders);
		for (UBoxComponent* Collider : Colliders)
		{
			if (Collider->GetFName() == TEXT("WeakPointCollider")) WeakPoint = Collider;
		}
		TestNotNull(TEXT("Cube has an aimable weak point"), WeakPoint);
		if (WeakPoint != nullptr && Health != nullptr)
		{
			FHitResult Hit;
			Hit.Component = WeakPoint;
			TestEqual(TEXT("Only the crown grants the authored critical multiplier"),
				Cube->GetCriticalHitMultiplier(Hit), 2.5f);
			const float Applied = UGameplayStatics::ApplyPointDamage(Cube, 25.0f, FVector::ForwardVector,
				Hit, nullptr, nullptr, UJTSCriticalDamageType::StaticClass());
			TestTrue(TEXT("Server accepts point damage"), Applied > 0.0f);
			TestEqual(TEXT("Health is reduced by the server hit"), Health->GetHealth(), 975.0f);
			FHitResult BodyHit;
			TestEqual(TEXT("Body hits have no weak-point bonus"), Cube->GetCriticalHitMultiplier(BodyHit), 1.0f);
		}
	}
	AJTSPlanetAnchor* Planet = World->SpawnActor<AJTSPlanetAnchor>();
	AJTSMoonCubeEnemy* SecondCube = World->SpawnActor<AJTSMoonCubeEnemy>();
	UJTSPlanetEnemySubsystem* Enemies = World->GetSubsystem<UJTSPlanetEnemySubsystem>();
	UMassEntitySubsystem* Mass = World->GetSubsystem<UMassEntitySubsystem>();
	TestNotNull(TEXT("World owns the shared enemy system"), Enemies);
	TestNotNull(TEXT("World owns Mass entity storage"), Mass);
	if (Planet != nullptr && Cube != nullptr && SecondCube != nullptr && Enemies != nullptr && Mass != nullptr)
	{
		const FVector Home = Planet->GetPlanetCenter() + FVector(0.0f, 0.0f, Planet->GetApproximateRadius());
		const FJTSPlanetEnemyBehavior Behavior;
		const FMassEntityHandle First = Enemies->RegisterEnemy(Cube, Planet, Home, Home, Behavior);
		const FMassEntityHandle Second = Enemies->RegisterEnemy(SecondCube, Planet, Home, Home, Behavior);
		TestTrue(TEXT("Multiple enemies share a Mass archetype"), First.IsValid() && Second.IsValid() && First != Second);
		TestTrue(TEXT("Both entity handles resolve"), Mass->GetEntityManager().IsEntityActive(First)
			&& Mass->GetEntityManager().IsEntityActive(Second));
		AJTSCharacter* Attacker = World->SpawnActor<AJTSCharacter>();
		TestNotNull(TEXT("A player character can be spawned for retaliation"), Attacker);
		if (Attacker != nullptr)
		{
			Attacker->SetGameplayPlanet(Planet);
			Attacker->SetActorLocation(Home);
			Enemies->NotifyDamaged(Second, Attacker);
			const FJTSPlanetEnemyPerceptionFragment& Perception =
				Mass->GetEntityManager().GetFragmentDataChecked<FJTSPlanetEnemyPerceptionFragment>(Second);
			TestTrue(TEXT("A valid attacker becomes the enemy's pursuit target"), Perception.Target.Get() == Attacker);
			TestTrue(TEXT("The hit grants a short pursuit memory"),
				Perception.RetaliationUntilTime > World->GetTimeSeconds());
		}
		Enemies->UnregisterEnemy(First);
		TestFalse(TEXT("Removal invalidates only the removed entity"), Mass->GetEntityManager().IsEntityActive(First));
		TestTrue(TEXT("The other enemy remains active"), Mass->GetEntityManager().IsEntityActive(Second));
		Enemies->UnregisterEnemy(Second);
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSWeaponTrajectoryRegression, "JTS.Combat.WeaponTrajectory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSWeaponTrajectoryRegression::RunTest(const FString& Parameters)
{
	struct FTestWorld
	{
		UWorld* World = nullptr;
		FTestWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}
		~FTestWorld()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
	} Fixture;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AJTSCharacter* Character = Fixture.World->SpawnActor<AJTSCharacter>(
		AJTSCharacter::StaticClass(), FVector(0.0f, 0.0f, 110.0f), FRotator::ZeroRotator, SpawnParams);
	AJTSMoonCubeEnemy* Cube = Fixture.World->SpawnActor<AJTSMoonCubeEnemy>(
		AJTSMoonCubeEnemy::StaticClass(), FVector(400.0f, 0.0f, 174.0f), FRotator::ZeroRotator, SpawnParams);
	if (!TestNotNull(TEXT("Trajectory test character spawns"), Character)
		|| !TestNotNull(TEXT("Trajectory test cube spawns"), Cube)) return false;
	Character->DispatchBeginPlay();
	Cube->DispatchBeginPlay();
	Character->GetCharacterMovement()->DisableMovement();
	UJTSInventoryComponent* Inventory = Character->FindComponentByClass<UJTSInventoryComponent>();
	UJTSWeaponVisualComponent* Visual = Character->FindComponentByClass<UJTSWeaponVisualComponent>();
	UJTSRangedWeaponComponent* Ranged = Character->FindComponentByClass<UJTSRangedWeaponComponent>();
	UJTSMeleeComponent* Melee = Character->FindComponentByClass<UJTSMeleeComponent>();
	UJTSHealthComponent* Health = Cube->GetHealthComponent();
	if (!TestNotNull(TEXT("Inventory exists"), Inventory)
		|| !TestNotNull(TEXT("Held-item presentation exists"), Visual)
		|| !TestNotNull(TEXT("Ranged rules exist"), Ranged)
		|| !TestNotNull(TEXT("Melee rules exist"), Melee)
		|| !TestNotNull(TEXT("Target health exists"), Health)) return false;
	if (!TestTrue(TEXT("Pistol can be equipped"), Inventory->TryAddItemById(EJTSItemId::RailPistol))) return false;
	Inventory->SelectQuickbarSlot(0);
	TestEqual(TEXT("Pistol is selected"), Inventory->GetActiveItemId(), EJTSItemId::RailPistol);
	Visual->RefreshWeaponVisual();
	USceneComponent* Muzzle = nullptr;
	TArray<USceneComponent*> Scenes;
	Character->GetComponents<USceneComponent>(Scenes);
	for (USceneComponent* Scene : Scenes)
	{
		if (Scene->GetFName() == TEXT("WeaponMuzzle")) Muzzle = Scene;
	}
	if (!TestNotNull(TEXT("Authored weapon muzzle exists"), Muzzle)) return false;
	Muzzle->SetWorldLocationAndRotation(FVector(100.0f, 0.0f, 174.0f), FRotator(0.0f, 90.0f, 0.0f));
	FTransform MuzzleTransform;
	TestTrue(TEXT("Visible gun exposes a firing transform"), Visual->GetMuzzleWorldTransform(MuzzleTransform));
	TestTrue(TEXT("Gun barrel points away from the camera target"),
		FVector::DotProduct(MuzzleTransform.GetUnitAxis(EAxis::X), FVector::RightVector) > 0.99f);
	const float InitialHealth = Health->GetHealth();
	TestTrue(TEXT("Cube starts alive for trajectory checks"), InitialHealth > 0.0f);
	Ranged->StartFire();
	Ranged->StopFire();
	TestEqual(TEXT("Camera target takes no damage when the muzzle points away"), Health->GetHealth(), InitialHealth);
	for (int32 Step = 0; Step < 10; ++Step) Fixture.World->Tick(LEVELTICK_All, 0.05f);
	Muzzle->SetWorldLocationAndRotation(FVector(100.0f, 0.0f, 174.0f), FRotator::ZeroRotator);
	TestTrue(TEXT("Fire interval has elapsed"), Fixture.World->GetTimeSeconds() >= 0.42f);
	TestTrue(TEXT("Barrel now faces the target"), Visual->GetMuzzleWorldTransform(MuzzleTransform)
		&& FVector::DotProduct(MuzzleTransform.GetUnitAxis(EAxis::X), FVector::ForwardVector) > 0.99f);
	FCollisionQueryParams ShotParams(SCENE_QUERY_STAT(JTSTrajectoryTest), false, Character);
	ShotParams.AddIgnoredActor(Character);
	FHitResult AlignedHit;
	Fixture.World->LineTraceSingleByChannel(AlignedHit, MuzzleTransform.GetLocation(),
		MuzzleTransform.GetLocation() + FVector::ForwardVector * 1000.0f, ECC_Visibility, ShotParams);
	TestEqual(TEXT("An aligned muzzle ray reaches the cube"), AlignedHit.GetActor(), static_cast<AActor*>(Cube));
	Ranged->StartFire();
	Ranged->StopFire();
	TestTrue(TEXT("The same target takes damage when the barrel intersects it"), Health->GetHealth() < InitialHealth);

	if (!TestTrue(TEXT("Knife can be equipped"), Inventory->TryAddItemById(EJTSItemId::ShortBlade))) return false;
	int32 KnifeSlot = INDEX_NONE;
	for (int32 Index = 0; Index < Inventory->GetItemSlots().Num(); ++Index)
	{
		if (Inventory->GetItemAtSlot(Index).ItemId == EJTSItemId::ShortBlade) KnifeSlot = Index;
	}
	if (!TestTrue(TEXT("Knife has a quickbar slot"), KnifeSlot != INDEX_NONE)) return false;
	Inventory->SelectQuickbarSlot(KnifeSlot);
	TestEqual(TEXT("Knife is selected"), Inventory->GetActiveItemId(), EJTSItemId::ShortBlade);
	Visual->RefreshWeaponVisual();
	Cube->SetActorLocation(FVector(160.0f, 0.0f, 170.0f));
	UBoxComponent* Body = Cast<UBoxComponent>(Cube->GetRootComponent());
	if (!TestNotNull(TEXT("Target has a strike collision body"), Body)) return false;
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Melee->AttackPressed();
	for (int32 Step = 0; Step < 6; ++Step)
	{
		Fixture.World->Tick(LEVELTICK_All, 0.05f);
		static_cast<UActorComponent*>(Visual)->TickComponent(0.05f, LEVELTICK_All, nullptr);
		static_cast<UActorComponent*>(Melee)->TickComponent(0.05f, LEVELTICK_All, nullptr);
	}
	Muzzle->SetWorldLocationAndRotation(FVector(0.0f, 200.0f, 170.0f), FRotator::ZeroRotator);
	Melee->PerformHitCheck(); // Prime the prior tip while the target is non-colliding.
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	const float BeforeMiss = Health->GetHealth();
	Melee->PerformHitCheck();
	TestEqual(TEXT("A center-screen melee target takes no damage without weapon contact"),
		Health->GetHealth(), BeforeMiss);
	Melee->AttackReleased();
	Melee->StopAttack();
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Melee->AttackPressed();
	for (int32 Step = 0; Step < 6; ++Step)
	{
		Fixture.World->Tick(LEVELTICK_All, 0.05f);
		static_cast<UActorComponent*>(Visual)->TickComponent(0.05f, LEVELTICK_All, nullptr);
		static_cast<UActorComponent*>(Melee)->TickComponent(0.05f, LEVELTICK_All, nullptr);
	}
	TestTrue(TEXT("Held weapon swing is in its contact window"), Melee->GetMeleeSwingPhase() > 0.0f);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Muzzle->SetWorldLocationAndRotation(FVector(150.0f, 0.0f, 170.0f), FRotator::ZeroRotator);
	FVector Tip;
	TestTrue(TEXT("The held knife exposes a strike tip"), Visual->GetHeldItemTipWorldLocation(Tip));
	TestTrue(TEXT("The knife tip sits inside the target body"), FVector::DistSquared(Tip, Cube->GetActorLocation()) < FMath::Square(40.0f));
	const float BeforeContact = Health->GetHealth();
	Melee->PerformHitCheck();
	TestTrue(TEXT("Held weapon contact can damage the target"), Health->GetHealth() < BeforeContact);
	Melee->AttackReleased();
	Melee->StopAttack();
	if (!TestTrue(TEXT("Knife can be put away for a punch"), Inventory->TryRemoveItem(EJTSItemId::ShortBlade, 1))) return false;
	Inventory->SelectQuickbarSlot(KnifeSlot);
	TestEqual(TEXT("Empty hands select the punch"), Inventory->GetActiveItemId(), EJTSItemId::None);
	Visual->RefreshWeaponVisual();
	const float BeforePunchMiss = Health->GetHealth();
	Melee->AttackPressed();
	Melee->PerformHitCheck();
	TestEqual(TEXT("A nearby camera-aligned target is not punched without fist contact"),
		Health->GetHealth(), BeforePunchMiss);
	Melee->AttackReleased();
	Melee->StopAttack();
	return true;
}

#endif
