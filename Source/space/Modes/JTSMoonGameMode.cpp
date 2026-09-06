// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Modes/JTSMoonGameMode.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "space/Components/JTSPlayerEquipmentComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Items/JTSWorldPickupItemType.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/UI/JTSPrototypeHUD.h"

namespace
{
	constexpr double SecondsPerMinute = 60.0;
}

AJTSMoonGameMode::AJTSMoonGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	DefaultPawnClass = AJTSCharacter::StaticClass();
	PlayerControllerClass = AJTSPlayerController::StaticClass();
	GameStateClass = AJTSGameState::StaticClass();
	HUDClass = AJTSPrototypeHUD::StaticClass();
}

int32 AJTSMoonGameMode::GetCrewCount() const
{
	return FMath::Max(0, CrewCount);
}

float AJTSMoonGameMode::GetFoodConsumptionPerPersonPerMinute() const
{
	return FMath::Max(0.0f, FoodConsumptionPerPersonPerMinute);
}

float AJTSMoonGameMode::GetWaterConsumptionPerPersonPerMinute() const
{
	return FMath::Max(0.0f, WaterConsumptionPerPersonPerMinute);
}

float AJTSMoonGameMode::GetConsumptionTickInterval() const
{
	return FMath::Max(0.0f, ConsumptionTickInterval);
}

float AJTSMoonGameMode::GetMinimumConsumptionUnit() const
{
	return FMath::Max(0.0f, MinimumConsumptionUnit);
}

int32 AJTSMoonGameMode::GetPickaxeRockCost() const
{
	return FMath::Max(1, PickaxeRockCost);
}

int32 AJTSMoonGameMode::GetBackpackRockCost() const
{
	return FMath::Max(1, BackpackRockCost);
}

int32 AJTSMoonGameMode::GetBackpackOreCost() const
{
	return FMath::Max(1, BackpackOreCost);
}

int32 AJTSMoonGameMode::GetLargeRockTotalYieldUnits() const
{
	return FMath::Max(1, LargeRockTotalYieldUnits);
}

int32 AJTSMoonGameMode::GetOreDepositTotalYieldUnits() const
{
	return FMath::Max(1, OreDepositTotalYieldUnits);
}

float AJTSMoonGameMode::GetPickupMaxDistance() const
{
	return FMath::Max(50.0f, PickupMaxDistance);
}

float AJTSMoonGameMode::GetPickupAcquireRadius() const
{
	return FMath::Max(1.0f, PickupAcquireRadius);
}

float AJTSMoonGameMode::GetPickupRetainRadius() const
{
	return FMath::Max(GetPickupAcquireRadius(), PickupRetainRadius);
}

float AJTSMoonGameMode::GetPickupAimRayRadius() const
{
	return FMath::Max(1.0f, PickupAimRayRadius);
}

AJTSSpacecraftActor* AJTSMoonGameMode::GetSpacecraft() const
{
	if (CachedSpacecraft.IsValid())
	{
		return CachedSpacecraft.Get();
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<AJTSSpacecraftActor> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			CachedSpacecraft = *It;
			return *It;
		}
	}

	return nullptr;
}

float AJTSMoonGameMode::GetSpacecraftMarkerShowDistance() const
{
	return FMath::Max(0.0f, SpacecraftMarkerShowDistance);
}

float AJTSMoonGameMode::GetSpacecraftMarkerScreenSafeMargin() const
{
	return FMath::Max(20.0f, SpacecraftMarkerScreenSafeMargin);
}

float AJTSMoonGameMode::GetPickupDropUpwardSpeed() const
{
	return FMath::Max(0.0f, PickupDropUpwardSpeed);
}

float AJTSMoonGameMode::GetPickupDropHorizontalSpeed() const
{
	return FMath::Max(0.0f, PickupDropHorizontalSpeed);
}

bool AJTSMoonGameMode::TryCraftPickaxe(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft)
{
	return TryBuyWorkshopEquipment(Player, Spacecraft, EJTSEquipmentType::Pickaxe);
}

