// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Net/Core/Connection/NetEnums.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "space/Core/JTSExpeditionTypes.h"
#include "space/Core/JTSMapPaths.h"

#include "JTSOnlineSessionSubsystem.generated.h"

class UNetDriver;
class UWorld;
class SWindow;

UENUM(BlueprintType)
enum class EJTSSessionOperationState : uint8
{
	Idle,
	Authenticating,
	Creating,
	Searching,
	Joining,
	Leaving,
	Failed
};

USTRUCT(BlueprintType)
struct SPACE_API FJTSSessionListing
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString JoinCode;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString HostName;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 MaximumPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString MapName;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	bool bPasswordProtected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	EJTSLobbyVisibility Visibility = EJTSLobbyVisibility::Public;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	EJTSGameplayPhase GameplayPhase = EJTSGameplayPhase::WaitingToStart;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	bool bAcceptingNewPlayers = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnJTSSessionOperationFinished, bool, bSucceeded, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJTSSessionListingsChanged);

/**
 * Thin wrapper around UE OnlineSubsystem sessions. It contains no UI and makes no direct EOS SDK calls.
 * If the configured provider is unavailable, it reports that capability gap instead of emulating a service.
 */
UCLASS(Config = Game)
class SPACE_API UJTSOnlineSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Online")
	void CreateExpedition(const FString& RequestedJoinCode, const FString& Password, int32 MaximumPlayers, EJTSLobbyVisibility LobbyVisibility = EJTSLobbyVisibility::Public);

	UFUNCTION(BlueprintCallable, Category = "Online")
	void FindExpeditions(const FString& JoinCode);

	UFUNCTION(BlueprintCallable, Category = "Online")
	void JoinExpedition(int32 ListingIndex, const FString& Password);

	UFUNCTION(BlueprintCallable, Category = "Online")
	void LeaveExpedition();

	/** Destroys an active session first, then requests a clean application exit from its completion callback. */
	UFUNCTION(BlueprintCallable, Category = "Online")
	void RequestApplicationQuit();

	/** Stops all project callbacks that could otherwise start travel while the application is closing. */
	void BeginShutdown();

	bool IsShuttingDown() const;

	/** Called by the server before it terminates a listen-hosted expedition. */
	void HandleHostLeftExpedition();

	/** Loads the host-owned snapshot, creates a fresh session/join code, then returns to the normal lobby flow. */
	UFUNCTION(BlueprintCallable, Category = "Online")
	void ContinueExpedition(int32 SaveSlot = -1);

	/** Generates a human-readable rendezvous key. Hosts normally see this as an automatic preview. */
	UFUNCTION(BlueprintCallable, Category = "Online")
	FString GenerateSuggestedJoinCode() const;

	/** Advertises the current phase and closes normal joining once the expedition begins. */
	void UpdateExpeditionPhase(EJTSGameplayPhase NewPhase);

	/** Updates lobby admission without changing the replicated gameplay phase. */
	void UpdateLobbyAdmission(bool bAllowNewPlayers);

	UFUNCTION(BlueprintPure, Category = "Online")
	EJTSSessionOperationState GetOperationState() const { return OperationState; }

	UFUNCTION(BlueprintPure, Category = "Online")
	FString GetCurrentJoinCode() const { return CurrentJoinCode; }

	UFUNCTION(BlueprintPure, Category = "Online")
	int32 GetCurrentMaximumPlayers() const { return CurrentMaximumPlayers; }

	UFUNCTION(BlueprintPure, Category = "Online")
	bool IsCurrentSessionPasswordProtected() const { return bCurrentSessionPasswordProtected; }

	UFUNCTION(BlueprintPure, Category = "Online")
	EJTSLobbyVisibility GetCurrentLobbyVisibility() const { return CurrentLobbyVisibility; }

	UFUNCTION(BlueprintPure, Category = "Online")
	TArray<FJTSSessionListing> GetDiscoveredListings() const { return DiscoveredListings; }

	UFUNCTION(BlueprintPure, Category = "Online")
	bool IsUsingLanFallback() const { return bUsingLanFallback; }

	UFUNCTION(BlueprintPure, Category = "Online")
	FString GetLastError() const { return LastError; }

	/** A local, UI-safe summary. It never requires a successful provider login. */
	UFUNCTION(BlueprintPure, Category = "Online")
	FString GetProviderStatus() const;

	UPROPERTY(BlueprintAssignable, Category = "Online")
	FOnJTSSessionOperationFinished OnSessionOperationFinished;

	UPROPERTY(BlueprintAssignable, Category = "Online")
	FOnJTSSessionListingsChanged OnSessionListingsChanged;

	/** First map a newly created/listen-hosted session opens. It is staging only, never Earth gameplay. */
	UPROPERTY(Config, EditAnywhere, Category = "Online")
	FString PreLaunchLobbyMapPath = TEXT("/Game/Space/Maps/L_PreLaunchLobby");

	/** Retained for existing config migration; gameplay travel is now owned by AJTSPreLaunchLobbyGameMode. */
	UPROPERTY(Config, EditAnywhere, Category = "Online")
	FString EarthMapPath = JTSMapPaths::Earth;

	UPROPERTY(Config, EditAnywhere, Category = "Online")
	FString FrontEndMapPath = JTSMapPaths::FrontEnd;

	UPROPERTY(Config, EditAnywhere, Category = "Online")
	FString BuildVersion = TEXT("JTS-1");

	UPROPERTY(Config, EditAnywhere, Category = "Online")
	bool bAllowLanFallback = true;

