// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Systems/JTSOnlineSessionSubsystem.h"

#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"
#include "Misc/SecureHash.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/Systems/JTSVoiceSubsystem.h"
#include "Widgets/SWindow.h"

namespace
{
	const FName JTSSessionName(TEXT("JumpToSpaceSession"));
	const FName JTSJoinCodeKey(TEXT("JTSJoinCode"));
	const FName JTSBuildKey(TEXT("JTSBuild"));
	const FName JTSPhaseKey(TEXT("JTSPhase"));
	const FName JTSAdmissionOpenKey(TEXT("JTSAdmissionOpen"));
	const FName JTSPasswordProtectedKey(TEXT("JTSPasswordProtected"));
	const FName JTSVisibilityKey(TEXT("JTSVisibility"));
	const TCHAR* JoinCodeAlphabet = TEXT("ABCDEFGHJKMNPQRSTUVWXYZ23456789");

	APlayerController* GetLocalPlayerController(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* const Controller = It->Get(); Controller != nullptr && Controller->IsLocalController())
			{
				return Controller;
			}
		}
		return nullptr;
	}
}

void UJTSOnlineSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!AcquireInterfaces())
	{
		LastError = TEXT("No OnlineSubsystem provider is available. The front end remains available; Start and Join can be retried after configuring a provider.");
	}
	if (GEngine != nullptr)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UJTSOnlineSessionSubsystem::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UJTSOnlineSessionSubsystem::HandleTravelFailure);
	}
	PreExitHandle = FCoreDelegates::OnPreExit.AddUObject(this, &UJTSOnlineSessionSubsystem::BeginShutdown);
	if (FSlateApplication::IsInitialized())
	{
		WindowBeingDestroyedHandle = FSlateApplication::Get().OnWindowBeingDestroyed().AddUObject(this, &UJTSOnlineSessionSubsystem::HandleWindowBeingDestroyed);
	}
}

FString UJTSOnlineSessionSubsystem::GetProviderStatus() const
{
	if (!SessionInterface.IsValid())
	{
		return LastError.IsEmpty()
			? TEXT("Online session provider unavailable. The front end is still usable.")
			: LastError;
	}

	return bUsingLanFallback
		? TEXT("Local/LAN session provider ready (NULL).")
		: TEXT("Online session provider ready.");
}

void UJTSOnlineSessionSubsystem::Deinitialize()
{
	BeginShutdown();
	SessionSearch.Reset();
	SearchResults.Reset();
	DiscoveredListings.Reset();
	SessionInterface.Reset();
	IdentityInterface.Reset();
	Super::Deinitialize();
}

bool UJTSOnlineSessionSubsystem::IsShuttingDown() const
{
	return bShuttingDown || IsEngineExitRequested();
}

void UJTSOnlineSessionSubsystem::BeginShutdown()
{
	if (bShuttingDown)
	{
		return;
	}

	bShuttingDown = true;
	DestroyCompletionAction = EDestroyCompletionAction::None;
	bReturningToFrontEnd = true;
	ClearOnlineDelegateBindings();
	ClearShutdownDelegateBindings();
}

void UJTSOnlineSessionSubsystem::ClearOnlineDelegateBindings()
{
	if (IdentityInterface.IsValid() && LoginCompleteHandle.IsValid())
	{
		IdentityInterface->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteHandle);
	}
	LoginCompleteHandle.Reset();

	if (SessionInterface.IsValid())
	{
		if (CreateSessionCompleteHandle.IsValid()) SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		if (FindSessionsCompleteHandle.IsValid()) SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		if (JoinSessionCompleteHandle.IsValid()) SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		if (DestroySessionCompleteHandle.IsValid()) SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
	}
	CreateSessionCompleteHandle.Reset();
	FindSessionsCompleteHandle.Reset();
	JoinSessionCompleteHandle.Reset();
	DestroySessionCompleteHandle.Reset();

	if (GEngine != nullptr)
	{
		if (NetworkFailureHandle.IsValid()) GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		if (TravelFailureHandle.IsValid()) GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	NetworkFailureHandle.Reset();
	TravelFailureHandle.Reset();
}

