// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Player/JTSPlayerState.h"

#include "GameFramework/OnlineReplStructs.h"
#include "Net/UnrealNetwork.h"

AJTSPlayerState::AJTSPlayerState()
{
	bReplicates = true;
}

int32 AJTSPlayerState::GetExperienceRequiredForNextLevel() const
{
	return ProgressionLevel >= FJTSPlayerProgressionRules::MaximumLevel
		? 0
		: FJTSPlayerProgressionRules::GetExperienceRequiredForNextLevel(ProgressionLevel);
}

int32 AJTSPlayerState::GetAbilityRank(const EJTSPlayerAbility Ability) const
{
	switch (Ability)
	{
	case EJTSPlayerAbility::InventorySlots: return InventorySlotAbilityRank;
	case EJTSPlayerAbility::StackLimit: return StackLimitAbilityRank;
	case EJTSPlayerAbility::RunSpeed: return RunSpeedAbilityRank;
	default: return 0;
	}
}

int32 AJTSPlayerState::GetInventorySlotCapacityBonus() const
{
	return FJTSPlayerProgressionRules::GetInventorySlotBonus(InventorySlotAbilityRank);
}

int32 AJTSPlayerState::GetItemStackLimit() const
{
	return FJTSPlayerProgressionRules::GetStackLimit(StackLimitAbilityRank);
}

float AJTSPlayerState::GetRunSpeedMultiplier() const
{
	return FJTSPlayerProgressionRules::GetRunSpeedMultiplier(RunSpeedAbilityRank);
}

FLinearColor AJTSPlayerState::GetAvatarLinearColor() const
{
	switch (AvatarColor)
	{
	case EJTSAvatarColor::Orange:
		return FLinearColor(1.0f, 0.34f, 0.06f, 1.0f);
	case EJTSAvatarColor::Green:
		return FLinearColor(0.18f, 0.85f, 0.28f, 1.0f);
	case EJTSAvatarColor::Purple:
		return FLinearColor(0.58f, 0.25f, 0.90f, 1.0f);
	case EJTSAvatarColor::Blue:
	default:
		return FLinearColor(0.10f, 0.45f, 1.0f, 1.0f);
	}
}

void AJTSPlayerState::SetReady(bool bNewReady)
{
	if (HasAuthority())
	{
		bReady = bNewReady;
		if (ExpeditionStatus == EJTSPlayerExpeditionStatus::InLobby || ExpeditionStatus == EJTSPlayerExpeditionStatus::Ready)
		{
			ExpeditionStatus = bReady ? EJTSPlayerExpeditionStatus::Ready : EJTSPlayerExpeditionStatus::InLobby;
		}
		OnRep_NetworkState();
	}
}

void AJTSPlayerState::SetExpeditionHost(bool bNewHost)
{
	if (HasAuthority())
	{
		bIsExpeditionHost = bNewHost;
		OnRep_NetworkState();
	}
}

void AJTSPlayerState::SetAvatarColor(EJTSAvatarColor NewAvatarColor)
{
	if (HasAuthority())
	{
		AvatarColor = NewAvatarColor;
		OnRep_NetworkState();
	}
}

void AJTSPlayerState::SetExpeditionStatus(EJTSPlayerExpeditionStatus NewStatus)
{
	if (HasAuthority())
	{
		ExpeditionStatus = NewStatus;
		OnRep_NetworkState();
	}
}

void AJTSPlayerState::SetBoarded(bool bNewBoarded)
{
	if (HasAuthority())
	{
		bIsBoarded = bNewBoarded;
		if (bNewBoarded && ExpeditionStatus == EJTSPlayerExpeditionStatus::Active)
		{
			ExpeditionStatus = EJTSPlayerExpeditionStatus::Boarded;
		}
		else if (!bNewBoarded && ExpeditionStatus == EJTSPlayerExpeditionStatus::Boarded)
		{
			ExpeditionStatus = EJTSPlayerExpeditionStatus::Active;
		}
		OnRep_NetworkState();
	}
}

