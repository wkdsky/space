#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "InputMappingContext.h"
#include "InputKeyEventArgs.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"
#include "TimerManager.h"
#include "space/Core/JTSGameState.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSRangedWeaponComponent.h"
#include "space/Components/JTSStellarLoadoutComponent.h"
#include "space/Components/JTSStellarTargetComponent.h"
#include "space/Components/JTSStellarWeaponComponent.h"
#include "space/Components/JTSWeaponVisualComponent.h"
#include "space/Items/JTSStellarLootTable.h"
#include "space/Items/JTSStellarProgression.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Weapons/JTSStellarEffectActor.h"
#include "space/Weapons/JTSBlackHoleField.h"
#include "space/UI/JTSStellarAttachmentDialog.h"

namespace
{
	struct FStellarWorld
	{
		UWorld* World;
		AJTSGameState* State;
		UJTSStellarLootTable* Table;
		uint64 OriginalFrameCounter = GFrameCounter;
		FStellarWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->InitializeActorsForPlay(FURL());
			State = World->SpawnActor<AJTSGameState>(); World->SetGameState(State);
			State->SetGameplayPhase(EJTSGameplayPhase::EarthCollection);
			Table = NewObject<UJTSStellarLootTable>(World);
			for (const TCHAR* Core : {TEXT("FireCore"), TEXT("LightCore"), TEXT("DarkCore")})
			{
				FJTSStellarLootEntry Entry; Entry.ItemId = Core; Entry.bCore = true; Table->Entries.Add(Entry);
			}
			const TCHAR* Cores[] = {TEXT("FireCore"), TEXT("LightCore"), TEXT("DarkCore")};
			const TCHAR* Attachments[] = {TEXT("JetTube"), TEXT("FocusTube"), TEXT("BlackHoleTube")};
			for (int32 Index=0;Index<3;++Index)
			{
				FJTSStellarLootEntry Entry; Entry.ItemId=Attachments[Index]; Entry.CompatibleCoreId=Cores[Index]; Table->Entries.Add(Entry);
			}
		}
		~FStellarWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = OriginalFrameCounter; }
		AJTSPlayerState* AddPlayer()
		{
			auto* PS = World->SpawnActor<AJTSPlayerState>(); PS->SetExpeditionStatus(EJTSPlayerExpeditionStatus::Active);
			State->AddPlayerState(PS); PS->GetStellarLoadout()->ConfigureLootTable(Table); return PS;
		}
		AJTSCharacter* AddCharacter(AJTSPlayerState* PS, EJTSStellarWeaponMode Mode)
		{
			auto* PC=World->SpawnActor<AJTSPlayerController>(); PC->SetOwner(nullptr); PS->SetOwner(PC);
			FindFProperty<FObjectProperty>(PC->GetClass(),TEXT("PlayerState"))->SetObjectPropertyValue_InContainer(PC,PS);
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Character=World->SpawnActor<AJTSCharacter>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
			PC->Possess(Character);
			FindFProperty<FObjectProperty>(Character->GetClass(),TEXT("PlayerState"))->SetObjectPropertyValue_InContainer(Character,PS);
			auto* Catalog=NewObject<UJTSStellarWeaponCatalog>(World); Catalog->LootTable=Table;
			FJTSStellarWeaponDefinition Def; Def.Mode=Mode; Def.StatusDamagePerSecond=0;
			Def.CoreId=Mode==EJTSStellarWeaponMode::Jet?TEXT("FireCore"):Mode==EJTSStellarWeaponMode::Focus?TEXT("LightCore"):TEXT("DarkCore");
			Def.AttachmentId=Mode==EJTSStellarWeaponMode::Jet?TEXT("JetTube"):Mode==EJTSStellarWeaponMode::Focus?TEXT("FocusTube"):TEXT("BlackHoleTube");
			Def.BaseDamage=Mode==EJTSStellarWeaponMode::Focus?56:Mode==EJTSStellarWeaponMode::Jet?300:150;
			Def.RangeCentimeters=Mode==EJTSStellarWeaponMode::Jet?600:3000;
			Def.EnergyPerSecond=Mode==EJTSStellarWeaponMode::BlackHole?18:22;
			Catalog->Weapons.Add(Def);
			Character->FindComponentByClass<UJTSStellarWeaponComponent>()->WeaponCatalog=Catalog;
			Character->DispatchBeginPlay();
			Character->GetCharacterMovement()->DisableMovement();
			TArray<FJTSItemInstance> Slots; Slots.SetNum(9); Slots[1]=Table->MakeTextItem(Def.CoreId); Slots[2]=Table->MakeTextItem(Def.AttachmentId);
			PS->GetStellarLoadout()->RestoreState(Slots,100); PS->GetStellarLoadout()->SelectWeapon(1);
			return Character;
		}
		AActor* Target(FVector Location, EJTSStellarTargetTier Tier=EJTSStellarTargetTier::Normal)
		{
			auto* Actor=World->SpawnActor<AActor>();
			auto* Box=NewObject<UBoxComponent>(Actor); Actor->SetRootComponent(Box); Actor->AddInstanceComponent(Box);
			Box->SetBoxExtent(FVector(20)); Box->SetCollisionObjectType(ECC_WorldDynamic);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionResponseToAllChannels(ECR_Ignore);
			Box->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block); Box->RegisterComponent(); Actor->SetActorLocation(Location);
			auto* Health=NewObject<UJTSHealthComponent>(Actor); Actor->AddInstanceComponent(Health); Health->RegisterComponent(); Health->SetMaxHealth(10000);
			auto* Target=NewObject<UJTSStellarTargetComponent>(Actor); Actor->AddInstanceComponent(Target);
			auto* TierProperty=FindFProperty<FEnumProperty>(Target->GetClass(),TEXT("Tier"));
			TierProperty->GetUnderlyingProperty()->SetIntPropertyValue(TierProperty->ContainerPtrToValuePtr<void>(Target),static_cast<uint64>(Tier));
			Target->RegisterComponent(); Actor->DispatchBeginPlay(); return Actor;
		}
		AActor* Ground(FVector Center = FVector(0,0,-20), FVector Extent = FVector(10000,10000,20))
		{
			auto* Actor = World->SpawnActor<AActor>();
			auto* Box = NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Box); Actor->AddInstanceComponent(Box);
			Box->SetBoxExtent(Extent); Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->RegisterComponent(); Actor->SetActorLocation(Center);
			return Actor;
		}
		void Step(float Seconds, UJTSStellarWeaponComponent* Weapon=nullptr)
		{
			if (!World->HasBegunPlay()) { World->BeginPlay(); State->HandleBeginPlay(); World->GetTimerManager().Tick(0); }
			for(int32 Index=0;Index<FMath::CeilToInt(Seconds/0.05f);++Index)
			{
				if(Weapon) Weapon->ServerUpdateAim(FVector::ForwardVector);
				// Synchronous world steps need distinct frames: UE timers tick at most once per GFrameCounter.
				++GFrameCounter;
				World->Tick(LEVELTICK_All,0.05f);
			}
		}
	};
	void CastAt(AJTSCharacter* Character, FVector Point, bool bSecondary = false)
	{
		const FVector Direction = (Point - Character->GetPawnViewLocation()).GetSafeNormal();
		Character->GetController()->SetControlRotation(Direction.Rotation());
		auto* Weapon = Character->FindComponentByClass<UJTSStellarWeaponComponent>();
		const auto Binding = Weapon->GetLoadout()->GetWeapons()[0];
		Weapon->ServerSetInput(true, bSecondary, Binding.CoreInstanceId, Binding.AttachmentInstanceId, Direction, Character->GetPawnViewLocation());
	}
	void ReleaseCast(UJTSStellarWeaponComponent* Weapon)
	{
		Weapon->ServerSetInput(false, false, FGuid(), FGuid(), FVector::ForwardVector, FVector::ZeroVector);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNormalWeaponReloadShortcutTest, "JTS.Character.NormalWeaponReloadShortcut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNormalWeaponReloadShortcutTest::RunTest(const FString&)
{
	FStellarWorld Fixture;
	auto* State = Fixture.AddPlayer();
	auto* Character = Fixture.AddCharacter(State, EJTSStellarWeaponMode::Jet);
	Fixture.Step(0.05f);
	State->GetStellarLoadout()->SelectWeapon(INDEX_NONE);
	auto* Inventory = Character->GetInventoryComponent();
	if (!TestTrue(TEXT("Ordinary rifle added"), Inventory->TryAddItemById(EJTSItemId::AssaultRifle))) return false;
	Inventory->SelectQuickbarSlot(0);
	auto* Ranged = Character->FindComponentByClass<UJTSRangedWeaponComponent>();
	const int32 Magazine = Ranged->GetMagazineSize();
	Ranged->StartFire(); Ranged->StopFire();
	if (!TestEqual(TEXT("One ordinary shot spends one round"), Ranged->GetAmmo(), Magazine - 1)) return false;
	Character->HandleWeaponUtilityStarted(FInputActionValue(true));
	TestTrue(TEXT("Z utility starts a manual ordinary reload"), Ranged->IsReloading());
	TestEqual(TEXT("Ready stellar pair is not selected by ordinary reload"), State->GetStellarLoadout()->GetActiveCoreSlot(), INDEX_NONE);
	TestEqual(TEXT("Manual reload does not instantly refill"), Ranged->GetAmmo(), Magazine - 1);
	Fixture.Step(5.0f);
	TestEqual(TEXT("Server timer refills the ordinary magazine"), Ranged->GetAmmo(), Magazine);
	TestFalse(TEXT("Reload ends"), Ranged->IsReloading());
	Character->HandleWeaponUtilityStarted(FInputActionValue(true));
	TestFalse(TEXT("Full magazine refuses redundant reload"), Ranged->IsReloading());
	State->GetStellarLoadout()->SelectWeapon(1);
	Character->HandleWeaponUtilityStarted(FInputActionValue(true));
	TestEqual(TEXT("Stellar Z retains stellar selection"), State->GetStellarLoadout()->GetActiveCoreSlot(), 1);
	TestFalse(TEXT("Stellar Z does not reload the ordinary weapon"), Ranged->IsReloading());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarKeyboardSelectionTest, "JTS.Stellar.ActivatedWeaponKeyboardSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStellarKeyboardSelectionTest::RunTest(const FString&)
{
	FStellarWorld Fixture;
	auto* State = Fixture.AddPlayer();
	auto* Character = Fixture.AddCharacter(State, EJTSStellarWeaponMode::Jet);
	Fixture.Step(0.05f);
	Cast<APlayerController>(Character->GetController())->SetAsLocalPlayerController();
	Character->PawnClientRestart();
	auto* Input = Cast<UEnhancedInputComponent>(Character->InputComponent);
	const auto* Context = Cast<UInputMappingContext>(FindFProperty<FObjectProperty>(Character->GetClass(),
		TEXT("InputMappingContext"))->GetObjectPropertyValue_InContainer(Character));
	if (!TestNotNull(TEXT("Character uses enhanced input"), Input)
		|| !TestNotNull(TEXT("Character input mapping exists"), Context)) return false;
	// Exercise the real key mapping and its bound handler, rather than calling the selection component directly.
	const auto PressKey = [&](FKey Key)
	{
		for (const auto& Mapping : Context->GetMappings()) if (Mapping.Key == Key)
		{
			for (const auto& Binding : Input->GetActionEventBindings())
				if (Binding->GetAction() == Mapping.Action && Binding->GetTriggerEvent() == ETriggerEvent::Started)
				{
					Binding->Execute(FInputActionInstance(Mapping.Action));
					return true;
				}
		}
		return false;
	};
	auto* Loadout = State->GetStellarLoadout();
	auto* Inventory = Character->GetInventoryComponent();
	if (!TestTrue(TEXT("Ordinary rifle added"), Inventory->TryAddItemById(EJTSItemId::AssaultRifle))) return false;
	Inventory->SelectQuickbarSlot(0);
	Loadout->SelectWeapon(INDEX_NONE);
	FInputKeyEventArgs TabEvent;
	TabEvent.Key = EKeys::Tab; TabEvent.Event = IE_Pressed;
	TestFalse(TEXT("Controller leaves gameplay Tab to enhanced input"), Cast<APlayerController>(Character->GetController())->InputKey(TabEvent));
	if (!TestTrue(TEXT("Tab has a started action binding"), PressKey(EKeys::Tab))) return false;
	TestEqual(TEXT("Tab selects first activated pair from an ordinary rifle"), Loadout->GetActiveCoreSlot(), 1);
	TestFalse(TEXT("Tab does not reload the ordinary rifle"), Character->FindComponentByClass<UJTSRangedWeaponComponent>()->IsReloading());
	PressKey(EKeys::Tab);
	TestEqual(TEXT("A single activated pair cycles to itself"), Loadout->GetActiveCoreSlot(), 1);
	PressKey(EKeys::One);
	TestEqual(TEXT("Selecting the same ordinary slot still leaves stellar mode"), Loadout->GetActiveCoreSlot(), INDEX_NONE);
	TestEqual(TEXT("Key 1 selects ordinary slot 1"), Inventory->GetSelectedQuickbarSlot(), 0);

	// Columns 1 and 3 are invalid, column 2 is complete, and column 4 lacks its lower attachment.
	// The compatible attachment in the orange spare cannot activate column 4.
	TArray<FJTSItemInstance> Slots; Slots.SetNum(9);
	Slots[0] = Fixture.Table->MakeTextItem(TEXT("JetTube"));
	Slots[1] = Fixture.Table->MakeTextItem(TEXT("FireCore"));
	Slots[2] = Fixture.Table->MakeTextItem(TEXT("FocusTube"));
	Slots[3] = Fixture.Table->MakeTextItem(TEXT("FireCore"));
	Slots[4] = Fixture.Table->MakeTextItem(TEXT("JetTube"));
	Slots[5] = Fixture.Table->MakeTextItem(TEXT("JetTube"));
	Slots[6] = Fixture.Table->MakeTextItem(TEXT("FireCore"));
	Slots[7] = Fixture.Table->MakeTextItem(TEXT("FireCore"));
	Loadout->RestoreState(Slots, 100);
	PressKey(EKeys::Tab);
	TestEqual(TEXT("Tab skips wrong, reversed, incomplete and spare combinations"), Loadout->GetActiveCoreSlot(), 3);
	Slots[5] = Fixture.Table->MakeTextItem(TEXT("LightCore"));
	Slots[6] = Fixture.Table->MakeTextItem(TEXT("FocusTube"));
	Slots[8] = Fixture.Table->MakeTextItem(TEXT("JetTube"));
	Loadout->RestoreState(Slots, 100); Loadout->SelectWeapon(INDEX_NONE);
	PressKey(EKeys::Tab);
	TestEqual(TEXT("First Tab selects leftmost activated pair"), Loadout->GetActiveCoreSlot(), 3);
	PressKey(EKeys::Tab);
	TestEqual(TEXT("Second Tab selects the next activated pair across inactive columns"), Loadout->GetActiveCoreSlot(), 7);
	PressKey(EKeys::Tab);
	TestEqual(TEXT("Tab wraps to the first activated pair"), Loadout->GetActiveCoreSlot(), 3);
	PressKey(EKeys::Two);
	TestEqual(TEXT("Key 2 leaves stellar mode"), Loadout->GetActiveCoreSlot(), INDEX_NONE);
	TestEqual(TEXT("Empty ordinary slots are still selectable"), Inventory->GetSelectedQuickbarSlot(), 1);
	PressKey(EKeys::Tab);
	TestEqual(TEXT("Returning from an ordinary slot starts at the first activated pair"), Loadout->GetActiveCoreSlot(), 3);
	PressKey(EKeys::Nine);
	TestEqual(TEXT("A locked ordinary slot does not change stellar selection"), Loadout->GetActiveCoreSlot(), 3);

	// Temporarily configure nine ordinary slots in this test world; production progression is unchanged.
	FindFProperty<FIntProperty>(Inventory->GetClass(), TEXT("BaseInventoryCapacity"))->SetPropertyValue_InContainer(Inventory, 9);
	Inventory->RefreshCapacityFromProgression();
	const FKey NumberKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
		EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(NumberKeys); ++Index)
	{
		PressKey(EKeys::Tab);
		if (!TestTrue(TEXT("Every number key has a started action binding"), PressKey(NumberKeys[Index]))) return false;
		TestEqual(TEXT("Number key returns to its corresponding ordinary slot"), Inventory->GetSelectedQuickbarSlot(), Index);
		TestEqual(TEXT("Number key clears stellar selection"), Loadout->GetActiveCoreSlot(), INDEX_NONE);
	}
	Slots[4].Clear(); Slots[8].Clear();
	Loadout->RestoreState(Slots, 100);
	PressKey(EKeys::Tab);
	TestEqual(TEXT("No activated pair leaves normal mode selected"), Loadout->GetActiveCoreSlot(), INDEX_NONE);
	TestEqual(TEXT("No activated pair preserves the ordinary selection"), Inventory->GetSelectedQuickbarSlot(), 8);
	Slots[1] = Fixture.Table->MakeTextItem(TEXT("FireCore"));
	Slots[2] = Fixture.Table->MakeTextItem(TEXT("JetTube"));
	Slots[4] = Fixture.Table->MakeTextItem(TEXT("JetTube"));
	Slots[8] = Fixture.Table->MakeTextItem(TEXT("JetTube"));
	Loadout->RestoreState(Slots, 100); Loadout->SelectWeapon(7);
	Fixture.AddPlayer(); Fixture.AddPlayer(); Fixture.AddPlayer();
	PressKey(EKeys::Tab);
	TestEqual(TEXT("Server refreshes participant limits before cycling and excludes sealed columns"), Loadout->GetActiveCoreSlot(), 1);
	Fixture.State->SetGameplayPhase(EJTSGameplayPhase::WaitingToStart);
	PressKey(EKeys::Tab); PressKey(EKeys::One);
	TestEqual(TEXT("Gameplay shortcuts are blocked outside active gameplay"), Loadout->GetActiveCoreSlot(), 1);
	TestEqual(TEXT("Blocked number key preserves ordinary selection"), Inventory->GetSelectedQuickbarSlot(), 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarHeldWeaponPresentationTest, "JTS.Stellar.HeldWeaponPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStellarHeldWeaponPresentationTest::RunTest(const FString&)
{
	// Asset references are intentionally confined to this integration test; runtime uses Blueprint configuration.
	const auto* AuthoredCatalog = LoadObject<UJTSStellarWeaponCatalog>(nullptr,
		TEXT("/Game/Space/Data/Weapons/DA_StellarWeaponCatalog.DA_StellarWeaponCatalog"));
	if (!TestNotNull(TEXT("Configured stellar catalog exists"), AuthoredCatalog)) return false;
	const auto* AuthoredTable = AuthoredCatalog->LootTable.LoadSynchronous();
	if (!TestNotNull(TEXT("Configured stellar loot table exists"), AuthoredTable)) return false;
	FStellarWorld Fixture;
	Fixture.Table->Entries = AuthoredTable->Entries;
	auto* State = Fixture.AddPlayer();
	auto* Character = Fixture.AddCharacter(State, EJTSStellarWeaponMode::Jet);
	auto* Stellar = Character->FindComponentByClass<UJTSStellarWeaponComponent>();
	auto* Visual = Character->FindComponentByClass<UJTSWeaponVisualComponent>();
	auto* Catalog = Stellar->WeaponCatalog.Get();
	Catalog->Weapons = AuthoredCatalog->Weapons;
	auto* Loadout = State->GetStellarLoadout();
	Fixture.Step(0.05f);
	TSet<UStaticMesh*> DistinctMeshes;
	int32 MatchingAttachments = 0;
	for (const auto& Entry : AuthoredTable->Entries)
		if (!Entry.bCore && !Entry.CompatibleCoreId.IsNone())
		{
			++MatchingAttachments;
			TestNotNull(TEXT("Every activated loot combination is registered for selection"),
				Catalog->Find(Entry.CompatibleCoreId, Entry.ItemId));
		}
	TestEqual(TEXT("Every matching attachment has its own catalog definition"), Catalog->Weapons.Num(), MatchingAttachments);
	for (const auto& Definition : Catalog->Weapons)
	{
		const auto* Def = &Definition;
		auto* Mesh = Def->HeldMesh.LoadSynchronous();
		if (!TestNotNull(TEXT("Each representative weapon has a real held mesh"), Mesh)) return false;
		DistinctMeshes.Add(Mesh);
		TestTrue(TEXT("Held model uses plausible centimetre dimensions"), Mesh->GetBounds().BoxExtent.GetMax() < 50);
		TestTrue(TEXT("Every stellar model uses an upright scepter presentation"), Def->bUprightScepter && !Def->bMeleePresentation);
		TestTrue(TEXT("Core emitter is above the grip"), Def->HeldMuzzleTransform.GetLocation().Z > 30);
		TestTrue(TEXT("The short shaft is the model's longest dimension"), Mesh->GetBounds().BoxExtent.Z > Mesh->GetBounds().BoxExtent.X * 2);
		TestTrue(TEXT("Core glow slot exists"), Def->CoreMaterialSlot >= 0 && Def->CoreMaterialSlot < Mesh->GetStaticMaterials().Num());
		TArray<FJTSItemInstance> Slots; Slots.SetNum(9);
		Slots[1] = Fixture.Table->MakeTextItem(Def->CoreId);
		Slots[2] = Fixture.Table->MakeTextItem(Def->AttachmentId);
		Loadout->RestoreState(Slots, 100);
		Stellar->CycleWeapon();
		TestEqual(TEXT("Tab cycling selects every activated catalog combination, including explosion"), Loadout->GetActiveCoreSlot(), 1);
		auto* Held = Visual->GetStellarHeldMesh();
		if (!TestNotNull(TEXT("Held mesh component is created"), Held)) return false;
		TestTrue(TEXT("Selecting a stellar pair equips its corresponding model"), Held->GetStaticMesh() == Mesh);
		Visual->SetAimAlpha(1.0f);
		TestTrue(TEXT("Scepter remains vertical while aiming"), Held->GetUpVector().Equals(Character->GetActorUpVector(), 0.001));
		Visual->SetAimAlpha(0.0f);
		TestTrue(TEXT("Selected model is visible immediately"), Held->IsVisible() && !Held->bHiddenInGame);
		TestTrue(TEXT("Hand pose updates stay enabled while holding a stellar weapon"), Visual->IsComponentTickEnabled());
		TestTrue(TEXT("Skeletal import scale is excluded from weapon size"), Held->GetComponentScale().Equals(FVector::OneVector, 0.001));
		FVector Muzzle;
		TestTrue(TEXT("Stellar model exposes a visible muzzle"), Visual->GetMuzzleWorldLocation(Muzzle));
		TestTrue(TEXT("Visible muzzle matches the model's authored pivot"), Muzzle.Equals(
			Held->GetComponentTransform().TransformPosition(Def->HeldMuzzleTransform.GetLocation()), 0.01));
		const auto* Anchor = Held->GetAttachParent()->GetAttachParent()->GetAttachParent();
		TestTrue(TEXT("Authored grip remains pinned to the hand anchor"), Held->GetComponentTransform()
			.TransformPosition(Def->HeldGripTransform.GetLocation()).Equals(Anchor->GetComponentLocation(), 0.01));
		Visual->SetClimbStowed(true);
		TestFalse(TEXT("Climbing stows the stellar model"), Held->IsVisible());
		TestFalse(TEXT("Stowed model has no visible muzzle"), Visual->GetMuzzleWorldLocation(Muzzle));
		Visual->SetClimbStowed(false);
		TestTrue(TEXT("Ending a climb restores the selected stellar model"), Held->IsVisible());
		Held->SetHiddenInGame(true);
		Visual->RestoreAfterCharacterMeshShown();
		TestTrue(TEXT("Restoring the character also restores its stellar model"), !Held->bHiddenInGame && Held->IsVisible());
		if (Def->Mode == EJTSStellarWeaponMode::PresentationOnly)
		{
			Stellar->SetPrimary(true);
			Fixture.Step(0.1f, Stellar);
			TestEqual(TEXT("Unfinished attacks do not run another weapon's combat or consume energy"), Loadout->GetEnergy(), 100.0f);
			Stellar->SetPrimary(false);
		}
		Stellar->ReturnToNormalWeapon();
		TestFalse(TEXT("Returning to a normal slot removes the stellar model"), Held->IsVisible());
	}
	TestEqual(TEXT("Every stellar combination has a distinct model asset"), DistinctMeshes.Num(), MatchingAttachments);

	// Simulated proxies never receive the owner's private slots, but still see every equipped combination.
	TArray<FJTSItemInstance> EmptySlots; EmptySlots.SetNum(9); Loadout->RestoreState(EmptySlots, 100);
	FindFProperty<FByteProperty>(Character->GetClass(), TEXT("Role"))->SetPropertyValue_InContainer(Character, ROLE_SimulatedProxy);
	auto* Replicated = FindFProperty<FStructProperty>(Stellar->GetClass(), TEXT("EquippedVisual"))
		->ContainerPtrToValuePtr<FJTSStellarEquippedVisual>(Stellar);
	for (const auto& Def : Catalog->Weapons)
	{
		Replicated->CoreId = Def.CoreId; Replicated->AttachmentId = Def.AttachmentId;
		Stellar->ProcessEvent(Stellar->FindFunctionChecked(TEXT("OnRep_Equipped")), nullptr);
		TestTrue(TEXT("Remote character knows a stellar weapon is equipped"), Stellar->HasActiveWeapon());
		TestTrue(TEXT("Remote character changes to the replicated combination's model"),
			Visual->GetStellarHeldMesh()->GetStaticMesh() == Def.HeldMesh.Get());
		TestTrue(TEXT("Remote model stays visible without private loadout replication"), Visual->GetStellarHeldMesh()->IsVisible());
	}
	Replicated->CoreId = NAME_None; Replicated->AttachmentId = NAME_None;
	Stellar->ProcessEvent(Stellar->FindFunctionChecked(TEXT("OnRep_Equipped")), nullptr);
	TestFalse(TEXT("Remote stellar model is hidden after unequipping"), Visual->GetStellarHeldMesh()->IsVisible());
	FindFProperty<FByteProperty>(Character->GetClass(), TEXT("Role"))->SetPropertyValue_InContainer(Character, ROLE_Authority);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarAdjacencyTest,"JTS.Stellar.AdjacencyAndFractionalBudget",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStellarAdjacencyTest::RunTest(const FString&)
{
	FStellarWorld W;
	TArray<FJTSItemInstance> Slots; Slots.SetNum(9);
	Slots[1]=W.Table->MakeTextItem(TEXT("LightCore")); Slots[1].StellarCoreLevel=19;
	Slots[2]=W.Table->MakeTextItem(TEXT("FocusTube")); Slots[2].StellarPoints={10,8,6,4,2,0};
	auto Weapons=FJTSStellarLoadoutRules::Assemble(Slots,9,W.Table);
	TestEqual(TEXT("Core circle and its lower diamond assemble"),Weapons.Num(),1);
	if(Weapons.Num()!=1)return false;
	TestEqual(TEXT("18/30 scaling preserves fractions"),Weapons[0].EffectiveLevels[1],4.8);
	TestEqual(TEXT("Recorded points are untouched"),Slots[2].StellarPoints[1],uint8(8));
	TestEqual(TEXT("A core across the sealed boundary is invalid"),FJTSStellarLoadoutRules::Assemble(Slots,2,W.Table).Num(),0);
	Swap(Slots[1],Slots[2]); TestEqual(TEXT("Reversed parts cannot assemble"),FJTSStellarLoadoutRules::Assemble(Slots,9,W.Table).Num(),0);
	Swap(Slots[1],Slots[2]);
	Slots[0]=Slots[1]; Slots[1]=Slots[2]; Slots[2].Clear();
	TestEqual(TEXT("Orange spare never activates with a nearby attachment"),FJTSStellarLoadoutRules::Assemble(Slots,9,W.Table).Num(),0);
	Slots[0].Clear(); Slots[1].Clear(); Slots[2]=W.Table->MakeTextItem(TEXT("LightCore")); Slots[3]=W.Table->MakeTextItem(TEXT("FocusTube"));
	TestEqual(TEXT("Adjacent slots from different columns cannot assemble"),FJTSStellarLoadoutRules::Assemble(Slots,9,W.Table).Num(),0);
	Slots[2].Clear(); Slots[3].Clear();
	for(int32 Pair=0;Pair<4;++Pair) { Slots[1+2*Pair]=W.Table->MakeTextItem(TEXT("LightCore")); Slots[2+2*Pair]=W.Table->MakeTextItem(TEXT("FocusTube")); }
	for(int32 Count=1;Count<=4;++Count)
	{
		const int32 Available=FJTSStellarLoadoutRules::AvailableSlots(Count);
		TestEqual(TEXT("Participant slot formula"),Available,11-Count*2);
		TestEqual(TEXT("Only visible vertical pairs activate"),FJTSStellarLoadoutRules::Assemble(Slots,Available,W.Table).Num(),5-Count);
	}
	Slots[2]=W.Table->MakeTextItem(TEXT("JetTube"));
	TestEqual(TEXT("A wrong attachment leaves that column inactive"),FJTSStellarLoadoutRules::Assemble(Slots,9,W.Table).Num(),3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarSealingTest,"JTS.Stellar.ParticipantsSealingAndPersistence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStellarSealingTest::RunTest(const FString&)
{
	FStellarWorld W; auto* PS=W.AddPlayer(); auto* Loadout=PS->GetStellarLoadout();
	TArray<FJTSItemInstance> Slots; Slots.SetNum(9); Slots[3]=W.Table->MakeTextItem(TEXT("FireCore")); Slots[4]=W.Table->MakeTextItem(TEXT("JetTube"));
	Slots[3].StellarCoreLevel=19; Slots[4].StellarPoints={10,8,6,4,2,0};
	Loadout->RestoreState(Slots,63); Loadout->SelectWeapon(3);
	TestEqual(TEXT("Solo pair selected"),Loadout->GetActiveCoreSlot(),3);
	TArray<AJTSPlayerState*> Added; for(int32 Index=0;Index<3;++Index)Added.Add(W.AddPlayer()); Loadout->RefreshParticipants();
	TestEqual(TEXT("Four participants seal to three slots"),Loadout->GetAvailableSlots(),3);
	TestEqual(TEXT("Cross-boundary channel deselected"),Loadout->GetActiveCoreSlot(),INDEX_NONE);
	TestEqual(TEXT("Sealed item remains physically stored"),Loadout->GetSlot(4).InstanceId,Slots[4].InstanceId);
	Loadout->ServerApplyPoints(4,Slots[4].InstanceId,Loadout->GetRevision(),{0,0,0,0,0,0});
	TestEqual(TEXT("Sealed points are read-only"),Loadout->GetSlot(4).StellarPoints[0],uint8(10));
	Loadout->ServerMoveSlot(3,Slots[3].InstanceId,6,FGuid()); TestTrue(TEXT("Cannot insert into a sealed position"),Loadout->GetSlot(6).IsEmpty());
	for(auto* Other:Added)W.State->RemovePlayerState(Other); Loadout->RefreshParticipants();
	TestEqual(TEXT("Leaving reopens the same nine positions"),Loadout->GetAvailableSlots(),9);
	TestEqual(TEXT("Unlock does not auto select a channel"),Loadout->GetActiveCoreSlot(),INDEX_NONE);
	TestEqual(TEXT("Unlock does not refill energy"),Loadout->GetEnergy(),63.0f);
	Loadout->ServerMoveSlot(4,FGuid::NewGuid(),0,FGuid()); TestTrue(TEXT("Stale item identity cannot move an item"),Loadout->GetSlot(0).IsEmpty());
	Loadout->ServerMoveSlot(4,Slots[4].InstanceId,0,FGuid()); TestEqual(TEXT("Moves preserve attachment identity"),Loadout->GetSlot(0).InstanceId,Slots[4].InstanceId);
	auto* Copy=W.World->SpawnActor<AJTSPlayerState>(); PS->CopyProperties(Copy);
	TestEqual(TEXT("Seamless transfer preserves energy"),Copy->GetStellarLoadout()->GetEnergy(),63.0f);
	TestEqual(TEXT("Seamless transfer preserves allocation"),Copy->GetStellarLoadout()->GetSlot(0).StellarPoints[1],uint8(8));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarStatusTest,"JTS.Stellar.StatusCapsResistanceAndRadialControl",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStellarStatusTest::RunTest(const FString&)
{
	FStellarWorld W; auto* Source=W.World->SpawnActor<APawn>();
	AActor* Normal=W.Target(FVector(300,0,64)); auto* N=Normal->FindComponentByClass<UJTSStellarTargetComponent>();
	AActor* Boss=W.Target(FVector(500,0,64),EJTSStellarTargetTier::Boss); auto* B=Boss->FindComponentByClass<UJTSStellarTargetComponent>();
	N->ApplyFire(Source,100,0,0); B->ApplyFire(Source,100,0,0); W.Step(2.3f);
	TestTrue(TEXT("Ignited ordinary target executes after two seconds"),Normal->FindComponentByClass<UJTSHealthComponent>()->IsDead());
	TestFalse(TEXT("Boss cannot be executed"),Boss->FindComponentByClass<UJTSHealthComponent>()->IsDead());
	AActor* Target=W.Target(FVector(0,300,64)); auto* Status=Target->FindComponentByClass<UJTSStellarTargetComponent>();
	for(int32 Index=0;Index<4;++Index)Status->ApplyLightBurn(W.World->SpawnActor<APawn>(),10,1,0,0);
	W.Step(0.25f); TestEqual(TEXT("Four players only apply three light damage sources"),Target->FindComponentByClass<UJTSHealthComponent>()->GetHealth(),9994.0f);
	Status->ApplyAttraction(Source,Source,FVector(100,400,300),300,1000,0,0.4f);
	const FVector Velocity=Status->GetFieldAcceleration(Target->GetActorLocation(),FVector::RightVector);
	TestTrue(TEXT("Control stays tangent on a planet side"),FMath::Abs(FVector::DotProduct(Velocity,FVector::RightVector))<0.001f);
	Status->RemoveForce(Source); TestTrue(TEXT("Ending a field immediately removes its pull"),Status->GetFieldAcceleration(Target->GetActorLocation(),FVector::UpVector).IsZero());
	B->ApplyAttraction(Source,Source,FVector::ZeroVector,600,1000,0,0.4f); TestTrue(TEXT("Boss position resists gravity control"),B->GetFieldAcceleration(Boss->GetActorLocation(),FVector::UpVector).IsZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarFocusTest,"JTS.Stellar.FocusZeroPointsPiercingAndEnergy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStellarFocusTest::RunTest(const FString&)
{
	FStellarWorld W; auto* PS=W.AddPlayer(); auto* Character=W.AddCharacter(PS,EJTSStellarWeaponMode::Focus);
	auto* Weapon=Character->FindComponentByClass<UJTSStellarWeaponComponent>(); auto* Loadout=PS->GetStellarLoadout();
	auto* First=W.Target(FVector(300,0,Character->BaseEyeHeight)); auto* Second=W.Target(FVector(500,0,Character->BaseEyeHeight));
	const auto Binding=Loadout->GetWeapons()[0];
	Weapon->ServerSetInput(true,false,FGuid::NewGuid(),Binding.AttachmentInstanceId,FVector::ForwardVector,Character->GetPawnViewLocation()); W.Step(0.05f);
	TestEqual(TEXT("Forged active-core identity produces no damage"),First->FindComponentByClass<UJTSHealthComponent>()->GetHealth(),10000.0f);
	Weapon->ServerSetInput(true,false,Binding.CoreInstanceId,Binding.AttachmentInstanceId,FVector::ForwardVector,Character->GetPawnViewLocation()); W.Step(0.05f,Weapon);
	TestEqual(TEXT("Zero-point focus deals its configured damage"),First->FindComponentByClass<UJTSHealthComponent>()->GetHealth(),9944.0f);
	TestTrue(TEXT("Base focus pierces one enemy with attenuation"),Second->FindComponentByClass<UJTSHealthComponent>()->GetHealth()<10000);
	TestTrue(TEXT("Every shot pays the full 2.75 energy"),FMath::IsNearlyEqual(Loadout->GetEnergy(),97.25f));
	Weapon->ServerSetInput(false,false,FGuid(),FGuid(),FVector::ForwardVector,FVector::ZeroVector); W.Step(0.25f);
	TestEqual(TEXT("Released channel does not fire additional shots"),First->FindComponentByClass<UJTSHealthComponent>()->GetHealth(),9944.0f);
	Loadout->RestoreState(Loadout->GetSlots(),0); Loadout->SelectWeapon(1);
	Weapon->ServerSetInput(true,false,Binding.CoreInstanceId,Binding.AttachmentInstanceId,FVector::ForwardVector,Character->GetPawnViewLocation()); W.Step(0.1f,Weapon);
	TestEqual(TEXT("Zero energy cannot produce a final free shot"),First->FindComponentByClass<UJTSHealthComponent>()->GetHealth(),9944.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarAreaTest,"JTS.Stellar.JetChannel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStellarAreaTest::RunTest(const FString&)
{
	for(auto Mode:{EJTSStellarWeaponMode::Jet})
	{
		FStellarWorld W; auto* PS=W.AddPlayer(); auto* Character=W.AddCharacter(PS,Mode);
		auto* Weapon=Character->FindComponentByClass<UJTSStellarWeaponComponent>(); auto* Loadout=PS->GetStellarLoadout();
		auto* Target=W.Target(FVector(300,0,Character->BaseEyeHeight)); const auto Binding=Loadout->GetWeapons()[0];
		Weapon->ServerSetInput(true,Mode==EJTSStellarWeaponMode::BlackHole,Binding.CoreInstanceId,Binding.AttachmentInstanceId,FVector::ForwardVector,Character->GetPawnViewLocation());
		W.Step(0.25f,Weapon);
		TestTrue(TEXT("Zero-point area channel damages an opted-in target"),Target->FindComponentByClass<UJTSHealthComponent>()->GetHealth()<10000);
		Loadout->SelectWeapon(INDEX_NONE); const float Health=Target->FindComponentByClass<UJTSHealthComponent>()->GetHealth(); W.Step(0.3f);
		TestEqual(TEXT("Returning to ordinary weapon cancels area damage"),Target->FindComponentByClass<UJTSHealthComponent>()->GetHealth(),Health);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarJetFocusTest,"JTS.Stellar.JetSecondaryConeAndRelease",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStellarJetFocusTest::RunTest(const FString&)
{
	const auto* Authored = LoadObject<UJTSStellarWeaponCatalog>(nullptr,
		TEXT("/Game/Space/Data/Weapons/DA_StellarWeaponCatalog.DA_StellarWeaponCatalog"));
	if (!TestNotNull(TEXT("Authored catalog exists"), Authored)) return false;
	const auto* Def = Authored->Find(TEXT("FireCore"), TEXT("JetTube"));
	if (!TestNotNull(TEXT("Jet has its authored settings"), Def)) return false;
	TestEqual(TEXT("Wide jet is a full one-hundred-twenty-degree cone"), Def->ConeAngleDegrees, 120.0f);
	TestEqual(TEXT("Focused jet is a full thirty-degree cone"), Def->FocusedConeAngleDegrees, 30.0f);
	TestTrue(TEXT("Focused jet has more than twice the base range"), Def->FocusedRangeCentimeters > Def->RangeCentimeters * 2);
	for (const bool bFocused : {false, true})
	{
		FStellarWorld W; auto* PS = W.AddPlayer(); auto* Character = W.AddCharacter(PS, EJTSStellarWeaponMode::Jet);
		auto* Weapon = Character->FindComponentByClass<UJTSStellarWeaponComponent>();
		Weapon->WeaponCatalog.Get()->Weapons[0] = *Def;
		Weapon->WeaponCatalog.Get()->Weapons[0].EffectClass = nullptr;
		Weapon->WeaponCatalog.Get()->Weapons[0].StatusDamagePerSecond = 0;
		Cast<APlayerController>(Character->GetController())->SetAsLocalPlayerController();
		W.Step(.05f); // Resolve GameState and the locally controlled input path before pressing.
		const float Eye = Character->BaseEyeHeight;
		auto AtAngle = [&](float Angle, float Distance)
		{
			return W.Target(FVector(Distance * FMath::Cos(FMath::DegreesToRadians(Angle)),
				Distance * FMath::Sin(FMath::DegreesToRadians(Angle)), Eye));
		};
		auto* Inner = AtAngle(bFocused ? 14 : 59, 800);
		auto* Outer = AtAngle(bFocused ? 16 : 61, 900);
		auto* Far = AtAngle(0, 2000);
		auto* TooFar = AtAngle(0, 2300);
		auto* WideEdge = AtAngle(55, 1600);
		// Go through the same local input methods used by the character (right alone must fire).
		if (bFocused) Weapon->SetSecondary(true); else Weapon->SetPrimary(true);
		TestEqual(TEXT("Only secondary enters aim mode"), Weapon->IsAiming(), bFocused);
		TestTrue(TEXT("Both attack modes raise the scepter"), Weapon->IsCasting());
		W.Step(.25f, Weapon);
		TestTrue(TEXT("Targets inside the half-angle are damaged"), Inner->FindComponentByClass<UJTSHealthComponent>()->GetHealth() < 10000);
		TestEqual(TEXT("Targets outside the half-angle are not damaged"), Outer->FindComponentByClass<UJTSHealthComponent>()->GetHealth(), 10000.0f);
		TestEqual(TEXT("Only focused jet reaches twenty metres"), Far->FindComponentByClass<UJTSHealthComponent>()->GetHealth() < 10000, bFocused);
		TestEqual(TEXT("Focused jet also respects its range limit"), TooFar->FindComponentByClass<UJTSHealthComponent>()->GetHealth(), 10000.0f);
		TestEqual(TEXT("Only wide jet reaches the visible cone's broad side inside ten metres forward"),
			WideEdge->FindComponentByClass<UJTSHealthComponent>()->GetHealth() < 10000, !bFocused);
		TestTrue(TEXT("Right alone pays channel energy"), PS->GetStellarLoadout()->GetEnergy() < 100);
		const float BeforeHitch = PS->GetStellarLoadout()->GetEnergy();
		++GFrameCounter; W.World->Tick(LEVELTICK_All, .6f);
		TestTrue(TEXT("A slow server frame does not cancel a locally held channel before its heartbeat runs"),
			PS->GetStellarLoadout()->GetEnergy() < BeforeHitch);
		Weapon->SetSecondary(false); Weapon->SetPrimary(false);
		TestFalse(TEXT("Release ends aiming and casting"), Weapon->IsAiming() || Weapon->IsCasting());
		const float Health = Inner->FindComponentByClass<UJTSHealthComponent>()->GetHealth(); W.Step(.3f);
		TestEqual(TEXT("Released jet stops causing damage"), Inner->FindComponentByClass<UJTSHealthComponent>()->GetHealth(), Health);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarBlackHoleCastTest,"JTS.Stellar.BlackHole.SinglePressLifetimeAndEnergy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStellarBlackHoleCastTest::RunTest(const FString&)
{
	FStellarWorld W; auto* PS=W.AddPlayer(); auto* Character=W.AddCharacter(PS,EJTSStellarWeaponMode::BlackHole);
	W.Ground(); W.Step(.05f);
	auto* Weapon=Character->FindComponentByClass<UJTSStellarWeaponComponent>(); auto* Loadout=PS->GetStellarLoadout();
	auto* Target=W.Target(FVector(1400,100,50)); auto* Health=Target->FindComponentByClass<UJTSHealthComponent>();
	CastAt(Character,FVector(1400,0,0));
	TestEqual(TEXT("One press creates one independent field"),Weapon->GetActiveBlackHoleCount(),1);
	TestEqual(TEXT("A cast pays its 30 energy once"),Loadout->GetEnergy(),70.0f);
	for(TActorIterator<AJTSBlackHoleField> It(W.World);It;++It)
	{
		TestFalse(TEXT("Black hole field has no physical collision"),It->GetActorEnableCollision());
		TestTrue(TEXT("Attraction targets the ground below the floating core"),FMath::IsNearlyZero(It->GetAttractionCenter().Z,2.0));
		TestTrue(TEXT("Visual core is visibly raised above its ground attraction point"),It->GetActorLocation().Z-It->GetAttractionCenter().Z>=100);
		const FVector Ground=It->GetAttractionCenter(); FHitResult WalkHit;
		Character->SetActorLocation(Ground+FVector(-300,0,110));
		Character->SetActorLocation(Ground+FVector(0,0,110),true,&WalkHit);
		TestFalse(TEXT("Player can walk directly under and through the visual black sphere"),WalkHit.bBlockingHit);
		Character->SetActorLocation(Ground+FVector(300,0,110),true,&WalkHit);
		TestFalse(TEXT("Player can leave the black hole normally"),WalkHit.bBlockingHit);
	}
	W.Step(.7f);
	CastAt(Character,FVector(1800,0,0));
	TestEqual(TEXT("Repeated held-input packets cannot cast again after cooldown"),Loadout->GetEnergy(),70.0f);
	TestEqual(TEXT("Holding does not create extra fields"),Weapon->GetActiveBlackHoleCount(),1);
	ReleaseCast(Weapon);
	const float BeforeRelease=Health->GetHealth(); W.Step(.25f);
	TestTrue(TEXT("Released black hole continues dealing damage without aim heartbeats"),Health->GetHealth()<BeforeRelease);
	Loadout->SelectWeapon(INDEX_NONE);
	const float BeforeSwitch=Health->GetHealth(); W.Step(.4f);
	TestTrue(TEXT("Changing to an ordinary weapon leaves the cast active"),Health->GetHealth()<BeforeSwitch);
	TestTrue(TEXT("Independent field maintains its pull"),!Target->FindComponentByClass<UJTSStellarTargetComponent>()->GetFieldAcceleration(Target->GetActorLocation(),FVector::UpVector).IsNearlyZero());
	W.Step(8.1f);
	TestEqual(TEXT("Field expires after its finite lifetime"),Weapon->GetActiveBlackHoleCount(),0);
	TestTrue(TEXT("Expiry removes this field's control immediately"),Target->FindComponentByClass<UJTSStellarTargetComponent>()->GetFieldAcceleration(Target->GetActorLocation(),FVector::UpVector).IsNearlyZero());
	const float AfterExpiry=Health->GetHealth(); W.Step(.4f);
	TestEqual(TEXT("Expired field produces no additional damage"),Health->GetHealth(),AfterExpiry);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarBlackHolePlacementTest,"JTS.Stellar.BlackHole.BoundedGroundPlacement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStellarBlackHolePlacementTest::RunTest(const FString&)
{
	FStellarWorld W; auto* PS=W.AddPlayer(); auto* Character=W.AddCharacter(PS,EJTSStellarWeaponMode::BlackHole);
	W.Ground(); W.Step(.05f);
	auto* Weapon=Character->FindComponentByClass<UJTSStellarWeaponComponent>(); auto* Loadout=PS->GetStellarLoadout();
	CastAt(Character,FVector(3050,0,0)); ReleaseCast(Weapon);
	TestEqual(TEXT("Ground beyond the 30-meter cast range creates no field"),Weapon->GetActiveBlackHoleCount(),0);
	TestEqual(TEXT("Out-of-range click pays no energy"),Loadout->GetEnergy(),100.0f);
	CastAt(Character,FVector(4000,0,5000)); ReleaseCast(Weapon);
	TestEqual(TEXT("Sky aiming does not substitute an air point at maximum range"),Weapon->GetActiveBlackHoleCount(),0);
	auto* Wall=W.Ground(FVector(500,0,Character->BaseEyeHeight),FVector(20,100,100));
	CastAt(Character,FVector(500,0,Character->BaseEyeHeight)); ReleaseCast(Weapon);
	TestEqual(TEXT("A vertical wall is not a ground placement"),Weapon->GetActiveBlackHoleCount(),0);
	TestEqual(TEXT("All rejected surface clicks leave energy unchanged"),Loadout->GetEnergy(),100.0f);
	Wall->Destroy();
	W.Target(FVector(1400,0,Character->BaseEyeHeight*.5f));
	const auto Binding=Loadout->GetWeapons()[0];
	Weapon->ServerSetInput(true,false,FGuid::NewGuid(),Binding.AttachmentInstanceId,FVector::ForwardVector,Character->GetPawnViewLocation());
	ReleaseCast(Weapon);
	TestEqual(TEXT("Forged equipment identity creates no field"),Weapon->GetActiveBlackHoleCount(),0);
	Loadout->RestoreState(Loadout->GetSlots(),0); Loadout->SelectWeapon(1);
	CastAt(Character,FVector(2950,0,0)); ReleaseCast(Weapon);
	TestEqual(TEXT("Insufficient energy cannot create a free persistent field"),Weapon->GetActiveBlackHoleCount(),0);
	Loadout->RestoreState(Loadout->GetSlots(),100); Loadout->SelectWeapon(1);
	auto* MovingGround=W.Ground(FVector(1500,0,80),FVector(300,300,20));
	Cast<UBoxComponent>(MovingGround->GetRootComponent())->SetCollisionObjectType(ECC_WorldDynamic);
	Character->SetActorLocation(FVector(0,0,200)); // The floor's upper face must actually be visible from the view point.
	CastAt(Character,FVector(1500,0,100)); ReleaseCast(Weapon);
	TestEqual(TEXT("A visible movable floor is a valid ground surface"),Weapon->GetActiveBlackHoleCount(),1);
	MovingGround->Destroy(); Character->SetActorLocation(FVector::ZeroVector); W.Step(.6f);
	CastAt(Character,FVector(2950,0,0)); ReleaseCast(Weapon);
	TestEqual(TEXT("Valid ground just within range creates one field"),Weapon->GetActiveBlackHoleCount(),1);
	AJTSBlackHoleField* First=nullptr;
	for(TActorIterator<AJTSBlackHoleField> It(W.World); It; ++It) if(!It->IsActorBeingDestroyed()) First=*It;
	if(!TestNotNull(TEXT("Cast produced a replicated field Actor"),First)) return false;
	TestTrue(TEXT("Placement uses ground, ignoring the enemy in front"),FMath::IsNearlyEqual(First->GetActorLocation().X,2950.0,1.0));
	W.Step(.6f); const float BeforeInvalid=Loadout->GetEnergy();
	CastAt(Character,FVector(4000,0,0)); ReleaseCast(Weapon);
	TestEqual(TEXT("A later invalid cast does not remove an existing field"),Weapon->GetActiveBlackHoleCount(),1);
	TestFalse(TEXT("Invalid placement does not replace the oldest cast"),First->IsActorBeingDestroyed());
	TestEqual(TEXT("Rejected replacement pays no energy"),Loadout->GetEnergy(),BeforeInvalid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarBlackHoleCapacityTest,"JTS.Stellar.BlackHole.UpgradeCapacityAndIndependentForces",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStellarBlackHoleCapacityTest::RunTest(const FString&)
{
	FStellarWorld W; auto* PS=W.AddPlayer(); auto* Character=W.AddCharacter(PS,EJTSStellarWeaponMode::BlackHole);
	W.Ground(); W.Step(.05f);
	auto* Weapon=Character->FindComponentByClass<UJTSStellarWeaponComponent>(); auto* Loadout=PS->GetStellarLoadout();
	auto* Catalog=Weapon->WeaponCatalog.Get(); Catalog->Weapons[0].BlackHoleEnergyPerCast=10; Catalog->Weapons[0].BlackHoleCastCooldown=.1f;
	auto Slots=Loadout->GetSlots(); Slots[1].StellarCoreLevel=5; Slots[2].StellarPoints.Init(0,6); Slots[2].StellarPoints[3]=4;
	Loadout->RestoreState(Slots,100); Loadout->SelectWeapon(1);
	const auto* Skills=FJTSStellarProgression::FindSkillDefs(TEXT("BlackHoleTube"));
	TestEqual(TEXT("Upgrade UI reports capacity three at four effective points"),FJTSStellarProgression::EvaluateSkill(Skills[3],4),3.0);
	TestEqual(TEXT("Capacity uses integer steps, respecting fractional core budgets"),FJTSStellarProgression::EvaluateSkill(Skills[3],1.99),1.0);
	CastAt(Character,FVector(500,0,0)); ReleaseCast(Weapon);
	TWeakObjectPtr<AJTSBlackHoleField> First;
	for(TActorIterator<AJTSBlackHoleField> It(W.World); It; ++It) if(!It->IsActorBeingDestroyed()) First=*It;
	W.Step(.15f); CastAt(Character,FVector(1000,0,0)); ReleaseCast(Weapon);
	W.Step(.15f); CastAt(Character,FVector(1500,0,0)); ReleaseCast(Weapon);
	TestEqual(TEXT("Quantity upgrade permits three simultaneous casts"),Weapon->GetActiveBlackHoleCount(),3);
	W.Step(.15f); CastAt(Character,FVector(2000,0,0)); ReleaseCast(Weapon);
	TestEqual(TEXT("A new cast at capacity retains the upgraded limit"),Weapon->GetActiveBlackHoleCount(),3);
	TestTrue(TEXT("The oldest field is replaced when capacity is full"),!First.IsValid() || First->IsActorBeingDestroyed());
	const float BeforeCooldown=Loadout->GetEnergy();
	CastAt(Character,FVector(2300,0,0)); ReleaseCast(Weapon);
	TestEqual(TEXT("Rapid repeated clicks obey server cast cooldown without charging"),Loadout->GetEnergy(),BeforeCooldown);
	auto* Target=W.Target(FVector(100,100,50)); auto* Status=Target->FindComponentByClass<UJTSStellarTargetComponent>();
	auto* A=W.World->SpawnActor<AActor>(); auto* B=W.World->SpawnActor<AActor>();
	Status->ApplyAttraction(Character,A,FVector(500,100,50),400,1000,0,.4f);
	Status->ApplyAttraction(Character,B,FVector(100,500,50),300,1000,0,.4f);
	Status->RemoveFieldForce(A);
	TestTrue(TEXT("Removing one player's field preserves their other field's pull"),Status->GetFieldAcceleration(Target->GetActorLocation(),FVector::UpVector).Y>0);
	const FVector OriginalPosition = Character->GetActorLocation(); Character->SetActorLocation(FVector(100,1000,50));
	Status->ApplyRepulsion(Character,180,1000,.4f);
	TestTrue(TEXT("Repulsion and attraction sum to a strong outward force inside the boundary"),Status->GetFieldAcceleration(Target->GetActorLocation(),FVector::UpVector).Y<0);
	Status->RemoveForce(Character); Character->SetActorLocation(OriginalPosition);
	TestTrue(TEXT("Stopping repulsion does not remove persistent-field pull"),Status->GetFieldAcceleration(Target->GetActorLocation(),FVector::UpVector).Y>0);
	B->Destroy();
	TestTrue(TEXT("Destroyed field no longer contributes a stale force"),Status->GetFieldAcceleration(Target->GetActorLocation(),FVector::UpVector).IsNearlyZero());
	Slots[1].StellarCoreLevel=1; Slots[2].StellarPoints[3]=0;
	Loadout->RestoreState(Slots,100); Loadout->SelectWeapon(1); W.Step(.15f);
	CastAt(Character,FVector(1800,0,0)); ReleaseCast(Weapon);
	TestEqual(TEXT("A new cast applies a reduced capacity immediately"),Weapon->GetActiveBlackHoleCount(),1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarCombatScaleTest,"JTS.Stellar.MatureCombatRangesAndCrowds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStellarCombatScaleTest::RunTest(const FString&)
{
	const auto* Authored = LoadObject<UJTSStellarWeaponCatalog>(nullptr, TEXT("/Game/Space/Data/Weapons/DA_StellarWeaponCatalog.DA_StellarWeaponCatalog"));
	if (!TestNotNull(TEXT("Authored weapon catalog exists"), Authored)) return false;
	const auto Configure = [&](AJTSCharacter* Character, const TCHAR* Core, const TCHAR* Attachment)
	{
		const auto* Definition = Authored->Find(Core, Attachment);
		if (!Definition) return false;
		auto* Weapon = Character->FindComponentByClass<UJTSStellarWeaponComponent>();
		auto* Catalog = Weapon->WeaponCatalog.Get();
		Catalog->Weapons[0] = *Definition;
		Catalog->Weapons[0].StatusDamagePerSecond = 0; // Isolate direct damage and geometric coverage.
		Catalog->Weapons[0].EffectClass = nullptr;
		return true;
	};
	const auto Fire = [](AJTSPlayerState* PS, UJTSStellarWeaponComponent* Weapon, bool bSecondary=false)
	{
		const auto Binding = PS->GetStellarLoadout()->GetWeapons()[0];
		Weapon->ServerSetInput(true, bSecondary, Binding.CoreInstanceId, Binding.AttachmentInstanceId, FVector::ForwardVector, FVector::ZeroVector);
	};
	{
		FStellarWorld W; auto* PS=W.AddPlayer(); auto* Character=W.AddCharacter(PS,EJTSStellarWeaponMode::Jet);
		if (!Configure(Character, TEXT("FireCore"), TEXT("JetTube"))) return false;
		auto* Weapon=Character->FindComponentByClass<UJTSStellarWeaponComponent>();
		TArray<AActor*> Crowd;
		for (int32 I=0; I<200; ++I) Crowd.Add(W.Target(FVector(400+(I%10)*60, (I/10-9.5f)*18, Character->BaseEyeHeight)));
		auto* Outside=W.Target(FVector(1500,0,Character->BaseEyeHeight));
		auto* Side=W.Target(FVector(300,700,Character->BaseEyeHeight));
		Fire(PS,Weapon); W.Step(.25f,Weapon);
		int32 Damaged=0;
		for (auto* Target:Crowd) if (Target->FindComponentByClass<UJTSHealthComponent>()->GetHealth()<10000) ++Damaged;
		TestEqual(TEXT("Wide flame reaches all 200 targets inside its authored near cone"),Damaged,200);
		TestEqual(TEXT("Wide flame rejects targets beyond 10 meters"),Outside->FindComponentByClass<UJTSHealthComponent>()->GetHealth(),10000.0f);
		TestEqual(TEXT("Flame cone rejects lateral targets"),Side->FindComponentByClass<UJTSHealthComponent>()->GetHealth(),10000.0f);
	}
	{
		FStellarWorld W; auto* PS=W.AddPlayer(); auto* Character=W.AddCharacter(PS,EJTSStellarWeaponMode::Focus);
		if (!Configure(Character,TEXT("LightCore"),TEXT("FocusTube"))) return false;
		auto* Weapon=Character->FindComponentByClass<UJTSStellarWeaponComponent>();
		TArray<AActor*> Direct;
		for(int32 I=0;I<5;++I) Direct.Add(W.Target(FVector(3500+I*300,I==4?28:0,Character->BaseEyeHeight)));
		auto* Chain=W.Target(FVector(4900,650,Character->BaseEyeHeight));
		Fire(PS,Weapon); W.Step(.05f,Weapon);
		for (auto* Target:Direct) TestTrue(TEXT("Focus pierces five targets beyond the old 30-meter range, including aim forgiveness"),Target->FindComponentByClass<UJTSHealthComponent>()->GetHealth()<10000);
		TestTrue(TEXT("Base focus visibly chains to a nearby off-axis target"),Chain->FindComponentByClass<UJTSHealthComponent>()->GetHealth()<10000);
		TestTrue(TEXT("One strengthened beam still costs exactly 2.75 energy"),FMath::IsNearlyEqual(PS->GetStellarLoadout()->GetEnergy(),97.25f));
	}
	for(auto Mode:{EJTSStellarWeaponMode::Jet,EJTSStellarWeaponMode::Focus})
	{
		FStellarWorld W; auto* PS=W.AddPlayer(); auto* Character=W.AddCharacter(PS,Mode);
		if(!Configure(Character,Mode==EJTSStellarWeaponMode::Jet?TEXT("FireCore"):TEXT("LightCore"),Mode==EJTSStellarWeaponMode::Jet?TEXT("JetTube"):TEXT("FocusTube"))) return false;
		auto* Wall=W.World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall);
		Wall->SetRootComponent(Box); Wall->AddInstanceComponent(Box); Box->SetBoxExtent(FVector(30,400,400));
		Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent(); Wall->SetActorLocation(FVector(900,0,Character->BaseEyeHeight));
		auto* Behind=W.Target(FVector(1200,0,Character->BaseEyeHeight));
		auto* Weapon=Character->FindComponentByClass<UJTSStellarWeaponComponent>(); Fire(PS,Weapon); W.Step(.25f,Weapon);
		TestEqual(TEXT("World geometry blocks strengthened flame and focus damage"),Behind->FindComponentByClass<UJTSHealthComponent>()->GetHealth(),10000.0f);
	}
	{
		FStellarWorld W; auto* PS=W.AddPlayer(); auto* Character=W.AddCharacter(PS,EJTSStellarWeaponMode::BlackHole);
		if(!Configure(Character,TEXT("DarkCore"),TEXT("BlackHoleTube"))) return false;
		auto* Weapon=Character->FindComponentByClass<UJTSStellarWeaponComponent>();
		auto* Edge=W.Target(FVector(3000,800,Character->BaseEyeHeight));
		auto* Outside=W.Target(FVector(3000,1000,Character->BaseEyeHeight));
		auto* Close=W.Target(FVector(0,600,Character->BaseEyeHeight));
		W.Ground(); CastAt(Character,FVector(2950,0,0),true); W.Step(.25f,Weapon);
		TestTrue(TEXT("30-meter placement damages targets eight meters from its center"),Edge->FindComponentByClass<UJTSHealthComponent>()->GetHealth()<10000);
		TestEqual(TEXT("Black hole rejects targets outside its nine-meter radius"),Outside->FindComponentByClass<UJTSHealthComponent>()->GetHealth(),10000.0f);
		TestTrue(TEXT("Expanded six-and-a-half-meter repulsion damages nearby attackers"),Close->FindComponentByClass<UJTSHealthComponent>()->GetHealth()<10000);
		TestTrue(TEXT("Expanded black hole applies radial tangent pull"),!Edge->FindComponentByClass<UJTSStellarTargetComponent>()->GetFieldAcceleration(Edge->GetActorLocation(),FVector::UpVector).IsNearlyZero());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarCombatPresentationTest,"JTS.Stellar.CombatPresentationMatchesCoverage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStellarCombatPresentationTest::RunTest(const FString&)
{
	FStellarWorld W;
	const auto* Catalog=LoadObject<UJTSStellarWeaponCatalog>(nullptr,TEXT("/Game/Space/Data/Weapons/DA_StellarWeaponCatalog.DA_StellarWeaponCatalog"));
	if(!TestNotNull(TEXT("Catalog exists"),Catalog)) return false;
	for(const auto& Def:Catalog->Weapons)
	{
		if(Def.Mode==EJTSStellarWeaponMode::PresentationOnly) continue;
		if(!TestNotNull(TEXT("Combat mode supplies a configured FX Blueprint"),Def.EffectClass.Get())) return false;
		auto* FX=W.World->SpawnActor<AJTSStellarEffectActor>(Def.EffectClass);
		const float Radius=Def.Mode==EJTSStellarWeaponMode::BlackHole?Def.AreaRadiusCentimeters:
			Def.Mode==EJTSStellarWeaponMode::Jet?Def.RangeCentimeters*FMath::Tan(FMath::DegreesToRadians(Def.ConeAngleDegrees*.5f)):Def.BeamRadiusCentimeters;
		const FVector End(Def.RangeCentimeters,0,0);
		FX->UpdateEffect(Def.Mode,FVector::ZeroVector,End,Radius,Def.Color,true,true,Def.RepulsionRadiusCentimeters,true,true);
		if(Def.Mode==EJTSStellarWeaponMode::BlackHole)
		{
			TestNotNull(TEXT("Black hole supplies an opaque core"),FX->Core->GetStaticMesh().Get());
			TestFalse(TEXT("Black hole visual actor cannot block players"),FX->GetActorEnableCollision());
			TestEqual(TEXT("Visual sphere contains no physics collider"),FX->Core->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
			TestTrue(TEXT("Only the compact core is visible"),FX->Core->IsVisible() && !FX->Field->IsVisible() && !FX->Orbit->IsVisible() && !FX->OrbitInner->IsVisible());
			TestTrue(TEXT("Held repulsion shows the configured dome"),FX->Repulsion->IsVisible());
			TestNotNull(TEXT("Repulsion supplies a hemisphere mesh"),FX->Repulsion->GetStaticMesh().Get());
			TestTrue(TEXT("Dome scale matches the gameplay radius on every axis"),
				FX->Repulsion->GetComponentScale().Equals(FVector(Def.RepulsionRadiusCentimeters/50),.01f));
			FX->UpdatePersistentField(End,FVector::RightVector,Radius,Def.BlackHoleCoreRadiusCentimeters,Def.Color);
			TestFalse(TEXT("Persistent black hole alone never displays a repulsion dome"),FX->Repulsion->IsVisible());
			TestTrue(TEXT("Core size is independent of upgraded attraction coverage"),FMath::IsNearlyEqual(static_cast<float>(FX->Core->GetComponentScale().X*50),Def.BlackHoleCoreRadiusCentimeters,.01f));
			TestTrue(TEXT("Black sphere remains centred on its physical core"),FX->Core->GetComponentLocation().Equals(End));
			TestNotNull(TEXT("Black hole has quiet loop audio"),FX->LoopSound.Get());
		}
		else
		{
			TestNotNull(TEXT("Beam supplies authored mesh"),FX->Beam->GetStaticMesh().Get());
			TestNotNull(TEXT("Beam supplies animated material"),FX->Beam->GetMaterial(0));
			TestTrue(TEXT("Visible beam follows its full attack length"),FMath::IsNearlyEqual(FX->Beam->GetComponentScale().Z*100,Def.RangeCentimeters));
			TestTrue(TEXT("Contact feedback is visible on impact"),FX->Impact->IsVisible());
			if(Def.Mode==EJTSStellarWeaponMode::Focus)
			{
				TestNotNull(TEXT("Focus has shot audio"),FX->ShotSound.Get());
				const FVector BounceStart(1000,500,100),BounceEnd(1400,700,100);
				FX->UpdateEffect(Def.Mode,BounceStart,BounceEnd,Radius,Def.Color,true,false,300,false,true);
				TestTrue(TEXT("Chain draws from its prior enemy, independently of the muzzle"),FX->ChainBeams[0]->GetComponentLocation().Equals((BounceStart+BounceEnd)*.5));
			}
			else TestNotNull(TEXT("Flame has loop audio"),FX->LoopSound.Get());
		}
		FX->Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarDensityTest,"JTS.Stellar.ThousandTargetBoundedQuery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStellarDensityTest::RunTest(const FString&)
{
	FStellarWorld W;
	for(int32 Index=0;Index<1000;++Index)W.Target(FVector((Index%20)*20,(Index/20)*20,64));
	TArray<AActor*> Targets; const double Start=FPlatformTime::Seconds();
	UJTSStellarTargetComponent::QueryTargets(W.World,FVector(200,300,64),1500,96,Targets);
	TestEqual(TEXT("A dense spatial query respects the per-pulse budget"),Targets.Num(),96);
	TSet<AActor*> Unique; for(auto* Target:Targets)Unique.Add(Target);
	TestEqual(TEXT("Multiple colliders never duplicate a hit target"),Unique.Num(),Targets.Num());
	AddInfo(FString::Printf(TEXT("1000-target broadphase query + nearest-96 selection: %.3f ms (CPU smoke test, not a four-client frame benchmark)"),(FPlatformTime::Seconds()-Start)*1000));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarInventoryTest,"JTS.Stellar.RestrictedInventoryTransfers",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStellarInventoryTest::RunTest(const FString&)
{
	FStellarWorld W; auto* PS=W.AddPlayer(); auto* Character=W.AddCharacter(PS,EJTSStellarWeaponMode::Focus);
	auto* Inventory=Character->GetInventoryComponent(); auto* Loadout=PS->GetStellarLoadout();
	FindFProperty<FIntProperty>(Inventory->GetClass(),TEXT("BaseInventoryCapacity"))->SetPropertyValue_InContainer(Inventory,9);
	Inventory->RefreshCapacityFromProgression();
	// This fixture bypasses PlayerState registration during possession; supply its normal pawn link.
	FindFProperty<FObjectProperty>(PS->GetClass(),TEXT("PawnPrivate"))->SetObjectPropertyValue_InContainer(PS,Character);
	FJTSStellarLootEntry Firmware; Firmware.ItemId=TEXT("Firmware"); Firmware.FirmwareUnits=2; W.Table->Entries.Add(Firmware);
	TArray<FJTSItemInstance> Ordinary; Ordinary.SetNum(Inventory->GetInventoryCapacity());
	Ordinary[0].ItemId=EJTSItemId::RailPistol; Ordinary[0].StackCount=1; Ordinary[0].InstanceId=FGuid::NewGuid();
	// Even a forged stellar label on an ordinary weapon cannot bypass the type restriction.
	Ordinary[0].StellarItemId=TEXT("Firmware");
	Ordinary[1]=W.Table->MakeTextItem(TEXT("Firmware"));
	Ordinary[2]=W.Table->MakeTextItem(TEXT("LightCore")); Ordinary[3]=W.Table->MakeTextItem(TEXT("FocusTube"));
	Inventory->RestoreItems(Ordinary,0);
	const FGuid FirmwareId=Ordinary[1].InstanceId;
	Loadout->ServerExchangeInventory(0,Ordinary[0].InstanceId,0,FGuid());
	TestTrue(TEXT("Ordinary weapons rejected without losing either item"),Loadout->GetSlot(0).IsEmpty() && Inventory->GetItemAtSlot(0).InstanceId==Ordinary[0].InstanceId);
	for(int32 Target : {0,3,4})
	{
		Loadout->ServerExchangeInventory(1,FirmwareId,Target,FGuid());
		TestTrue(TEXT("Firmware cannot enter any equipment shape"),Loadout->GetSlot(Target).IsEmpty() && Inventory->GetItemAtSlot(1).InstanceId==FirmwareId);
	}
	for(int32 Source : {2,3})
	{
		const FGuid ItemId=Ordinary[Source].InstanceId;
		Loadout->ServerExchangeInventory(Source,ItemId,0,FGuid());
		TestEqual(TEXT("Orange spare accepts either core or attachment"),Loadout->GetSlot(0).InstanceId,ItemId);
		TestTrue(TEXT("Insertion removes its source exactly once"),Inventory->GetItemAtSlot(Source).IsEmpty());
		Loadout->ServerExchangeInventory(Source,ItemId,4,FGuid());
		TestTrue(TEXT("Stale source cannot duplicate an item"),Loadout->GetSlot(4).IsEmpty());
		Loadout->ServerMoveSlot(0,ItemId,4,FGuid());
		TestEqual(TEXT("Grey diamond accepts either core or attachment"),Loadout->GetSlot(4).InstanceId,ItemId);
		Loadout->ServerExchangeInventory(Source,FGuid(),4,ItemId);
		TestTrue(TEXT("Stellar equipment cannot return to an empty ordinary slot"),Inventory->GetItemAtSlot(Source).IsEmpty());
		TestEqual(TEXT("Rejected extraction preserves stellar identity"),Loadout->GetSlot(4).InstanceId,ItemId);
		Loadout->ServerExchangeInventory(0,Ordinary[0].InstanceId,4,ItemId);
		TestTrue(TEXT("Stellar equipment cannot swap into an occupied ordinary slot"),
			Inventory->GetItemAtSlot(0).InstanceId==Ordinary[0].InstanceId && Loadout->GetSlot(4).InstanceId==ItemId);
		Loadout->ServerMoveSlot(4,ItemId,Source+3,FGuid());
	}
	TArray<FJTSItemInstance> Slots=Loadout->GetSlots(); Slots[8]=W.Table->MakeTextItem(TEXT("Firmware")); const auto SealedId=Slots[8].InstanceId;
	Loadout->RestoreState(Slots,43);
	for(int32 Index=0;Index<3;++Index)W.AddPlayer(); Loadout->RefreshParticipants();
	Loadout->ServerExchangeInventory(2,Ordinary[2].InstanceId,8,SealedId);
	TestEqual(TEXT("Sealed slot forbids swapping in a new stellar item"),Loadout->GetSlot(8).InstanceId,SealedId);
	FJTSItemInstance RemovedOrdinary;
	Inventory->TryExtractItemAtSlot(0,Ordinary[0].InstanceId,RemovedOrdinary);
	Loadout->ServerExchangeInventory(0,FGuid(),8,SealedId);
	TestTrue(TEXT("Sealed items also cannot enter ordinary storage"),Inventory->GetItemAtSlot(0).IsEmpty());
	TestEqual(TEXT("Rejected sealed extraction preserves its source"),Loadout->GetSlot(8).InstanceId,SealedId);
	// Legacy saved firmware is still spendable, but cannot be inserted again.
	Slots=Loadout->GetSlots(); Slots[0]=W.Table->MakeTextItem(TEXT("Firmware")); Loadout->RestoreState(Slots,43);
	TestEqual(TEXT("Upgrade UI counts equipped and legacy firmware, excluding sealed items and forged labels"),Loadout->GetAvailableFirmwareUnits(),4);
	const FGuid CoreId=Loadout->GetSlot(1).InstanceId; const int32 Revision=Loadout->GetRevision();
	Loadout->ServerUpgradeCore(1,CoreId,Revision-1);
	TestEqual(TEXT("Stale popup revision cannot consume firmware"),Loadout->GetSlot(1).StellarCoreLevel,1);
	Loadout->ServerUpgradeCore(1,CoreId,Revision);
	TestEqual(TEXT("Core upgrade consumes only the required firmware units"),Loadout->GetSlot(1).StellarCoreLevel,2);
	TestTrue(TEXT("Spent firmware item removed exactly once"),Loadout->GetSlot(0).IsEmpty());
	TestEqual(TEXT("Surplus firmware units retained"),PS->GetStellarFirmwareUnits(),1);
	TestEqual(TEXT("Upgrade UI includes banked surplus with legacy inventory firmware"),Loadout->GetAvailableFirmwareUnits(),3);
	PS->RestoreStellarFirmwareUnits(0);
	Loadout->ServerUpgradeCore(1,CoreId,Loadout->GetRevision());
	TestEqual(TEXT("Legacy ordinary firmware remains usable for upgrades"),Loadout->GetSlot(1).StellarCoreLevel,3);
	TestTrue(TEXT("Ordinary firmware is consumed only once"),Inventory->GetItemAtSlot(1).IsEmpty());
	TestEqual(TEXT("Inventory operations and upgrades preserve energy"),Loadout->GetEnergy(),43.0f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarContinuousRepulsionTest,"JTS.Stellar.BlackHole.ContinuousHeldRepulsion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStellarContinuousRepulsionTest::RunTest(const FString&)
{
	FStellarWorld W; auto* PS=W.AddPlayer(); auto* Character=W.AddCharacter(PS,EJTSStellarWeaponMode::BlackHole);
	auto* Weapon=Character->FindComponentByClass<UJTSStellarWeaponComponent>();
	const auto Binding=PS->GetStellarLoadout()->GetWeapons()[0];
	auto* Target=W.Target(FVector(200,0,45));
	auto* Status=Target->FindComponentByClass<UJTSStellarTargetComponent>();
	Weapon->ServerSetInput(false,true,Binding.CoreInstanceId,Binding.AttachmentInstanceId,FVector::ForwardVector,Character->GetPawnViewLocation());
	for(int32 I=0;I<32;++I)
	{
		W.Step(.05f,Weapon);
		TestTrue(TEXT("Held repulsion never gaps between damage pulses"),Status->GetFieldAcceleration(Target->GetActorLocation(),FVector::UpVector).X>0);
	}
	TestTrue(TEXT("Holding consumes channel energy over time"),PS->GetStellarLoadout()->GetEnergy()<80);
	auto* NewTarget=W.Target(FVector(100,80,45));
	W.Step(.05f,Weapon);
	TestTrue(TEXT("An enemy spawning during an existing channel is repelled at the next force refresh"),
		!NewTarget->FindComponentByClass<UJTSStellarTargetComponent>()->GetFieldAcceleration(NewTarget->GetActorLocation(),FVector::UpVector).IsNearlyZero());
	ReleaseCast(Weapon);
	TestTrue(TEXT("Releasing immediately removes repulsion"),Status->GetFieldAcceleration(Target->GetActorLocation(),FVector::UpVector).IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarProgressionAccessTest,"JTS.Stellar.Progression.CoreIndependentAndAttachmentActivation",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStellarProgressionAccessTest::RunTest(const FString&)
{
	FStellarWorld W;
	auto* PS=W.AddPlayer(); auto* Character=W.AddCharacter(PS,EJTSStellarWeaponMode::Focus);
	auto* Loadout=PS->GetStellarLoadout();
	TArray<FJTSItemInstance> Slots; Slots.SetNum(9);
	Slots[0]=W.Table->MakeTextItem(TEXT("DarkCore"));
	Slots[1]=W.Table->MakeTextItem(TEXT("FireCore")); Slots[1].StellarCoreLevel=3;
	Slots[2]=W.Table->MakeTextItem(TEXT("JetTube")); Slots[2].StellarPoints={1,0,0,0,0,0};
	Slots[3]=W.Table->MakeTextItem(TEXT("LightCore")); Slots[3].StellarCoreLevel=11;
	Slots[4]=W.Table->MakeTextItem(TEXT("JetTube")); Slots[4].StellarPoints={1,0,0,0,0,0};
	Slots[6]=W.Table->MakeTextItem(TEXT("DarkCore"));
	Slots[7]=W.Table->MakeTextItem(TEXT("DarkCore")); Slots[8]=W.Table->MakeTextItem(TEXT("BlackHoleTube"));
	Loadout->RestoreState(Slots,60); PS->RestoreStellarFirmwareUnits(3);
	Loadout->ServerUpgradeCore(0,Slots[0].InstanceId,Loadout->GetRevision());
	TestEqual(TEXT("An inactive spare core gains exactly one point"),Loadout->GetSlot(0).StellarCoreLevel,2);
	TestEqual(TEXT("A single permanent point spends its material cost"),PS->GetStellarFirmwareUnits(),2);
	Loadout->ServerUpgradeCore(6,Slots[6].InstanceId,Loadout->GetRevision());
	TestEqual(TEXT("A core in a grey diamond upgrades without activation"),Loadout->GetSlot(6).StellarCoreLevel,2);
	TestEqual(TEXT("Upgrading cores never selects a weapon"),Loadout->GetActiveCoreSlot(),INDEX_NONE);
	Loadout->ServerApplyPoints(2,Slots[2].InstanceId,Loadout->GetRevision(),{1,1,0,0,0,0});
	TestEqual(TEXT("A matched, unequipped attachment can use its core's two points"),Loadout->GetSlot(2).StellarPoints[1],uint8(1));
	Loadout->ServerApplyPoints(2,Slots[2].InstanceId,Loadout->GetRevision(),{2,1,0,0,0,0});
	TestEqual(TEXT("Allocation cannot exceed the paired core's budget"),Loadout->GetSlot(2).StellarPoints[0],uint8(1));
	const int32 Revision=Loadout->GetRevision();
	Loadout->ServerApplyPoints(4,Slots[4].InstanceId,Revision,{0,0,0,0,0,0});
	TestEqual(TEXT("A mismatched vertical pair cannot even reset allocation"),Loadout->GetRevision(),Revision);
	TestEqual(TEXT("Rejected inactive reset preserves the attachment's points"),Loadout->GetSlot(4).StellarPoints[0],uint8(1));
	Loadout->ServerApplyPoints(8,Slots[8].InstanceId,Revision,{0,0,0,0,0,0});
	TestTrue(TEXT("A valid zero-budget weapon is still activated"),Loadout->GetRevision()>Revision);
	// Exercise the actual popup entry gate with a local player context, without adding rejected dialogs to a viewport.
	auto* PC=CastChecked<AJTSPlayerController>(Character->GetController());
	auto* LocalPlayer=NewObject<ULocalPlayer>(GEngine);
	PC->Player=LocalPlayer; LocalPlayer->PlayerController=PC;
	auto* Dialog=CreateWidget<UJTSStellarAttachmentDialog>(PC);
	if (!TestNotNull(TEXT("A progression dialog can be constructed"),Dialog)) return false;
	TestFalse(TEXT("Clicking a mismatched attachment never opens a popup"),Dialog->OpenForLoadoutSlot(4));
	Loadout->ServerMoveSlot(2,Slots[2].InstanceId,5,FGuid());
	TestFalse(TEXT("A nonadjacent matching core cannot open attachment allocation"),Dialog->OpenForLoadoutSlot(5));
	Loadout->ServerMoveSlot(5,Slots[2].InstanceId,0,Slots[0].InstanceId);
	TestFalse(TEXT("An attachment in the orange spare cannot open allocation"),Dialog->OpenForLoadoutSlot(0));
	TestFalse(TEXT("Rejected clicks leave the dialog closed"),Dialog->IsDialogOpen());
	PC->Player=nullptr; LocalPlayer->PlayerController=nullptr;
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStellarBlackHoleReticleTest,"JTS.Stellar.BlackHole.ReticleDistanceAndCameraValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStellarBlackHoleReticleTest::RunTest(const FString&)
{
	FStellarWorld W; auto* PS=W.AddPlayer(); auto* Character=W.AddCharacter(PS,EJTSStellarWeaponMode::BlackHole);
	W.Ground(); W.Step(.05f);
	auto* Weapon=Character->FindComponentByClass<UJTSStellarWeaponComponent>(); auto* Loadout=PS->GetStellarLoadout();
	const auto Binding=Loadout->GetWeapons()[0];
	const auto Request = [&](FVector CameraOrigin, FVector Point)
	{
		const FVector Direction=(Point-CameraOrigin).GetSafeNormal();
		Character->GetController()->SetControlRotation(Direction.Rotation());
		Weapon->ServerSetInput(true,false,Binding.CoreInstanceId,Binding.AttachmentInstanceId,Direction,CameraOrigin);
		ReleaseCast(Weapon);
	};
	Request(FVector(0,0,600),FVector(4000,0,0));
	TestEqual(TEXT("Far reticle ground is invalid even when an eye-parallel ray would hit nearby ground"),Weapon->GetActiveBlackHoleCount(),0);
	TestEqual(TEXT("Third-person parallax cannot charge for an out-of-range reticle"),Loadout->GetEnergy(),100.0f);
	Request(FVector(20000,0,600),FVector(1400,0,0));
	TestEqual(TEXT("Forged remote camera origin is rejected without energy use"),Loadout->GetEnergy(),100.0f);
	auto* Wall=W.Ground(FVector(700,0,100),FVector(20,200,200));
	Request(FVector(0,0,1000),FVector(1400,0,0));
	TestEqual(TEXT("An elevated camera cannot cast around cover that blocks the character"),Weapon->GetActiveBlackHoleCount(),0);
	Wall->Destroy();
	Request(FVector(0,0,600),FVector(1400,0,0));
	TestEqual(TEXT("Valid near reticle ground produces one cast"),Weapon->GetActiveBlackHoleCount(),1);
	for(TActorIterator<AJTSBlackHoleField> It(W.World); It; ++It) if(!It->IsActorBeingDestroyed())
		TestTrue(TEXT("The cast uses the actual camera reticle point, not nearer eye-ray ground"),FMath::IsNearlyEqual(It->GetActorLocation().X,1400.0,1.0));
	return true;
}
#endif
