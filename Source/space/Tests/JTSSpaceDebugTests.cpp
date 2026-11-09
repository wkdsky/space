#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SWidget.h"
#include "UObject/UnrealType.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Characters/JTSPlanetBoss.h"
#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/World/JTSSpaceDebugTravelManager.h"
#include "space/UI/JTSPrototypeHUDWidget.h"
#include "space/UI/JTSSpaceDebugWidget.h"

namespace
{
	struct FDebugTestWorld
	{
		UWorld* World;
		FDebugTestWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
		~FDebugTestWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSInventoryMouseTransactionTest, "JTS.Debug.InventoryMouseTransactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSInventoryMouseTransactionTest::RunTest(const FString& Parameters)
{
	FDebugTestWorld Fixture;
	auto* Character = Fixture.World->SpawnActor<AJTSCharacter>();
	auto* Inventory = Character->GetInventoryComponent();
	Inventory->TryAddItemById(EJTSItemId::WaistLamp);
	const auto Lamp = Inventory->GetItemAtSlot(0);
	TestFalse(TEXT("Fixture contains a real instance"), Lamp.IsEmpty());
	Inventory->ServerMoveInventorySlot(0, FGuid::NewGuid(), 1, FGuid());
	TestEqual(TEXT("A stale drag cannot move a different item"), Inventory->GetItemAtSlot(0).InstanceId, Lamp.InstanceId);
	Inventory->ServerMoveInventorySlot(0, Lamp.InstanceId, 1, FGuid());
	TestTrue(TEXT("Source becomes empty"), Inventory->GetItemAtSlot(0).IsEmpty());
	TestEqual(TEXT("Payload identity survives move"), Inventory->GetItemAtSlot(1).InstanceId, Lamp.InstanceId);
	Inventory->ServerEquipInventorySlot(0, Lamp.InstanceId);
	TestEqual(TEXT("Stale equip cannot select emptied source"), Inventory->GetSelectedQuickbarSlot(), 1);
	Inventory->ServerEquipInventorySlot(1, Lamp.InstanceId);
	TestTrue(TEXT("Right-click equip lights the belt lamp"), Inventory->IsWaistLampEquipped());
	Inventory->ServerDropInventorySlot(1, FGuid::NewGuid());
	TestEqual(TEXT("Stale drop preserves inventory"), Inventory->GetItemAtSlot(1).InstanceId, Lamp.InstanceId);
	Inventory->ServerMoveInventorySlot(1, Lamp.InstanceId, 99, FGuid());
	TestEqual(TEXT("Out of range destination rejected"), Inventory->GetUsedSlotCount(), 1);
	Inventory->ServerDropInventorySlot(1, Lamp.InstanceId);
	TestEqual(TEXT("A failed world drop preserves the carried payload"), Inventory->GetItemAtSlot(1).InstanceId, Lamp.InstanceId);
	auto* Ground = Fixture.World->SpawnActor<AStaticMeshActor>();
	Ground->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Ground->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Ground->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Ground->SetActorLocation(FVector(0, 0, -100));
	Ground->SetActorScale3D(FVector(100, 100, 1));
	Inventory->ServerDropInventorySlot(1, Lamp.InstanceId);
	TestTrue(TEXT("Valid drop removes the complete carried payload"), Inventory->GetItemAtSlot(1).IsEmpty());
	TestFalse(TEXT("Dropped lamp no longer remains equipped"), Inventory->IsWaistLampEquipped());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSDebugTravelAuthorityTest, "JTS.Debug.TravelAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSDebugTravelAuthorityTest::RunTest(const FString& Parameters)
{
	FDebugTestWorld Fixture;
	auto* Debug = Fixture.World->SpawnActor<AJTSSpaceDebugTravelManager>();
	auto* Controller = Fixture.World->SpawnActor<AJTSPlayerController>();
	auto* State = Fixture.World->SpawnActor<AJTSPlayerState>();
	FindFProperty<FObjectProperty>(Controller->GetClass(), TEXT("PlayerState"))->SetObjectPropertyValue_InContainer(Controller, State);
	TestFalse(TEXT("Non-host cannot mutate shared travel"), Debug->CanUse(Controller));
	State->SetExpeditionHost(true);
	TestTrue(TEXT("Host can use enabled development controls"), Debug->CanUse(Controller));
	TestFalse(TEXT("An arbitrary planet is never implicitly a debug destination"), Debug->ContainsDestination(TEXT("UnconfiguredPlanet")));
	TestFalse(TEXT("None is never a valid destination"), Debug->ContainsDestination(NAME_None));
	FindFProperty<FBoolProperty>(Debug->GetClass(), TEXT("bEnabled"))->SetPropertyValue_InContainer(Debug, false);
	TestFalse(TEXT("Disabled controls reject the host as well"), Debug->CanUse(Controller));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSPlayerInvulnerabilityTest, "JTS.Debug.PlayerInvulnerability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSPlayerInvulnerabilityTest::RunTest(const FString& Parameters)
{
	FDebugTestWorld Fixture;
	Fixture.World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	if (!TestTrue(TEXT("Damage fixture has an authoritative GameMode"),
		Fixture.World->SetGameMode(FURL(nullptr, TEXT("/Temp/DebugImmunity?game=/Script/Engine.GameModeBase"), TRAVEL_Absolute)))) return false;
	auto* Controller = Fixture.World->SpawnActor<AJTSPlayerController>();
	auto* Character = Fixture.World->SpawnActor<AJTSCharacter>();
	Character->DispatchBeginPlay();
	Controller->Possess(Character);
	auto* Health = Character->GetHealthComponent();
	Controller->ServerSetDebugInvulnerable(true);
	TestFalse(TEXT("Missing development controls reject immunity"), Controller->IsDebugInvulnerable());
	auto* Debug = Fixture.World->SpawnActor<AJTSSpaceDebugTravelManager>();
	Controller->ServerSetDebugInvulnerable(true);
	TestTrue(TEXT("Enabled own-player request grants server damage immunity"), Health->IsInvulnerable());
	const float Before = Health->GetHealth();
	TestEqual(TEXT("Lethal direct damage is blocked"), Health->ApplyDamage(10000, nullptr, nullptr), 0.0f);
	TestEqual(TEXT("Armor bypass cannot bypass immunity"), Health->ApplyDamage(10000, nullptr, nullptr, true), 0.0f);
	UGameplayStatics::ApplyDamage(Character, 10000, nullptr, nullptr, nullptr);
	TestEqual(TEXT("Standard UE damage leaves health unchanged"), Health->GetHealth(), Before);
	TestFalse(TEXT("Immune character never enters death"), Health->IsDead());
	auto* OtherCharacter = Fixture.World->SpawnActor<AJTSCharacter>();
	TestTrue(TEXT("Other players still take damage"), OtherCharacter->GetHealthComponent()->ApplyDamage(1, nullptr, nullptr) > 0);
	Controller->UnPossess();
	Character->Destroy();
	auto* Replacement = Fixture.World->SpawnActor<AJTSCharacter>();
	Replacement->DispatchBeginPlay();
	Controller->Possess(Replacement);
	Health = Replacement->GetHealthComponent();
	TestTrue(TEXT("The replacement pawn inherits the controller toggle"), Health->IsInvulnerable());
	FindFProperty<FBoolProperty>(Debug->GetClass(), TEXT("bEnabled"))->SetPropertyValue_InContainer(Debug, false);
	Controller->ServerSetDebugInvulnerable(false);
	TestFalse(TEXT("Disabling remains possible after debug controls turn off"), Health->IsInvulnerable());
	Controller->ServerSetDebugInvulnerable(true);
	TestFalse(TEXT("Disabled development controls cannot re-enable immunity"), Controller->IsDebugInvulnerable());
	Replacement->SetRole(ROLE_SimulatedProxy);
	Health->SetInvulnerable(true);
	TestFalse(TEXT("A client cannot change authoritative immunity"), Health->IsInvulnerable());
	Replacement->SetRole(ROLE_Authority);
	const float BeforeNormalDamage = Health->GetHealth();
	UGameplayStatics::ApplyDamage(Replacement, 1, nullptr, nullptr, nullptr);
	TestTrue(TEXT("Standard UE damage resumes after disabling"), Health->GetHealth() < BeforeNormalDamage);
	TestTrue(TEXT("Disabling resumes lethal damage and death"), Health->ApplyDamage(10000, nullptr, nullptr) > 0 && Health->IsDead());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSDebugOneHitKillTest, "JTS.Debug.OneHitKillOutgoingOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSDebugOneHitKillTest::RunTest(const FString& Parameters)
{
	FDebugTestWorld Fixture;
	Fixture.World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	Fixture.World->SetGameMode(FURL(nullptr, TEXT("/Temp/DebugOneHit?game=/Script/Engine.GameModeBase"), TRAVEL_Absolute));
	auto* Controller = Fixture.World->SpawnActor<AJTSPlayerController>();
	auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	Controller->SetPlayer(LocalPlayer);
	auto* Player = Fixture.World->SpawnActor<AJTSCharacter>();
	Player->DispatchBeginPlay(); Controller->Possess(Player);
	Controller->ServerSetDebugOneHitKill(true);
	TestFalse(TEXT("Missing debug controls reject the toggle"), Controller->IsDebugOneHitKillEnabled());
	auto* Debug = Fixture.World->SpawnActor<AJTSSpaceDebugTravelManager>();
	auto* Widget = CreateWidget<UJTSSpaceDebugWidget>(Controller);
	if (!TestNotNull(TEXT("Debug panel"), Widget)) return false;
	const auto SlateWidget = Widget->TakeWidget();
	auto* Toggle = Cast<UButton>(Widget->GetWidgetFromName(TEXT("OneHitKillToggle")));
	if (!TestNotNull(TEXT("Debug panel contains the one-hit toggle"), Toggle)) return false;
	Toggle->OnClicked.Broadcast();
	TestTrue(TEXT("Clicking the panel enables the owner's server toggle"), Controller->IsDebugOneHitKillEnabled());
	const auto SpawnCreature = [&]()
	{
		auto* Creature = Fixture.World->SpawnActor<AJTSPlanetSurfaceCreature>();
		Creature->DispatchBeginPlay();
		return Creature;
	};
	auto* Creature = SpawnCreature();
	auto* Health = Creature->FindComponentByClass<UJTSHealthComponent>();
	FindFProperty<FFloatProperty>(Health->GetClass(), TEXT("ArmorDamageReduction"))->SetPropertyValue_InContainer(Health, .8f);
	Health->SetAbilityDefense(10000);
	Health->ApplyDamage(1, Controller, Player);
	TestTrue(TEXT("One point of player damage kills an armored creature"), Health->IsDead());
	auto* Boss = Fixture.World->SpawnActor<AJTSPlanetBoss>(); Boss->DispatchBeginPlay();
	Boss->Health->SetMaxHealth(10000, true);
	Boss->Health->ApplyDamage(1, Controller, Player);
	TestTrue(TEXT("Boss health cannot resist a valid one-hit attack"), Boss->Health->IsDead());
	auto* RangedTarget = SpawnCreature();
	UGameplayStatics::ApplyDamage(RangedTarget, 1, Controller, Player, nullptr);
	TestTrue(TEXT("Standard UE weapon damage uses the same rule"), RangedTarget->FindComponentByClass<UJTSHealthComponent>()->IsDead());
	auto* FallbackTarget = SpawnCreature();
	FallbackTarget->FindComponentByClass<UJTSHealthComponent>()->ApplyDamage(1, nullptr, Player);
	TestTrue(TEXT("Direct melee can resolve its player damage causer"), FallbackTarget->FindComponentByClass<UJTSHealthComponent>()->IsDead());
	auto* OtherController = Fixture.World->SpawnActor<AJTSPlayerController>();
	auto* NormalTarget = SpawnCreature();
	auto* NormalHealth = NormalTarget->FindComponentByClass<UJTSHealthComponent>();
	NormalHealth->ApplyDamage(1, OtherController, nullptr);
	TestFalse(TEXT("Another player's attacks keep normal damage"), NormalHealth->IsDead());
	NormalHealth->ApplyDamage(0, Controller, Player);
	TestFalse(TEXT("Zero damage cannot execute a target"), NormalHealth->IsDead());
	const float PlayerBefore = Player->GetHealthComponent()->GetHealth();
	UGameplayStatics::ApplyDamage(Player, 1, nullptr, NormalTarget, nullptr);
	TestEqual(TEXT("Enemy attacks still remove only their normal damage"), Player->GetHealthComponent()->GetHealth(), PlayerBefore - 1);
	auto* Teammate = Fixture.World->SpawnActor<AJTSCharacter>(); Teammate->DispatchBeginPlay();
	const float TeammateBefore = Teammate->GetHealthComponent()->GetHealth();
	Teammate->GetHealthComponent()->ApplyDamage(1, Controller, Player);
	TestEqual(TEXT("Player targets remain excluded even while unpossessed"), Teammate->GetHealthComponent()->GetHealth(), TeammateBefore - 1);
	NormalTarget->SetRole(ROLE_SimulatedProxy);
	NormalHealth->ApplyDamage(1, Controller, Player);
	TestFalse(TEXT("Clients cannot execute a target locally"), NormalHealth->IsDead());
	NormalTarget->SetRole(ROLE_Authority);
	Toggle->OnClicked.Broadcast();
	TestFalse(TEXT("The same panel button disables the toggle"), Controller->IsDebugOneHitKillEnabled());
	NormalHealth->ApplyDamage(1, Controller, Player);
	TestFalse(TEXT("Disabling restores ordinary outgoing damage"), NormalHealth->IsDead());
	Controller->ServerSetDebugOneHitKill(true);
	FindFProperty<FBoolProperty>(Debug->GetClass(), TEXT("bEnabled"))->SetPropertyValue_InContainer(Debug, false);
	NormalHealth->ApplyDamage(1, Controller, Player);
	TestFalse(TEXT("Disabling development controls also disables executions"), NormalHealth->IsDead());
	Controller->ServerSetDebugOneHitKill(false);
	Controller->ServerSetDebugOneHitKill(true);
	TestFalse(TEXT("Disabled development controls reject re-enabling"), Controller->IsDebugOneHitKillEnabled());
	Widget->ConditionalBeginDestroy();
	Controller->Player = nullptr; LocalPlayer->PlayerController = nullptr;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSRespawnHealthHudTest, "JTS.UI.RespawnHealthRebind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSRespawnHealthHudTest::RunTest(const FString& Parameters)
{
	FDebugTestWorld Fixture;
	Fixture.World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	auto* Controller = Fixture.World->SpawnActor<AJTSPlayerController>();
	auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	Controller->Player = LocalPlayer; LocalPlayer->PlayerController = Controller;
	auto* Character = Fixture.World->SpawnActor<AJTSCharacter>();
	Character->DispatchBeginPlay();
	Controller->Possess(Character);
	auto* Widget = CreateWidget<UJTSPrototypeHUDWidget>(Controller);
	if (!TestNotNull(TEXT("Player HUD constructs"), Widget)) return false;
	const auto SlateWidget = Widget->TakeWidget();
	auto* Bar = Cast<UProgressBar>(Widget->GetWidgetFromName(TEXT("PlayerHealthProgressBar")));
	if (!TestNotNull(TEXT("HUD has its real health progress bar"), Bar)) return false;
	auto* OldHealth = Character->GetHealthComponent();
	OldHealth->ApplyDamage(10000, nullptr, nullptr);
	TestEqual(TEXT("Death empties the bound HUD bar"), Bar->GetPercent(), 0.0f);
	Controller->UnPossess();
	auto* Replacement = Fixture.World->SpawnActor<AJTSCharacter>();
	Replacement->DispatchBeginPlay();
	Controller->Possess(Replacement);
	SlateWidget->Tick(FGeometry(), 0.0, 0.016f);
	TestEqual(TEXT("Respawn rebinds the persistent HUD to the full-health new pawn"), Bar->GetPercent(), 1.0f);
	OldHealth->OnHealthChanged.Broadcast(0, OldHealth->GetMaxHealth());
	TestEqual(TEXT("Old pawn notifications cannot overwrite new health"), Bar->GetPercent(), 1.0f);
	Replacement->GetHealthComponent()->ApplyDamage(1, nullptr, nullptr);
	TestEqual(TEXT("New pawn damage immediately reaches the HUD"), Bar->GetPercent(), Replacement->GetHealthComponent()->GetHealthNormalized());
	Widget->ConditionalBeginDestroy();
	Controller->Player = nullptr; LocalPlayer->PlayerController = nullptr;
	return true;
}
#endif