void UJTSOnlineSessionSubsystem::ClearShutdownDelegateBindings()
{
	if (FSlateApplication::IsInitialized() && WindowBeingDestroyedHandle.IsValid())
	{
		FSlateApplication::Get().OnWindowBeingDestroyed().Remove(WindowBeingDestroyedHandle);
	}
	WindowBeingDestroyedHandle.Reset();

	if (PreExitHandle.IsValid())
	{
		FCoreDelegates::OnPreExit.Remove(PreExitHandle);
	}
	PreExitHandle.Reset();
}

void UJTSOnlineSessionSubsystem::ClearDestroySessionDelegate()
{
	if (SessionInterface.IsValid() && DestroySessionCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
	}
	DestroySessionCompleteHandle.Reset();
}

bool UJTSOnlineSessionSubsystem::AcquireInterfaces()
{
	if (SessionInterface.IsValid())
	{
		return true;
	}
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem == nullptr && bAllowLanFallback)
	{
		OnlineSubsystem = IOnlineSubsystem::Get(TEXT("NULL"));
		bUsingLanFallback = OnlineSubsystem != nullptr;
	}
	if (OnlineSubsystem == nullptr)
	{
		return false;
	}
	bUsingLanFallback = OnlineSubsystem->GetSubsystemName() == FName(TEXT("NULL"));
	SessionInterface = OnlineSubsystem->GetSessionInterface();
	IdentityInterface = OnlineSubsystem->GetIdentityInterface();
	return SessionInterface.IsValid();
}

bool UJTSOnlineSessionSubsystem::EnsureAuthenticated()
{
	if (!AcquireInterfaces())
	{
		FinishOperation(false, TEXT("No configured OnlineSubsystem provider is available."));
		return false;
	}
	if (bUsingLanFallback || !IdentityInterface.IsValid() || IdentityInterface->GetLoginStatus(0) == ELoginStatus::LoggedIn)
	{
		return true;
	}
	if (OperationState == EJTSSessionOperationState::Authenticating)
	{
		return false;
	}
	DeferredOperationState = OperationState;
	OperationState = EJTSSessionOperationState::Authenticating;
	LoginCompleteHandle = IdentityInterface->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &UJTSOnlineSessionSubsystem::HandleLoginComplete));
	// Let command-line/dev credentials satisfy AutoLogin first. EOS account portal is the interactive
	// fallback, so the front end never needs to know provider-specific identity details.
	bAttemptingAutoLogin = IdentityInterface->AutoLogin(0);
	if (!bAttemptingAutoLogin)
	{
		const FOnlineAccountCredentials Credentials(TEXT("AccountPortal"), FString(), FString());
		if (!IdentityInterface->Login(0, Credentials))
		{
			IdentityInterface->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteHandle);
			LoginCompleteHandle.Reset();
			FinishOperation(false, TEXT("The OnlineSubsystem rejected authentication."));
		}
	}
	return false;
}

void UJTSOnlineSessionSubsystem::CreateExpedition(const FString& RequestedJoinCode, const FString& Password, int32 MaximumPlayers, EJTSLobbyVisibility LobbyVisibility)
{
	if (IsShuttingDown())
	{
		return;
	}

	bReturningToFrontEnd = false;
	if (OperationState != EJTSSessionOperationState::Idle && OperationState != EJTSSessionOperationState::Failed)
	{
		FinishOperation(false, TEXT("Another online operation is already active."));
		return;
	}
	PendingJoinCode = NormalizeJoinCode(RequestedJoinCode);
	PendingPassword = Password;
	PendingMaximumPlayers = FMath::Clamp(MaximumPlayers, 1, 4);
	PendingLobbyVisibility = LobbyVisibility;
	bLobbyAdmissionOpen = true;
	if (UJTSExpeditionSubsystem* const Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>())
	{
		// New Expedition Setup has already prepared the selected slot and its metadata. Do not
		// overwrite that authoritative choice with a generic snapshot here.
		if (!bCreateFromSavedSnapshot && !Expedition->HasActiveExpedition())
		{
			Expedition->StartNewExpedition(FGuid::NewGuid().ToString(EGuidFormats::Digits));
		}
	}
	OperationState = EJTSSessionOperationState::Creating;
	if (EnsureAuthenticated())
	{
		BeginCreateSession();
	}
}

