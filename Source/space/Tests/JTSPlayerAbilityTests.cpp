#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSStaminaComponent.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerState.h"

namespace
{
	struct FAbilityTestWorld
	{
		UWorld* World;
		FAbilityTestWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->InitializeActorsForPlay(FURL());
		}
		~FAbilityTestWorld()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSFixedStackLimitTest, "JTS.Inventory.Fixed99StackLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSFixedStackLimitTest::RunTest(const FString& Parameters)
{
	FAbilityTestWorld Fixture;
	AActor* Owner = Fixture.World->SpawnActor<AActor>();
	UJTSInventoryComponent* Inventory = NewObject<UJTSInventoryComponent>(Owner);
	Owner->AddInstanceComponent(Inventory);
	Inventory->RegisterComponent();
	Inventory->RestoreItems({}, 0);
	TestEqual(TEXT("Resources stack to 99 without a PlayerState or ability"), Inventory->GetEffectiveStackLimit(EJTSItemId::Rock), 99);
	TestEqual(TEXT("Weapon instances stay separate"), Inventory->GetEffectiveStackLimit(EJTSItemId::RailPistol), 1);
	TestTrue(TEXT("First 99 resources fit in one slot"), Inventory->TryAddItemById(EJTSItemId::Rock, 99));
	TestEqual(TEXT("First slot reaches 99"), Inventory->GetItemAtSlot(0).StackCount, 99);
	TestTrue(TEXT("The hundredth resource fits in the next slot"), Inventory->TryAddItemById(EJTSItemId::Rock, 1));
	TestEqual(TEXT("Second slot receives the hundredth resource"), Inventory->GetItemAtSlot(1).StackCount, 1);
	TestTrue(TEXT("Second stack can also reach 99"), Inventory->TryAddItemById(EJTSItemId::Rock, 98));
	TestFalse(TEXT("Full inventory rejects more resources"), Inventory->TryAddItemById(EJTSItemId::Rock, 1));
	TestEqual(TEXT("Rejected pickup loses no inventory items"), Inventory->GetItemCount(EJTSItemId::Rock), 198);
	Inventory->RestoreItems({UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId::Rock, 100)}, 0);
	TestEqual(TEXT("Restoring a large stack caps the first slot"), Inventory->GetItemAtSlot(0).StackCount, 99);
	TestEqual(TEXT("Restoring a large stack uses the second visible slot"), Inventory->GetItemAtSlot(1).StackCount, 1);
	TestNotEqual(TEXT("Split stacks have distinct instance identities"), Inventory->GetItemAtSlot(0).InstanceId, Inventory->GetItemAtSlot(1).InstanceId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSHealthDefenseAbilityTest, "JTS.Progression.HealthDefenseAndLegacyRefund",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSHealthDefenseAbilityTest::RunTest(const FString& Parameters)
{
	FAbilityTestWorld Fixture;
	APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>();
	AJTSPlayerState* State = Fixture.World->SpawnActor<AJTSPlayerState>();
	Controller->SetPlayerState(State);
	State->SetOwner(Controller);
	AJTSCharacter* Character = Fixture.World->SpawnActor<AJTSCharacter>();
	Character->DispatchBeginPlay();
	Controller->Possess(Character);
	UJTSHealthComponent* Health = Character->GetHealthComponent();
	const float BaseHealth = Health->GetMaxHealth();
	Health->ApplyDamage(2.0f, nullptr, nullptr);
	State->GrantDebugLevels();
	FJTSAbilityAllocation Allocation;
	Allocation.HealthRanks = 2;
	Allocation.DefenseRanks = 3;
	TestTrue(TEXT("Server accepts health and defense together"), State->CommitAbilityAllocation(Allocation));
	TestEqual(TEXT("Both abilities consume their points"), State->GetUnspentAbilityPoints(), 5);
	TestTrue(TEXT("Two health ranks add exactly four health"), FMath::IsNearlyEqual(Health->GetMaxHealth(), BaseHealth + 4.0f));
	TestTrue(TEXT("Health upgrade preserves existing missing health"), FMath::IsNearlyEqual(Health->GetHealth(), Health->GetMaxHealth() - 2.0f));
	TestTrue(TEXT("Three defense ranks subtract 1.5 from a six-damage hit"), FMath::IsNearlyEqual(Health->ApplyDamage(6.0f, nullptr, nullptr), 4.5f, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Defense retains at least one damage on small normal hits"), FMath::IsNearlyEqual(Health->ApplyDamage(2.0f, nullptr, nullptr), 1.0f, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Sub-one incoming damage is never increased by the minimum"), FMath::IsNearlyEqual(Health->ApplyDamage(0.25f, nullptr, nullptr), 0.25f, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Armor bypass still bypasses ability defense"), FMath::IsNearlyEqual(Health->ApplyDamage(2.0f, nullptr, nullptr, true), 2.0f));
	const float InjuredHealth = Health->GetHealth();
	State->GrantExperience(1);
	TestEqual(TEXT("Unrelated progression updates do not heal"), Health->GetHealth(), InjuredHealth);
	Allocation = FJTSAbilityAllocation();
	Allocation.HealthRanks = MAX_int32;
	TestFalse(TEXT("Untrusted oversized rank is rejected without overflow"), State->CommitAbilityAllocation(Allocation));
	Allocation.HealthRanks = -1;
	TestFalse(TEXT("Negative rank is rejected"), State->CommitAbilityAllocation(Allocation));
	Allocation.HealthRanks = 0;
	Allocation.StackLimitRanks = 1;
	TestFalse(TEXT("Retired stack allocation is rejected"), State->CommitAbilityAllocation(Allocation));
	Allocation.StackLimitRanks = 0;
	Allocation.DefenseRanks = 3;
	TestFalse(TEXT("Ability rank cannot exceed five"), State->CommitAbilityAllocation(Allocation));
	AJTSPlayerState* TravelState = Fixture.World->SpawnActor<AJTSPlayerState>();
	State->CopyProperties(TravelState);
	TestEqual(TEXT("Health rank survives seamless travel"), TravelState->GetAbilityRank(EJTSPlayerAbility::Health), 2);
	TestEqual(TEXT("Defense rank survives seamless travel"), TravelState->GetAbilityRank(EJTSPlayerAbility::Defense), 3);
	AJTSPlayerState* RestoredState = Fixture.World->SpawnActor<AJTSPlayerState>();
	RestoredState->OverrideWith(TravelState);
	TestEqual(TEXT("Inactive player restore retains defense"), RestoredState->GetAbilityRank(EJTSPlayerAbility::Defense), 3);
	State->RestoreProgression(11, 0, 2, 0, 4, 0, 0, 0, 2, 3);
	TestEqual(TEXT("Old stack points are refunded"), State->GetUnspentAbilityPoints(), 6);
	TestEqual(TEXT("Retired rank stays zero in new snapshots"), State->GetAbilityRank(EJTSPlayerAbility::StackLimit), 0);
	TestEqual(TEXT("Health snapshot rank restores"), State->GetAbilityRank(EJTSPlayerAbility::Health), 2);
	TestEqual(TEXT("Stack cap stays 99 after old-save restore"), State->GetItemStackLimit(), 99);
	Health->ApplyDamage(1000.0f, nullptr, nullptr, true);
	Allocation = FJTSAbilityAllocation();
	Allocation.HealthRanks = 1;
	TestTrue(TEXT("Server can record another health rank"), State->CommitAbilityAllocation(Allocation));
	TestTrue(TEXT("Health upgrade cannot revive a dead player"), Health->IsDead());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSFixedAbilityGrowthTest, "JTS.Progression.FixedGrowthAndCombatBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSFixedAbilityGrowthTest::RunTest(const FString& Parameters)
{
	FAbilityTestWorld Fixture;
	APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>();
	AJTSPlayerState* State = Fixture.World->SpawnActor<AJTSPlayerState>();
	Controller->SetPlayerState(State);
	State->SetOwner(Controller);
	AJTSCharacter* Character = Fixture.World->SpawnActor<AJTSCharacter>();
	// Exercise a different Blueprint-authored base: additive health must not scale with that base.
	FFloatProperty* BaseHealthProperty = FindFProperty<FFloatProperty>(AJTSCharacter::StaticClass(), TEXT("PlayerMaxHealth"));
	if (!TestNotNull(TEXT("Character exposes authored base health"), BaseHealthProperty)) return false;
	BaseHealthProperty->SetPropertyValue_InContainer(Character, 50.0f);
	Character->DispatchBeginPlay();
	Controller->Possess(Character);
	State->GrantDebugLevels();
	FJTSAbilityAllocation Allocation;
	Allocation.HealthRanks = 2;
	Allocation.RunSpeedRanks = 2;
	Allocation.StaminaRanks = 2;
	Allocation.InventorySlotRanks = 2;
	Allocation.CriticalChanceRanks = 2;
	TestTrue(TEXT("Server applies mixed fixed-growth abilities"), State->CommitAbilityAllocation(Allocation));
	TestEqual(TEXT("Changing base health to 50 still adds only four health"), Character->GetHealthComponent()->GetMaxHealth(), 54.0f);
	TestEqual(TEXT("Normal movement receives a flat 40 cm/s bonus"), Character->GetCharacterMovement()->MaxWalkSpeed, 540.0f);
	bool bSprintRequested = true;
	Character->ProcessEvent(Character->FindFunctionChecked(TEXT("ServerSetSprintIntent")), &bSprintRequested);
	TestEqual(TEXT("Sprint receives the same flat 40 cm/s bonus"), Character->GetCharacterMovement()->MaxWalkSpeed, 840.0f);
	TestEqual(TEXT("Two stamina ranks add forty capacity"), Character->GetStaminaComponent()->GetMaxStamina(), 140.0f);
	TestEqual(TEXT("Two cargo ranks add four slots"), State->GetInventorySlotCapacityBonus(), 4);
	TestEqual(TEXT("Critical chance still grows in percentage points"), State->GetCriticalChancePercent(), 10);

	// Validate the current lunar damage budget through the actual damage/death path.
	BaseHealthProperty->SetPropertyValue_InContainer(Character, 10.0f);
	State->RestoreProgression(11, 0, 0, 0, 0, 0, 0, 0, 5, 5);
	UJTSHealthComponent* Health = Character->GetHealthComponent();
	TestEqual(TEXT("Max health is twenty at the current ten-health baseline"), Health->GetMaxHealth(), 20.0f);
	Health->RestoreAuthoritativeHealth(20.0f);
	for (int32 Hit = 1; Hit <= 6; ++Hit)
	{
		Health->ApplyDamage(6.0f, nullptr, nullptr);
		TestEqual(FString::Printf(TEXT("Six-damage attack: death only on hit six (%d)"), Hit), Health->IsDead(), Hit == 6);
	}
	Health->RestoreAuthoritativeHealth(20.0f);
	for (int32 Hit = 1; Hit <= 3; ++Hit)
	{
		Health->ApplyDamage(12.0f, nullptr, nullptr);
		TestEqual(FString::Printf(TEXT("Twelve-damage attack: death only on hit three (%d)"), Hit), Health->IsDead(), Hit == 3);
	}
	return true;
}
#endif
