// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Modes/JTSMoonGameMode.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "space/Core/JTSGameState.h"
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

void AJTSMoonGameMode::BeginPlay()
{
	Super::BeginPlay();

	FoodConsumptionAccumulator = 0.0;
	WaterConsumptionAccumulator = 0.0;
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
			TEXT("JumpToSpace Moon Config: Crew=%d FoodRate=%.2f WaterRate=%.2f ResourceCount=%d SpawnRadius=%.1f"),
			GetCrewCount(),
			GetFoodConsumptionPerPersonPerMinute(),
			GetWaterConsumptionPerPersonPerMinute(),
			FMath::Max(0, MoonResourceSpawnSettings.TotalResourceCount),
			FMath::Max(0.0f, MoonResourceSpawnSettings.SpawnRadius));

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

	AJTSSpacecraftActor* Spacecraft = nullptr;
	for (TActorIterator<AJTSSpacecraftActor> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			Spacecraft = *It;
			break;
		}
	}

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
