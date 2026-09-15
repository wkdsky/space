// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"

#include "JTSVoiceSubsystem.generated.h"

class IVoiceChat;
class IVoiceChatUser;

UENUM(BlueprintType)
enum class EJTSVoiceConnectionState : uint8
{
	Unavailable,
	Initializing,
	Connected,
	Failed
};

UENUM(BlueprintType)
enum class EJTSVoiceRadioMode : uint8
{
	Proximity,
	Radio
};

/** Explicit extension point for future provider audio/DSP routes; baseline remains volume-only. */
UENUM(BlueprintType)
enum class EJTSVoiceAcousticMode : uint8
{
	Baseline,
	ProviderPositional,
	FutureAdvancedDsp
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnJTSVoiceStateChanged, EJTSVoiceConnectionState, NewState, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnJTSPlayerTalkingChanged, const FString&, VoiceIdentity, bool, bIsTalking);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnJTSVoiceParticipantChanged, const FString&, VoiceIdentity, bool, bJoined);

/**
 * Presentation/control layer over UE VoiceChat. Authentication and channel tokens remain owned by
 * the configured platform/session provider; this class never sends raw audio or invents a backend.
 */
UCLASS(Config = Game)
class SPACE_API UJTSVoiceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Voice")
	void InitializeVoice();

	/** Starts the provider transport after OnlineSubsystem has established its lobby/session voice channel. */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void AttachSessionVoiceTransport(const FString& AuthorizedChannelName);

	/** Stops local transmission and disconnects the UE provider without inventing a voice backend. */
	void ShutdownVoice();

	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetInputMuted(bool bMuted);

	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetPlayerMuted(const FString& PlayerName, bool bMuted);

	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetRadioMode(EJTSVoiceRadioMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetRoutingMode(EJTSVoiceRadioMode NewMode) { SetRadioMode(NewMode); }

	UFUNCTION(BlueprintPure, Category = "Voice")
	bool CanUseRadio() const;

	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetInputVolume(float NewVolume);

	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetOutputVolume(float NewVolume);

	UFUNCTION(BlueprintPure, Category = "Voice")
	EJTSVoiceConnectionState GetConnectionState() const { return ConnectionState; }

	UFUNCTION(BlueprintPure, Category = "Voice")
	bool IsVoiceAvailable() const { return VoiceChat != nullptr && ConnectionState == EJTSVoiceConnectionState::Connected; }

	UFUNCTION(BlueprintPure, Category = "Voice")
	bool SupportsProviderPositionalVoice() const { return bSupportsProviderPositionalVoice; }

	UFUNCTION(BlueprintPure, Category = "Voice")
	bool IsPlayerTalking(const FString& VoiceIdentity) const { return TalkingPlayers.Contains(VoiceIdentity); }

	UFUNCTION(BlueprintPure, Category = "Voice")
	bool IsPlayerMuted(const FString& VoiceIdentity) const;

	UFUNCTION(BlueprintPure, Category = "Voice")
	bool IsInputMuted() const { return bInputMuted; }

	UFUNCTION(BlueprintPure, Category = "Voice")
	TArray<FString> GetInputDeviceNames() const;

	UFUNCTION(BlueprintPure, Category = "Voice")
	TArray<FString> GetOutputDeviceNames() const;

	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SelectInputDevice(const FString& DeviceId);

	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SelectOutputDevice(const FString& DeviceId);

	UPROPERTY(BlueprintAssignable, Category = "Voice")
	FOnJTSVoiceStateChanged OnVoiceStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Voice")
	FOnJTSPlayerTalkingChanged OnPlayerTalkingChanged;

	UPROPERTY(BlueprintAssignable, Category = "Voice")
	FOnJTSVoiceParticipantChanged OnVoiceParticipantChanged;

	UPROPERTY(Config, EditAnywhere, Category = "Voice|Proximity", meta = (ClampMin = "100.0"))
	float ProximityRange = 2500.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Voice|Proximity", meta = (ClampMin = "0.0"))
	float FullVolumeDistance = 300.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Voice|Proximity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OccludedVolumeMultiplier = 0.35f;

	UPROPERTY(Config, EditAnywhere, Category = "Voice|Proximity", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float SpatialUpdateInterval = 0.10f;

	UPROPERTY(Config, EditAnywhere, Category = "Voice|Levels")
	float InputVolume = 1.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Voice|Levels")
	float OutputVolume = 1.0f;

	/** Only enable when the configured provider exposes an authorized positional channel. */
	UPROPERTY(Config, EditAnywhere, Category = "Voice|Provider")
	bool bSupportsProviderPositionalVoice = false;

	/** An actual provider-authorized positional channel. Leave empty to use the portable volume-only fallback. */
	UPROPERTY(Config, EditAnywhere, Category = "Voice|Provider")
	FString ProviderPositionalChannelName;

	/** Reserved for a future authenticated radio channel; false prevents fake token/channel routing. */
	UPROPERTY(Config, EditAnywhere, Category = "Voice|Radio")
	bool bRadioFeatureEnabled = false;

	UPROPERTY(Config, EditAnywhere, Category = "Voice|Radio")
	FString RadioChannelName;

private:
	void HandleVoiceConnected(const struct FVoiceChatResult& Result);
	void HandleTalkingChanged(const FString& ChannelName, const FString& PlayerName, bool bIsTalking);
	void HandleParticipantAdded(const FString& ChannelName, const FString& PlayerName);
	void HandleParticipantRemoved(const FString& ChannelName, const FString& PlayerName);
	void UpdateSpatialVoice();
	float ComputeProximityGain(float Distance) const;
	float ComputeOcclusionGain(UWorld* World, const class APawn* LocalPawn, const class APawn* RemotePawn) const;
	void ApplyProviderVoiceMix(const FString& VoiceIdentity, float Volume, const FVector& LocalPosition);
	FString ResolveVoiceIdentity(const FString& VoiceIdentityOrDisplayName) const;
	void SetState(EJTSVoiceConnectionState NewState, const FString& Message);

	IVoiceChat* VoiceChat = nullptr;
	/** The default IVoiceChat instance is also its default IVoiceChatUser in UE 5.8. */
	IVoiceChatUser* VoiceUser = nullptr;
	FDelegateHandle TalkingDelegateHandle;
	FDelegateHandle ParticipantAddedDelegateHandle;
	FDelegateHandle ParticipantRemovedDelegateHandle;
	FTimerHandle SpatialUpdateTimer;
	EJTSVoiceConnectionState ConnectionState = EJTSVoiceConnectionState::Unavailable;
	EJTSVoiceRadioMode RadioMode = EJTSVoiceRadioMode::Proximity;
	TSet<FString> LocallyMutedPlayers;
	TSet<FString> TalkingPlayers;

	UPROPERTY(Config)
	bool bInputMuted = false;

	UPROPERTY(Config)
	FString SelectedInputDeviceId;

	UPROPERTY(Config)
	FString SelectedOutputDeviceId;
};
