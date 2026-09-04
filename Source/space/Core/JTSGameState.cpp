// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSGameState.h"

#include "Engine/World.h"

AJTSGameState::AJTSGameState()
{
	PrimaryActorTick.bCanEverTick = false;
}

EJTSGameplayPhase AJTSGameState::GetGameplayPhase() const
{
	return GameplayPhase;
}

float AJTSGameState::GetEarthCollectionRemainingTime() const
{
	if (GameplayPhase != EJTSGameplayPhase::EarthCollection)
	{
		return 0.0f;
	}

	const UWorld* const World = GetWorld();
	if (World == nullptr || EarthCollectionEndTimeSeconds <= 0.0)
	{
		return FMath::Max(0.0f, EarthCollectionRemainingTime);
	}

	return FMath::Max(
		0.0f,
		static_cast<float>(EarthCollectionEndTimeSeconds - static_cast<double>(World->GetTimeSeconds())));
}

bool AJTSGameState::IsEarthCollectionActive() const
{
	return GameplayPhase == EJTSGameplayPhase::EarthCollection;
}

bool AJTSGameState::IsWaitingToStart() const
{
	return GameplayPhase == EJTSGameplayPhase::WaitingToStart;
}

bool AJTSGameState::IsEarthCollectionFinished() const
{
	return GameplayPhase == EJTSGameplayPhase::EarthCollectionFinished;
}

bool AJTSGameState::IsLaunching() const
{
	return GameplayPhase == EJTSGameplayPhase::Launching;
}

bool AJTSGameState::IsEarthCaptureFailure() const
{
	return GameplayPhase == EJTSGameplayPhase::EarthCaptureFailure;
}

bool AJTSGameState::IsMoonArrivalSuccess() const
{
	return GameplayPhase == EJTSGameplayPhase::MoonArrivalSuccess;
}

bool AJTSGameState::IsSuccessfulOutcome() const
{
	return IsMoonArrivalSuccess();
}

EJTSFailureReason AJTSGameState::GetFailureReason() const
{
	return FailureReason;
}

void AJTSGameState::SetFailureReason(EJTSFailureReason NewFailureReason)
{
	FailureReason = NewFailureReason;
}

void AJTSGameState::SetGameplayPhase(EJTSGameplayPhase NewGameplayPhase)
{
	if (GameplayPhase == NewGameplayPhase)
	{
		if (NewGameplayPhase != EJTSGameplayPhase::EarthCollection)
		{
			EarthCollectionEndTimeSeconds = 0.0;
			SetEarthCollectionRemainingTime(0.0f);
		}

		return;
	}

	GameplayPhase = NewGameplayPhase;
	if (GameplayPhase == EJTSGameplayPhase::EarthCollection)
	{
		RefreshCachedRemainingTime();
	}
	else
	{
		EarthCollectionEndTimeSeconds = 0.0;
		SetEarthCollectionRemainingTime(0.0f);
	}

	OnGameplayPhaseChanged.Broadcast(GameplayPhase);
}

void AJTSGameState::SetEarthCollectionEndTime(double NewEndTimeSeconds)
{
	EarthCollectionEndTimeSeconds = FMath::Max(0.0, NewEndTimeSeconds);
	RefreshCachedRemainingTime();
}

void AJTSGameState::SetEarthCollectionRemainingTime(float NewRemainingTime)
{
	const float ClampedRemainingTime = GameplayPhase == EJTSGameplayPhase::EarthCollection
		? FMath::Max(0.0f, NewRemainingTime)
		: 0.0f;

	if (FMath::IsNearlyEqual(EarthCollectionRemainingTime, ClampedRemainingTime))
	{
		return;
	}

	EarthCollectionRemainingTime = ClampedRemainingTime;
	OnEarthCollectionTimeChanged.Broadcast(EarthCollectionRemainingTime);
}

void AJTSGameState::RefreshCachedRemainingTime()
{
	if (GameplayPhase != EJTSGameplayPhase::EarthCollection || EarthCollectionEndTimeSeconds <= 0.0)
	{
		SetEarthCollectionRemainingTime(0.0f);
		return;
	}

	const UWorld* const World = GetWorld();
	const float RemainingTime = World != nullptr
		? FMath::Max(
			0.0f,
			static_cast<float>(EarthCollectionEndTimeSeconds - static_cast<double>(World->GetTimeSeconds())))
		: 0.0f;
	SetEarthCollectionRemainingTime(RemainingTime);
}
