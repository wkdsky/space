// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Player/JTSPlayerState.h"

#include "GameFramework/OnlineReplStructs.h"
#include "Net/UnrealNetwork.h"

AJTSPlayerState::AJTSPlayerState()
{
	bReplicates = true;
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

void AJTSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSPlayerState, bReady);
	DOREPLIFETIME(AJTSPlayerState, bIsExpeditionHost);
	DOREPLIFETIME(AJTSPlayerState, AvatarColor);
	DOREPLIFETIME(AJTSPlayerState, ExpeditionStatus);
	DOREPLIFETIME(AJTSPlayerState, bIsBoarded);
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
	}
}
