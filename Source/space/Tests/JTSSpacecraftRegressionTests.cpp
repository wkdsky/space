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
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSStellarLoadoutComponent.h"
#include "space/Components/JTSRangedWeaponComponent.h"
#include "space/Core/JTSGameState.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "space/Components/JTSSpacecraftFlightMovementComponent.h"
#include "space/Components/JTSSpacecraftPresentationComponent.h"
#include "space/Items/JTSItemTypes.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSStellarLootTable.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/UI/JTSCruiseNavigationWidget.h"
#include "space/Items/JTSStellarProgression.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetLandingManager.h"
#include "space/World/JTSPlanetLandingSite.h"
#include "space/World/JTSSpaceWorldManager.h"

namespace
{
	float PhysicalAltitudeAtNavigationKilometers(float Kilometers)
	{
		return 8000.0f * (FMath::Exp(Kilometers / 12.0f) - 1.0f);
	}

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
	TestTrue(TEXT("Holdable can enter the inventory"), Inventory->TryAddItemById(EJTSItemId::RailPistol));
	TestTrue(TEXT("Holdable is placed in the selected empty quickbar slot"), Inventory->GetItemAtSlot(1).ItemId == EJTSItemId::RailPistol);
	TestTrue(TEXT("Placed holdable becomes the active item"), Inventory->GetActiveItemId() == EJTSItemId::RailPistol);
	TestTrue(TEXT("Unselected empty quickbar slot remains untouched"), Inventory->GetItemAtSlot(0).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSItemDefinitionCacheGcRegression, "JTS.Inventory.ItemDefinitionSurvivesGarbageCollection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSItemDefinitionCacheGcRegression::RunTest(const FString& Parameters)
{
	const FString Before = UJTSItemDefinitionLibrary::GetItemDisplayName(EJTSItemId::RailPistol).ToString();
	CollectGarbage(RF_NoFlags);
	const FString After = UJTSItemDefinitionLibrary::GetItemDisplayName(EJTSItemId::RailPistol).ToString();
	TestEqual(TEXT("Cached definition survives garbage collection"), After, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSUnarmedAimRegression, "JTS.Character.UnarmedAim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSUnarmedAimRegression::RunTest(const FString& Parameters)
{
	FShipTestWorld Fixture;
	AJTSCharacter* Character = nullptr;
	Fixture.AddPlayer(Character);
	UJTSInventoryComponent* const Inventory = IsValid(Character) ? Character->GetInventoryComponent() : nullptr;
	UJTSRangedWeaponComponent* const Aim = IsValid(Character)
		? Character->FindComponentByClass<UJTSRangedWeaponComponent>() : nullptr;
	if (!TestNotNull(TEXT("Unarmed character has an inventory"), Inventory)
		|| !TestNotNull(TEXT("Unarmed character has an aim component"), Aim))
	{
		return false;
	}

	Inventory->RestoreItems({}, 0);
	Aim->StartAim();
	TestTrue(TEXT("Empty hands can enter view aim"), Aim->IsAiming());
	Aim->StopAim();
	TestFalse(TEXT("Releasing aim returns empty hands to normal view"), Aim->IsAiming());
	FJTSItemInstance HeadLamp;
	HeadLamp.ItemId = EJTSItemId::WaistLamp;
	HeadLamp.StackCount = 1;
	Inventory->RestoreItems({ HeadLamp }, 0);
	TestTrue(TEXT("Headlamp occupies the selected inventory slot"), Inventory->GetActiveItemId() == EJTSItemId::WaistLamp);
	Aim->StartAim();
	TestTrue(TEXT("Selecting an unlit headlamp leaves hands free to aim"), Aim->IsAiming());
	Aim->StopAim();
	Inventory->RequestToggleWaistLamp();
	TestTrue(TEXT("Headlamp can be worn while selected"), Inventory->IsWaistLampEquipped());
	Aim->StartAim();
	TestTrue(TEXT("Wearing a lit headlamp leaves hands free to aim"), Aim->IsAiming());
	Aim->StopAim();
	TestTrue(TEXT("A melee tool can enter the inventory beside the headlamp"), Inventory->TryAddItemById(EJTSItemId::PowerHammer));
	TestTrue(TEXT("A melee tool can be selected while wearing the headlamp"), Inventory->SelectQuickbarSlot(1));
	Aim->StartAim();
	TestFalse(TEXT("A held melee tool does not enter ranged aim even with the headlamp on"), Aim->IsAiming());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSShipLockerRegression, "JTS.Spacecraft.ShipLockerAndStellarLoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSShipLockerRegression::RunTest(const FString& Parameters)
{
	FShipTestWorld Fixture;
	AJTSCharacter* Character = nullptr;
	APlayerController* const Controller = Fixture.AddPlayer(Character);
	AJTSPlayerState* const LockerState = Controller->GetPlayerState<AJTSPlayerState>();
	UJTSInventoryComponent* const CarriedInventory = Character->GetInventoryComponent();
	if (!TestNotNull(TEXT("Player has locker state"), LockerState)
		|| !TestNotNull(TEXT("Player has carried inventory"), CarriedInventory)) return false;
	const UJTSStellarLootTable* const Table = Fixture.Ship->GetStellarLootTable();
	if (!TestNotNull(TEXT("Blueprint supplies the stellar loot data asset"), Table)) return false;

	for (int32 Index = 0; Index < AJTSPlayerState::ShipLockerCapacity; ++Index)
	{
		TestTrue(TEXT("Purchase fills a distinct terminal slot"),
			LockerState->TryStorePurchasedItem(UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::RailPistol)));
	}
	TestFalse(TEXT("Thirty-slot locker rejects overflow"), LockerState->HasFreeShipLockerSlot());
	TestFalse(TEXT("Overflow purchase stays rejected"),
		LockerState->TryStorePurchasedItem(UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::RailPistol)));
	TestEqual(TEXT("Purchases do not enter carried inventory"), CarriedInventory->GetItemCount(EJTSItemId::RailPistol), 0);

	const FJTSShipLockerSlot First = LockerState->GetShipLockerSlot(0);
	TestFalse(TEXT("Stale drag token cannot delete"), LockerState->TryDeleteShipLockerSlot(0, FGuid::NewGuid()));
	TestTrue(TEXT("Matching drag token deletes one slot"), LockerState->TryDeleteShipLockerSlot(0, First.SlotToken));
	TestTrue(TEXT("Deletion frees a slot"), LockerState->HasFreeShipLockerSlot());
	const FJTSShipLockerSlot Second = LockerState->GetShipLockerSlot(1);
	TestTrue(TEXT("A standard item can be taken deliberately"),
		LockerState->TryTakeShipLockerItem(1, Second.SlotToken, CarriedInventory, Table));
	TestEqual(TEXT("Taking adds to carried inventory"), CarriedInventory->GetItemCount(EJTSItemId::RailPistol), 1);
	int32 CarriedPistolSlot = INDEX_NONE;
	for (int32 Index = 0; Index < CarriedInventory->GetItemSlots().Num(); ++Index)
	{
		if (CarriedInventory->GetItemAtSlot(Index).ItemId == EJTSItemId::RailPistol)
		{
			CarriedPistolSlot = Index;
			break;
		}
	}
	TestTrue(TEXT("Purchase fills an empty locker slot for exchange"),
		LockerState->TryStorePurchasedItem(UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::PowerHammer)));
	const FJTSShipLockerSlot BeforeSwap = LockerState->GetShipLockerSlot(0);
	const FGuid OriginalCarriedId = CarriedInventory->GetItemAtSlot(CarriedPistolSlot).InstanceId;
	TestFalse(TEXT("Exchange rejects stale carried instance"),
		LockerState->TryExchangeShipLockerItemWithCarriedSlot(0, BeforeSwap.SlotToken,
			CarriedInventory, CarriedPistolSlot, FGuid::NewGuid(), Table));
	TestTrue(TEXT("Occupied carried slot exchanges exact item payloads"),
		LockerState->TryExchangeShipLockerItemWithCarriedSlot(0, BeforeSwap.SlotToken,
			CarriedInventory, CarriedPistolSlot, OriginalCarriedId, Table));
	TestEqual(TEXT("Locker receives replaced carried item"),
		LockerState->GetShipLockerSlot(0).StandardItem.ItemId, EJTSItemId::RailPistol);
	TestEqual(TEXT("Carried target receives locker item"),
		CarriedInventory->GetItemAtSlot(CarriedPistolSlot).ItemId, EJTSItemId::PowerHammer);
	TestFalse(TEXT("Exchange invalidates the old locker token"),
		LockerState->TryExchangeShipLockerItemWithCarriedSlot(0, BeforeSwap.SlotToken,
			CarriedInventory, CarriedPistolSlot,
			CarriedInventory->GetItemAtSlot(CarriedPistolSlot).InstanceId, Table));
	TestTrue(TEXT("Replaced locker item can be discarded"), LockerState->TryDeleteShipLockerSlot(0,
		LockerState->GetShipLockerSlot(0).SlotToken));
	TestFalse(TEXT("Stale carried item cannot be stored"),
		LockerState->TryStoreCarriedItemAtSlot(0, CarriedInventory, CarriedPistolSlot, FGuid::NewGuid()));
	TestTrue(TEXT("Carried item moves into the chosen locker slot"),
		LockerState->TryStoreCarriedItemAtSlot(0, CarriedInventory, CarriedPistolSlot,
			CarriedInventory->GetItemAtSlot(CarriedPistolSlot).InstanceId));
	TestEqual(TEXT("Transfer removes the carried copy"), CarriedInventory->GetItemCount(EJTSItemId::PowerHammer), 0);
	TestTrue(TEXT("Transfer preserves the item in the locker"),
		LockerState->GetShipLockerSlot(0).StandardItem.ItemId == EJTSItemId::PowerHammer);
	TestTrue(TEXT("Dropping locker item on an empty carried slot fills that exact slot"),
		LockerState->TryExchangeShipLockerItemWithCarriedSlot(0,
			LockerState->GetShipLockerSlot(0).SlotToken, CarriedInventory, CarriedPistolSlot, FGuid(), Table));
	TestTrue(TEXT("Locker slot becomes empty after direct placement"), LockerState->GetShipLockerSlot(0).IsEmpty());
	TestEqual(TEXT("Direct placement keeps the chosen carried slot"),
		CarriedInventory->GetItemAtSlot(CarriedPistolSlot).ItemId, EJTSItemId::PowerHammer);
	TestTrue(TEXT("Carried item can return to the locker"),
		LockerState->TryStoreCarriedItemAtSlot(0, CarriedInventory, CarriedPistolSlot,
			CarriedInventory->GetItemAtSlot(CarriedPistolSlot).InstanceId));
	TestTrue(TEXT("Transferred item can be discarded"), LockerState->TryDeleteShipLockerSlot(0,
		LockerState->GetShipLockerSlot(0).SlotToken));
	TestTrue(TEXT("Stellar text item uses the terminal slot"), LockerState->TryStoreStellarItem(TEXT("FireCore")));
	TestTrue(TEXT("A standard item occupies the carried target before a stellar swap"),
		CarriedInventory->TryAddItemById(EJTSItemId::RailPistol, 1));
	const FGuid CarriedBeforeStellarSwap = CarriedInventory->GetItemAtSlot(CarriedPistolSlot).InstanceId;
	const FGuid StellarLockerToken = LockerState->GetShipLockerSlot(0).SlotToken;
	TestFalse(TEXT("Stellar text cannot exchange with an occupied ordinary slot"),
		LockerState->TryExchangeShipLockerItemWithCarriedSlot(0,
			LockerState->GetShipLockerSlot(0).SlotToken, CarriedInventory,
			CarriedPistolSlot, CarriedBeforeStellarSwap, Table));
	TestEqual(TEXT("Rejected swap preserves the ordinary item"),
		CarriedInventory->GetItemAtSlot(CarriedPistolSlot).InstanceId, CarriedBeforeStellarSwap);
	TestEqual(TEXT("Rejected swap preserves the stellar item"), LockerState->GetShipLockerSlot(0).SlotToken, StellarLockerToken);
	TestFalse(TEXT("Take button cannot send stellar text into ordinary inventory"),
		LockerState->TryTakeShipLockerItem(0, LockerState->GetShipLockerSlot(0).SlotToken,
			CarriedInventory, Table));
	FJTSItemInstance RemovedCarriedItem;
	CarriedInventory->TryExtractItemAtSlot(CarriedPistolSlot, CarriedBeforeStellarSwap, RemovedCarriedItem);
	TestFalse(TEXT("Stellar text cannot enter an empty ordinary slot either"),
		LockerState->TryExchangeShipLockerItemWithCarriedSlot(0, StellarLockerToken,
			CarriedInventory, CarriedPistolSlot, FGuid(), Table));
	TestTrue(TEXT("Rejected placement keeps ordinary target empty"), CarriedInventory->GetItemAtSlot(CarriedPistolSlot).IsEmpty());
	TestTrue(TEXT("Stellar goods can move within the thirty-slot shop inventory"),
		LockerState->TryMoveShipLockerSlot(0, StellarLockerToken, 1, FGuid()));
	TestEqual(TEXT("Shop relocation preserves stellar identity"), LockerState->GetShipLockerSlot(1).StellarItemId, FName(TEXT("FireCore")));

	TestEqual(TEXT("Document pool has eighteen unique entries"), Table->Entries.Num(), 18);
	TestTrue(TEXT("Fire jet pairs with fire core"), Table->CanCombine(TEXT("FireCore"), TEXT("JetTube")));
	TestTrue(TEXT("Light focus pairs with light core"), Table->CanCombine(TEXT("LightCore"), TEXT("FocusTube")));
	TestTrue(TEXT("Dark black hole pairs with dark core"), Table->CanCombine(TEXT("DarkCore"), TEXT("BlackHoleTube")));
	TestFalse(TEXT("Mismatched core and attachment cannot combine"), Table->CanCombine(TEXT("WaterCore"), TEXT("JetTube")));
	TestFalse(TEXT("Firmware cannot act as an attachment"), Table->CanCombine(TEXT("FireCore"), TEXT("UpgradeFirmware")));
	TArray<double> BaseProbabilities;
	TestTrue(TEXT("Empty history has a valid roll distribution"),
		Table->GetRollProbabilities({}, BaseProbabilities));
	double ProbabilityTotal = 0.0;
	for (const double Probability : BaseProbabilities) ProbabilityTotal += Probability;
	TestTrue(TEXT("Displayed roll probabilities add up to one"),
		FMath::IsNearlyEqual(ProbabilityTotal, 1.0, 0.000001));
	const int32 FireCoreIndex = Table->Entries.IndexOfByPredicate([](const FJTSStellarLootEntry& Entry)
	{
		return Entry.ItemId == FName(TEXT("FireCore"));
	});
	if (!TestTrue(TEXT("Fire core is in the configured pool"), FireCoreIndex != INDEX_NONE)) return false;
	// Personal history U: each newly discovered core kind lowers every core and never shrinks the core total's order.
	TArray<FName> History;
	TArray<double> PreviousProbabilities = BaseProbabilities;
	for (const FJTSStellarLootEntry& Entry : Table->Entries)
	{
		if (!Entry.bCore) continue;
		History.Add(Entry.ItemId);
		TArray<double> Next;
		TestTrue(TEXT("History step has a valid distribution"), Table->GetRollProbabilities(History, Next));
		double NextTotal = 0.0;
		for (const double Probability : Next) NextTotal += Probability;
		TestTrue(TEXT("History step probabilities add up to one"), FMath::IsNearlyEqual(NextTotal, 1.0, 0.000001));
		for (int32 Index = 0; Index < Table->Entries.Num(); ++Index)
		{
			if (Table->Entries[Index].bCore)
			{
				TestTrue(TEXT("Every core gets rarer as more kinds are discovered"),
					Next[Index] < PreviousProbabilities[Index]);
			}
		}
		PreviousProbabilities = Next;
	}
	TArray<FName> RepeatedHistory = { TEXT("FireCore"), TEXT("FireCore"), TEXT("JetTube") };
	TestEqual(TEXT("Repeated cores and non-cores do not grow U"),
		Table->CountDiscoveredCoreKinds(RepeatedHistory), 1);
	FName Result;
	TestTrue(TEXT("Configured pool rolls a known item"), Table->Roll(LockerState->GetDiscoveredStellarCoreIds(), Result));
	TestTrue(TEXT("Rolled item exists in the configured pool"), Table->FindEntry(Result) != nullptr);

	LockerState->RestoreShipLockerSlots({});
	Character->SetActorLocation(Fixture.Ship->GetActorLocation());
	if (!TestTrue(TEXT("Fixture player can use the parked ship terminal"), Fixture.Ship->CanUseShipTerminal(Character))) return false;
	TestTrue(TEXT("Ship accepts shop materials"), Fixture.Ship->DepositResourceAmounts({ { EJTSResourceType::Rock, 20 }, { EJTSResourceType::Ore, 20 } }));
	TestEqual(TEXT("Purchase succeeds into locker"), Fixture.Ship->TryPurchase(Character, EJTSItemId::PowerHammer), EJTSShopPurchaseResult::Succeeded);
	TestEqual(TEXT("Purchase remains out of carried inventory"), CarriedInventory->GetItemCount(EJTSItemId::PowerHammer), 0);
	{
		// Normal-weapon upgrade: server charges Rock/Ore, raises the level, and validates the skill budget.
		const FGuid WeaponToken = LockerState->GetShipLockerSlot(0).SlotToken;
		const int32 RockBefore = Fixture.Ship->GetResourceAmount(EJTSResourceType::Rock);
		const int32 OreBefore = Fixture.Ship->GetResourceAmount(EJTSResourceType::Ore);
		TestFalse(TEXT("A stale token cannot upgrade a weapon"), Fixture.Ship->TryUpgradeLockerWeapon(Character, 0, FGuid::NewGuid()));
		TestEqual(TEXT("A rejected upgrade charges nothing"), Fixture.Ship->GetResourceAmount(EJTSResourceType::Rock), RockBefore);
		TestFalse(TEXT("Five points exceed the level one budget"), LockerState->TryApplyWeaponPoints(0, WeaponToken, { 5, 0, 0, 0, 0, 0 }));
		TestTrue(TEXT("Four points fit the level one budget"), LockerState->TryApplyWeaponPoints(0, WeaponToken, { 4, 0, 0, 0, 0, 0 }));
		TestTrue(TEXT("Weapon upgrade succeeds with materials"), Fixture.Ship->TryUpgradeLockerWeapon(Character, 0, WeaponToken));
		const FJTSItemInstance Upgraded = LockerState->GetShipLockerSlot(0).StandardItem;
		TestEqual(TEXT("Body level rises by one"), Upgraded.WeaponBodyLevel, 2);
		TestEqual(TEXT("Recorded skill points survive the upgrade"), static_cast<int32>(Upgraded.WeaponPoints.IsValidIndex(0) ? Upgraded.WeaponPoints[0] : 0), 4);
		TestTrue(TEXT("Upgrade spent Rock and Ore"), Fixture.Ship->GetResourceAmount(EJTSResourceType::Rock) < RockBefore
			&& Fixture.Ship->GetResourceAmount(EJTSResourceType::Ore) < OreBefore);
		TestTrue(TEXT("Level two budget accepts eight points"), LockerState->TryApplyWeaponPoints(0, WeaponToken, { 4, 4, 0, 0, 0, 0 }));
		TestTrue(TEXT("Free reset clears the points"), LockerState->TryApplyWeaponPoints(0, WeaponToken, {})
			&& LockerState->GetShipLockerSlot(0).StandardItem.WeaponPoints.IsEmpty());
		Fixture.Ship->DepositResourceAmounts({ { EJTSResourceType::Rock, RockBefore - Fixture.Ship->GetResourceAmount(EJTSResourceType::Rock) },
			{ EJTSResourceType::Ore, OreBefore - Fixture.Ship->GetResourceAmount(EJTSResourceType::Ore) } });
	}
	const FGuid RollRequestId = FGuid::NewGuid();
	FName AwardedItem;
	int32 AwardedSlot = INDEX_NONE;
	TestEqual(TEXT("Server roll succeeds"), Fixture.Ship->TryRollStellarItem(Character, AwardedItem, AwardedSlot, RollRequestId), EJTSStellarRollResult::Succeeded);
	const FJTSShipLockerSlot Reserved = LockerState->GetShipLockerSlot(AwardedSlot);
	TestTrue(TEXT("Roll reserves one locked slot before reveal"), AwardedSlot == 1 && Reserved.bPendingStellarReveal);
	TestFalse(TEXT("Pending award cannot be deleted"), LockerState->TryDeleteShipLockerSlot(AwardedSlot, Reserved.SlotToken));
	TestTrue(TEXT("Award can be revealed"), LockerState->TryRevealStellarItem(AwardedSlot, Reserved.SlotToken));
	TestTrue(TEXT("Revealed award is stored"), !AwardedItem.IsNone()
		&& LockerState->GetShipLockerSlot(AwardedSlot).StellarItemId == AwardedItem
		&& !LockerState->GetShipLockerSlot(AwardedSlot).bPendingStellarReveal);
	FName ReplayedItem;
	int32 ReplayedSlot = INDEX_NONE;
	TestEqual(TEXT("A resent request replays the stored roll"),
		Fixture.Ship->TryRollStellarItem(Character, ReplayedItem, ReplayedSlot, RollRequestId), EJTSStellarRollResult::Succeeded);
	TestTrue(TEXT("Replay returns the same prize and slot without a second charge"),
		ReplayedItem == AwardedItem && ReplayedSlot == AwardedSlot);
	for (int32 Index = 2; Index < AJTSPlayerState::ShipLockerCapacity; ++Index)
	{
		LockerState->TryStoreStellarItem(TEXT("FireCore"));
	}
	TestEqual(TEXT("Full locker blocks purchase before spending"), Fixture.Ship->TryPurchase(Character, EJTSItemId::PowerHammer), EJTSShopPurchaseResult::InventoryFull);
	TestEqual(TEXT("Full locker blocks a roll"), Fixture.Ship->TryRollStellarItem(Character, AwardedItem, AwardedSlot), EJTSStellarRollResult::InventoryFull);

	// Stellar progression rules: 30 recorded points under an 18-point core scale to 60 percent and never rewrite the record.
	{
		const TArray<uint8> Recorded = { 10, 8, 6, 4, 2, 0 };
		TArray<double> Effective;
		FJTSStellarProgression::ComputeEffectiveLevels(Recorded, 18, Effective);
		TestTrue(TEXT("18 point core scales 30 recorded points"), FMath::IsNearlyEqual(Effective[0], 6.0) && FMath::IsNearlyEqual(Effective[1], 4.8));
		FJTSStellarProgression::ComputeEffectiveLevels(Recorded, 40, Effective);
		TestTrue(TEXT("A larger budget never exceeds the recorded points"), FMath::IsNearlyEqual(Effective[0], 10.0));
		TestEqual(TEXT("Core level 1 has no budget"), FJTSStellarProgression::GetCoreBudget(1), 0);
		TestEqual(TEXT("Core level 61 has the full budget"), FJTSStellarProgression::GetCoreBudget(61), 60);
		int32 TotalUnits = 0;
		for (int32 Level = 1; Level < 61; ++Level) TotalUnits += FJTSStellarProgression::GetUpgradeUnitCost(Level);
		TestEqual(TEXT("Maxing a core costs 210 firmware units"), TotalUnits, 210);
		const FJTSStellarSkillDef* const Jet = FJTSStellarProgression::FindSkillDefs(TEXT("JetTube"));
		TestTrue(TEXT("Fire spread is floor(x/2)"), Jet && FMath::IsNearlyEqual(FJTSStellarProgression::EvaluateSkill(Jet[4], 5.9), 2.0));

		// Server-authoritative allocation on a combined weapon.
		LockerState->RestoreShipLockerSlots({});
		LockerState->TryStoreStellarItem(TEXT("FireCore"));
		LockerState->TryStoreStellarItem(TEXT("JetTube"));
		TestFalse(TEXT("An uncombined locker attachment cannot reset its allocation"), LockerState->TryApplyStellarPoints(1,
			LockerState->GetShipLockerSlot(1).SlotToken, {}, Table));
		TestTrue(TEXT("Combine for allocation test"), LockerState->TryCombineStellarSlots(0,
			LockerState->GetShipLockerSlot(0).SlotToken, LockerState->GetShipLockerSlot(1).SlotToken, Table));
		const FGuid WeaponToken = LockerState->GetShipLockerSlot(0).SlotToken;
		TestFalse(TEXT("A level 1 core has no points to spend"), LockerState->TryApplyStellarPoints(0, WeaponToken, { 1, 0, 0, 0, 0, 0 }, Table));
		LockerState->RestoreStellarFirmwareUnits(1);
		TestTrue(TEXT("Banked firmware upgrades the core"), LockerState->TryUpgradeStellarCore(0, WeaponToken, Table));
		TestEqual(TEXT("Core reached level 2"), LockerState->GetShipLockerSlot(0).StellarCoreLevel, 2);
		TestFalse(TEXT("A skill cannot exceed ten"), LockerState->TryApplyStellarPoints(0, WeaponToken, { 11, 0, 0, 0, 0, 0 }, Table));
		TestFalse(TEXT("Total cannot exceed the budget"), LockerState->TryApplyStellarPoints(0, WeaponToken, { 1, 1, 0, 0, 0, 0 }, Table));
		TestFalse(TEXT("A stale token is rejected"), LockerState->TryApplyStellarPoints(0, FGuid::NewGuid(), { 1, 0, 0, 0, 0, 0 }, Table));
		TestTrue(TEXT("One point fits the level 2 budget"), LockerState->TryApplyStellarPoints(0, WeaponToken, { 1, 0, 0, 0, 0, 0 }, Table));
		TestTrue(TEXT("Points can be reset for free"), LockerState->TryApplyStellarPoints(0, WeaponToken, {}, Table));
	}

	LockerState->RestoreShipLockerSlots({});
	CarriedInventory->RestoreItems({}, 0);
	TestTrue(TEXT("Core occupies the first slot"), LockerState->TryStoreStellarItem(TEXT("FireCore")));
	TestTrue(TEXT("Wrong attachment occupies the next slot"), LockerState->TryStoreStellarItem(TEXT("HealingTube")));
	TestTrue(TEXT("Matching attachment occupies a later slot"), LockerState->TryStoreStellarItem(TEXT("JetTube")));
	const FJTSShipLockerSlot CoreBeforeMove = LockerState->GetShipLockerSlot(0);
	const FJTSShipLockerSlot WrongAttachment = LockerState->GetShipLockerSlot(1);
	const FJTSShipLockerSlot MatchingAttachment = LockerState->GetShipLockerSlot(2);
	TestFalse(TEXT("A nonadjacent matching attachment cannot combine"),
		LockerState->TryCombineStellarSlots(0, CoreBeforeMove.SlotToken, MatchingAttachment.SlotToken, Table));
	TestFalse(TEXT("An adjacent but wrong attachment cannot combine"),
		LockerState->TryCombineStellarSlots(0, CoreBeforeMove.SlotToken, WrongAttachment.SlotToken, Table));
	TestFalse(TEXT("A stale destination token cannot move an item"),
		LockerState->TryMoveShipLockerSlot(2, MatchingAttachment.SlotToken, 1, FGuid::NewGuid()));
	TestTrue(TEXT("Locker drag can swap accessories into order"),
		LockerState->TryMoveShipLockerSlot(2, MatchingAttachment.SlotToken, 1, WrongAttachment.SlotToken));
	const FJTSShipLockerSlot CoreForAssembly = LockerState->GetShipLockerSlot(0);
	const FJTSShipLockerSlot JetForAssembly = LockerState->GetShipLockerSlot(1);
	TestFalse(TEXT("Moved attachment invalidates its old drag token"),
		LockerState->TryCombineStellarSlots(0, CoreForAssembly.SlotToken, MatchingAttachment.SlotToken, Table));
	TestTrue(TEXT("Adjacent core followed by matching attachment assembles"),
		LockerState->TryCombineStellarSlots(0, CoreForAssembly.SlotToken, JetForAssembly.SlotToken, Table));
	TestEqual(TEXT("Assembled slot records its core"), LockerState->GetShipLockerSlot(0).StellarCoreId, FName(TEXT("FireCore")));
	TestEqual(TEXT("Assembled slot records its attachment"), LockerState->GetShipLockerSlot(0).StellarItemId, FName(TEXT("JetTube")));
	TestTrue(TEXT("Assembly consumes the second slot"), LockerState->GetShipLockerSlot(1).IsEmpty());
	TestFalse(TEXT("An assembled weapon cannot be assembled a second time"),
		LockerState->TryCombineStellarSlots(0, LockerState->GetShipLockerSlot(0).SlotToken,
			LockerState->GetShipLockerSlot(1).SlotToken, Table));
	TestFalse(TEXT("Assembled weapon cannot move to ordinary inventory"),
		LockerState->TryTakeShipLockerItem(0, LockerState->GetShipLockerSlot(0).SlotToken, CarriedInventory, Table));
	TestTrue(TEXT("Rejected assembled transfer keeps the ordinary inventory empty"), CarriedInventory->GetActiveItem().IsEmpty());
	TestEqual(TEXT("Rejected transfer keeps the assembled core"), LockerState->GetShipLockerSlot(0).StellarCoreId, FName(TEXT("FireCore")));
	TestEqual(TEXT("Rejected transfer keeps the assembled attachment"), LockerState->GetShipLockerSlot(0).StellarItemId, FName(TEXT("JetTube")));
	Character->SetActorLocation(Fixture.Ship->GetActorLocation());
	FindFProperty<FObjectProperty>(LockerState->GetClass(), TEXT("PawnPrivate"))->SetObjectPropertyValue_InContainer(LockerState, Character);
	auto* Loadout = LockerState->GetStellarLoadout();
	Loadout->RestoreState({}, 100);
	const FJTSShipLockerSlot Assembled = LockerState->GetShipLockerSlot(0);
	Loadout->ServerExchangeLocker(Fixture.Ship, 0, Assembled.SlotToken, 0, FGuid());
	TestTrue(TEXT("A legacy combined weapon cannot be squeezed into the spare diamond"),
		Loadout->GetSlot(0).IsEmpty() && LockerState->GetShipLockerSlot(0).SlotToken == Assembled.SlotToken);
	Loadout->ServerExchangeLocker(Fixture.Ship, 0, Assembled.SlotToken, 1, FGuid());
	TestTrue(TEXT("Shop weapon moves into the character's vertical equipment pair"),
		LockerState->GetShipLockerSlot(0).IsEmpty() && Loadout->GetWeapons().Num() == 1);
	TestEqual(TEXT("Circle receives the exact original core"), Loadout->GetSlot(1).InstanceId, Assembled.StellarInstanceId);
	TestEqual(TEXT("Diamond receives the exact original attachment"), Loadout->GetSlot(2).InstanceId, Assembled.StellarAttachmentInstanceId);
	Loadout->ServerExchangeLocker(Fixture.Ship, 0, Assembled.SlotToken, 3, FGuid());
	TestTrue(TEXT("Repeated stale shop drag cannot duplicate either item"), Loadout->GetSlot(3).IsEmpty());
	Loadout->ServerExchangeLocker(Fixture.Ship, 0, FGuid(), 1, Loadout->GetSlot(1).InstanceId);
	TestTrue(TEXT("Stellar equipment can return to shop storage"),
		Loadout->GetSlot(1).IsEmpty() && LockerState->GetShipLockerSlot(0).StellarItemId == FName(TEXT("FireCore")));
	TestTrue(TEXT("Returning only the core deactivates its former pair"), Loadout->GetWeapons().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSStellarDebugGrantRegression, "JTS.Spacecraft.StellarDebugGrant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSStellarDebugGrantRegression::RunTest(const FString& Parameters)
{
	FShipTestWorld Fixture;
	AJTSCharacter* Character = nullptr;
	AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(Fixture.AddPlayer(Character, true));
	AJTSPlayerState* const State = Controller ? Controller->GetPlayerState<AJTSPlayerState>() : nullptr;
	const UJTSStellarLootTable* const Table = Fixture.Ship->GetStellarLootTable();
	if (!TestNotNull(TEXT("Debug grant has a player state"), State)
		|| !TestNotNull(TEXT("Debug grant reads the Blueprint-configured pool"), Table)) return false;
	Character->SetActorLocation(Fixture.Ship->GetActorLocation());
	AJTSGameState* const GameState = Fixture.World->SpawnActor<AJTSGameState>();
	Fixture.World->SetGameState(GameState);
	GameState->SetActiveSpacecraft(Fixture.Ship);

	// Duplicate entries and non-weapon materials must not create extra debug grants.
	UJTSStellarLootTable* const DebugTable = DuplicateObject<UJTSStellarLootTable>(Table, Fixture.Ship);
	TSet<FName> ExpectedIds;
	for (const FJTSStellarLootEntry& Entry : Table->Entries)
	{
		if (!Entry.ItemId.IsNone() && Entry.FirmwareUnits == 0 && (Entry.bCore || !Entry.CompatibleCoreId.IsNone()))
			ExpectedIds.Add(Entry.ItemId);
	}
	if (!TestTrue(TEXT("Configured core and attachment set fits in an empty locker"),
		ExpectedIds.Num() > 0 && ExpectedIds.Num() < AJTSPlayerState::ShipLockerCapacity)) return false;
	DebugTable->Entries.Append(Table->Entries);
	FJTSStellarLootEntry Material;
	Material.ItemId = TEXT("DebugNonWeaponMaterial");
	DebugTable->Entries.Add(Material);
	FJTSStellarLootEntry InvalidFirmware;
	InvalidFirmware.ItemId = TEXT("DebugFirmwareWithCoreFlag");
	InvalidFirmware.bCore = true;
	InvalidFirmware.FirmwareUnits = 1;
	DebugTable->Entries.Add(InvalidFirmware);
	FindFProperty<FSoftObjectProperty>(Fixture.Ship->GetClass(), TEXT("StellarLootTable"))
		->SetObjectPropertyValue_InContainer(Fixture.Ship, DebugTable);

	Controller->ServerRequestDebugStellarItems(nullptr);
	TestFalse(TEXT("Invalid ship requests preserve the empty locker"),
		State->GetShipLockerSlots().ContainsByPredicate([](const FJTSShipLockerSlot& Slot) { return !Slot.IsEmpty(); }));
	GameState->SetActiveSpacecraft(nullptr);
	Controller->ServerRequestDebugStellarItems(Fixture.Ship);
	TestFalse(TEXT("RPC rejects a ship outside the active expedition"),
		State->GetShipLockerSlots().ContainsByPredicate([](const FJTSShipLockerSlot& Slot) { return !Slot.IsEmpty(); }));
	GameState->SetActiveSpacecraft(Fixture.Ship);

	// Preserve an existing ordinary item, its identity, and the shared resource wallet.
	TestTrue(TEXT("Existing ordinary item enters the shop locker"),
		State->TryStorePurchasedItem(UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::ShortBlade)));
	const FJTSShipLockerSlot Existing = State->GetShipLockerSlot(0);
	const int32 RockBefore = Fixture.Ship->GetResourceAmount(EJTSResourceType::Rock);
	Controller->ServerRequestDebugStellarItems(Fixture.Ship);
	TestTrue(TEXT("Debug request preserves the existing item token"), State->GetShipLockerSlot(0).SlotToken == Existing.SlotToken);
	TSet<FName> GrantedIds;
	TSet<FGuid> InstanceIds;
	int32 GrantedCount = 0;
	for (const FJTSShipLockerSlot& Slot : State->GetShipLockerSlots())
	{
		if (Slot.StellarItemId.IsNone()) continue;
		++GrantedCount;
		TestTrue(TEXT("Only configured cores and attachments are granted"), ExpectedIds.Contains(Slot.StellarItemId));
		TestFalse(TEXT("Debug items are available immediately without a reveal timer"), Slot.bPendingStellarReveal);
		TestTrue(TEXT("Each debug item has a valid identity and drag token"), Slot.StellarInstanceId.IsValid() && Slot.SlotToken.IsValid());
		GrantedIds.Add(Slot.StellarItemId);
		InstanceIds.Add(Slot.StellarInstanceId);
	}
	TestEqual(TEXT("One item of every eligible kind is granted"), GrantedCount, ExpectedIds.Num());
	TestEqual(TEXT("Duplicate pool rows grant each kind only once"), GrantedIds.Num(), ExpectedIds.Num());
	TestEqual(TEXT("Granted item instances have unique identities"), InstanceIds.Num(), GrantedCount);
	TestEqual(TEXT("Debug grants do not spend ship resources"), Fixture.Ship->GetResourceAmount(EJTSResourceType::Rock), RockBefore);
	TestTrue(TEXT("Debug grants do not equip items on the character"), State->GetStellarLoadout()->GetWeapons().IsEmpty());

	while (State->HasFreeShipLockerSlot()) State->TryStoreStellarItem(*ExpectedIds.CreateConstIterator());
	const int32 FreeIndex = 3;
	State->TryDeleteShipLockerSlot(FreeIndex, State->GetShipLockerSlot(FreeIndex).SlotToken);
	const FGuid PreservedNeighbor = State->GetShipLockerSlot(FreeIndex + 1).SlotToken;
	int32 AddedCount = -1;
	int32 RequestedCount = -1;
	TestTrue(TEXT("Limited capacity accepts the available part of a set"),
		Fixture.Ship->TryGrantDebugStellarItems(Character, AddedCount, RequestedCount));
	TestEqual(TEXT("Exactly one free slot is filled"), AddedCount, 1);
	TestEqual(TEXT("Partial grant reports the full eligible count"), RequestedCount, ExpectedIds.Num());
	TestFalse(TEXT("The hole is filled without overwriting occupied slots"), State->GetShipLockerSlot(FreeIndex).IsEmpty());
	TestTrue(TEXT("Neighbor token is preserved"), State->GetShipLockerSlot(FreeIndex + 1).SlotToken == PreservedNeighbor);
	TestTrue(TEXT("A full locker returns a valid result"), Fixture.Ship->TryGrantDebugStellarItems(Character, AddedCount, RequestedCount));
	TestEqual(TEXT("Full locker grants zero items"), AddedCount, 0);

	State->TryDeleteShipLockerSlot(FreeIndex, State->GetShipLockerSlot(FreeIndex).SlotToken);
	Character->SetActorLocation(Fixture.Ship->GetActorLocation() + FVector(100000.0, 0.0, 0.0));
	Controller->ServerRequestDebugStellarItems(Fixture.Ship);
	TestTrue(TEXT("Out-of-range requests cannot fill locker slots"), State->GetShipLockerSlot(FreeIndex).IsEmpty());
	Character->SetActorLocation(Fixture.Ship->GetActorLocation());
	Fixture.Ship->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Client-role ship cannot mutate the locker"), Fixture.Ship->TryGrantDebugStellarItems(Character, AddedCount, RequestedCount));
	Fixture.Ship->SetRole(ROLE_Authority);
	TestTrue(TEXT("Rejected mutations leave the available slot empty"), State->GetShipLockerSlot(FreeIndex).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSStellarFirmwareDragRegression, "JTS.Spacecraft.StellarFirmwareDrag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSStellarFirmwareDragRegression::RunTest(const FString& Parameters)
{
	FShipTestWorld Fixture;
	AJTSCharacter* Character = nullptr;
	AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(Fixture.AddPlayer(Character, true));
	AJTSPlayerState* const State = Controller ? Controller->GetPlayerState<AJTSPlayerState>() : nullptr;
	const UJTSStellarLootTable* const Table = Fixture.Ship->GetStellarLootTable();
	if (!TestNotNull(TEXT("Drag has a player state"), State) || !TestNotNull(TEXT("Drag has a loot table"), Table)) return false;
	Character->SetActorLocation(Fixture.Ship->GetActorLocation());
	if (!TestTrue(TEXT("Drag can use the ship terminal"), Fixture.Ship->CanUseShipTerminal(Character))) return false;
	const FJTSStellarLootEntry* const Firmware = Table->FindEntry(TEXT("UpgradeFirmware"));
	if (!TestTrue(TEXT("Firmware supplies upgrade units"), Firmware && Firmware->FirmwareUnits > 0)) return false;
	State->TryStoreStellarItem(TEXT("FireCore"));
	State->TryStoreStellarItem(TEXT("UpgradeFirmware"));
	State->TryStoreStellarItem(TEXT("UpgradeFirmware"));
	const FGuid CoreToken = State->GetShipLockerSlot(0).SlotToken;
	const FGuid EarlierFirmwareToken = State->GetShipLockerSlot(1).SlotToken;
	const FGuid DraggedFirmwareToken = State->GetShipLockerSlot(2).SlotToken;

	Controller->ServerMoveShipLockerSlot(Fixture.Ship, 2, FGuid::NewGuid(), 0, CoreToken);
	Controller->ServerMoveShipLockerSlot(Fixture.Ship, 2, DraggedFirmwareToken, 0, FGuid::NewGuid());
	TestEqual(TEXT("Stale drag tokens cannot upgrade"), State->GetShipLockerSlot(0).StellarCoreLevel, 1);
	TestTrue(TEXT("Failed drags preserve the firmware"), State->GetShipLockerSlot(2).SlotToken == DraggedFirmwareToken);

	Controller->ServerMoveShipLockerSlot(Fixture.Ship, 2, DraggedFirmwareToken, 0, CoreToken);
	TestEqual(TEXT("Dropping firmware raises the core one level"), State->GetShipLockerSlot(0).StellarCoreLevel, 2);
	TestTrue(TEXT("Drop consumes the dragged firmware"), State->GetShipLockerSlot(2).IsEmpty());
	TestTrue(TEXT("Drop preserves earlier firmware slots"), State->GetShipLockerSlot(1).SlotToken == EarlierFirmwareToken);
	TestEqual(TEXT("Unused firmware units are banked"), State->GetStellarFirmwareUnits(),
		Firmware->FirmwareUnits - FJTSStellarProgression::GetUpgradeUnitCost(1));
	Controller->ServerMoveShipLockerSlot(Fixture.Ship, 2, DraggedFirmwareToken, 0, CoreToken);
	TestEqual(TEXT("A repeated drop cannot upgrade twice"), State->GetShipLockerSlot(0).StellarCoreLevel, 2);

	FJTSShipLockerSlot Core = State->GetShipLockerSlot(0);
	const FJTSShipLockerSlot RemainingFirmware = State->GetShipLockerSlot(1);
	Core.StellarCoreLevel = FJTSStellarProgression::MaxCoreLevel;
	State->RestoreShipLockerSlots({ Core, RemainingFirmware });
	Controller->ServerMoveShipLockerSlot(Fixture.Ship, 1, EarlierFirmwareToken, 0, CoreToken);
	TestTrue(TEXT("A max-level core does not consume firmware"), State->GetShipLockerSlot(1).SlotToken == EarlierFirmwareToken);
	Core.StellarCoreLevel = 51;
	State->RestoreShipLockerSlots({ Core, RemainingFirmware });
	State->RestoreStellarFirmwareUnits(0);
	Controller->ServerMoveShipLockerSlot(Fixture.Ship, 1, EarlierFirmwareToken, 0, CoreToken);
	TestEqual(TEXT("Insufficient firmware leaves the core unchanged"), State->GetShipLockerSlot(0).StellarCoreLevel, 51);
	TestTrue(TEXT("Insufficient firmware leaves the item unchanged"), State->GetShipLockerSlot(1).SlotToken == EarlierFirmwareToken);

	Core.StellarCoreLevel = 1;
	Core.StellarItemId = TEXT("JetTube");
	Core.StellarCoreId = TEXT("FireCore");
	State->RestoreShipLockerSlots({ Core, RemainingFirmware });
	Character->SetActorLocation(Fixture.Ship->GetActorLocation() + FVector(1000000.0, 0.0, 0.0));
	Controller->ServerMoveShipLockerSlot(Fixture.Ship, 1, EarlierFirmwareToken, 0, CoreToken);
	TestEqual(TEXT("An out-of-range player cannot upgrade"), State->GetShipLockerSlot(0).StellarCoreLevel, 1);
	Character->SetActorLocation(Fixture.Ship->GetActorLocation());
	Controller->ServerMoveShipLockerSlot(Fixture.Ship, 1, EarlierFirmwareToken, 0, CoreToken);
	TestEqual(TEXT("A combined weapon's core can receive a firmware drop"), State->GetShipLockerSlot(0).StellarCoreLevel, 2);
	TestEqual(TEXT("The combined attachment stays equipped"), State->GetShipLockerSlot(0).StellarItemId, FName(TEXT("JetTube")));
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
		TestTrue(TEXT("Character movement accepts the configured 50 degree walkable slope"),
			Character->GetCharacterMovement()->GetWalkableFloorAngle() >= 49.99f
			&& Character->GetCharacterMovement()->GetWalkableFloorAngle() <= 50.01f);
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
	TestTrue(TEXT("Landed driver R input restores original character possession"), Controllers[0]->GetPawn() == Characters[0]);
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
	APlayerController* DriverController = Fixture.AddPlayer(DriverCharacter, true);
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
	const bool bHeadlightKeyMapped = Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
		[&Fixture](const FEnhancedActionKeyMapping& Mapping)
		{
			return Mapping.Action == Fixture.Ship->FlightHeadlightAction && Mapping.Key == EKeys::L;
		});
	TestTrue(TEXT("L remains mapped to headlights"), bHeadlightKeyMapped);
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
	TestTrue(TEXT("Arrow keys provide direct hull steering"), bArrowKeysSteerTheHull);

	const FQuat RotationBeforeMouseLook = Fixture.Ship->GetActorQuat();
	const float QuarterTurnMouseInput = 90.0f / FMath::Max(Fixture.Ship->FlightCameraLookSensitivity, KINDA_SMALL_NUMBER);
	Fixture.Ship->FlightLookYaw(FInputActionValue(QuarterTurnMouseInput));
	TestTrue(TEXT("Orbiting the camera does not steer the spacecraft"),
		Fixture.Ship->GetActorQuat().Equals(RotationBeforeMouseLook, 0.0001f));

	const bool bSixAxisKeysMapped = Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
		[&Fixture](const FEnhancedActionKeyMapping& Mapping)
		{
			return Mapping.Action == Fixture.Ship->FlightRightAction && Mapping.Key == EKeys::D;
		})
		&& Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
			[&Fixture](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == Fixture.Ship->FlightRightAction && Mapping.Key == EKeys::A;
			})
		&& Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
			[&Fixture](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == Fixture.Ship->FlightTurnAroundAction && Mapping.Key == EKeys::T;
			})
		&& Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
			[&Fixture](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == Fixture.Ship->FlightRollAction && Mapping.Key == EKeys::Q;
			})
		&& Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
			[&Fixture](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == Fixture.Ship->FlightFreeLookAction && Mapping.Key == EKeys::LeftAlt;
			});
	TestTrue(TEXT("Strafe, roll, turnaround and free-look controls are mapped"), bSixAxisKeysMapped);
	const bool bLandedDisembarkKeyMapped = Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
		[&Fixture](const FEnhancedActionKeyMapping& Mapping)
		{
			return Mapping.Action == Fixture.Ship->FlightDisembarkAction && Mapping.Key == EKeys::R;
		});
	TestTrue(TEXT("R remains mapped to landed disembark"), bLandedDisembarkKeyMapped);
	const bool bRetiredFlightKeyMapped = Fixture.Ship->FlightInputMappingContext->GetMappings().ContainsByPredicate(
		[](const FEnhancedActionKeyMapping& Mapping)
		{
			return Mapping.Key == EKeys::V || Mapping.Key == EKeys::C
				|| Mapping.Key == EKeys::H;
		});
	TestFalse(TEXT("V, C and H no longer trigger spacecraft flight actions"), bRetiredFlightKeyMapped);
	Fixture.Ship->FlightDisembarkReleased(FInputActionValue(false));
	Fixture.Ship->FlightDisembarkStarted(FInputActionValue(true));
	TestFalse(TEXT("R cannot request disembark while flying"), Fixture.Ship->bDisembarkRequestPending);
	TestTrue(TEXT("R while flying leaves the driver aboard"), DriverController->GetPawn() == Fixture.Ship);
	FJTSSpacecraftInputState RetiredOptions = Fixture.Ship->LocalFlightInput;
	RetiredOptions.bFlightAssistEnabled = false;
	RetiredOptions.SpeedLimit = 0.25f;
	RetiredOptions.bBraking = true;
	Fixture.Ship->ApplyFlightInputOnServer(RetiredOptions);
	TestTrue(TEXT("Server keeps full speed and flight assist even for retired RPC options"),
		Movement->IsFlightAssistEnabled() && FMath::IsNearlyEqual(Movement->GetSpeedLimit(), 1.0f));
	Fixture.Ship->FlightFreeLookStarted(FInputActionValue(true));
	const float OriginalCameraArmLength = Fixture.Ship->GetFlightCameraBoom()->TargetArmLength;
	Fixture.Ship->FlightCameraZoom(FInputActionValue(-1.0f));
	TestTrue(TEXT("Alt and wheel no longer changes speed or camera distance"),
		FMath::IsNearlyEqual(Fixture.Ship->GetFlightCameraBoom()->TargetArmLength, OriginalCameraArmLength)
		&& FMath::IsNearlyEqual(Movement->GetSpeedLimit(), 1.0f)
		&& FMath::IsNearlyEqual(Fixture.Ship->GetFlightSpeedLimit(), 1.0f));
	Fixture.Ship->FlightFreeLookStopped(FInputActionValue(false));
	Fixture.Ship->FlightCameraZoom(FInputActionValue(-1.0f));
	TestTrue(TEXT("Wheel alone still zooms the flight camera"),
		Fixture.Ship->GetFlightCameraBoom()->TargetArmLength > OriginalCameraArmLength);
	Fixture.Ship->FlightCameraZoom(FInputActionValue(1.0f));
	Fixture.Ship->FlightMoveRight(FInputActionValue(1.0f));
	TestEqual(TEXT("Lateral input reaches driver intent"), Fixture.Ship->LocalFlightInput.MoveRight, 1.0f);
	Fixture.Ship->FlightMoveRight(FInputActionValue(0.0f));
	Fixture.Ship->FlightMoveForward(FInputActionValue(-1.0f));
	TestEqual(TEXT("Reverse input remains signed"), Fixture.Ship->LocalFlightInput.MoveForward, -1.0f);
	Fixture.Ship->FlightMoveForward(FInputActionValue(0.0f));
	Fixture.Ship->FlightRecenterCamera(FInputActionValue(true));
	Fixture.Ship->Tick(1.0f);
	Fixture.Ship->FlightLookYaw(FInputActionValue(25.0f / Fixture.Ship->FlightCameraLookSensitivity));
	Fixture.Ship->Tick(0.06f);
	TestTrue(TEXT("Mouse camera lead commands a bounded hull yaw"),
		Fixture.Ship->LocalFlightInput.Yaw > 0.1f && Fixture.Ship->LocalFlightInput.Yaw <= 1.0f);
	Fixture.Ship->FlightFreeLookStarted(FInputActionValue(true));
	Fixture.Ship->FlightLookYaw(FInputActionValue(20.0f / Fixture.Ship->FlightCameraLookSensitivity));
	Fixture.Ship->FlightMoveForward(FInputActionValue(1.0f));
	Fixture.Ship->FlightMoveRight(FInputActionValue(1.0f));
	DriverController->SetIgnoreLookInput(true);
	Fixture.Ship->Tick(0.06f);
	TestTrue(TEXT("Alt free look and ignored look input preserve WASD thrust"),
		Fixture.Ship->LocalFlightInput.MoveForward > 0.99f
		&& Fixture.Ship->LocalFlightInput.MoveRight > 0.99f
		&& Fixture.Ship->IsFlightFreeLooking());
	const FVector ThrustForward = Fixture.Ship->GetActorForwardVector();
	const FVector ThrustRight = Fixture.Ship->GetActorRightVector();
	const FVector ThrustStart = Fixture.Ship->GetActorLocation();
	Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	const FVector ThrustDelta = Fixture.Ship->GetActorLocation() - ThrustStart;
	TestTrue(TEXT("Alt plus WASD translates on the hull's own forward/right axes"),
		FVector::DotProduct(ThrustDelta, ThrustForward) > 5.0f
		&& FVector::DotProduct(ThrustDelta, ThrustRight) > 5.0f);
	DriverController->SetIgnoreLookInput(false);
	Fixture.Ship->FlightMoveForward(FInputActionValue(0.0f));
	Fixture.Ship->FlightMoveRight(FInputActionValue(0.0f));
	Movement->StopMovementImmediately();
	Fixture.Ship->FlightSteerYaw(FInputActionValue(1.0f));
	const FQuat HullBeforeFreeLookTurn = Fixture.Ship->GetActorQuat();
	const FVector FreeLookForward = Fixture.Ship->FlightCameraAimRotation.GetForwardVector();
	Fixture.Ship->SetActorRotation(FQuat(Fixture.Ship->GetActorUpVector(), FMath::DegreesToRadians(12.0f))
		* HullBeforeFreeLookTurn);
	Fixture.Ship->UpdateFlightCameraFrame(1.0f / 60.0f);
	TestTrue(TEXT("Held free look keeps the view fixed while the hull turns"),
		!Fixture.Ship->bFlightCameraRecentering
		&& FVector::DotProduct(Fixture.Ship->FlightCameraAimRotation.GetForwardVector(), FreeLookForward) > 0.9999f);
	Fixture.Ship->SetActorRotation(HullBeforeFreeLookTurn);
	Fixture.Ship->UpdateFlightCameraFrame(1.0f / 60.0f);
	Fixture.Ship->FlightSteerYaw(FInputActionValue(0.0f));
	Fixture.Ship->Tick(0.06f);
	TestEqual(TEXT("Held free look sends no mouse steering"), Fixture.Ship->LocalFlightInput.Yaw, 0.0f);
	Fixture.Ship->FlightFreeLookStopped(FInputActionValue(false));
	Fixture.Ship->Tick(1.0f);
	TestTrue(TEXT("Releasing free look recentres without a steering command"),
		FMath::IsNearlyZero(Fixture.Ship->LocalFlightInput.Yaw));

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
	TestTrue(TEXT("Positive direct yaw turns the hull right"), YawedRight > 0.2f);
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
	TestTrue(TEXT("Negative direct yaw turns the hull left"), YawedLeft < -0.2f);
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
		TestTrue(TEXT("Turnaround keeps the deck plane fixed while the nose swings"),
			FVector::DotProduct(Fixture.Ship->GetActorUpVector(), TurnAroundDeckUp) > 0.999f);
	}
	const FVector TurnedInDeck = FVector::VectorPlaneProject(
		Fixture.Ship->GetActorForwardVector(),
		TurnAroundDeckUp).GetSafeNormal();
	TestTrue(TEXT("Turnaround points the nose toward the old tail"),
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
	TestTrue(TEXT("Holding turnaround after the swap does not keep spinning"),
		Fixture.Ship->GetActorQuat().AngularDistance(HeldTurnRotation) < 0.05f);
	Fixture.Ship->FlightTurnAround(FInputActionValue(false));

	// A level start hides a turnaround that flattens onto radial up. Pitch the hull first, then T
	// must yaw inside that tilted deck instead of rolling the belly over.
	Fixture.Ship->FlightSteerPitch(FInputActionValue(1.0f));
	for (int32 PitchStep = 0; PitchStep < 5; ++PitchStep)
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
	Fixture.Ship->FlightSteerPitch(FInputActionValue(0.0f));
	Movement->StopMovementImmediately();
	const FVector CameraTestUp = Fixture.Ship->GetFlightReferenceUp();
	const FVector CameraTestForward = FVector::VectorPlaneProject(
		Fixture.Ship->GetActorForwardVector(), CameraTestUp).GetSafeNormal();
	Fixture.Ship->SetActorRotation(FRotationMatrix::MakeFromXZ(CameraTestForward, CameraTestUp).ToQuat());
	Fixture.Ship->FlightRecenterCamera(FInputActionValue(true));
	Fixture.Ship->Tick(1.0f);
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
	for (int32 DiveStep = 0; DiveStep < 12; ++DiveStep)
	{
		Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	}
	const float HullDiveComponent = FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), DiveUp);
	AddInfo(FString::Printf(
		TEXT("Surface pitch authority: assist=%.4f hullVertical=%.4f"),
		Movement->GetSurfaceFlightAssistAlpha(),
		HullDiveComponent));
	TestTrue(TEXT("Near-terrain pitch follows the pilot instead of leveling the hull"),
		HullDiveComponent < -0.35f
		&& FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), DiveStartForward) < 0.85f);
	const float PitchDownMouseInput = -60.0f / FMath::Max(Fixture.Ship->FlightCameraLookSensitivity, KINDA_SMALL_NUMBER);
	const FQuat AttitudeBeforeDiveCameraLook = Fixture.Ship->GetActorQuat();
	const FVector CameraBeforeDiveLook = Fixture.Ship->GetFlightCameraForward(DiveUp);
	Fixture.Ship->FlightLookPitch(FInputActionValue(PitchDownMouseInput));
	const FVector DiveCameraForward = Fixture.Ship->GetFlightCameraForward(DiveUp);
	TestTrue(TEXT("Camera pitch remains independent of the unrestricted hull attitude"),
		!DiveCameraForward.Equals(CameraBeforeDiveLook, 0.01f)
		&& Fixture.Ship->GetActorQuat().Equals(AttitudeBeforeDiveCameraLook, 0.0001f));

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
	Movement->SetSteeringInput(FVector2D::ZeroVector);
	Movement->SetMoveInput(FVector2D(0.0f, 1.0f));
	const FVector DepartureStart = Fixture.Ship->GetActorLocation();
	for (int32 DepartureStep = 0; DepartureStep < 30; ++DepartureStep)
	{
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	}
	AddInfo(FString::Printf(TEXT("Surface climb: assist=%.3f vertical=%.3f displacement=%.1f speed=%.1f"),
		Movement->GetSurfaceFlightAssistAlpha(),
		FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), DepartureUp),
		FVector::DotProduct(Fixture.Ship->GetActorLocation() - DepartureStart, DepartureUp),
		Movement->GetCurrentSpeed()));
	TestTrue(TEXT("Nose-up hull thrust climbs while terrain clearance remains active"),
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSSixAxisFlightRegression, "JTS.Spacecraft.SixAxisInertialFlight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSSixAxisFlightRegression::RunTest(const FString& Parameters)
{
	FShipTestWorld Fixture;
	if (!TestTrue(TEXT("Six-axis flight begins from a parked real surface"), Fixture.Park(FVector::UpVector))
		|| !TestTrue(TEXT("Six-axis flight takes off"), Fixture.Ship->BeginSurfaceTakeoff())) return false;
	UJTSSpacecraftFlightMovementComponent* const Movement = Fixture.Ship->GetFlightMovementComponent();
	Fixture.Ship->SetFlightTargetPlanet(nullptr);
	Fixture.Ship->SetActorLocation(FVector(500000.0f, 500000.0f, 500000.0f));
	Fixture.Ship->SetActorRotation(FQuat::Identity);
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Deep space has no surface attitude assist"), Movement->IsUsingPlanetSurfaceFlightFrame());

	const FVector StrafeStart = Fixture.Ship->GetActorLocation();
	Movement->SetMoveInput(FVector2D(1.0f, 0.0f));
	Movement->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Positive lateral thrust moves toward ship right"),
		FVector::DotProduct(Fixture.Ship->GetActorLocation() - StrafeStart, FVector::RightVector) > 20.0f);
	Movement->StopMovementImmediately();
	const FVector ReverseStart = Fixture.Ship->GetActorLocation();
	Movement->SetMoveInput(FVector2D(0.0f, -1.0f));
	Movement->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Signed reverse thrust moves toward the tail"),
		FVector::DotProduct(Fixture.Ship->GetActorLocation() - ReverseStart, -FVector::ForwardVector) > 20.0f);
	Movement->SetMoveInput(FVector2D::ZeroVector);
	Movement->StopMovementImmediately();
	Movement->SetRollInput(1.0f);
	for (int32 Step = 0; Step < 12; ++Step) Movement->TickComponent(0.05f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Deep-space roll banks the actual hull"),
		FVector::DotProduct(Fixture.Ship->GetActorUpVector(), FVector::UpVector) < 0.9f);
	Movement->SetRollInput(0.0f);
	Movement->SetSpeedLimit(0.25f);
	Movement->StopMovementImmediately();
	Movement->SetMoveInput(FVector2D(0.0f, 1.0f));
	Movement->TickComponent(1.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Precision speed limit caps the commanded forward velocity"),
		Movement->GetCurrentSpeed() <= Movement->GetEffectiveStats().MaxMoveSpeed * 0.25f + 1.0f);
	Movement->SetSpeedLimit(1.0f);
	Movement->SetFlightAssistEnabled(false);
	Movement->StopMovementImmediately();
	Movement->SetMoveInput(FVector2D(0.0f, 1.0f));
	for (int32 Step = 0; Step < 6; ++Step) Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	const FVector CoastingVelocity = Movement->Velocity;
	Movement->SetMoveInput(FVector2D::ZeroVector);
	Movement->SetSteeringInput(FVector2D(1.0f, 0.0f));
	for (int32 Step = 0; Step < 5; ++Step) Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Inertial flight preserves velocity while changing attitude"),
		Movement->Velocity.Equals(CoastingVelocity, 1.0f)
		&& Fixture.Ship->GetActorForwardVector().Dot(CoastingVelocity.GetSafeNormal()) < 0.98f);
	Movement->SetBraking(true);
	Movement->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Brake arrests momentum even with flight assist disabled"),
		Movement->GetCurrentSpeed() < CoastingVelocity.Size());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSSurfaceAttitudeRegression, "JTS.Spacecraft.TerrainIndependentAttitude",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSSurfaceAttitudeRegression::RunTest(const FString& Parameters)
{
	FShipTestWorld Fixture;
	if (!TestTrue(TEXT("Terrain-attitude test parks the ship"), Fixture.Park(FVector::UpVector))) return false;
	AJTSCharacter* Driver = nullptr;
	APlayerController* const DriverController = Fixture.AddPlayer(Driver, true);
	if (!TestTrue(TEXT("Terrain-attitude test boards a local pilot"), Fixture.Ship->TryBoardPlayer(Driver))
		|| !TestTrue(TEXT("Terrain-attitude test takes off"), Fixture.Ship->BeginSurfaceTakeoff())) return false;
	AStaticMeshActor* const Surface = Cast<AStaticMeshActor>(Fixture.Planet->GetGameplaySurfaceActor());
	if (!TestNotNull(TEXT("Terrain-attitude test has a real collision mesh"), Surface)) return false;
	Surface->GetStaticMeshComponent()->SetStaticMesh(
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Surface->SetActorScale3D(FVector(200.0f));
	const FVector RadialUp = FVector(0.4f, 0.3f, 0.866f).GetSafeNormal();
	FJTSPlanetSurfaceFrame TerrainFrame;
	if (!TestTrue(TEXT("Uneven mesh resolves a surface frame"),
		Fixture.Planet->GetSurfaceFrameAt(RadialUp * 20000.0f, FVector::ForwardVector, TerrainFrame))) return false;
	TestTrue(TEXT("Terrain normal differs from radial up"),
		FVector::DotProduct(TerrainFrame.Up, RadialUp) < 0.98f);
	UJTSSpacecraftFlightMovementComponent* const Movement = Fixture.Ship->GetFlightMovementComponent();
	const FQuat PilotAttitude = FRotator(25.0f, 15.0f, 30.0f).Quaternion();
	Fixture.Ship->SetActorRotation(PilotAttitude);
	Movement->ClearInput();
	Fixture.Ship->ActivateFlightCameraThirdPerson();
	Fixture.Ship->Tick(2.0f);
	const FVector PilotViewForward = DriverController->GetControlRotation().Vector();
	for (const float Height : {2200.0f, 2600.0f, 2900.0f, 3200.0f})
	{
		Fixture.Ship->SetActorLocation(TerrainFrame.Location + RadialUp * Height);
		Movement->StopMovementImmediately();
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
		Fixture.Ship->Tick(1.0f / 60.0f);
		TestTrue(TEXT("Crossing the surface-flight band keeps pilot attitude unchanged"),
			Fixture.Ship->GetActorQuat().AngularDistance(PilotAttitude) < 0.001f);
		TestTrue(TEXT("Crossing the surface-flight band keeps the pilot view steady"),
			FVector::DotProduct(DriverController->GetControlRotation().Vector(), PilotViewForward) > 0.9999f);
	}
	const FVector OtherRadial = FVector(0.866f, 0.3f, 0.4f).GetSafeNormal();
	FJTSPlanetSurfaceFrame OtherFrame;
	if (!TestTrue(TEXT("Second terrain face resolves a surface frame"),
		Fixture.Planet->GetSurfaceFrameAt(OtherRadial * 20000.0f, FVector::ForwardVector, OtherFrame))) return false;
	TestTrue(TEXT("Test terrain faces have different normals"),
		FVector::DotProduct(TerrainFrame.Up, OtherFrame.Up) < 0.5f);
	Fixture.Ship->SetActorLocation(OtherFrame.Location + OtherRadial * 2200.0f);
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	Fixture.Ship->Tick(1.0f / 60.0f);
	TestTrue(TEXT("A different terrain normal does not reorient the ship or camera"),
		Fixture.Ship->GetActorQuat().AngularDistance(PilotAttitude) < 0.001f
		&& FVector::DotProduct(DriverController->GetControlRotation().Vector(), PilotViewForward) > 0.9999f);
	const FVector DescentForward = FVector::VectorPlaneProject(FVector::UpVector, OtherRadial).GetSafeNormal();
	Fixture.Ship->SetActorRotation(FRotationMatrix::MakeFromXZ(DescentForward, OtherRadial).ToQuat());
	FBoolProperty* const SurfaceFrameSetting = FindFProperty<FBoolProperty>(
		Movement->GetClass(), TEXT("bUsePlanetSurfaceFlightFrame"));
	if (!TestNotNull(TEXT("Surface-frame option is configurable"), SurfaceFrameSetting)) return false;
	SurfaceFrameSetting->SetPropertyValue_InContainer(Movement, false);
	TestFalse(TEXT("Pilot can disable radial lift without disabling clearance"),
		Movement->IsUsingPlanetSurfaceFlightFrame());
	Movement->SetVerticalInput(-1.0f);
	for (int32 DescentStep = 0; DescentStep < 240; ++DescentStep)
	{
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	}
	Movement->SetVerticalInput(0.0f);
	float ProtectedAltitude = 0.0f;
	const float HullClearance = Fixture.Ship->GetLandingCollisionClearance(OtherRadial);
	const bool bResolvedProtectedAltitude = Movement->GetResolvedSurfaceAltitude(Fixture.Planet, ProtectedAltitude);
	AddInfo(FString::Printf(TEXT("Uneven terrain descent: altitude=%.1f clearance=%.1f"),
		ProtectedAltitude, HullClearance));
	TestTrue(TEXT("Uneven terrain stops descent before the physical hull reaches the mesh"),
		bResolvedProtectedAltitude
		&& ProtectedAltitude >= HullClearance + 60.0f
		&& ProtectedAltitude < 2150.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSNosePitchGuardRegression, "JTS.Spacecraft.NearSurfaceNosePitchGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSNosePitchGuardRegression::RunTest(const FString& Parameters)
{
	FShipTestWorld Fixture;
	if (!TestTrue(TEXT("Nose-guard test parks above a real surface"), Fixture.Park(FVector::UpVector))) return false;
	AJTSCharacter* Driver = nullptr;
	APlayerController* const DriverController = Fixture.AddPlayer(Driver, true);
	if (!TestTrue(TEXT("Nose-guard test boards a local pilot"), Fixture.Ship->TryBoardPlayer(Driver))
		|| !TestTrue(TEXT("Nose-guard test takes off"), Fixture.Ship->BeginSurfaceTakeoff())) return false;
	UJTSSpacecraftFlightMovementComponent* const Movement = Fixture.Ship->GetFlightMovementComponent();
	AStaticMeshActor* const Surface = Cast<AStaticMeshActor>(Fixture.Planet->GetGameplaySurfaceActor());
	if (!TestNotNull(TEXT("Nose-guard test has a real collision mesh"), Surface)) return false;
	Surface->GetStaticMeshComponent()->SetStaticMesh(
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Surface->SetActorScale3D(FVector(200.0f));
	FJTSPlanetSurfaceHit GroundHit;
	if (!TestTrue(TEXT("Nose-guard test resolves the mesh"),
		Fixture.Planet->TraceToSurface(FVector::UpVector * 12000.0f, GroundHit))) return false;
	const FQuat LevelRotation = FRotationMatrix::MakeFromXZ(
		FVector::ForwardVector, FVector::UpVector).ToQuat();
	Fixture.Ship->SetActorRotation(LevelRotation);
	const float LowAltitude = Fixture.Ship->GetLandingCollisionClearance(FVector::UpVector) + 220.0f;
	Fixture.Ship->SetActorLocation(GroundHit.ImpactPoint + FVector::UpVector * LowAltitude);
	Movement->ClearInput();
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	Fixture.Ship->ActivateFlightCameraThirdPerson();
	Fixture.Ship->Tick(1.0f);
	Fixture.Ship->FlightLookPitch(FInputActionValue(
		-50.0f / FMath::Max(Fixture.Ship->FlightCameraLookSensitivity, KINDA_SMALL_NUMBER)));
	Fixture.Ship->Tick(0.06f);
	TestTrue(TEXT("Looking down without Alt still requests hull pitch"),
		Fixture.Ship->LocalFlightInput.Pitch < -0.1f);
	for (int32 Step = 0; Step < 12; ++Step)
	{
		Movement->TickComponent(0.05f, LEVELTICK_All, nullptr);
		Fixture.Ship->Tick(0.05f);
	}
	AddInfo(FString::Printf(TEXT("Near-surface pitch: camera=%.3f nose=%.3f altitude=%.1f"),
		FVector::DotProduct(DriverController->GetControlRotation().Vector(), FVector::UpVector),
		FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), FVector::UpVector),
		FVector::Distance(Fixture.Ship->GetActorLocation(), GroundHit.ImpactPoint)));
	TestTrue(TEXT("Near the surface the camera can look down while the nose stays at terrain pitch"),
		FVector::DotProduct(DriverController->GetControlRotation().Vector(), FVector::UpVector) < -0.4f
		&& FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), FVector::UpVector) > -0.03f);
	// A held down command must settle against the same safety pitch used by automatic
	// recovery. Different floors caused repeated small corrections even on a flat mesh.
	Fixture.Ship->SetActorRotation(FRotationMatrix::MakeFromXZ(
		FVector(1.0f, 0.0f, 0.18f).GetSafeNormal(), FVector::UpVector).ToQuat());
	Movement->StopMovementImmediately();
	Movement->SetSteeringInput(FVector2D(0.0f, -1.0f));
	float LowestSettledPitch = BIG_NUMBER;
	float HighestSettledPitch = -BIG_NUMBER;
	for (int32 Step = 0; Step < 120; ++Step)
	{
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
		if (Step >= 60)
		{
			const float PitchDegrees = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(
				FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), FVector::UpVector), -1.0f, 1.0f)));
			LowestSettledPitch = FMath::Min(LowestSettledPitch, PitchDegrees);
			HighestSettledPitch = FMath::Max(HighestSettledPitch, PitchDegrees);
		}
	}
	TestTrue(TEXT("Held nose-down input settles above flat terrain without repeated pitch corrections"),
		LowestSettledPitch >= 1.0f && HighestSettledPitch - LowestSettledPitch < 1.0f);
	AddInfo(FString::Printf(TEXT("Settled flat-terrain pitch: min=%.2f max=%.2f"),
		LowestSettledPitch, HighestSettledPitch));
	const FFloatProperty* const SafetyMarginProperty = FindFProperty<FFloatProperty>(
		Movement->GetClass(), TEXT("SurfaceClearanceSafetyMargin"));
	const FFloatProperty* const GuardDistanceProperty = FindFProperty<FFloatProperty>(
		Movement->GetClass(), TEXT("SurfaceNosePitchGuardDistance"));
	const FFloatProperty* const ReleaseDistanceProperty = FindFProperty<FFloatProperty>(
		Movement->GetClass(), TEXT("SurfaceNosePitchReleaseDistance"));
	if (!TestNotNull(TEXT("Surface safety margin is configurable"), SafetyMarginProperty)
		|| !TestNotNull(TEXT("Nose guard distance is configurable"), GuardDistanceProperty)
		|| !TestNotNull(TEXT("Nose guard release distance is configurable"), ReleaseDistanceProperty)) return false;
	const float EdgeDither = FMath::Max(1.0f, FMath::Min(25.0f,
		ReleaseDistanceProperty->GetPropertyValue_InContainer(Movement) * 0.25f));
	float LowestThresholdPitch = BIG_NUMBER;
	float HighestThresholdPitch = -BIG_NUMBER;
	for (int32 Step = 0; Step < 24; ++Step)
	{
		const float GuardEdgeAltitude = Fixture.Ship->GetLandingCollisionClearance(FVector::UpVector)
			+ SafetyMarginProperty->GetPropertyValue_InContainer(Movement)
			+ GuardDistanceProperty->GetPropertyValue_InContainer(Movement);
		Fixture.Ship->SetActorLocation(GroundHit.ImpactPoint + FVector::UpVector
			* (GuardEdgeAltitude + (Step % 2 == 0 ? EdgeDither : -EdgeDither)));
		Movement->TickComponent(0.05f, LEVELTICK_All, nullptr);
		const float PitchDegrees = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(
			FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), FVector::UpVector), -1.0f, 1.0f)));
		LowestThresholdPitch = FMath::Min(LowestThresholdPitch, PitchDegrees);
		HighestThresholdPitch = FMath::Max(HighestThresholdPitch, PitchDegrees);
	}
	TestTrue(TEXT("Crossing the surface guard edge repeatedly does not pump the nose"),
		LowestThresholdPitch >= 1.0f && HighestThresholdPitch - LowestThresholdPitch < 1.0f);

	// The same held input must regain full pitch authority after climbing away from the mesh.
	Fixture.Ship->SetActorLocation(GroundHit.ImpactPoint + FVector::UpVector * 2500.0f);
	Fixture.Ship->SetActorRotation(LevelRotation);
	Movement->StopMovementImmediately();
	Movement->SetSteeringInput(FVector2D(0.0f, -1.0f));
	for (int32 Step = 0; Step < 8; ++Step)
	{
		Movement->TickComponent(0.05f, LEVELTICK_All, nullptr);
	}
	TestTrue(TEXT("Away from terrain the pilot can pitch the nose below the horizon"),
		FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), FVector::UpVector) < -0.3f);

	// A tilted radial direction over a flat cube face gives a genuine non-radial ground slope.
	const FVector RadialUp = FVector(0.4f, 0.0f, 0.916515f).GetSafeNormal();
	FJTSPlanetSurfaceFrame TerrainFrame;
	if (!TestTrue(TEXT("Slope test resolves the uneven terrain face"),
		Fixture.Planet->GetSurfaceFrameAt(RadialUp * 20000.0f, FVector::ForwardVector, TerrainFrame))) return false;
	const FQuat TerrainParallelRotation = FRotationMatrix::MakeFromXZ(
		FVector::ForwardVector, TerrainFrame.Up).ToQuat();
	Fixture.Ship->SetActorRotation(TerrainParallelRotation);
	const float SlopedLowAltitude = Fixture.Ship->GetLandingCollisionClearance(RadialUp) + 220.0f;
	Fixture.Ship->SetActorLocation(TerrainFrame.Location + RadialUp * SlopedLowAltitude);
	Movement->ClearInput();
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	Movement->SetSteeringInput(FVector2D(0.0f, -1.0f));
	for (int32 Step = 0; Step < 12; ++Step)
	{
		Movement->TickComponent(0.05f, LEVELTICK_All, nullptr);
	}
	TestTrue(TEXT("A non-radial surface stops the nose before it dips below its local slope"),
		FVector::DotProduct(Fixture.Ship->GetActorForwardVector(), TerrainFrame.Up) > -0.03f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSPredictiveTerrainAvoidanceRegression,
	"JTS.Spacecraft.PredictiveTerrainAvoidance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSPredictiveTerrainAvoidanceRegression::RunTest(const FString& Parameters)
{
	FShipTestWorld Fixture;
	if (!TestTrue(TEXT("Terrain-avoidance test parks on a real surface"), Fixture.Park(FVector::UpVector))) return false;
	AJTSCharacter* Driver = nullptr;
	APlayerController* const DriverController = Fixture.AddPlayer(Driver, true);
	if (!TestTrue(TEXT("Terrain-avoidance test boards the pilot"), Fixture.Ship->TryBoardPlayer(Driver))
		|| !TestTrue(TEXT("Terrain-avoidance test takes off"), Fixture.Ship->BeginSurfaceTakeoff())) return false;
	AStaticMeshActor* const OldSurface = Cast<AStaticMeshActor>(Fixture.Planet->GetGameplaySurfaceActor());
	if (!TestNotNull(TEXT("Test has an original surface"), OldSurface)) return false;
	OldSurface->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// A shallow valley floor transitions into a rising ramp. Both boxes are real collision surfaces
	// owned by the planet, so the probe and hull sweep see the same geometry.
	AActor* const Terrain = Fixture.World->SpawnActor<AActor>();
	USceneComponent* const TerrainRoot = NewObject<USceneComponent>(Terrain, TEXT("TerrainRoot"));
	Terrain->AddInstanceComponent(TerrainRoot);
	Terrain->SetRootComponent(TerrainRoot);
	TerrainRoot->RegisterComponent();
	const auto AddTerrainBox = [Terrain, TerrainRoot](const TCHAR* Name, const FVector& Extent,
		const FVector& Location, const FRotator& Rotation)
	{
		UBoxComponent* const Box = NewObject<UBoxComponent>(Terrain, FName(Name));
		Terrain->AddInstanceComponent(Box);
		Box->SetupAttachment(TerrainRoot);
		Box->SetBoxExtent(Extent);
		Box->SetRelativeLocation(Location);
		Box->SetRelativeRotation(Rotation);
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->RegisterComponent();
		return Box;
	};
	AddTerrainBox(TEXT("ValleyFloor"), FVector(1000.0f, 5000.0f, 100.0f),
		FVector(0.0f, 0.0f, 9900.0f), FRotator::ZeroRotator);
	AddTerrainBox(TEXT("RisingRamp"), FVector(1677.0f, 5000.0f, 50.0f),
		FVector(2500.0f, 0.0f, 10750.0f), FRotator(26.565f, 0.0f, 0.0f));
	FindFProperty<FObjectProperty>(Fixture.Planet->GetClass(), TEXT("GameplaySurfaceActor"))
		->SetObjectPropertyValue_InContainer(Fixture.Planet, Terrain);
	FJTSPlanetSurfaceHit ValleyHit;
	FJTSPlanetSurfaceHit RampHit;
	if (!TestTrue(TEXT("Valley floor resolves"),
		Fixture.Planet->TraceToSurface(FVector(-400.0f, 0.0f, 11800.0f), ValleyHit))
		|| !TestTrue(TEXT("Uphill surface resolves ahead"),
		Fixture.Planet->TraceToSurface(FVector(1600.0f, 0.0f, 11800.0f), RampHit))) return false;
	TestTrue(TEXT("The valley rises toward the ship's flight path"),
		RampHit.ImpactPoint.Z > ValleyHit.ImpactPoint.Z + 150.0f);

	Fixture.Ship->SetActorLocation(FVector(-400.0f, 0.0f, 11800.0f));
	Fixture.Ship->SetActorRotation(FRotator(-30.0f, 0.0f, 0.0f).Quaternion());
	UJTSSpacecraftFlightMovementComponent* const Movement = Fixture.Ship->GetFlightMovementComponent();
	FJTSSpacecraftFlightStats CruiseStats = Movement->GetEffectiveStats();
	CruiseStats.MaxMoveSpeed = 1900.0f;
	Movement->SetEffectiveStats(CruiseStats);
	Movement->ClearInput();
	Movement->StopMovementImmediately();
	const FQuat HoverAttitude = Fixture.Ship->GetActorQuat();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("A stationary nose-down hover is not auto-levelled"),
		Fixture.Ship->GetActorQuat().AngularDistance(HoverAttitude) < 0.001f);
	Movement->Velocity = FVector(1700.0f, 0.0f, -900.0f);
	Fixture.Ship->ActivateFlightCameraThirdPerson();
	Fixture.Ship->FlightLookPitch(FInputActionValue(
		-40.0f / FMath::Max(Fixture.Ship->FlightCameraLookSensitivity, KINDA_SMALL_NUMBER)));
	Fixture.Ship->FlightMoveForward(FInputActionValue(1.0f));
	Fixture.Ship->Tick(0.06f);
	TestTrue(TEXT("Downward coupled view still submits pilot pitch before terrain assistance"),
		Fixture.Ship->LocalFlightInput.Pitch < -0.1f);
	const FVector PilotView = DriverController->GetControlRotation().Vector();
	const float InitialNoseHeight = Fixture.Ship->GetActorForwardVector().Z;
	float MinimumHullSeparation = BIG_NUMBER;
	const auto TrackHullSeparation = [&Fixture, &MinimumHullSeparation]()
	{
		float Altitude = 0.0f;
		if (Fixture.Planet->GetAltitudeAboveSurface(Fixture.Ship->GetActorLocation(), Altitude))
		{
			const FVector Up = Fixture.Planet->GetRadialUpVector(Fixture.Ship->GetActorLocation());
			MinimumHullSeparation = FMath::Min(MinimumHullSeparation,
				Altitude - Fixture.Ship->GetLandingCollisionClearance(Up));
		}
	};
	for (int32 Step = 0; Step < 15; ++Step)
	{
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
		TrackHullSeparation();
		Fixture.Ship->Tick(1.0f / 60.0f);
	}
	AddInfo(FString::Printf(TEXT("Approach recovery: x=%.1f noseZ=%.3f startZ=%.3f"),
		Fixture.Ship->GetActorLocation().X,
		Fixture.Ship->GetActorForwardVector().Z, InitialNoseHeight));
	TestTrue(TEXT("The existing nose-down attitude starts rising before reaching the ramp"),
		Fixture.Ship->GetActorLocation().X < 1000.0f
		&& Fixture.Ship->GetActorForwardVector().Z > InitialNoseHeight + 0.1f);
	TestTrue(TEXT("The coupled view keeps looking down while terrain assistance raises the hull"),
		FVector::DotProduct(DriverController->GetControlRotation().Vector(), PilotView) > 0.999f);
	Fixture.Ship->FlightFreeLookStarted(FInputActionValue(true));
	Fixture.Ship->Tick(0.06f);
	const FVector FreeLookView = DriverController->GetControlRotation().Vector();
	for (int32 Step = 0; Step < 120; ++Step)
	{
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
		TrackHullSeparation();
		Fixture.Ship->Tick(1.0f / 60.0f);
	}
	float FinalAltitude = 0.0f;
	const bool bHasFinalAltitude = Fixture.Planet->GetAltitudeAboveSurface(
		Fixture.Ship->GetActorLocation(), FinalAltitude);
	const FVector FinalUp = Fixture.Planet->GetRadialUpVector(Fixture.Ship->GetActorLocation());
	const float HullClearance = Fixture.Ship->GetLandingCollisionClearance(FinalUp);
	AddInfo(FString::Printf(TEXT("Ramp traversal: x=%.1f altitude=%.1f hull=%.1f noseZ=%.3f"),
		Fixture.Ship->GetActorLocation().X, FinalAltitude, HullClearance,
		Fixture.Ship->GetActorForwardVector().Z));
	TestTrue(TEXT("The ship climbs across the valley ramp without remaining stuck at its foot"),
		Fixture.Ship->GetActorLocation().X > 1250.0f
		&& bHasFinalAltitude
		&& FinalAltitude >= HullClearance - 20.0f
		&& FinalAltitude < 2600.0f);
	TestTrue(TEXT("The hull stays outside the real terrain during the whole ramp approach"),
		MinimumHullSeparation >= -20.0f && MinimumHullSeparation < BIG_NUMBER);
	TestTrue(TEXT("Alt free look remains independent while the ship climbs the ramp"),
		FVector::DotProduct(DriverController->GetControlRotation().Vector(), FreeLookView) > 0.999f);

	// A nearly vertical nose has no horizontal forward projection. Recovery must use the
	// hull's deck direction instead of rotating toward the opposite side of the valley.
	Movement->ClearInput();
	Movement->StopMovementImmediately();
	Movement->SetFlightAssistEnabled(false);
	Fixture.Ship->SetActorRotation(FRotator(-90.0f, 0.0f, 0.0f).Quaternion());
	const float VerticalHullClearance = Fixture.Ship->GetLandingCollisionClearance(FVector::UpVector);
	Fixture.Ship->SetActorLocation(FVector(0.0f, 0.0f, 10000.0f + VerticalHullClearance + 850.0f));
	Movement->Velocity = FVector(0.0f, 0.0f, -1800.0f);
	for (int32 Step = 0; Step < 60; ++Step)
	{
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	}
	float VerticalAltitude = 0.0f;
	const bool bHasVerticalAltitude = Fixture.Planet->GetAltitudeAboveSurface(
		Fixture.Ship->GetActorLocation(), VerticalAltitude);
	AddInfo(FString::Printf(TEXT("Vertical recovery: noseX=%.3f noseZ=%.3f altitude=%.1f hull=%.1f"),
		Fixture.Ship->GetActorForwardVector().X, Fixture.Ship->GetActorForwardVector().Z,
		VerticalAltitude, Fixture.Ship->GetLandingCollisionClearance(FVector::UpVector)));
	TestTrue(TEXT("A vertical nose recovers toward its original forward heading"),
		Fixture.Ship->GetActorForwardVector().X > 0.9f
		&& Fixture.Ship->GetActorForwardVector().Z > -0.1f);
	TestTrue(TEXT("Vertical recovery maintains physical clearance"),
		bHasVerticalAltitude && VerticalAltitude >=
			Fixture.Ship->GetLandingCollisionClearance(FVector::UpVector) - 20.0f);
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
	const float SpaceFlightAltitude = PhysicalAltitudeAtNavigationKilometers(
		AJTSSpaceWorldManager::CruiseTransitionKilometers);
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
	TestEqual(TEXT("The 18 km handoff retains the takeoff state through the shallow flight band"),
		Fixture.Manager->GetCurrentTravelState(), EJTSSpaceTravelState::Takeoff);
	TestTrue(TEXT("Free-flight camera preserves the departure horizon without snapping to World-Z"),
		FVector::DotProduct(Fixture.Ship->GetFlightReferenceUp(), SourceUp) > 0.999f);
	TestTrue(TEXT("Surface recovery does not overwrite the departure inertial horizon"),
		FVector::DotProduct(Fixture.Ship->GetFlightReferenceUp(), SourceUp) > 0.999f);
	Fixture.Ship->SetActorLocation(Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (SourceRadius + Fixture.Planet->GetSpaceFlightAltitude() + 100.0f));
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Passing the former 6 km dial handoff leaves the travel state steady"),
		Fixture.Manager->GetCurrentTravelState(), EJTSSpaceTravelState::Takeoff);
	TestEqual(TEXT("Passing the former handoff retains the planet flight target"),
		Fixture.Ship->GetFlightPlanet(), Fixture.Planet);

	Fixture.Ship->SetActorLocation(Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (SourceRadius + SpaceFlightAltitude + 100.0f));
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Takeoff transitions to the shared free-space travel state"),
		Fixture.Manager->GetCurrentTravelState(), EJTSSpaceTravelState::SpaceFlight);
	TestNull(TEXT("Free space clears the departed planet flight target"), Fixture.Ship->GetFlightPlanet());

	Fixture.Ship->SetActorLocation(Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (SourceRadius + SpaceFlightAltitude + 1000.0f));
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
		TestTrue(TEXT("Deep-space turnaround keeps the captured deck plane"),
			FVector::DotProduct(Fixture.Ship->GetActorUpVector(), DeckUp) > 0.999f);
	}
	const FVector TurnedDeckForward = FVector::VectorPlaneProject(
		Fixture.Ship->GetActorForwardVector(),
		DeckUp).GetSafeNormal();
	TestTrue(TEXT("Deep-space turnaround points the nose to the old tail inside the ship's plane"),
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
	// The map's gravity range remains 200 m; all presentation handoffs use the
	// shared heights printed by the near-surface navigation dial.
	FindFProperty<FFloatProperty>(Fixture.Planet->GetClass(), TEXT("GravityInfluenceRange"))
		->SetPropertyValue_InContainer(Fixture.Planet, 20000.0f);
	const float ContentAltitude = PhysicalAltitudeAtNavigationKilometers(
		AJTSSpaceWorldManager::SurfaceContentTransitionKilometers);
	const float ReleaseAltitude = PhysicalAltitudeAtNavigationKilometers(
		AJTSSpaceWorldManager::CruiseTransitionKilometers);

	AJTSPlanetAnchor* const Mars = Fixture.AddFlightPlanet(TEXT("Mars"), FVector(180000.0f, 0.0f, 0.0f));
	AJTSPlanetAnchor* const Deimos = Fixture.AddFlightPlanet(TEXT("Deimos"), FVector(210000.0f, 0.0f, 0.0f));
	AStaticMeshActor* const MarsSurface = Cast<AStaticMeshActor>(Mars->GetGameplaySurfaceActor());
	if (!TestTrue(TEXT("Free-flight test places a Mars surface"), IsValid(MarsSurface)))
	{
		return false;
	}

	FindFProperty<FFloatProperty>(Fixture.Planet->GetClass(), TEXT("HeliocentricDistanceKilometers"))
		->SetPropertyValue_InContainer(Fixture.Planet, 149600000.0f);
	FindFProperty<FFloatProperty>(Mars->GetClass(), TEXT("HeliocentricDistanceKilometers"))
		->SetPropertyValue_InContainer(Mars, 227940000.0f);
	FindFProperty<FFloatProperty>(Deimos->GetClass(), TEXT("HeliocentricDistanceKilometers"))
		->SetPropertyValue_InContainer(Deimos, 227963400.0f);
	FindFProperty<FNameProperty>(Deimos->GetClass(), TEXT("ParentPlanetId"))
		->SetPropertyValue_InContainer(Deimos, FName(TEXT("Mars")));
	UJTSSpacecraftPresentationComponent* const Presentation =
		Fixture.Ship->FindComponentByClass<UJTSSpacecraftPresentationComponent>();
	if (!TestNotNull(TEXT("The ship has a cruise presentation component"), Presentation))
	{
		return false;
	}
	UStaticMeshComponent* Field = nullptr;
	TInlineComponentArray<UStaticMeshComponent*> ShipMeshes(Fixture.Ship);
	for (UStaticMeshComponent* const Mesh : ShipMeshes)
	{
		if (IsValid(Mesh) && Mesh->GetName() == TEXT("AntigravityField"))
		{
			Field = Mesh;
			break;
		}
	}
	if (!TestNotNull(TEXT("The ship Blueprint provides the antigravity field mesh and material"), Field))
	{
		return false;
	}
	UMaterialInstanceDynamic* const FieldMaterial = Cast<UMaterialInstanceDynamic>(Field->GetMaterial(0));
	if (!TestNotNull(TEXT("The field has a dynamic formation material"), FieldMaterial))
	{
		return false;
	}
	const FVector SourceUp = Fixture.Planet->GetRadialUpVector(Fixture.Ship->GetActorLocation()).GetSafeNormal();
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
	Fixture.Ship->SetActorLocation(Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (Fixture.Planet->GetApproximateRadius() + 19900.0f));
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Inside 200 m the Moon is still the gameplay planet"),
		Fixture.Manager->GetCurrentPlanet(), Fixture.Planet);
	Fixture.Ship->SetActorLocation(Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (Fixture.Planet->GetApproximateRadius() + 20100.0f));
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Crossing the old gravity edge does not release the travel planet"),
		Fixture.Manager->GetCurrentPlanet(), Fixture.Planet);
	TestTrue(TEXT("The near-surface navigation retains the departed Moon"),
		Fixture.Manager->GetNavigationReferencePlanet(Fixture.Ship) == Fixture.Planet);
	TestFalse(TEXT("Leaving gravity influence does not start the star chart or field"),
		Fixture.Manager->IsCruisePresentationActive(Fixture.Ship));
	TestTrue(TEXT("The old 14-15 km band leaves the Moon's visible surface unchanged"),
		Fixture.Planet->GetGameplaySurfaceActor()->GetActorScale3D().Equals(FVector(200.0), 0.1f)
		&& !Fixture.Planet->IsHidden());
	Fixture.Ship->SetActorLocation(Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (Fixture.Planet->GetApproximateRadius() + ContentAltitude - 100.0f));
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Surface interaction remains visible below 17 km"), DepartureMarker->IsHidden());
	Fixture.Ship->SetActorLocation(Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (Fixture.Planet->GetApproximateRadius() + ContentAltitude + 100.0f));
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("17 km hides surface interaction before the planet shrinks"),
		DepartureMarker->IsHidden() && !DepartureMarker->GetActorEnableCollision()
		&& Fixture.Planet->GetGameplaySurfaceActor()->GetActorScale3D().Equals(FVector(200.0), 0.1f));
	Fixture.Ship->SetActorLocation(Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (Fixture.Planet->GetApproximateRadius() + ContentAltitude - 100.0f));
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Returning through 17 km restores interaction"),
		!DepartureMarker->IsHidden() && DepartureMarker->GetActorEnableCollision());
	Fixture.Ship->SetActorLocation(Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (Fixture.Planet->GetApproximateRadius() + ReleaseAltitude - 100.0f));
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Just below 18 km still uses exhaust and the surface dial"),
		Fixture.Manager->IsCruisePresentationActive(Fixture.Ship));
	const float ExpectedSurfaceDialCentimeters = 1200000.0f
		* FMath::Loge(1.0f + (ReleaseAltitude - 100.0f) / 8000.0f);
	TestTrue(TEXT("The near-surface dial preserves its kilometre-scale logarithmic climb"),
		FMath::IsNearlyEqual(Fixture.Manager->GetNavigationSurfaceRangeCentimeters(
			Fixture.Ship, Fixture.Planet), ExpectedSurfaceDialCentimeters, 1000.0f));
	Movement->Velocity = SourceUp * 1000.0f;
	TestTrue(TEXT("The top navigation speed follows the scaled near-surface climb"),
		Fixture.Manager->GetNavigationSpeedCentimetersPerSecond(Fixture.Ship) > 10000.0f);
	Movement->StopMovementImmediately();
	Presentation->TickComponent(0.3f, LEVELTICK_All, nullptr);
	Fixture.Ship->Tick(1.0f);
	const float NearFlightFOV = Fixture.Ship->GetFlightCamera()->FieldOfView;
	const FVector FieldScaleBeforeHandoff = Field->GetRelativeScale3D();
	const FVector CameraSocketBeforeHandoff = Fixture.Ship->GetFlightCameraBoom()->SocketOffset;
	UMaterialInstanceDynamic* const Medium = Fixture.Ship->CruiseMediumDistortionInstance;
	if (!TestNotNull(TEXT("The ship Blueprint equips the camera medium-refraction material"), Medium))
	{
		return false;
	}
	// A pilot may be rolled relative to the planet when crossing the shell. The handoff itself
	// must keep that exact view frame instead of rebuilding it from the new flight reference Up.
	Fixture.Ship->FlightCameraAimRotation = (FQuat(Fixture.Ship->GetActorForwardVector(), HALF_PI)
		* Fixture.Ship->GetActorQuat()).GetNormalized();
	Fixture.Ship->LastFlightCameraHullRotation = Fixture.Ship->GetActorQuat();
	Fixture.Ship->bFlightCameraFrameInitialized = true;
	Fixture.Ship->bFlightCameraRecentering = false;
	Fixture.Ship->UpdateFlightCameraFrame(0.0f);
	const FQuat HullBeforeRelease = Fixture.Ship->GetActorQuat();
	const FQuat CameraBeforeRelease = Fixture.Ship->FlightCameraAimRotation;
	TestFalse(TEXT("The antigravity envelope stays hidden in near-surface flight"), Field->IsVisible());
	constexpr double AstronomicalUnitCentimeters = 1.495978707e13;
	const FVector ReleaseLocation = Fixture.Planet->GetPlanetCenter()
		+ FVector::ForwardVector * (Fixture.Planet->GetApproximateRadius() + ReleaseAltitude + 100.0f);
	Fixture.Ship->SetActorLocation(ReleaseLocation);
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("18 km starts mesh shrink, free travel, star chart and antigravity together"),
		Fixture.Manager->IsCruisePresentationActive(Fixture.Ship)
		&& Fixture.Manager->GetCurrentTravelState() == EJTSSpaceTravelState::SpaceFlight
		&& Fixture.Manager->GetCurrentPlanet() == nullptr
		&& Fixture.Planet->GetGameplaySurfaceActor()->GetActorScale3D().GetAbsMax() < 200.0f);
	Presentation->TickComponent(0.16f, LEVELTICK_All, nullptr);
	Fixture.Ship->Tick(0.16f);
	const FQuat ExpectedReleaseCamera = (Fixture.Ship->GetActorQuat()
		* HullBeforeRelease.Inverse() * CameraBeforeRelease).GetNormalized();
	TestTrue(TEXT("18 km drive engagement preserves camera attitude through the planet handoff"),
		Fixture.Ship->bFlightCameraFrameInitialized
		&& Fixture.Ship->FlightCameraAimRotation.AngularDistance(ExpectedReleaseCamera)
			< FMath::DegreesToRadians(0.5f)
		&& Fixture.Ship->GetFlightCameraBoom()->GetComponentQuat().AngularDistance(ExpectedReleaseCamera)
			< FMath::DegreesToRadians(0.5f));
	TestTrue(TEXT("The antigravity envelope wraps the ship at the release shell"), Field->IsVisible());
	TestTrue(TEXT("Drive engagement keeps only a small axial push while the medium refracts"),
		Fixture.Ship->GetFlightCamera()->FieldOfView > NearFlightFOV + 0.75f
		&& Fixture.Ship->GetFlightCamera()->FieldOfView < NearFlightFOV + 2.25f
		&& FMath::IsNearlyEqual(Fixture.Ship->GetFlightCameraBoom()->SocketOffset.X,
			CameraSocketBeforeHandoff.X - Fixture.Ship->CruiseEngageCameraPushback, 1.0f)
		&& FMath::IsNearlyEqual(Fixture.Ship->GetFlightCameraBoom()->SocketOffset.Y, CameraSocketBeforeHandoff.Y)
		&& FMath::IsNearlyEqual(Fixture.Ship->GetFlightCameraBoom()->SocketOffset.Z, CameraSocketBeforeHandoff.Z)
		&& Field->GetRelativeScale3D().Equals(FieldScaleBeforeHandoff, 0.001f));
	AddInfo(FString::Printf(TEXT("Departure medium: configured=%.4f runtime=%.4f material=%.4f radius=%.4f fringe=%.3f"),
		Fixture.Ship->CruiseMediumWarpStrength, Fixture.Ship->CurrentMediumWarpStrength,
		Medium->K2_GetScalarParameterValue(TEXT("WarpStrength")),
		Medium->K2_GetScalarParameterValue(TEXT("WaveRadius")),
		Fixture.Ship->GetFlightCamera()->PostProcessSettings.SceneFringeIntensity));
	TestTrue(TEXT("Leaving the planet sends a refractive wave from the view centre outward"),
		Medium->K2_GetScalarParameterValue(TEXT("WarpStrength")) > 0.005f
		&& Medium->K2_GetScalarParameterValue(TEXT("WaveRadius")) < 0.4f
		&& Fixture.Ship->GetFlightCamera()->PostProcessSettings.SceneFringeIntensity
			> Fixture.Ship->BaseFlightCameraFringeIntensity + 0.2f);
	const FQuat CameraBeforeReplication = Fixture.Ship->FlightCameraAimRotation;
	Fixture.Ship->OnRep_FlightState();
	TestTrue(TEXT("A replicated flight-target update cannot reinitialize the driver's camera"),
		Fixture.Ship->bFlightCameraFrameInitialized
		&& Fixture.Ship->FlightCameraAimRotation.AngularDistance(CameraBeforeReplication)
			< FMath::DegreesToRadians(0.1f));
	TestTrue(TEXT("The bubble forms through its material instead of geometry growth"),
		FieldMaterial->K2_GetScalarParameterValue(TEXT("FieldFormation")) > 0.0f
		&& FieldMaterial->K2_GetScalarParameterValue(TEXT("FieldFormation")) < 1.0f);
	Presentation->TickComponent(0.3f, LEVELTICK_All, nullptr);
	Fixture.Ship->Tick(0.55f);
	TestTrue(TEXT("The acceleration cue settles to a fully formed steady field"),
		FMath::IsNearlyEqual(FieldMaterial->K2_GetScalarParameterValue(TEXT("FieldFormation")), 1.0f, 0.001f)
		&& FMath::IsNearlyEqual(Fixture.Ship->GetFlightCamera()->FieldOfView, NearFlightFOV, 0.1f)
		&& Fixture.Ship->GetFlightCameraBoom()->SocketOffset.Equals(CameraSocketBeforeHandoff, 0.1f)
		&& FMath::IsNearlyZero(Medium->K2_GetScalarParameterValue(TEXT("WarpStrength")), 0.0001f)
		&& Fixture.Ship->GetFlightCamera()->PostProcessSettings.bOverride_SceneFringeIntensity
			== Fixture.Ship->bBaseFlightCameraFringeOverride);
	const FQuat HullBeforeApproach = Fixture.Ship->GetActorQuat();
	const FQuat CameraBeforeApproach = Fixture.Ship->FlightCameraAimRotation;
	Fixture.Ship->SetActorLocation(Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (Fixture.Planet->GetApproximateRadius() + ReleaseAltitude - 100.0f));
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	Presentation->TickComponent(0.16f, LEVELTICK_All, nullptr);
	Fixture.Ship->Tick(0.16f);
	const FQuat ExpectedApproachCamera = (Fixture.Ship->GetActorQuat()
		* HullBeforeApproach.Inverse() * CameraBeforeApproach).GetNormalized();
	TestTrue(TEXT("18 km planet approach preserves camera attitude without a roll"),
		Fixture.Ship->bFlightCameraFrameInitialized
		&& Fixture.Ship->FlightCameraAimRotation.AngularDistance(ExpectedApproachCamera)
			< FMath::DegreesToRadians(0.5f)
		&& Fixture.Ship->GetFlightCameraBoom()->GetComponentQuat().AngularDistance(ExpectedApproachCamera)
			< FMath::DegreesToRadians(0.5f));
	TestTrue(TEXT("Planet approach keeps only a small axial brake while the field withdraws"),
		Fixture.Ship->GetFlightCamera()->FieldOfView < NearFlightFOV - 0.75f
		&& Fixture.Ship->GetFlightCamera()->FieldOfView > NearFlightFOV - 2.25f
		&& FMath::IsNearlyEqual(Fixture.Ship->GetFlightCameraBoom()->SocketOffset.X,
			CameraSocketBeforeHandoff.X + Fixture.Ship->CruiseBrakeCameraLurch, 1.0f)
		&& FieldMaterial->K2_GetScalarParameterValue(TEXT("FieldFormation")) < 1.0f
		&& Field->GetRelativeScale3D().Equals(FieldScaleBeforeHandoff, 0.001f));
	AddInfo(FString::Printf(TEXT("Approach medium: runtime=%.4f material=%.4f radius=%.4f"),
		Fixture.Ship->CurrentMediumWarpStrength,
		Medium->K2_GetScalarParameterValue(TEXT("WarpStrength")),
		Medium->K2_GetScalarParameterValue(TEXT("WaveRadius"))));
	TestTrue(TEXT("Entering the planet pulls the refractive boundary inward"),
		Medium->K2_GetScalarParameterValue(TEXT("WarpStrength")) < -0.005f
		&& Medium->K2_GetScalarParameterValue(TEXT("WaveRadius")) > 0.4f);
	const float BrakeFOV = Fixture.Ship->GetFlightCamera()->FieldOfView;
	const float BrakeCameraOffset = Fixture.Ship->GetFlightCameraBoom()->SocketOffset.X;
	const float BrakeWarpStrength = Medium->K2_GetScalarParameterValue(TEXT("WarpStrength"));
	Fixture.Ship->SetActorLocation(ReleaseLocation);
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	Presentation->TickComponent(0.016f, LEVELTICK_All, nullptr);
	Fixture.Ship->Tick(0.016f);
	TestTrue(TEXT("Rapid 18 km reversal withdraws the lens cue without a snap"),
		FMath::Abs(Fixture.Ship->GetFlightCamera()->FieldOfView - BrakeFOV) < 1.5f
		&& FMath::Abs(Fixture.Ship->GetFlightCameraBoom()->SocketOffset.X - BrakeCameraOffset) < 25.0f
		&& FMath::Abs(Medium->K2_GetScalarParameterValue(TEXT("WarpStrength")) - BrakeWarpStrength) < 0.002f);
	Presentation->TickComponent(0.3f, LEVELTICK_All, nullptr);
	Fixture.Ship->Tick(0.55f);
	for (const FVector Heading : { FVector::ForwardVector, -FVector::ForwardVector, FVector::RightVector })
	{
		Fixture.Ship->SetActorRotation(FRotationMatrix::MakeFromX(Heading).ToQuat());
		Fixture.Ship->SetActorLocation(Fixture.Planet->GetPlanetCenter()
			+ SourceUp * (Fixture.Planet->GetApproximateRadius() + ReleaseAltitude - 100.0f));
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
		TestFalse(TEXT("All headings return to near flight below 18 km"),
			Fixture.Manager->IsCruisePresentationActive(Fixture.Ship));
		Fixture.Ship->SetActorLocation(Fixture.Planet->GetPlanetCenter()
			+ SourceUp * (Fixture.Planet->GetApproximateRadius() + ReleaseAltitude + 100.0f));
		Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
		TestTrue(TEXT("All headings enter cruise above 18 km"),
			Fixture.Manager->IsCruisePresentationActive(Fixture.Ship));
	}
	Fixture.Ship->SetActorLocation(ReleaseLocation);
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	Fixture.Planet->GetGameplaySurfaceActor()->SetActorScale3D(FVector(200.0f));
	Fixture.Ship->SetRole(ROLE_SimulatedProxy);
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("A client updates the visible planet shrink before switching navigation"),
		Fixture.Planet->GetGameplaySurfaceActor()->GetActorScale3D().GetAbsMax() < 200.0f
		&& Fixture.Manager->IsCruisePresentationActive(Fixture.Ship));
	Fixture.Ship->SetRole(ROLE_Authority);
	Movement->Velocity = FVector::ForwardVector * 5000.0f;
	TestTrue(TEXT("Antigravity flight reports cruise speed immediately after mesh shrink"),
		Fixture.Manager->GetNavigationSpeedCentimetersPerSecond(Fixture.Ship) > 100000000.0f);
	Movement->Velocity = FVector::RightVector * 5000.0f;
	TestTrue(TEXT("Turning across the route does not reduce cruise speed to m/s"),
		Fixture.Manager->GetNavigationSpeedCentimetersPerSecond(Fixture.Ship) > 100000000.0f);
	Movement->StopMovementImmediately();
	TestTrue(TEXT("Cruise range at the shrink boundary is below one hundredth AU"),
		Fixture.Manager->GetCruiseRangeCentimeters(Fixture.Ship, Fixture.Planet)
			< 0.01 * AstronomicalUnitCentimeters);
	TestEqual(TEXT("Even 120,000 km still displays below 0.01 AU"),
		UJTSCruiseNavigationWidget::FormatAstronomicalRange(120000.0f * 100000.0f),
		FString(TEXT("<0.01 AU")));
	TestEqual(TEXT("The top speed changes units at one kilometre per second"),
		UJTSCruiseNavigationWidget::FormatNavigationSpeed(100000.0f), FString(TEXT("1.0 km/s")));
	const float InnerBeforeCrossover = UJTSCruiseNavigationWidget::CruiseRangeRadialFraction(
		static_cast<float>(0.42 * AstronomicalUnitCentimeters));
	const float OuterBeforeCrossover = UJTSCruiseNavigationWidget::CruiseRangeRadialFraction(
		static_cast<float>(0.43 * AstronomicalUnitCentimeters));
	TestTrue(TEXT("Nearly equal AU ranges stay nearly adjacent when their order changes"),
		InnerBeforeCrossover < OuterBeforeCrossover
		&& OuterBeforeCrossover - InnerBeforeCrossover < 0.01f);
	TestTrue(TEXT("The foreign planet retains its configured half-AU route"),
		Fixture.Manager->GetCruiseRangeCentimeters(Fixture.Ship, Mars)
			> 0.5 * AstronomicalUnitCentimeters);
	const float ReleaseRange = Fixture.Manager->GetCruiseRangeCentimeters(Fixture.Ship, Fixture.Planet);
	Fixture.Ship->SetActorLocation(ReleaseLocation + FVector::RightVector * 10000.0f);
	TestTrue(TEXT("Leaving the Moon in any direction increases its cruise range"),
		Fixture.Manager->GetCruiseRangeCentimeters(Fixture.Ship, Fixture.Planet) > ReleaseRange);
	const FVector CorridorLocation = FMath::Lerp(ReleaseLocation, Mars->GetPlanetCenter(), 0.25f);
	Fixture.Ship->SetActorLocation(CorridorLocation);
	const float MoonRangeBeforeReferenceChange = Fixture.Manager->GetCruiseRangeCentimeters(Fixture.Ship, Fixture.Planet);
	const float MarsRangeBeforeReferenceChange = Fixture.Manager->GetCruiseRangeCentimeters(Fixture.Ship, Mars);
	const float DeimosRangeBeforeReferenceChange = Fixture.Manager->GetCruiseRangeCentimeters(Fixture.Ship, Deimos);
	Fixture.Ship->SetFlightTargetPlanet(Mars);
	TestTrue(TEXT("Every cruise contact keeps its AU range when the flight reference changes"),
		FMath::IsNearlyEqual(Fixture.Manager->GetCruiseRangeCentimeters(Fixture.Ship, Fixture.Planet),
			MoonRangeBeforeReferenceChange, MoonRangeBeforeReferenceChange * 0.0001f)
		&& FMath::IsNearlyEqual(Fixture.Manager->GetCruiseRangeCentimeters(Fixture.Ship, Mars),
			MarsRangeBeforeReferenceChange, MarsRangeBeforeReferenceChange * 0.0001f)
		&& FMath::IsNearlyEqual(Fixture.Manager->GetCruiseRangeCentimeters(Fixture.Ship, Deimos),
			DeimosRangeBeforeReferenceChange, DeimosRangeBeforeReferenceChange * 0.0001f));
	Fixture.Ship->SetFlightTargetPlanet(nullptr);
	TestTrue(TEXT("Travel into the compressed cruise corridor grows the AU reading"),
		Fixture.Manager->GetCruiseRangeCentimeters(Fixture.Ship, Fixture.Planet) > ReleaseRange);
	Movement->Velocity = (Mars->GetPlanetCenter() - Fixture.Planet->GetPlanetCenter()).GetSafeNormal() * 4200.0f;
	const float CruiseNavigationSpeed = Fixture.Manager->GetNavigationSpeedCentimetersPerSecond(Fixture.Ship);
	TestTrue(TEXT("The top navigation speed reflects the compressed AU route in cruise"),
		CruiseNavigationSpeed > 4200000.0f);
	const FVector CruiseVelocity = Movement->Velocity;
	Fixture.Ship->SetActorLocation(CorridorLocation + CruiseVelocity * 0.05f);
	const float RangeAhead = Fixture.Manager->GetCruiseRangeCentimeters(Fixture.Ship, Fixture.Planet);
	Fixture.Ship->SetActorLocation(CorridorLocation - CruiseVelocity * 0.05f);
	const float RangeBehind = Fixture.Manager->GetCruiseRangeCentimeters(Fixture.Ship, Fixture.Planet);
	Fixture.Ship->SetActorLocation(CorridorLocation);
	TestTrue(TEXT("The top speed agrees with the route distance change per second"),
		FMath::IsNearlyEqual(CruiseNavigationSpeed, (RangeAhead - RangeBehind) / 0.1f,
			CruiseNavigationSpeed * 0.1f));
	Movement->StopMovementImmediately();
	FRepMovement ReplicatedShipMovement = Fixture.Ship->GetReplicatedMovement();
	ReplicatedShipMovement.LinearVelocity = FVector(3000.0f, 4000.0f, 0.0f);
	Fixture.Ship->SetReplicatedMovement(ReplicatedShipMovement);
	Fixture.Ship->SetRole(ROLE_SimulatedProxy);
	TestTrue(TEXT("A client reads ship speed from replicated movement"),
		FMath::IsNearlyEqual(Fixture.Ship->GetCurrentSpeed(), 5000.0f, 1.0f));
	Fixture.Ship->SetRole(ROLE_Authority);

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
		+ SourceUp * (Fixture.Planet->GetApproximateRadius() + ReleaseAltitude + 30000.0f);
	Fixture.Ship->SetActorLocation(ShrinkProbeLocation);
	Movement->SetMoveInput(FVector2D::ZeroVector);
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	const float ScaleJustPastRelease = Fixture.Planet->GetGameplaySurfaceActor()->GetActorScale3D().GetAbsMax();
	TestTrue(TEXT("Past 18 km the departure body shrinks continuously and remains visible"),
		ScaleJustPastRelease < 200.0f * 0.9f && ScaleJustPastRelease > 200.0f * 0.05f);
	TestFalse(TEXT("The shrinking departure body remains on screen"), Fixture.Planet->IsHidden());
	Fixture.Ship->SetActorLocation(OutsideLocation);
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("The distant destination is hidden while its disk is smaller than a pixel"), Mars->IsHidden());
	const FVector MarsSurfaceLocation = MarsSurface->GetActorLocation();
	Fixture.Ship->SetActorRotation(FRotationMatrix::MakeFromX(FVector::UpVector).ToQuat());
	Fixture.Ship->SetActorLocation(Mars->GetPlanetCenter()
		+ FVector::UpVector * (Mars->GetApproximateRadius() + ReleaseAltitude - 100.0f));
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("18 km reacquires the arriving planet even while facing away"),
		Fixture.Manager->GetCurrentPlanet() == Mars
		&& Fixture.Manager->GetCurrentTravelState() == EJTSSpaceTravelState::Approach
		&& !Fixture.Manager->IsCruisePresentationActive(Fixture.Ship));
	Fixture.Ship->SetActorLocation(Mars->GetPlanetCenter()
		+ FVector::UpVector * (Mars->GetApproximateRadius() + ReleaseAltitude + 100.0f));
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Departing a destination also releases it at 18 km"),
		Fixture.Manager->GetCurrentPlanet() == nullptr
		&& Fixture.Manager->GetCurrentTravelState() == EJTSSpaceTravelState::SpaceFlight
		&& Fixture.Manager->IsCruisePresentationActive(Fixture.Ship));

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
	TestTrue(TEXT("Leaving the 18 km band lets the departure body shrink"),
		DepartureScaleAfterCruise.GetAbsMax() < 200.0f * 0.5f);
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("A landing marker left behind a shrunken body is hidden"), DepartureMarker->IsHidden());
	TestFalse(TEXT("A landing marker left behind a shrunken body has no collision"), DepartureMarker->GetActorEnableCollision());
	TestEqual(TEXT("Flying into the destination influence reacquires that planet"),
		Fixture.Manager->GetCurrentPlanet(), Mars);

	const FVector AuthoredBandLocation = Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (Fixture.Planet->GetApproximateRadius() + ContentAltitude - 1000.0f);
	Fixture.Ship->SetActorLocation(AuthoredBandLocation);
	Movement->StopMovementImmediately();
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Within 17 km of the surface the body keeps its authored scale"),
		Fixture.Planet->GetGameplaySurfaceActor()->GetActorScale3D().Equals(FVector(200.0), 0.1));
	TestFalse(TEXT("Within 17 km of the surface the landing marker is shown"), DepartureMarker->IsHidden());

	const FVector ReturnLocation = Fixture.Planet->GetPlanetCenter()
		+ SourceUp * (Fixture.Planet->GetApproximateRadius() + Fixture.Planet->GetGravityInfluenceRange() * 0.5f);
	Fixture.Ship->SetActorLocation(ReturnLocation);
	Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Turning back makes the departure body visible again"), Fixture.Planet->IsHidden());
	TestTrue(TEXT("Returning inside 18 km restores the authored surface scale"),
		Fixture.Planet->GetGameplaySurfaceActor()->GetActorScale3D().Equals(FVector(200.0), 0.1));
	TestFalse(TEXT("Returning inside 17 km shows the landing marker again"), DepartureMarker->IsHidden());
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
