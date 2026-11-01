// Copyright Vedant Vats. Based on the MultiplayerSessions plugin from Stephen Ulibarri's UE multiplayer course.
// Ported to UE 5.6.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"

#include "MultiplayerSessionsSubsystem.generated.h"

// Delegates the menu (or any UI) binds to.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnCreateSessionComplete, bool, bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& /*SessionResults*/, bool /*bWasSuccessful*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnJoinSessionComplete, EOnJoinSessionCompleteResult::Type /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnDestroySessionComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnStartSessionComplete, bool, bWasSuccessful);
// Fired when HostGame / FindAndJoinGame fails, with a reason the UI can show.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnSessionFlowFailed, const FString&, Reason);

/**
 * Wraps the Online Session Interface (Steam or NULL/LAN) behind simple calls + delegates.
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMultiplayerSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UMultiplayerSessionsSubsystem();

	// USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Session calls. Results arrive on the delegates below.
	void CreateSession(int32 NumPublicConnections, const FString& MatchType);
	void FindSessions(int32 MaxSearchResults);
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);
	void DestroySession();
	void StartSession();

	// ---- Blueprint helpers (one call does the whole flow, including travel) ----

	/** Create a session and ServerTravel to LobbyMapPath as a listen server. */
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	void HostGame(int32 NumPublicConnections = 4, const FString& MatchType = TEXT("FreeForAll"), const FString& LobbyMapPath = TEXT("/Game/Lobby"));

	/** Find a session with this MatchType, join it and ClientTravel to it. */
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	void FindAndJoinGame(const FString& MatchType = TEXT("FreeForAll"));

	/** Connect straight to an IP (e.g. "127.0.0.1" or "192.168.1.5:7777"). Skips session search. */
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	void JoinByAddress(const FString& Address);

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions")
	FMultiplayerOnSessionFlowFailed OnSessionFlowFailed;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions")
	FMultiplayerOnCreateSessionComplete MultiplayerOnCreateSessionComplete;
	FMultiplayerOnFindSessionsComplete MultiplayerOnFindSessionsComplete;
	FMultiplayerOnJoinSessionComplete MultiplayerOnJoinSessionComplete;
	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions")
	FMultiplayerOnDestroySessionComplete MultiplayerOnDestroySessionComplete;
	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Sessions")
	FMultiplayerOnStartSessionComplete MultiplayerOnStartSessionComplete;

	int32 DesiredNumPublicConnections = 0;
	FString DesiredMatchType;

protected:
	// Callbacks for the Online Session Interface.
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);

private:
	/** True when the active online subsystem is NULL (LAN / PIE testing). */
	bool IsLAN() const;

	/** Net id of the first local player, or null if there is none yet. */
	FUniqueNetIdPtr GetLocalNetId() const;

	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;

	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate;
	FDelegateHandle DestroySessionCompleteDelegateHandle;
	FOnStartSessionCompleteDelegate StartSessionCompleteDelegate;
	FDelegateHandle StartSessionCompleteDelegateHandle;

	// Used when CreateSession is called while a session already exists:
	// destroy first, then create with these values.
	// State for the HostGame / FindAndJoinGame helpers.
	bool bHostFlowActive = false;
	bool bJoinFlowActive = false;
	FString PendingLobbyPath;
	FString PendingMatchType;
	void FailFlow(const FString& Reason);

	bool bCreateSessionOnDestroy = false;
	int32 LastNumPublicConnections = 0;
	FString LastMatchType;
};
