// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "space/Core/JTSExpeditionTypes.h"

#include "JTSPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJTSPlayerNetworkStateChanged);

/** Replicated, player-specific state for the four-player expedition lobby and runtime. */
UCLASS()
class SPACE_API AJTSPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AJTSPlayerState();

	UFUNCTION(BlueprintPure, Category = "Expedition")
	bool IsReady() const { return bReady; }

	UFUNCTION(BlueprintPure, Category = "Expedition")
	bool IsExpeditionHost() const { return bIsExpeditionHost; }

	UFUNCTION(BlueprintPure, Category = "Expedition")
	EJTSAvatarColor GetAvatarColor() const { return AvatarColor; }

	UFUNCTION(BlueprintPure, Category = "Expedition")
	EJTSPlayerExpeditionStatus GetExpeditionStatus() const { return ExpeditionStatus; }

	UFUNCTION(BlueprintPure, Category = "Expedition")
	bool IsBoarded() const { return bIsBoarded; }

	/** Stable online identity when an OSS provider supplies one; falls back to the replicated player id for LAN. */
	UFUNCTION(BlueprintPure, Category = "Online")
	FString GetOnlineIdentityString() const;

	/** Voice providers use the same stable identity mapping as lobby/player presentation. */
	UFUNCTION(BlueprintPure, Category = "Voice")
	FString GetVoiceIdentityString() const { return GetOnlineIdentityString(); }

	UFUNCTION(BlueprintPure, Category = "Expedition")
	FLinearColor GetAvatarLinearColor() const;

	void SetReady(bool bNewReady);
	void SetExpeditionHost(bool bNewHost);
	void SetAvatarColor(EJTSAvatarColor NewAvatarColor);
	void SetExpeditionStatus(EJTSPlayerExpeditionStatus NewStatus);
	void SetBoarded(bool bNewBoarded);

	UPROPERTY(BlueprintAssignable, Category = "Expedition")
	FOnJTSPlayerNetworkStateChanged OnNetworkStateChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OverrideWith(APlayerState* PlayerState) override;

private:
	UFUNCTION()
	void OnRep_NetworkState();

	UPROPERTY(ReplicatedUsing = OnRep_NetworkState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Expedition", meta = (AllowPrivateAccess = "true"))
	bool bReady = false;

	UPROPERTY(ReplicatedUsing = OnRep_NetworkState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Expedition", meta = (AllowPrivateAccess = "true"))
	bool bIsExpeditionHost = false;

	UPROPERTY(ReplicatedUsing = OnRep_NetworkState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Expedition", meta = (AllowPrivateAccess = "true"))
	EJTSAvatarColor AvatarColor = EJTSAvatarColor::Blue;

	UPROPERTY(ReplicatedUsing = OnRep_NetworkState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Expedition", meta = (AllowPrivateAccess = "true"))
	EJTSPlayerExpeditionStatus ExpeditionStatus = EJTSPlayerExpeditionStatus::InLobby;

	UPROPERTY(ReplicatedUsing = OnRep_NetworkState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Expedition", meta = (AllowPrivateAccess = "true"))
	bool bIsBoarded = false;
};