void UJTSOnlineSessionSubsystem::BeginCreateSession()
{
	if (!SessionInterface.IsValid())
	{
		FinishOperation(false, TEXT("Session interface is unavailable."));
		return;
	}
	if (SessionInterface->GetNamedSession(JTSSessionName) != nullptr)
	{
		FinishOperation(false, TEXT("A local expedition session already exists."));
		return;
	}
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = PendingMaximumPlayers;
	Settings.NumPrivateConnections = 0;
	Settings.bIsLANMatch = bUsingLanFallback;
	Settings.bShouldAdvertise = true;
	// A lobby is joinable until the host starts the expedition. UpdateExpeditionPhase closes it later.
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUseLobbiesVoiceChatIfAvailable = true;
	Settings.Set(JTSJoinCodeKey, PendingJoinCode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(JTSBuildKey, BuildVersion, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(JTSPhaseKey, static_cast<int32>(EJTSGameplayPhase::WaitingToStart), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(JTSAdmissionOpenKey, true, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(JTSPasswordProtectedKey, !PendingPassword.IsEmpty(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(JTSVisibilityKey, static_cast<int32>(PendingLobbyVisibility), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(SETTING_MAPNAME, PreLaunchLobbyMapPath, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	CreateSessionCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UJTSOnlineSessionSubsystem::HandleCreateSessionComplete));
	if (!SessionInterface->CreateSession(0, JTSSessionName, Settings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		FinishOperation(false, TEXT("Could not start session creation."));
	}
}

void UJTSOnlineSessionSubsystem::FindExpeditions(const FString& JoinCode)
{
	if (IsShuttingDown())
	{
		return;
	}

	bReturningToFrontEnd = false;
	if (OperationState != EJTSSessionOperationState::Idle && OperationState != EJTSSessionOperationState::Failed)
	{
		FinishOperation(false, TEXT("Another online operation is already active."));
		return;
	}
	PendingJoinCode = NormalizeJoinCode(JoinCode);
	OperationState = EJTSSessionOperationState::Searching;
	if (EnsureAuthenticated())
	{
		BeginFindSessions();
	}
}

void UJTSOnlineSessionSubsystem::BeginFindSessions()
{
	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = 100;
	SessionSearch->bIsLanQuery = bUsingLanFallback;
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SessionSearch->QuerySettings.Set(JTSBuildKey, BuildVersion, EOnlineComparisonOp::Equals);
	SessionSearch->QuerySettings.Set(JTSPhaseKey, static_cast<int32>(EJTSGameplayPhase::WaitingToStart), EOnlineComparisonOp::Equals);
	if (!PendingJoinCode.IsEmpty())
	{
		SessionSearch->QuerySettings.Set(JTSJoinCodeKey, PendingJoinCode, EOnlineComparisonOp::Equals);
	}
	FindSessionsCompleteHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UJTSOnlineSessionSubsystem::HandleFindSessionsComplete));
	if (!SessionInterface->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		FinishOperation(false, TEXT("Could not begin expedition search."));
	}
}

void UJTSOnlineSessionSubsystem::JoinExpedition(int32 ListingIndex, const FString& Password)
{
	if (IsShuttingDown())
	{
		return;
	}

	bReturningToFrontEnd = false;
	if (!SearchResults.IsValidIndex(ListingIndex) || !DiscoveredListings.IsValidIndex(ListingIndex) || !SessionInterface.IsValid())
	{
		FinishOperation(false, TEXT("The selected expedition is no longer available."));
		return;
	}
	if (!PendingJoinCode.IsEmpty() && DiscoveredListings.Num() > 1)
	{
		// A join code is a human-facing rendezvous key, not an authority token.  Never
		// choose an arbitrary result if a backend advertises a collision.
		FinishOperation(false, TEXT("Multiple expeditions use this Join Code. Ask the host to create a new expedition."));
		return;
	}
	const FJTSSessionListing& Listing = DiscoveredListings[ListingIndex];
	if (Listing.GameplayPhase != EJTSGameplayPhase::WaitingToStart
		|| !Listing.bAcceptingNewPlayers
		|| Listing.CurrentPlayers >= Listing.MaximumPlayers)
	{
		FinishOperation(false, TEXT("That expedition is no longer accepting players."));
		return;
	}
	PendingJoinIndex = ListingIndex;
	PendingPassword = Password;
	OperationState = EJTSSessionOperationState::Joining;
	JoinSessionCompleteHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UJTSOnlineSessionSubsystem::HandleJoinSessionComplete));
	if (!SessionInterface->JoinSession(0, JTSSessionName, SearchResults[ListingIndex]))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		FinishOperation(false, TEXT("Could not begin joining that expedition."));
	}
}

void UJTSOnlineSessionSubsystem::LeaveExpedition()
{
	if (IsShuttingDown())
	{
		return;
	}

	if (DestroySessionCompleteHandle.IsValid())
	{
		DestroyCompletionAction = EDestroyCompletionAction::ReturnToFrontEnd;
		return;
	}

	if (!SessionInterface.IsValid() || SessionInterface->GetNamedSession(JTSSessionName) == nullptr)
	{
		ResetSessionRuntimeState();
		if (UJTSVoiceSubsystem* const Voice = GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>())
		{
			Voice->ShutdownVoice();
		}
		if (UJTSExpeditionSubsystem* const Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>())
		{
			Expedition->ResetClientTransientState();
		}
		TravelToFrontEnd();
		FinishOperation(true, TEXT("Returned to the front end."));
		return;
	}

	BeginDestroySession(EDestroyCompletionAction::ReturnToFrontEnd);
}

void UJTSOnlineSessionSubsystem::RequestApplicationQuit()
{
	if (IsShuttingDown())
	{
		return;
	}

	if (DestroySessionCompleteHandle.IsValid())
	{
		DestroyCompletionAction = EDestroyCompletionAction::QuitApplication;
		return;
	}

	if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(JTSSessionName) != nullptr)
	{
		BeginDestroySession(EDestroyCompletionAction::QuitApplication);
		return;
	}

	CompleteApplicationQuit();
}

void UJTSOnlineSessionSubsystem::BeginDestroySession(EDestroyCompletionAction CompletionAction)
{
	if (IsShuttingDown() || !SessionInterface.IsValid())
	{
		return;
	}

	DestroyCompletionAction = CompletionAction;
	OperationState = EJTSSessionOperationState::Leaving;
	DestroySessionCompleteHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UJTSOnlineSessionSubsystem::HandleDestroySessionComplete));
	if (!SessionInterface->DestroySession(JTSSessionName))
	{
		ClearDestroySessionDelegate();
		const EDestroyCompletionAction FailedAction = DestroyCompletionAction;
		DestroyCompletionAction = EDestroyCompletionAction::None;
		if (FailedAction == EDestroyCompletionAction::QuitApplication)
		{
			CompleteApplicationQuit();
		}
		else
		{
			FinishOperation(false, TEXT("Could not leave the active expedition."));
		}
	}
}