bool AJTSMoonGameMode::TryCraftBackpack(AJTSCharacter* Player, AJTSSpacecraftActor* Spacecraft)
{
	return TryBuyWorkshopEquipment(Player, Spacecraft, EJTSEquipmentType::Backpack);
}

bool AJTSMoonGameMode::TryBuyWorkshopEquipment(
	AJTSCharacter* Player,
	AJTSSpacecraftActor* Spacecraft,
	EJTSEquipmentType EquipmentType)
{
	if (EquipmentType != EJTSEquipmentType::Pickaxe && EquipmentType != EJTSEquipmentType::Backpack)
	{
		UE_LOG(LogTemp, Warning, TEXT("JumpToSpace Shop Buy: Result=Failed Reason=UnsupportedItem"));
		return false;
	}

	const TCHAR* const ItemName = EquipmentType == EJTSEquipmentType::Backpack ? TEXT("Backpack") : TEXT("Pickaxe");
	const int32 RockCost = EquipmentType == EJTSEquipmentType::Backpack ? GetBackpackRockCost() : GetPickaxeRockCost();
	const int32 OreCost = EquipmentType == EJTSEquipmentType::Backpack ? GetBackpackOreCost() : 0;
	auto LogBuyFailure = [ItemName, RockCost, OreCost](const TCHAR* Reason)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("JumpToSpace Shop Buy: Item=%s RockCost=%d OreCost=%d Result=Failed Reason=%s"),
			ItemName,
			RockCost,
			OreCost,
			Reason);
	};

	const AJTSGameState* const JTSGameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr;
	if (!IsValid(JTSGameState) || !JTSGameState->IsMoonExploration())
	{
		LogBuyFailure(TEXT("NotMoonExploration"));
		return false;
	}

	if (!IsValid(Player) || !IsValid(Spacecraft))
	{
		LogBuyFailure(TEXT("InvalidPlayerOrShip"));
		return false;
	}

	if (Player->GetNearbySpacecraft() != Spacecraft || !Spacecraft->IsPawnInBoardingRange(Player))
	{
		LogBuyFailure(TEXT("NotNearShip"));
		return false;
	}

	UJTSPlayerEquipmentComponent* const EquipmentComponent = Player->GetEquipmentComponent();
	if (!IsValid(EquipmentComponent))
	{
		LogBuyFailure(TEXT("MissingEquipment"));
		return false;
	}

	TMap<EJTSResourceType, int32> ResourceCosts;
	ResourceCosts.Add(EJTSResourceType::Rock, RockCost);
	if (OreCost > 0)
	{
		ResourceCosts.Add(EJTSResourceType::Ore, OreCost);
	}
	if (!Spacecraft->HasResource(EJTSResourceType::Rock, RockCost)
		|| (OreCost > 0 && !Spacecraft->HasResource(EJTSResourceType::Ore, OreCost)))
	{
		LogBuyFailure(TEXT("NotEnoughResources"));
		return false;
	}

	const bool bCanAutoEquip = !EquipmentComponent->HasEquippedItem(EquipmentType)
		&& EquipmentComponent->HasAvailableSlot();
	if (bCanAutoEquip)
	{
		if (!EquipmentComponent->TryEquipItem(EquipmentType))
		{
			LogBuyFailure(TEXT("AutoEquipFailed"));
			return false;
		}

		if (!Spacecraft->TryConsumeResourceAmounts(ResourceCosts))
		{
			EquipmentComponent->UnequipItem(EquipmentType);
			LogBuyFailure(TEXT("ConsumeFailedRolledBack"));
			return false;
		}

		UE_LOG(LogTemp, Log, TEXT("JumpToSpace Shop Buy: Item=%s Result=AutoEquipped"), ItemName);
		return true;
	}

	const EJTSWorldPickupItemType PickupItemType = EquipmentType == EJTSEquipmentType::Backpack
		? EJTSWorldPickupItemType::Backpack
		: EJTSWorldPickupItemType::Pickaxe;
	AJTSWorldPickupActor* const Pickup = AJTSWorldPickupActor::SpawnGroundedPickup(
		GetWorld(),
		PickupItemType,
		Player->GetActorLocation(),
		Player,
		Spacecraft,
		Player->GetActorForwardVector());
	if (!IsValid(Pickup))
	{
		LogBuyFailure(TEXT("DropSpawnFailed"));
		return false;
	}

	if (!Spacecraft->TryConsumeResourceAmounts(ResourceCosts))
	{
		Pickup->Destroy();
		LogBuyFailure(TEXT("ConsumeFailedRolledBack"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("JumpToSpace Shop Buy: Item=%s Result=DroppedNearPlayer"), ItemName);
	return true;
}

void AJTSMoonGameMode::BeginPlay()
{
	Super::BeginPlay();

	FoodConsumptionAccumulator = 0.0;
	WaterConsumptionAccumulator = 0.0;
	CachedSpacecraft.Reset();
	bMissingSpacecraftLogged = false;

	if (AJTSGameState* const JTSGameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AJTSGameState>() : nullptr)
	{
		JTSGameState->SetFailureReason(EJTSFailureReason::None);
		JTSGameState->SetGameplayPhase(EJTSGameplayPhase::MoonExploration);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Jump to Space Moon GameMode could not enter MoonExploration because its GameState is unavailable."));
	}

	if (UWorld* const World = GetWorld())
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("JumpToSpace Moon Config: Crew=%d FoodRate=%.2f WaterRate=%.2f ResourceCount=%d SpawnRadius=%.1f PickaxeCost=%d BackpackRockCost=%d BackpackOreCost=%d LargeYield=%d OreYield=%d PickupMaxDistance=%.0f PickupAcquireRadius=%.0f PickupRetainRadius=%.0f PickupAimRayRadius=%.0f ShipMarkerDistance=%.0f PickupUpwardSpeed=%.0f PickupHorizontalSpeed=%.0f"),
			GetCrewCount(),
			GetFoodConsumptionPerPersonPerMinute(),
			GetWaterConsumptionPerPersonPerMinute(),
			FMath::Max(0, MoonResourceSpawnSettings.TotalResourceCount),
			FMath::Max(0.0f, MoonResourceSpawnSettings.SpawnRadius),
			GetPickaxeRockCost(),
			GetBackpackRockCost(),
			GetBackpackOreCost(),
			GetLargeRockTotalYieldUnits(),
			GetOreDepositTotalYieldUnits(),
			GetPickupMaxDistance(),
			GetPickupAcquireRadius(),
			GetPickupRetainRadius(),
			GetPickupAimRayRadius(),
			GetSpacecraftMarkerShowDistance(),
			GetPickupDropUpwardSpeed(),
			GetPickupDropHorizontalSpeed());

		World->GetTimerManager().ClearTimer(MoonResourceInitializationTimerHandle);
		MoonResourceInitializationTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			this,
			&AJTSMoonGameMode::InitializeMoonResources);

		World->GetTimerManager().ClearTimer(ExpeditionConsumptionTimerHandle);
		const float ConsumptionInterval = GetConsumptionTickInterval();
		if (ConsumptionInterval > 0.0f)
		{
			World->GetTimerManager().SetTimer(
				ExpeditionConsumptionTimerHandle,
				this,
				&AJTSMoonGameMode::ConsumeExpeditionSupplies,
				ConsumptionInterval,
				true,
				ConsumptionInterval);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Jump to Space Moon GameMode did not start expedition supply consumption because ConsumptionTickInterval is zero."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Jump to Space Moon GameMode could not start its expedition supply timer because its World is unavailable."));
	}
}

void AJTSMoonGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpeditionConsumptionTimerHandle);
		World->GetTimerManager().ClearTimer(MoonResourceInitializationTimerHandle);
	}

	FoodConsumptionAccumulator = 0.0;
	WaterConsumptionAccumulator = 0.0;
	CachedSpacecraft.Reset();
	Super::EndPlay(EndPlayReason);
}

void AJTSMoonGameMode::InitializeMoonResources()
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	AJTSMoonResourceSpawner* ResourceSpawner = nullptr;
	int32 ResourceSpawnerCount = 0;
	for (TActorIterator<AJTSMoonResourceSpawner> It(World); It; ++It)
	{
		if (!IsValid(*It))
		{
			continue;
		}

		++ResourceSpawnerCount;
		if (ResourceSpawner == nullptr)
		{
			ResourceSpawner = *It;
		}
	}

	if (ResourceSpawner == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Jump to Space Moon GameMode found no AJTSMoonResourceSpawner. Moon exploration will start without automatically generated resources."));
		return;
	}

	if (ResourceSpawnerCount > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Jump to Space Moon GameMode found %d AJTSMoonResourceSpawner actors; using the first valid spawner only."), ResourceSpawnerCount);
	}

	ResourceSpawner->ApplyMoonSpawnSettings(MoonResourceSpawnSettings);
	ResourceSpawner->GenerateResources();
}

