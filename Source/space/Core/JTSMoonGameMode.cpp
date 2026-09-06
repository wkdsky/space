// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSMoonGameMode.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/UI/JTSPrototypeHUD.h"

namespace
{
	constexpr float ExpeditionConsumptionInterval = 1.0f;
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

	const double SafeCrewCount = static_cast<double>(GetCrewCount());
	FoodConsumptionAccumulator += static_cast<double>(GetFoodConsumptionPerPersonPerMinute())
		* SafeCrewCount / SecondsPerMinute;
	WaterConsumptionAccumulator += static_cast<double>(GetWaterConsumptionPerPersonPerMinute())
		* SafeCrewCount / SecondsPerMinute;

	const int32 FoodResourcesDue = GetWholeResources(FoodConsumptionAccumulator);
	const int32 WaterResourcesDue = GetWholeResources(WaterConsumptionAccumulator);
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
		FoodConsumptionAccumulator - static_cast<double>(FoodResourcesDue));
	WaterConsumptionAccumulator = FMath::Max(
		0.0,
		WaterConsumptionAccumulator - static_cast<double>(WaterResourcesDue));
}

int32 AJTSMoonGameMode::GetWholeResources(double Accumulator)
{
	if (!FMath::IsFinite(Accumulator) || Accumulator <= 0.0)
	{
		return 0;
	}

	const double WholeResources = FMath::FloorToDouble(Accumulator + 1.0e-9);
	return WholeResources >= static_cast<double>(MAX_int32)
		? MAX_int32
		: static_cast<int32>(WholeResources);
}
