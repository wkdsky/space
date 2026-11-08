#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Math/RotationMatrix.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "space/Components/JTSCombatFacingComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSMeleeComponent.h"
#include "space/Components/JTSMiningStrikeComponent.h"
#include "space/Components/JTSRangedWeaponComponent.h"
#include "space/Components/JTSWeaponVisualComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"

namespace
{
	struct FCombatControlWorld
	{
		UWorld* World;
		AJTSCharacter* Player;
		FCombatControlWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			const auto Class = LoadClass<AJTSCharacter>(nullptr,
				TEXT("/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2.BP_JTSPlayer_Casual_2_C"));
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<AJTSCharacter>(Class, FVector(0,0,96), FRotator::ZeroRotator, Params);
			if (Player && !Player->HasActorBegunPlay()) Player->DispatchBeginPlay();
		}
		~FCombatControlWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		void Equip(EJTSItemId Item)
		{
			auto* Inventory = Player->GetInventoryComponent();
			Inventory->TryAddItemById(Item);
			for (int32 I=0; I<Inventory->GetItemSlots().Num(); ++I)
				if (Inventory->GetItemAtSlot(I).ItemId==Item) { Inventory->SelectQuickbarSlot(I); break; }
			Player->FindComponentByClass<UJTSWeaponVisualComponent>()->RefreshWeaponVisual();
		}
		AJTSPlanetSurfaceCreature* Mirror(FVector Location)
		{
			auto Class = LoadClass<AJTSPlanetSurfaceCreature>(nullptr,
				TEXT("/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_MirrorSkimmer.BP_MirrorSkimmer_C"));
			auto* Creature = World->SpawnActor<AJTSPlanetSurfaceCreature>(Class, Location, FRotator::ZeroRotator);
			if (!Creature->HasActorBegunPlay()) Creature->DispatchBeginPlay();
			Creature->SetActorTickEnabled(false);
			return Creature;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSCombatFacingModes, "JTS.Character.CombatFacingModes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSCombatFacingModes::RunTest(const FString& Parameters)
{
	FCombatControlWorld F;
	if (!TestNotNull(TEXT("Playable pawn exists"), F.Player)) return false;
	auto* Facing = F.Player->FindComponentByClass<UJTSCombatFacingComponent>();
	auto* View = F.Player->FindComponentByClass<UJTSRangedWeaponComponent>();
	auto* Melee = F.Player->FindComponentByClass<UJTSMeleeComponent>();
	auto* Movement = F.Player->GetCharacterMovement();
	F.Equip(EJTSItemId::Pickaxe);
	for (const FVector Up : {FVector::UpVector, FVector::RightVector})
	{
		const FVector Body = Up.Equals(FVector::UpVector) ? FVector::ForwardVector : FVector::UpVector;
		F.Player->SetActorRotation(FRotationMatrix::MakeFromXZ(Body,Up).ToQuat());
		const FVector Attack = -Body;
		View->AcceptViewIntent(-Attack*300,Attack);
		Movement->Velocity=FVector::ZeroVector;
		const FVector Before=F.Player->GetActorForwardVector();
		static_cast<AActor*>(F.Player)->Tick(.016f);
		TestTrue(TEXT("Free camera orbit leaves a stationary body alone"), F.Player->GetActorForwardVector().Equals(Before,.01));
		Melee->StartAttack();
		TestTrue(TEXT("A 180-degree camera orbit turns the entire body before the swing"),
			FVector::DotProduct(F.Player->GetActorForwardVector(),Attack)>.999);
		View->AcceptViewIntent(-Body*300,Body);
		Movement->Velocity=Body*500;
		static_cast<AActor*>(F.Player)->Tick(.016f);
		TestTrue(TEXT("Backward movement and later camera motion cannot reverse an active swing"),
			FVector::DotProduct(F.Player->GetActorForwardVector(),Attack)>.999);
		TestTrue(TEXT("Facing preserves the gravity-relative up axis"), FVector::DotProduct(F.Player->GetActorUpVector(),Up)>.999);
		Melee->StopAttack();
	}
	F.Player->SetActorRotation(FRotator::ZeroRotator);
	F.Equip(EJTSItemId::AssaultRifle);
	View->AcceptViewIntent(FVector(-300,60,0),FVector::ForwardVector);
	View->StartAim();
	Movement->Velocity=-FVector::ForwardVector*300;
	static_cast<AActor*>(F.Player)->Tick(.016f);
	TestTrue(TEXT("ADS backpedals while continuing to face the reticle"), F.Player->GetActorForwardVector().X>.999);
	View->StopAim();
	View->StartFire();
	View->AcceptViewIntent(FVector(0,-300,0),FVector::RightVector);
	static_cast<AActor*>(F.Player)->Tick(.016f);
	TestTrue(TEXT("Held hip fire follows camera yaw while moving"), F.Player->GetActorForwardVector().Y>.999);
	View->StopFire();
	FVector Intent;
	TestFalse(TEXT("Releasing aim and fire restores free movement"), Facing->GetDesiredFacing(Intent));
	TestFalse(TEXT("Server rejects an impossible camera location"), View->AcceptViewIntent(FVector(100000,0,0),FVector::ForwardVector));
	TestFalse(TEXT("Server rejects a non-unit attack direction"), View->AcceptViewIntent(FVector::ZeroVector,FVector(20,0,0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSPickaxeCloseCreature, "JTS.Combat.PickaxeCloseCreature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSPickaxeCloseCreature::RunTest(const FString& Parameters)
{
	FCombatControlWorld F;
	F.Equip(EJTSItemId::Pickaxe);
	F.Player->GetCharacterMovement()->DisableMovement();
	auto* Tool=F.Player->FindComponentByClass<UJTSMiningStrikeComponent>();
	auto* Melee=F.Player->FindComponentByClass<UJTSMeleeComponent>();
	auto* View=F.Player->FindComponentByClass<UJTSRangedWeaponComponent>();
	const auto* Item=Tool->GetToolDefinition();
	if (!TestNotNull(TEXT("Directed pickaxe is equipped"),Item)) return false;
	TestTrue(TEXT("Held pickaxe uses a compact mesh and matching tip"),
		Item->HeldPresentation.MeshScale<.75f && Item->HeldPresentation.MuzzleTransform.GetLocation().Size()<75);
	for (const FVector Location : {FVector(48,0,30), FVector(75,30,30), FVector(65,-25,35)})
	{
		auto* Bug=F.Mirror(Location);
		auto* Health=Bug->FindComponentByClass<UJTSHealthComponent>();
		const float Before=Health->GetHealth();
		View->AcceptViewIntent(FVector(-300,60,0),FVector::ForwardVector);
		Melee->StartAttack();
		F.World->Tick(LEVELTICK_All,Item->MiningSwingDuration*Item->MiningContactFraction+.005f);
		FVector Grip, Tip; FQuat Rotation; float Load;
		TestTrue(TEXT("Tool provides a hand and head pose for the close strike"),Tool->GetGripPose(Grip,Rotation,Tip,Load));
		TestTrue(TEXT("The visible pick head reaches the creature's collision surface"),FVector::Distance(Tip,Bug->GetActorLocation())<31.f);
		Tool->ResolveContact();
		TestTrue(FString::Printf(TEXT("Close mirror at %.0f/%.0f receives the strike"),Location.X,Location.Y),Health->GetHealth()<Before);
		const float After=Health->GetHealth();
		Tool->ResolveContact();
		TestEqual(TEXT("One swing damages a target once"),Health->GetHealth(),After);
		Melee->StopAttack();
		Bug->Destroy();
	}
	auto* Distant=F.Mirror(FVector(160,0,30));
	const float DistantHealth=Distant->FindComponentByClass<UJTSHealthComponent>()->GetHealth();
	Melee->StartAttack(); Tool->ResolveContact(); Melee->StopAttack();
	TestEqual(TEXT("Shortening the tool does not create an invisible extended hit"),Distant->FindComponentByClass<UJTSHealthComponent>()->GetHealth(),DistantHealth);
	Distant->Destroy();
	// A nearby actor behind the player is never selected by the close-range assist.
	auto* Behind=F.Mirror(FVector(-50,0,30));
	const float BackHealth=Behind->FindComponentByClass<UJTSHealthComponent>()->GetHealth();
	Melee->StartAttack(); Tool->ResolveContact(); Melee->StopAttack();
	TestEqual(TEXT("The pickaxe cannot auto-hit a creature behind the body"),Behind->FindComponentByClass<UJTSHealthComponent>()->GetHealth(),BackHealth);
	Behind->Destroy();
	auto* Bug=F.Mirror(FVector(75,0,30));
	auto* Wall=F.World->SpawnActor<AStaticMeshActor>();
	Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
	Wall->SetActorLocation(FVector(40,0,75)); Wall->SetActorScale3D(FVector(.1f,2,2));
	Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	const float Before=Bug->FindComponentByClass<UJTSHealthComponent>()->GetHealth();
	Melee->StartAttack(); Tool->ResolveContact(); Melee->StopAttack();
	TestEqual(TEXT("A real wall prevents the close-range assisted strike"),Bug->FindComponentByClass<UJTSHealthComponent>()->GetHealth(),Before);
	return true;
}

#endif