void UJTSOnlineSessionSubsystem::HandleHostLeftExpedition()
{
	if (IsShuttingDown())
	{
		return;
	}

	// A listen host cannot migrate authority. Tear down this client's local session record if possible,
	// then show a deterministic, player-facing reason in the front end.
	if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(JTSSessionName) != nullptr)
	{
		SessionInterface->DestroySession(JTSSessionName);
	}
	ReturnToFrontEndAfterFailure(TEXT("The host left the expedition."));
}

void UJTSOnlineSessionSubsystem::ContinueExpedition(int32 SaveSlot)
{
	if (IsShuttingDown())
	{
		return;
	}

	if (OperationState != EJTSSessionOperationState::Idle && OperationState != EJTSSessionOperationState::Failed)
	{
		FinishOperation(false, TEXT("Another online operation is already active."));
		return;
	}

	UJTSExpeditionSubsystem* const Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>();
	const bool bLoaded = Expedition != nullptr
		&& (SaveSlot >= 1 && SaveSlot <= UJTSExpeditionSubsystem::MaximumSaveSlots
			? Expedition->LoadExpeditionSlot(SaveSlot)
			: Expedition->LoadMostRecentSnapshot());
	if (!bLoaded)
	{
		FinishOperation(false, TEXT("No host expedition save is available to continue."));
		return;
	}

	Expedition->MarkResumeRequested();
	bCreateFromSavedSnapshot = true;
	CreateExpedition(FString(), FString(), 4, EJTSLobbyVisibility::Public);
}