void AJTSMoonGameMode::ConsumeExpeditionSupplies()
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	AJTSSpacecraftActor* const Spacecraft = GetSpacecraft();

	if (!IsValid(Spacecraft))
	{
		FoodConsumptionAccumulator = 0.0;
		WaterConsumptionAccumulator = 0.0;
		if (!bMissingSpacecraftLogged)
		{
			UE_LOG(LogTemp, Error, TEXT("Jump to Space Moon GameMode cannot consume expedition supplies because no spacecraft was found."));
			bMissingSpacecraftLogged = true;
		}
		return;
	}
	bMissingSpacecraftLogged = false;

	const double ConsumptionTickSeconds = static_cast<double>(GetConsumptionTickInterval());
	const double ConsumptionUnit = static_cast<double>(GetMinimumConsumptionUnit());
	if (ConsumptionTickSeconds <= 0.0 || ConsumptionUnit <= 0.0)
	{
		return;
	}

	const double SafeCrewCount = static_cast<double>(GetCrewCount());
	FoodConsumptionAccumulator += static_cast<double>(GetFoodConsumptionPerPersonPerMinute())
		* SafeCrewCount * ConsumptionTickSeconds / SecondsPerMinute;
	WaterConsumptionAccumulator += static_cast<double>(GetWaterConsumptionPerPersonPerMinute())
		* SafeCrewCount * ConsumptionTickSeconds / SecondsPerMinute;

	const int32 FoodResourcesDue = GetWholeConsumptionUnits(FoodConsumptionAccumulator, ConsumptionUnit);
	const int32 WaterResourcesDue = GetWholeConsumptionUnits(WaterConsumptionAccumulator, ConsumptionUnit);
	if (FoodResourcesDue <= 0 && WaterResourcesDue <= 0)
	{
		return;
	}

	const int32 FoodResourcesToConsume = FMath::Min(
		FoodResourcesDue,
		Spacecraft->GetResourceAmount(EJTSResourceType::Food));
	const int32 WaterResourcesToConsume = FMath::Min(
		WaterResourcesDue,
		Spacecraft->GetResourceAmount(EJTSResourceType::Water));

	if (FoodResourcesToConsume > 0)
	{
		Spacecraft->TryConsumeResource(EJTSResourceType::Food, FoodResourcesToConsume);
	}
	if (WaterResourcesToConsume > 0)
	{
		Spacecraft->TryConsumeResource(EJTSResourceType::Water, WaterResourcesToConsume);
	}

	FoodConsumptionAccumulator = FMath::Max(
		0.0,
		FoodConsumptionAccumulator - static_cast<double>(FoodResourcesDue) * ConsumptionUnit);
	WaterConsumptionAccumulator = FMath::Max(
		0.0,
		WaterConsumptionAccumulator - static_cast<double>(WaterResourcesDue) * ConsumptionUnit);
}

int32 AJTSMoonGameMode::GetWholeConsumptionUnits(double Accumulator, double MinimumConsumptionUnit)
{
	if (!FMath::IsFinite(Accumulator)
		|| !FMath::IsFinite(MinimumConsumptionUnit)
		|| Accumulator <= 0.0
		|| MinimumConsumptionUnit <= 0.0)
	{
		return 0;
	}

	const double WholeConsumptionUnits = FMath::FloorToDouble((Accumulator / MinimumConsumptionUnit) + 1.0e-9);
	return WholeConsumptionUnits >= static_cast<double>(MAX_int32)
		? MAX_int32
		: static_cast<int32>(WholeConsumptionUnits);
}
