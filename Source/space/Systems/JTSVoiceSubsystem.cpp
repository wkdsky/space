// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Systems/JTSVoiceSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "space/Player/JTSPlayerState.h"
#include "VoiceChat.h"

void UJTSVoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UJTSVoiceSubsystem::Deinitialize()
{
	ShutdownVoice();
	VoiceChat = nullptr;
	VoiceUser = nullptr;
	Super::Deinitialize();
}

void UJTSVoiceSubsystem::InitializeVoice()
{
	VoiceChat = IVoiceChat::Get();
	if (VoiceChat == nullptr)
	{
		SetState(EJTSVoiceConnectionState::Unavailable, TEXT("No UE VoiceChat provider is loaded."));
		return;
	}
	if (!VoiceChat->IsInitialized() && !VoiceChat->Initialize())
	{
		SetState(EJTSVoiceConnectionState::Failed, TEXT("The VoiceChat provider could not initialize."));
		return;
	}
	// UE 5.8's default IVoiceChat object is also its default IVoiceChatUser. We deliberately
	// use that user interface so participant/mute/device APIs remain provider-owned.
	VoiceUser = static_cast<IVoiceChatUser*>(VoiceChat);
	if (!TalkingDelegateHandle.IsValid() && VoiceUser != nullptr)
	{
		TalkingDelegateHandle = VoiceUser->OnVoiceChatPlayerTalkingUpdated().AddUObject(this, &UJTSVoiceSubsystem::HandleTalkingChanged);
		ParticipantAddedDelegateHandle = VoiceUser->OnVoiceChatPlayerAdded().AddUObject(this, &UJTSVoiceSubsystem::HandleParticipantAdded);
		ParticipantRemovedDelegateHandle = VoiceUser->OnVoiceChatPlayerRemoved().AddUObject(this, &UJTSVoiceSubsystem::HandleParticipantRemoved);
		VoiceUser->SetAudioInputVolume(FMath::Clamp(InputVolume, 0.0f, 2.0f));
		VoiceUser->SetAudioOutputVolume(FMath::Clamp(OutputVolume, 0.0f, 2.0f));
		VoiceUser->SetAudioInputDeviceMuted(bInputMuted);
		VoiceUser->SetInputDeviceId(SelectedInputDeviceId);
		VoiceUser->SetOutputDeviceId(SelectedOutputDeviceId);
	}
	if (VoiceChat->IsConnected())
	{
		SetState(EJTSVoiceConnectionState::Connected, TEXT("Voice provider connected."));
		return;
	}
	SetState(EJTSVoiceConnectionState::Initializing, TEXT("Connecting voice provider."));
	VoiceChat->Connect(FOnVoiceChatConnectCompleteDelegate::CreateUObject(this, &UJTSVoiceSubsystem::HandleVoiceConnected));
}

void UJTSVoiceSubsystem::AttachSessionVoiceTransport(const FString& AuthorizedChannelName)
{
	// EOS lobby voice is joined by the OnlineSubsystem session setting. A join code is deliberately
	// not treated as a VoiceChat channel/token; providers own those credentials.
	(void)AuthorizedChannelName;
	if (VoiceChat == nullptr)
	{
		InitializeVoice();
	}
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SpatialUpdateTimer,
			this,
			&UJTSVoiceSubsystem::UpdateSpatialVoice,
			FMath::Clamp(SpatialUpdateInterval, 0.05f, 1.0f),
			true);
	}
}

void UJTSVoiceSubsystem::ShutdownVoice()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpatialUpdateTimer);
	}
	if (VoiceUser != nullptr)
	{
		VoiceUser->TransmitToNoChannels();
		if (TalkingDelegateHandle.IsValid()) VoiceUser->OnVoiceChatPlayerTalkingUpdated().Remove(TalkingDelegateHandle);
		if (ParticipantAddedDelegateHandle.IsValid()) VoiceUser->OnVoiceChatPlayerAdded().Remove(ParticipantAddedDelegateHandle);
		if (ParticipantRemovedDelegateHandle.IsValid()) VoiceUser->OnVoiceChatPlayerRemoved().Remove(ParticipantRemovedDelegateHandle);
	}
	TalkingDelegateHandle.Reset();
	ParticipantAddedDelegateHandle.Reset();
	ParticipantRemovedDelegateHandle.Reset();
	LocallyMutedPlayers.Reset();
	TalkingPlayers.Reset();
	if (VoiceChat != nullptr && VoiceChat->IsConnected())
	{
		VoiceChat->Disconnect(FOnVoiceChatDisconnectCompleteDelegate::CreateLambda([](const FVoiceChatResult&) {}));
	}
	SetState(EJTSVoiceConnectionState::Unavailable, TEXT("Voice disconnected."));
}

void UJTSVoiceSubsystem::SetInputMuted(bool bMuted)
{
	bInputMuted = bMuted;
	if (VoiceUser != nullptr)
	{
		VoiceUser->SetAudioInputDeviceMuted(bMuted);
	}
	SaveConfig();
}