void AJTSPlayerState::GrantExperience(const int32 Amount)
{
	if (!HasAuthority() || Amount <= 0 || ProgressionLevel >= FJTSPlayerProgressionRules::MaximumLevel)
	{
		return;
	}

	ExperienceInCurrentLevel = FMath::Min(MAX_int32 - Amount, ExperienceInCurrentLevel) + Amount;
	while (ProgressionLevel < FJTSPlayerProgressionRules::MaximumLevel)
	{
		const int32 RequiredExperience = GetExperienceRequiredForNextLevel();
		if (RequiredExperience <= 0 || ExperienceInCurrentLevel < RequiredExperience)
		{
			break;
		}

		ExperienceInCurrentLevel -= RequiredExperience;
		++ProgressionLevel;
		++UnspentAbilityPoints;
	}

	if (ProgressionLevel >= FJTSPlayerProgressionRules::MaximumLevel)
	{
		ExperienceInCurrentLevel = 0;
	}

	++ProgressionRevision;
	NotifyProgressionChanged();
}

bool AJTSPlayerState::GrantDebugLevels()
{
	if (!HasAuthority() || ProgressionLevel + 10 > FJTSPlayerProgressionRules::MaximumLevel)
	{
		return false;
	}

	ProgressionLevel += 10;
	UnspentAbilityPoints += 10;
	++ProgressionRevision;
	NotifyProgressionChanged();
	return true;
}

bool AJTSPlayerState::CommitAbilityAllocation(const FJTSAbilityAllocation& Allocation)
{
	if (!HasAuthority()
		|| Allocation.InventorySlotRanks < 0
		|| Allocation.StackLimitRanks < 0
		|| Allocation.RunSpeedRanks < 0)
	{
		return false;
	}

	const int32 TotalCost = Allocation.GetTotalPointCost();
	if (TotalCost <= 0 || TotalCost > UnspentAbilityPoints
		|| InventorySlotAbilityRank + Allocation.InventorySlotRanks > FJTSPlayerProgressionRules::MaximumAbilityRank
		|| StackLimitAbilityRank + Allocation.StackLimitRanks > FJTSPlayerProgressionRules::MaximumAbilityRank
		|| RunSpeedAbilityRank + Allocation.RunSpeedRanks > FJTSPlayerProgressionRules::MaximumAbilityRank)
	{
		return false;
	}

	InventorySlotAbilityRank += Allocation.InventorySlotRanks;
	StackLimitAbilityRank += Allocation.StackLimitRanks;
	RunSpeedAbilityRank += Allocation.RunSpeedRanks;
	UnspentAbilityPoints -= TotalCost;
	++ProgressionRevision;
	NotifyProgressionChanged();
	return true;
}

void AJTSPlayerState::RestoreProgression(
	const int32 NewLevel,
	const int32 NewExperienceInCurrentLevel,
	const int32 NewUnspentAbilityPoints,
	const int32 NewInventorySlotRank,
	const int32 NewStackLimitRank,
	const int32 NewRunSpeedRank)
{
	if (!HasAuthority())
	{
		return;
	}

	ProgressionLevel = FMath::Clamp(NewLevel, 1, FJTSPlayerProgressionRules::MaximumLevel);
	ExperienceInCurrentLevel = ProgressionLevel >= FJTSPlayerProgressionRules::MaximumLevel
		? 0
		: FMath::Clamp(NewExperienceInCurrentLevel, 0, FMath::Max(0, GetExperienceRequiredForNextLevel() - 1));
	UnspentAbilityPoints = FMath::Max(0, NewUnspentAbilityPoints);
	InventorySlotAbilityRank = FMath::Clamp(NewInventorySlotRank, 0, FJTSPlayerProgressionRules::MaximumAbilityRank);
	StackLimitAbilityRank = FMath::Clamp(NewStackLimitRank, 0, FJTSPlayerProgressionRules::MaximumAbilityRank);
	RunSpeedAbilityRank = FMath::Clamp(NewRunSpeedRank, 0, FJTSPlayerProgressionRules::MaximumAbilityRank);
	++ProgressionRevision;
	NotifyProgressionChanged();
}

