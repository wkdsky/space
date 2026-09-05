// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSMoonGameMode.h"

#include "Engine/World.h"
#include "space/Core/JTSGameInstance.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/UI/JTSPrototypeHUD.h"

namespace
{
	constexpr float ExpeditionConsumptionInterval = 1.0f;
	constexpr double SecondsPerMinute = 60.0;
	constexpr double ExpeditionResourceUnit = 0.1;
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

void AJTSMoonGameMode::BeginPlay()
{
	Super::BeginPlay();

	FoodConsumptionAccumulator = 0.0;
	WaterConsumptionAccumulator = 0.0;
	bMissingGameInstanceLogged = false;

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
		World->GetTimerManager().ClearTimer(ExpeditionConsumptionTimerHandle);
		World->GetTimerManager().SetTimer(
			ExpeditionConsumptionTimerHandle,
			this,
			&AJTSMoonGameMode::ConsumeExpeditionSupplies,
			ExpeditionConsumptionInterval,
			true,
			ExpeditionConsumptionInterval);
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
	}

	FoodConsumptionAccumulator = 0.0;
	WaterConsumptionAccumulator = 0.0;
	Super::EndPlay(EndPlayReason);
}

void AJTSMoonGameMode::ConsumeExpeditionSupplies()
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UJTSGameInstance* const GameInstance = World->GetGameInstance<UJTSGameInstance>();
	if (!IsValid(GameInstance))
	{
		FoodConsumptionAccumulator = 0.0;
		WaterConsumptionAccumulator = 0.0;
		if (!bMissingGameInstanceLogged)
		{
			UE_LOG(LogTemp, Error, TEXT("Jump to Space Moon GameMode cannot consume expedition supplies because the configured GameInstance is not UJTSGameInstance."));
			bMissingGameInstanceLogged = true;
		}
		return;
	}
	bMissingGameInstanceLogged = false;

	const double SafeCrewCount = static_cast<double>(GetCrewCount());
	FoodConsumptionAccumulator += static_cast<double>(GetFoodConsumptionPerPersonPerMinute())
		* SafeCrewCount / SecondsPerMinute;
	WaterConsumptionAccumulator += static_cast<double>(GetWaterConsumptionPerPersonPerMinute())
		* SafeCrewCount / SecondsPerMinute;

	const int32 FoodUnitsDue = GetWholeTenths(FoodConsumptionAccumulator);
	const int32 WaterUnitsDue = GetWholeTenths(WaterConsumptionAccumulator);
	if (FoodUnitsDue <= 0 && WaterUnitsDue <= 0)
	{
		return;
	}

	const int32 AvailableFoodUnits = FMath::Max(0, FMath::RoundToInt(GameInstance->GetExpeditionFood() * 10.0f));
	const int32 AvailableWaterUnits = FMath::Max(0, FMath::RoundToInt(GameInstance->GetExpeditionWater() * 10.0f));
	const int32 FoodUnitsToConsume = FMath::Min(FoodUnitsDue, AvailableFoodUnits);
	const int32 WaterUnitsToConsume = FMath::Min(WaterUnitsDue, AvailableWaterUnits);

	if (FoodUnitsToConsume > 0 || WaterUnitsToConsume > 0)
	{
		GameInstance->ConsumeExpeditionSupplies(
			static_cast<float>(FoodUnitsToConsume) * 0.1f,
			static_cast<float>(WaterUnitsToConsume) * 0.1f);
	}

	FoodConsumptionAccumulator = FMath::Max(
		0.0,
		FoodConsumptionAccumulator - static_cast<double>(FoodUnitsDue) * ExpeditionResourceUnit);
	WaterConsumptionAccumulator = FMath::Max(
		0.0,
		WaterConsumptionAccumulator - static_cast<double>(WaterUnitsDue) * ExpeditionResourceUnit);
}

int32 AJTSMoonGameMode::GetWholeTenths(double Accumulator)
{
	if (!FMath::IsFinite(Accumulator) || Accumulator <= 0.0)
	{
		return 0;
	}

	const double WholeTenths = FMath::FloorToDouble((Accumulator + 1.0e-9) / ExpeditionResourceUnit);
	return WholeTenths >= static_cast<double>(MAX_int32)
		? MAX_int32
		: static_cast<int32>(WholeTenths);
}