FString UJTSOnlineSessionSubsystem::GenerateSuggestedJoinCode() const
{
	return NormalizeJoinCode(FString());
}

void UJTSOnlineSessionSubsystem::UpdateExpeditionPhase(EJTSGameplayPhase NewPhase)
{
	if (!SessionInterface.IsValid())
	{
		return;
	}

	FNamedOnlineSession* const Session = SessionInterface->GetNamedSession(JTSSessionName);
	if (Session == nullptr)
	{
		return;
	}

	if (NewPhase != EJTSGameplayPhase::WaitingToStart)
	{
		bLobbyAdmissionOpen = false;
	}

	FOnlineSessionSettings UpdatedSettings = Session->SessionSettings;
	UpdatedSettings.Set(JTSPhaseKey, static_cast<int32>(NewPhase), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	UpdatedSettings.Set(JTSAdmissionOpenKey, bLobbyAdmissionOpen, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	UpdatedSettings.bAllowJoinInProgress = NewPhase == EJTSGameplayPhase::WaitingToStart && bLobbyAdmissionOpen;
	SessionInterface->UpdateSession(JTSSessionName, UpdatedSettings, true);
	if (NewPhase != EJTSGameplayPhase::WaitingToStart)
	{
		SessionInterface->StartSession(JTSSessionName);
	}
}

void UJTSOnlineSessionSubsystem::UpdateLobbyAdmission(bool bAllowNewPlayers)
{
	bLobbyAdmissionOpen = bAllowNewPlayers;
	if (!SessionInterface.IsValid())
	{
		return;
	}

	FNamedOnlineSession* const Session = SessionInterface->GetNamedSession(JTSSessionName);
	if (Session == nullptr)
	{
		return;
	}

	int32 PhaseValue = static_cast<int32>(EJTSGameplayPhase::WaitingToStart);
	Session->SessionSettings.Get(JTSPhaseKey, PhaseValue);
	const bool bWaitingForPlayers = static_cast<EJTSGameplayPhase>(PhaseValue) == EJTSGameplayPhase::WaitingToStart;
	FOnlineSessionSettings UpdatedSettings = Session->SessionSettings;
	UpdatedSettings.Set(JTSAdmissionOpenKey, bLobbyAdmissionOpen, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	UpdatedSettings.bAllowJoinInProgress = bWaitingForPlayers && bLobbyAdmissionOpen;
	SessionInterface->UpdateSession(JTSSessionName, UpdatedSettings, true);
}

void UJTSOnlineSessionSubsystem::HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	if (IsShuttingDown())
	{
		return;
	}

	if (!bWasSuccessful)
	{
		if (bAttemptingAutoLogin && IdentityInterface.IsValid())
		{
			bAttemptingAutoLogin = false;
			const FOnlineAccountCredentials Credentials(TEXT("AccountPortal"), FString(), FString());
			if (IdentityInterface->Login(LocalUserNum, Credentials))
			{
				return;
			}
		}
		if (IdentityInterface.IsValid())
		{
			IdentityInterface->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, LoginCompleteHandle);
		}
		LoginCompleteHandle.Reset();
		FinishOperation(false, Error.IsEmpty() ? TEXT("Online authentication failed.") : Error);
		return;
	}
	if (IdentityInterface.IsValid())
	{
		IdentityInterface->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, LoginCompleteHandle);
	}
	LoginCompleteHandle.Reset();
	bAttemptingAutoLogin = false;
	const EJTSSessionOperationState CompletedOperation = DeferredOperationState;
	DeferredOperationState = EJTSSessionOperationState::Idle;
	OperationState = CompletedOperation;
	if (CompletedOperation == EJTSSessionOperationState::Creating)
	{
		BeginCreateSession();
	}
	else if (CompletedOperation == EJTSSessionOperationState::Searching)
	{
		BeginFindSessions();
	}
	else
	{
		FinishOperation(false, TEXT("Authentication completed without a pending session operation."));
	}
}

void UJTSOnlineSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid() && CreateSessionCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
	}
	CreateSessionCompleteHandle.Reset();
	if (IsShuttingDown())
	{
		return;
	}

	if (!bWasSuccessful)
	{
		FinishOperation(false, TEXT("OnlineSubsystem could not create the expedition session."));
		return;
	}
	CurrentJoinCode = PendingJoinCode;
	CurrentMaximumPlayers = PendingMaximumPlayers;
	bCurrentSessionPasswordProtected = !PendingPassword.IsEmpty();
	CurrentLobbyVisibility = PendingLobbyVisibility;
	if (UJTSExpeditionSubsystem* const Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>())
	{
		Expedition->SetJoinCredentials(CurrentJoinCode, PendingPassword);
	}
	if (UJTSVoiceSubsystem* const Voice = GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>())
	{
		Voice->AttachSessionVoiceTransport(CurrentJoinCode);
	}
	bCreateFromSavedSnapshot = false;
	TravelHostToPreLaunchLobby();
}

void UJTSOnlineSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface.IsValid() && FindSessionsCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
	}
	FindSessionsCompleteHandle.Reset();
	if (IsShuttingDown())
	{
		return;
	}

	const TArray<FOnlineSessionSearchResult> RawSearchResults = bWasSuccessful && SessionSearch.IsValid()
		? SessionSearch->SearchResults
		: TArray<FOnlineSessionSearchResult>();
	SearchResults.Reset();
	DiscoveredListings.Reset();
	for (const FOnlineSessionSearchResult& Result : RawSearchResults)
	{
		FJTSSessionListing Listing;
		Result.Session.SessionSettings.Get(JTSJoinCodeKey, Listing.JoinCode);
		Result.Session.SessionSettings.Get(SETTING_MAPNAME, Listing.MapName);
		Result.Session.SessionSettings.Get(JTSPasswordProtectedKey, Listing.bPasswordProtected);
		int32 VisibilityValue = static_cast<int32>(EJTSLobbyVisibility::Public);
		Result.Session.SessionSettings.Get(JTSVisibilityKey, VisibilityValue);
		Listing.Visibility = static_cast<EJTSLobbyVisibility>(VisibilityValue);
		int32 PhaseValue = static_cast<int32>(EJTSGameplayPhase::WaitingToStart);
		Result.Session.SessionSettings.Get(JTSPhaseKey, PhaseValue);
		Listing.GameplayPhase = static_cast<EJTSGameplayPhase>(PhaseValue);
		Result.Session.SessionSettings.Get(JTSAdmissionOpenKey, Listing.bAcceptingNewPlayers);
		Listing.HostName = Result.Session.OwningUserName;
		Listing.CurrentPlayers = FMath::Max(0, Result.Session.SessionSettings.NumPublicConnections - Result.Session.NumOpenPublicConnections);
		Listing.MaximumPlayers = Result.Session.SessionSettings.NumPublicConnections;
		if (Listing.Visibility == EJTSLobbyVisibility::Private && PendingJoinCode.IsEmpty())
		{
			continue;
		}
		SearchResults.Add(Result);
		DiscoveredListings.Add(MoveTemp(Listing));
	}
	OnSessionListingsChanged.Broadcast();
	FinishOperation(bWasSuccessful, bWasSuccessful ? TEXT("Expedition search complete.") : TEXT("Expedition search failed."));
}

void UJTSOnlineSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface.IsValid() && JoinSessionCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
	}
	JoinSessionCompleteHandle.Reset();
	if (IsShuttingDown())
	{
		return;
	}

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		FinishOperation(false, TEXT("Could not join the selected expedition."));
		return;
	}
	FString ConnectString;
	if (!SessionInterface->GetResolvedConnectString(JTSSessionName, ConnectString))
	{
		FinishOperation(false, TEXT("The expedition did not provide a valid connection address."));
		return;
	}
	const FString JoinCode = DiscoveredListings.IsValidIndex(PendingJoinIndex) ? DiscoveredListings[PendingJoinIndex].JoinCode : PendingJoinCode;
	PendingJoinCode = JoinCode;
	ConnectString += MakeTravelOptions(PendingPassword);
	if (APlayerController* const Controller = GetLocalPlayerController(GetWorld()))
	{
		Controller->ClientTravel(ConnectString, TRAVEL_Absolute);
		CurrentJoinCode = JoinCode;
		CurrentMaximumPlayers = DiscoveredListings.IsValidIndex(PendingJoinIndex) ? DiscoveredListings[PendingJoinIndex].MaximumPlayers : 4;
		bCurrentSessionPasswordProtected = DiscoveredListings.IsValidIndex(PendingJoinIndex) && DiscoveredListings[PendingJoinIndex].bPasswordProtected;
		CurrentLobbyVisibility = DiscoveredListings.IsValidIndex(PendingJoinIndex)
			? DiscoveredListings[PendingJoinIndex].Visibility
			: EJTSLobbyVisibility::Public;
		if (UJTSVoiceSubsystem* const Voice = GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>())
		{
			Voice->AttachSessionVoiceTransport(CurrentJoinCode);
		}
		FinishOperation(true, TEXT("Joining expedition."));
		return;
	}
	FinishOperation(false, TEXT("No local player controller is available for travel."));
}

void UJTSOnlineSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	ClearDestroySessionDelegate();
	const EDestroyCompletionAction CompletionAction = DestroyCompletionAction;
	DestroyCompletionAction = EDestroyCompletionAction::None;
	if (IsShuttingDown())
	{
		return;
	}

	ResetSessionRuntimeState();
	if (CompletionAction == EDestroyCompletionAction::QuitApplication)
	{
		CompleteApplicationQuit();
		return;
	}

	if (UJTSVoiceSubsystem* const Voice = GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>())
	{
		Voice->ShutdownVoice();
	}
	if (UJTSExpeditionSubsystem* const Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>())
	{
		Expedition->ResetClientTransientState();
	}
	if (CompletionAction == EDestroyCompletionAction::ReturnToFrontEnd)
	{
		TravelToFrontEnd();
		FinishOperation(bWasSuccessful, bWasSuccessful ? TEXT("Left expedition.") : TEXT("The session closed while leaving."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Jump to Space ignored an unrequested session-destroy completion; no travel will be started."));
}

void UJTSOnlineSessionSubsystem::ResetSessionRuntimeState()
{
	CurrentJoinCode.Reset();
	bCurrentSessionPasswordProtected = false;
	CurrentLobbyVisibility = EJTSLobbyVisibility::Public;
	bCreateFromSavedSnapshot = false;
}

bool UJTSOnlineSessionSubsystem::CanTravelToFrontEnd() const
{
	const UWorld* const World = GetWorld();
	return !IsShuttingDown()
		&& IsValid(World)
		&& !World->bIsTearingDown
		&& World->GetGameInstance() == GetGameInstance();
}

void UJTSOnlineSessionSubsystem::TravelToFrontEnd()
{
	if (!CanTravelToFrontEnd())
	{
		return;
	}

	if (APlayerController* const Controller = GetLocalPlayerController(GetWorld()))
	{
		Controller->ClientTravel(FrontEndMapPath.IsEmpty() ? JTSMapPaths::FrontEnd : FrontEndMapPath, TRAVEL_Absolute);
	}
}

void UJTSOnlineSessionSubsystem::CompleteApplicationQuit()
{
	if (IsShuttingDown())
	{
		return;
	}

	BeginShutdown();
	FPlatformMisc::RequestExit(false, TEXT("UJTSOnlineSessionSubsystem::CompleteApplicationQuit"));
}

void UJTSOnlineSessionSubsystem::FinishOperation(bool bSucceeded, const FString& Message)
{
	DeferredOperationState = EJTSSessionOperationState::Idle;
	OperationState = bSucceeded ? EJTSSessionOperationState::Idle : EJTSSessionOperationState::Failed;
	if (bSucceeded)
	{
		LastError.Reset();
	}
	else
	{
		LastError = Message;
		bCreateFromSavedSnapshot = false;
	}
	if (!IsShuttingDown())
	{
		OnSessionOperationFinished.Broadcast(bSucceeded, Message);
	}
}

void UJTSOnlineSessionSubsystem::TravelHostToPreLaunchLobby()
{
	if (IsShuttingDown() || GetWorld() == nullptr)
	{
		return;
	}

	const FString Options = FString::Printf(TEXT("listen%s"), *MakeTravelOptions(PendingPassword));
	const FString& Destination = PreLaunchLobbyMapPath.IsEmpty() ? FString(TEXT("/Game/Space/Maps/L_PreLaunchLobby")) : PreLaunchLobbyMapPath;
	UGameplayStatics::OpenLevel(GetWorld(), FName(*Destination), true, Options);
	FinishOperation(true, TEXT("Expedition created; hosting the pre-launch lobby."));
}

FString UJTSOnlineSessionSubsystem::MakeTravelOptions(const FString& Password) const
{
	const FString PasswordHash = Password.IsEmpty() ? FString() : FMD5::HashAnsiString(*Password);
	return FString::Printf(TEXT("?JTSBuild=%s?JTSJoinCode=%s?JTSPassword=%s"), *BuildVersion, *PendingJoinCode, *PasswordHash);
}

FString UJTSOnlineSessionSubsystem::NormalizeJoinCode(const FString& Value)
{
	FString Result = Value.ToUpper();
	Result.ReplaceInline(TEXT(" "), TEXT(""));
	Result.ReplaceInline(TEXT("-"), TEXT(""));
	if (Result.IsEmpty())
	{
		for (int32 CharacterIndex = 0; CharacterIndex < 6; ++CharacterIndex)
		{
			Result.AppendChar(JoinCodeAlphabet[FMath::RandRange(0, FCString::Strlen(JoinCodeAlphabet) - 1)]);
		}
	}
	return Result.Left(16);
}

void UJTSOnlineSessionSubsystem::HandleNetworkFailure(
	UWorld* World,
	UNetDriver* NetDriver,
	ENetworkFailure::Type FailureType,
	const FString& Error)
{
	if (IsShuttingDown())
	{
		return;
	}

	(void)NetDriver;
	(void)FailureType;
	if (World == nullptr || World->GetGameInstance() != GetGameInstance() || World->GetNetMode() != NM_Client)
	{
		return;
	}
	ReturnToFrontEndAfterFailure(Error.IsEmpty() ? TEXT("Connection to the expedition was lost.") : Error);
}

void UJTSOnlineSessionSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	if (IsShuttingDown())
	{
		return;
	}

	(void)FailureType;
	if (World == nullptr || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	ReturnToFrontEndAfterFailure(Error.IsEmpty() ? TEXT("Travel to the expedition failed.") : Error);
}

void UJTSOnlineSessionSubsystem::ReturnToFrontEndAfterFailure(const FString& Error)
{
	if (IsShuttingDown() || bReturningToFrontEnd)
	{
		return;
	}
	bReturningToFrontEnd = true;
	LastError = Error;
	OperationState = EJTSSessionOperationState::Failed;
	CurrentJoinCode.Reset();
	bCurrentSessionPasswordProtected = false;
	CurrentLobbyVisibility = EJTSLobbyVisibility::Public;
	if (UJTSVoiceSubsystem* const Voice = GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>())
	{
		Voice->ShutdownVoice();
	}
	if (UJTSExpeditionSubsystem* const Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>())
	{
		Expedition->ResetClientTransientState();
	}
	TravelToFrontEnd();
	if (!IsShuttingDown())
	{
		OnSessionOperationFinished.Broadcast(false, Error);
	}
}

void UJTSOnlineSessionSubsystem::HandleWindowBeingDestroyed(const SWindow& Window)
{
	if (GEngine == nullptr || GEngine->GameViewport == nullptr)
	{
		return;
	}

	const TSharedPtr<SWindow> GameWindow = GEngine->GameViewport->GetWindow();
	if (GameWindow.IsValid() && GameWindow.Get() == &Window)
	{
		// Slate delivers this before the game viewport is detached and before the next engine tick
		// notices that all windows are closed. Suppress any host/session callback that would try to
		// begin map travel in that gap.
		BeginShutdown();
	}
}