void UJTSVoiceSubsystem::SetPlayerMuted(const FString& PlayerName, bool bMuted)
{
	const FString VoiceIdentity = ResolveVoiceIdentity(PlayerName);
	if (bMuted) LocallyMutedPlayers.Add(VoiceIdentity); else LocallyMutedPlayers.Remove(VoiceIdentity);
	if (VoiceUser != nullptr)
	{
		VoiceUser->SetPlayerMuted(VoiceIdentity, bMuted);
	}
}

bool UJTSVoiceSubsystem::IsPlayerMuted(const FString& VoiceIdentity) const
{
	return LocallyMutedPlayers.Contains(ResolveVoiceIdentity(VoiceIdentity));
}

void UJTSVoiceSubsystem::SetRadioMode(EJTSVoiceRadioMode NewMode)
{
	RadioMode = NewMode == EJTSVoiceRadioMode::Radio && !CanUseRadio()
		? EJTSVoiceRadioMode::Proximity
		: NewMode;
	if (VoiceUser == nullptr)
	{
		return;
	}
	if (RadioMode == EJTSVoiceRadioMode::Radio)
	{
		TSet<FString> RadioChannels;
		RadioChannels.Add(RadioChannelName);
		VoiceUser->TransmitToSpecificChannels(RadioChannels);
	}
	else
	{
		VoiceUser->TransmitToAllChannels();
	}
}

bool UJTSVoiceSubsystem::CanUseRadio() const
{
	return bRadioFeatureEnabled && VoiceUser != nullptr && !RadioChannelName.IsEmpty();
}

void UJTSVoiceSubsystem::SetInputVolume(float NewVolume)
{
	InputVolume = FMath::Clamp(NewVolume, 0.0f, 2.0f);
	if (VoiceUser != nullptr) VoiceUser->SetAudioInputVolume(InputVolume);
	SaveConfig();
}

void UJTSVoiceSubsystem::SetOutputVolume(float NewVolume)
{
	OutputVolume = FMath::Clamp(NewVolume, 0.0f, 2.0f);
	if (VoiceUser != nullptr) VoiceUser->SetAudioOutputVolume(OutputVolume);
	SaveConfig();
}

TArray<FString> UJTSVoiceSubsystem::GetInputDeviceNames() const
{
	TArray<FString> Result;
	if (VoiceUser != nullptr)
	{
		for (const FVoiceChatDeviceInfo& Device : VoiceUser->GetAvailableInputDeviceInfos()) Result.Add(Device.DisplayName);
	}
	return Result;
}

TArray<FString> UJTSVoiceSubsystem::GetOutputDeviceNames() const
{
	TArray<FString> Result;
	if (VoiceUser != nullptr)
	{
		for (const FVoiceChatDeviceInfo& Device : VoiceUser->GetAvailableOutputDeviceInfos()) Result.Add(Device.DisplayName);
	}
	return Result;
}

void UJTSVoiceSubsystem::SelectInputDevice(const FString& DeviceId)
{
	SelectedInputDeviceId = DeviceId;
	if (VoiceUser != nullptr)
	{
		for (const FVoiceChatDeviceInfo& Device : VoiceUser->GetAvailableInputDeviceInfos())
		{
			if (Device.DisplayName == DeviceId)
			{
				SelectedInputDeviceId = Device.Id;
				break;
			}
		}
		VoiceUser->SetInputDeviceId(SelectedInputDeviceId);
	}
	SaveConfig();
}

void UJTSVoiceSubsystem::SelectOutputDevice(const FString& DeviceId)
{
	SelectedOutputDeviceId = DeviceId;
	if (VoiceUser != nullptr)
	{
		for (const FVoiceChatDeviceInfo& Device : VoiceUser->GetAvailableOutputDeviceInfos())
		{
			if (Device.DisplayName == DeviceId)
			{
				SelectedOutputDeviceId = Device.Id;
				break;
			}
		}
		VoiceUser->SetOutputDeviceId(SelectedOutputDeviceId);
	}
	SaveConfig();
}

void UJTSVoiceSubsystem::HandleVoiceConnected(const FVoiceChatResult& Result)
{
	SetState(Result.IsSuccess() ? EJTSVoiceConnectionState::Connected : EJTSVoiceConnectionState::Failed, Result.IsSuccess() ? TEXT("Voice provider connected.") : LexToString(Result));
}

void UJTSVoiceSubsystem::HandleTalkingChanged(const FString& ChannelName, const FString& PlayerName, bool bIsTalking)
{
	(void)ChannelName;
	const FString VoiceIdentity = ResolveVoiceIdentity(PlayerName);
	if (bIsTalking) TalkingPlayers.Add(VoiceIdentity); else TalkingPlayers.Remove(VoiceIdentity);
	OnPlayerTalkingChanged.Broadcast(VoiceIdentity, bIsTalking);
}

void UJTSVoiceSubsystem::HandleParticipantAdded(const FString& ChannelName, const FString& PlayerName)
{
	(void)ChannelName;
	OnVoiceParticipantChanged.Broadcast(ResolveVoiceIdentity(PlayerName), true);
}

