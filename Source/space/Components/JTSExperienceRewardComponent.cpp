// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSExperienceRewardComponent.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Player/JTSPlayerState.h"

UJTSExperienceRewardComponent::UJTSExperienceRewardComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UJTSExperienceRewardComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* const Owner = GetOwner(); IsValid(Owner) && Owner->HasAuthority())
	{
		if (UJTSHealthComponent* const Health = Owner->FindComponentByClass<UJTSHealthComponent>())
		{
			BoundHealthComponent = Health;
			Health->OnDeath.AddDynamic(this, &UJTSExperienceRewardComponent::HandleOwnerDeath);
		}
	}
}

void UJTSExperienceRewardComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UJTSHealthComponent* const Health = BoundHealthComponent.Get())
	{
		Health->OnDeath.RemoveDynamic(this, &UJTSExperienceRewardComponent::HandleOwnerDeath);
	}
	BoundHealthComponent.Reset();
	Super::EndPlay(EndPlayReason);
}

bool UJTSExperienceRewardComponent::IsEligibleKiller(const AJTSPlayerState* const PlayerState)
{
	return IsValid(PlayerState)
		&& (PlayerState->GetExpeditionStatus() == EJTSPlayerExpeditionStatus::Active
			|| PlayerState->GetExpeditionStatus() == EJTSPlayerExpeditionStatus::Boarded);
}

bool UJTSExperienceRewardComponent::IsEligibleBossTeammate(const AJTSPlayerState* const PlayerState)
{
	return IsEligibleKiller(PlayerState)
		|| (IsValid(PlayerState) && PlayerState->GetExpeditionStatus() == EJTSPlayerExpeditionStatus::Dead);
}

void UJTSExperienceRewardComponent::HandleOwnerDeath(AController* const InstigatorController, AActor* const DamageCauser)
{
	static_cast<void>(DamageCauser);
	AActor* const Owner = GetOwner();
	if (bRewardGranted || !IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	AJTSPlayerState* const KillerState = InstigatorController != nullptr
		? InstigatorController->GetPlayerState<AJTSPlayerState>()
		: nullptr;
	if (!IsEligibleKiller(KillerState))
	{
		return;
	}

	bRewardGranted = true;
	const int32 Reward = FMath::Max(1, ExperienceReward);
	if (!bBossReward)
	{
		KillerState->GrantExperience(Reward);
		return;
	}

	// A Boss is one shared expedition event. The killer gets an explicit double reward; every other
	// expedition teammate gets the unmodified value, including a teammate waiting to respawn.
	// Spectators and players that never entered the expedition do not receive passive progression.
	KillerState->GrantExperience(Reward * 2);
	if (const UWorld* const World = GetWorld())
	{
		if (const AGameStateBase* const GameState = World->GetGameState())
		{
			for (APlayerState* const RawPlayerState : GameState->PlayerArray)
			{
				AJTSPlayerState* const Teammate = Cast<AJTSPlayerState>(RawPlayerState);
				if (Teammate != KillerState && IsEligibleBossTeammate(Teammate))
				{
					Teammate->GrantExperience(Reward);
				}
			}
		}
	}
}