FString AJTSPlayerState::GetOnlineIdentityString() const
{
	const FUniqueNetIdRepl& NetId = GetUniqueId();
	if (NetId.IsValid())
	{
		return NetId->ToString();
	}
	return FString::Printf(TEXT("LocalPlayer-%d"), GetPlayerId());
}

void AJTSPlayerState::OnRep_NetworkState()
{
	OnNetworkStateChanged.Broadcast();
}

void AJTSPlayerState::OnRep_Progression()
{
	NotifyProgressionChanged();
}

void AJTSPlayerState::NotifyProgressionChanged()
{
	OnProgressionChanged.Broadcast();
	// Existing character/view bindings already listen to the network-state delegate.  Reuse that
	// notification path rather than requiring every presentation consumer to duplicate bindings.
	OnNetworkStateChanged.Broadcast();
}

void AJTSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSPlayerState, bReady);
	DOREPLIFETIME(AJTSPlayerState, bIsExpeditionHost);
	DOREPLIFETIME(AJTSPlayerState, AvatarColor);
	DOREPLIFETIME(AJTSPlayerState, ExpeditionStatus);
	DOREPLIFETIME(AJTSPlayerState, bIsBoarded);
	DOREPLIFETIME(AJTSPlayerState, ProgressionLevel);
	DOREPLIFETIME(AJTSPlayerState, ExperienceInCurrentLevel);
	DOREPLIFETIME(AJTSPlayerState, UnspentAbilityPoints);
	DOREPLIFETIME(AJTSPlayerState, InventorySlotAbilityRank);
	DOREPLIFETIME(AJTSPlayerState, StackLimitAbilityRank);
	DOREPLIFETIME(AJTSPlayerState, RunSpeedAbilityRank);
	DOREPLIFETIME(AJTSPlayerState, ProgressionRevision);
}

void AJTSPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	if (AJTSPlayerState* const Target = Cast<AJTSPlayerState>(PlayerState))
	{
		Target->bReady = bReady;
		Target->bIsExpeditionHost = bIsExpeditionHost;
		Target->AvatarColor = AvatarColor;
		Target->ExpeditionStatus = ExpeditionStatus;
		Target->bIsBoarded = bIsBoarded;
		Target->ProgressionLevel = ProgressionLevel;
		Target->ExperienceInCurrentLevel = ExperienceInCurrentLevel;
		Target->UnspentAbilityPoints = UnspentAbilityPoints;
		Target->InventorySlotAbilityRank = InventorySlotAbilityRank;
		Target->StackLimitAbilityRank = StackLimitAbilityRank;
		Target->RunSpeedAbilityRank = RunSpeedAbilityRank;
		Target->ProgressionRevision = ProgressionRevision;
	}
}

void AJTSPlayerState::OverrideWith(APlayerState* PlayerState)
{
	Super::OverrideWith(PlayerState);
	if (const AJTSPlayerState* const Source = Cast<AJTSPlayerState>(PlayerState))
	{
		bReady = Source->bReady;
		bIsExpeditionHost = Source->bIsExpeditionHost;
		AvatarColor = Source->AvatarColor;
		ExpeditionStatus = Source->ExpeditionStatus;
		bIsBoarded = Source->bIsBoarded;
		ProgressionLevel = Source->ProgressionLevel;
		ExperienceInCurrentLevel = Source->ExperienceInCurrentLevel;
		UnspentAbilityPoints = Source->UnspentAbilityPoints;
		InventorySlotAbilityRank = Source->InventorySlotAbilityRank;
		StackLimitAbilityRank = Source->StackLimitAbilityRank;
		RunSpeedAbilityRank = Source->RunSpeedAbilityRank;
		ProgressionRevision = Source->ProgressionRevision;
	}
}