private:
	enum class EDestroyCompletionAction : uint8
	{
		None,
		ReturnToFrontEnd,
		QuitApplication
	};

	bool AcquireInterfaces();
	bool EnsureAuthenticated();
	void BeginCreateSession();
	void BeginFindSessions();
	void BeginDestroySession(EDestroyCompletionAction CompletionAction);
	void FinishOperation(bool bSucceeded, const FString& Message);
	void TravelHostToPreLaunchLobby();
	void TravelToFrontEnd();
	void CompleteApplicationQuit();
	void ClearOnlineDelegateBindings();
	void ClearShutdownDelegateBindings();
	void ClearDestroySessionDelegate();
	void ResetSessionRuntimeState();
	bool CanTravelToFrontEnd() const;
	FString MakeTravelOptions(const FString& Password) const;
	static FString NormalizeJoinCode(const FString& Value);

	void HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleNetworkFailure(UWorld* World, class UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);
	void ReturnToFrontEndAfterFailure(const FString& Error);
	void HandleWindowBeingDestroyed(const SWindow& Window);

	IOnlineSessionPtr SessionInterface;
	IOnlineIdentityPtr IdentityInterface;
	TSharedPtr<class FOnlineSessionSearch> SessionSearch;
	TArray<FOnlineSessionSearchResult> SearchResults;
	TArray<FJTSSessionListing> DiscoveredListings;
	FDelegateHandle LoginCompleteHandle;
	FDelegateHandle CreateSessionCompleteHandle;
	FDelegateHandle FindSessionsCompleteHandle;
	FDelegateHandle JoinSessionCompleteHandle;
	FDelegateHandle DestroySessionCompleteHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle WindowBeingDestroyedHandle;
	FDelegateHandle PreExitHandle;

	EJTSSessionOperationState OperationState = EJTSSessionOperationState::Idle;
	/** Operation which was paused while an asynchronous platform login completes. */
	EJTSSessionOperationState DeferredOperationState = EJTSSessionOperationState::Idle;
	FString PendingJoinCode;
	FString PendingPassword;
	int32 PendingMaximumPlayers = 4;
	EJTSLobbyVisibility PendingLobbyVisibility = EJTSLobbyVisibility::Public;
	int32 PendingJoinIndex = INDEX_NONE;
	FString CurrentJoinCode;
	int32 CurrentMaximumPlayers = 4;
	bool bCurrentSessionPasswordProtected = false;
	EJTSLobbyVisibility CurrentLobbyVisibility = EJTSLobbyVisibility::Public;
	FString LastError;
	bool bUsingLanFallback = false;
	bool bCreateFromSavedSnapshot = false;
	bool bLobbyAdmissionOpen = true;
	bool bAttemptingAutoLogin = false;
	bool bShuttingDown = false;
	/** Avoid a cascading failure notification while the local client is already travelling back to Entry. */
	bool bReturningToFrontEnd = false;
	EDestroyCompletionAction DestroyCompletionAction = EDestroyCompletionAction::None;
};