void UJTSVoiceSubsystem::HandleParticipantRemoved(const FString& ChannelName, const FString& PlayerName)
{
	(void)ChannelName;
	OnVoiceParticipantChanged.Broadcast(ResolveVoiceIdentity(PlayerName), false);
}

void UJTSVoiceSubsystem::UpdateSpatialVoice()
{
	if (VoiceUser == nullptr || VoiceChat == nullptr || !VoiceChat->IsConnected() || RadioMode == EJTSVoiceRadioMode::Radio)
	{
		return;
	}
	UWorld* const World = GetWorld();
	APlayerController* LocalController = nullptr;
	if (World != nullptr)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* const Candidate = It->Get(); Candidate != nullptr && Candidate->IsLocalController())
			{
				LocalController = Candidate;
				break;
			}
		}
	}
	APawn* const LocalPawn = LocalController != nullptr ? LocalController->GetPawn() : nullptr;
	if (World == nullptr || LocalPawn == nullptr || World->GetGameState() == nullptr)
	{
		return;
	}
	if (bSupportsProviderPositionalVoice && !ProviderPositionalChannelName.IsEmpty())
	{
		VoiceUser->Set3DPosition(ProviderPositionalChannelName, LocalPawn->GetActorLocation());
	}
	for (APlayerState* const PlayerState : World->GetGameState()->PlayerArray)
	{
		APawn* const OtherPawn = PlayerState != nullptr ? PlayerState->GetPawn() : nullptr;
		if (OtherPawn == nullptr || OtherPawn == LocalPawn)
		{
			continue;
		}
		const AJTSPlayerState* const JTSPlayerState = Cast<AJTSPlayerState>(PlayerState);
		const FString VoiceIdentity = JTSPlayerState != nullptr
			? JTSPlayerState->GetVoiceIdentityString()
			: PlayerState->GetPlayerName();
		const float Distance = FVector::Distance(LocalPawn->GetActorLocation(), OtherPawn->GetActorLocation());
		const float Volume = ComputeProximityGain(Distance) * ComputeOcclusionGain(World, LocalPawn, OtherPawn);
		ApplyProviderVoiceMix(VoiceIdentity, LocallyMutedPlayers.Contains(VoiceIdentity) ? 0.0f : Volume, LocalPawn->GetActorLocation());
	}
}

float UJTSVoiceSubsystem::ComputeProximityGain(float Distance) const
{
	const float SafeFullDistance = FMath::Max(0.0f, FullVolumeDistance);
	const float SafeMaxDistance = FMath::Max(SafeFullDistance + KINDA_SMALL_NUMBER, ProximityRange);
	if (Distance <= SafeFullDistance) return 1.0f;
	if (Distance >= SafeMaxDistance) return 0.0f;
	const float Alpha = FMath::Clamp((Distance - SafeFullDistance) / (SafeMaxDistance - SafeFullDistance), 0.0f, 1.0f);
	return 1.0f - FMath::SmoothStep(0.0f, 1.0f, Alpha);
}

float UJTSVoiceSubsystem::ComputeOcclusionGain(UWorld* World, const APawn* LocalPawn, const APawn* RemotePawn) const
{
	if (World == nullptr || LocalPawn == nullptr || RemotePawn == nullptr)
	{
		return 0.0f;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JTSVoiceOcclusion), false, LocalPawn);
	Params.AddIgnoredActor(RemotePawn);
	return World->LineTraceSingleByChannel(Hit, LocalPawn->GetActorLocation(), RemotePawn->GetActorLocation(), ECC_Visibility, Params)
		? FMath::Clamp(OccludedVolumeMultiplier, 0.0f, 1.0f)
		: 1.0f;
}

void UJTSVoiceSubsystem::ApplyProviderVoiceMix(const FString& VoiceIdentity, float Volume, const FVector& LocalPosition)
{
	(void)LocalPosition;
	if (VoiceUser != nullptr)
	{
		VoiceUser->SetPlayerVolume(VoiceIdentity, FMath::Clamp(Volume, 0.0f, 2.0f));
	}
}

FString UJTSVoiceSubsystem::ResolveVoiceIdentity(const FString& VoiceIdentityOrDisplayName) const
{
	const UWorld* const World = GetWorld();
	if (World != nullptr && World->GetGameState() != nullptr)
	{
		for (APlayerState* const PlayerState : World->GetGameState()->PlayerArray)
		{
			if (const AJTSPlayerState* const JTSPlayerState = Cast<AJTSPlayerState>(PlayerState))
			{
				if (JTSPlayerState->GetVoiceIdentityString() == VoiceIdentityOrDisplayName
					|| JTSPlayerState->GetPlayerName() == VoiceIdentityOrDisplayName)
				{
					return JTSPlayerState->GetVoiceIdentityString();
				}
			}
		}
	}
	return VoiceIdentityOrDisplayName;
}

void UJTSVoiceSubsystem::SetState(EJTSVoiceConnectionState NewState, const FString& Message)
{
	ConnectionState = NewState;
	OnVoiceStateChanged.Broadcast(NewState, Message);
}
