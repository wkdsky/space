// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Core/JTSGameState.h"

#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AJTSGameState::AJTSGameState()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

EJTSGameplayPhase AJTSGameState::GetGameplayPhase() const { return GameplayPhase; }

float AJTSGameState::GetEarthCollectionRemainingTime() const
{
	if (GameplayPhase != EJTSGameplayPhase::EarthCollection)
	{
		return 0.0f;
	}
	if (EarthCollectionEndTimeSeconds <= 0.0)
	{
		return FMath::Max(0.0f, EarthCollectionRemainingTime);
	}
	return FMath::Max(0.0f, static_cast<float>(EarthCollectionEndTimeSeconds - GetSynchronizedServerTimeSeconds()));
}

bool AJTSGameState::IsEarthCollectionActive() const { return GameplayPhase == EJTSGameplayPhase::EarthCollection; }
bool AJTSGameState::IsWaitingToStart() const { return GameplayPhase == EJTSGameplayPhase::WaitingToStart; }
bool AJTSGameState::IsEarthCollectionFinished() const { return GameplayPhase == EJTSGameplayPhase::EarthCollectionFinished; }
bool AJTSGameState::IsLaunching() const { return GameplayPhase == EJTSGameplayPhase::Launching; }
bool AJTSGameState::IsEarthCaptureFailure() const { return GameplayPhase == EJTSGameplayPhase::EarthCaptureFailure; }
bool AJTSGameState::IsMoonArrivalSuccess() const { return GameplayPhase == EJTSGameplayPhase::MoonArrivalSuccess; }
bool AJTSGameState::IsMoonExploration() const { return GameplayPhase == EJTSGameplayPhase::MoonExploration; }
bool AJTSGameState::IsSpaceFlight() const { return GameplayPhase == EJTSGameplayPhase::SpaceFlight; }
bool AJTSGameState::IsSuccessfulOutcome() const { return IsMoonArrivalSuccess(); }
EJTSFailureReason AJTSGameState::GetFailureReason() const { return FailureReason; }

double AJTSGameState::GetSynchronizedServerTimeSeconds() const { return GetServerWorldTimeSeconds(); }

void AJTSGameState::SetGameplayPhase(EJTSGameplayPhase NewGameplayPhase)
{
	if (!HasAuthority())
	{
		return;
	}
	const bool bChanged = GameplayPhase != NewGameplayPhase;
	GameplayPhase = NewGameplayPhase;
	if (GameplayPhase != EJTSGameplayPhase::WaitingToStart)
	{
		bAcceptingNewPlayers = false;
	}
	if (GameplayPhase == EJTSGameplayPhase::EarthCollection)
	{
		RefreshCachedRemainingTime();
	}
	else
	{
		EarthCollectionEndTimeSeconds = 0.0;
		SetEarthCollectionRemainingTime(0.0f);
	}
	if (bChanged)
	{
		OnRep_GameplayPhase();
	}
}

void AJTSGameState::SetAcceptingNewPlayers(bool bNewAcceptingNewPlayers)
{
	if (HasAuthority() && bAcceptingNewPlayers != bNewAcceptingNewPlayers)
	{
		bAcceptingNewPlayers = bNewAcceptingNewPlayers;
		OnRep_ExpeditionState();
	}
}

void AJTSGameState::SetFailureReason(EJTSFailureReason NewFailureReason)
{
	if (HasAuthority())
	{
		FailureReason = NewFailureReason;
		OnRep_GameplayPhase();
	}
}

void AJTSGameState::SetEarthCollectionEndTime(double NewEndTimeSeconds)
{
	if (HasAuthority())
	{
		EarthCollectionEndTimeSeconds = FMath::Max(0.0, NewEndTimeSeconds);
		RefreshCachedRemainingTime();
		OnRep_EarthCollectionDeadline();
	}
}

void AJTSGameState::SetEarthCollectionRemainingTime(float NewRemainingTime)
{
	if (!HasAuthority())
	{
		return;
	}
	const float ClampedRemainingTime = GameplayPhase == EJTSGameplayPhase::EarthCollection ? FMath::Max(0.0f, NewRemainingTime) : 0.0f;
	if (!FMath::IsNearlyEqual(EarthCollectionRemainingTime, ClampedRemainingTime))
	{
		EarthCollectionRemainingTime = ClampedRemainingTime;
		OnEarthCollectionTimeChanged.Broadcast(EarthCollectionRemainingTime);
	}
}

void AJTSGameState::SetEarthLaunchFuelRequirement(float NewRequirement)
{
	if (HasAuthority())
	{
		EarthLaunchFuelRequirement = FMath::Max(0.0f, NewRequirement);
		OnRep_ExpeditionState();
	}
}

void AJTSGameState::SetActiveSpacecraft(AJTSSpacecraftActor* NewSpacecraft)
{
	if (HasAuthority())
	{
		ActiveSpacecraft = NewSpacecraft;
		OnRep_ExpeditionState();
	}
}

void AJTSGameState::SetCurrentPlanetId(const FString& NewPlanetId)
{
	if (HasAuthority())
	{
		CurrentPlanetId = NewPlanetId;
		OnRep_ExpeditionState();
	}
}

void AJTSGameState::SetExpeditionId(const FString& NewExpeditionId)
{
	if (HasAuthority())
	{
		ExpeditionId = NewExpeditionId;
		OnRep_ExpeditionState();
	}
}

void AJTSGameState::RefreshCachedRemainingTime()
{
	if (GameplayPhase != EJTSGameplayPhase::EarthCollection || EarthCollectionEndTimeSeconds <= 0.0)
	{
		SetEarthCollectionRemainingTime(0.0f);
		return;
	}
	SetEarthCollectionRemainingTime(FMath::Max(0.0f, static_cast<float>(EarthCollectionEndTimeSeconds - GetSynchronizedServerTimeSeconds())));
}

void AJTSGameState::OnRep_GameplayPhase() { OnGameplayPhaseChanged.Broadcast(GameplayPhase); }
void AJTSGameState::OnRep_EarthCollectionDeadline() { OnEarthCollectionTimeChanged.Broadcast(GetEarthCollectionRemainingTime()); }
void AJTSGameState::OnRep_ExpeditionState() { OnExpeditionStateChanged.Broadcast(); }

void AJTSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSGameState, GameplayPhase);
	DOREPLIFETIME(AJTSGameState, bAcceptingNewPlayers);
	DOREPLIFETIME(AJTSGameState, FailureReason);
	DOREPLIFETIME(AJTSGameState, EarthCollectionEndTimeSeconds);
	DOREPLIFETIME(AJTSGameState, EarthCollectionRemainingTime);
	DOREPLIFETIME(AJTSGameState, EarthLaunchFuelRequirement);
	DOREPLIFETIME(AJTSGameState, ActiveSpacecraft);
	DOREPLIFETIME(AJTSGameState, CurrentPlanetId);
	DOREPLIFETIME(AJTSGameState, ExpeditionId);
}
