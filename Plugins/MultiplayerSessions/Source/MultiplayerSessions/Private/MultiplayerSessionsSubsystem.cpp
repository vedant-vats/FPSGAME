// Copyright Vedant Vats. Based on the MultiplayerSessions plugin from Stephen Ulibarri's UE multiplayer course.
// Ported to UE 5.6.

#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h" // SEARCH_LOBBIES lives here in UE 5.x
#include "Engine/LocalPlayer.h"

#ifndef SEARCH_LOBBIES
#define SEARCH_LOBBIES FName(TEXT("LOBBYSEARCH"))
#endif
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogMultiplayerSessions, Log, All);

UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem()
	: CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete))
	, FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete))
	, JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete))
	, DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete))
	, StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete))
{
}

void UMultiplayerSessionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Grab the session interface here, not in the constructor (the constructor also runs for the CDO).
	if (const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld()))
	{
		SessionInterface = Subsystem->GetSessionInterface();
		UE_LOG(LogMultiplayerSessions, Log, TEXT("Using online subsystem: %s"), *Subsystem->GetSubsystemName().ToString());
	}
	else
	{
		UE_LOG(LogMultiplayerSessions, Warning, TEXT("No online subsystem found."));
	}
}

void UMultiplayerSessionsSubsystem::Deinitialize()
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	}
	SessionInterface.Reset();
	Super::Deinitialize();
}

bool UMultiplayerSessionsSubsystem::IsLAN() const
{
	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	return !Subsystem || Subsystem->GetSubsystemName() == NULL_SUBSYSTEM;
}

FUniqueNetIdPtr UMultiplayerSessionsSubsystem::GetLocalNetId() const
{
	const UWorld* World = GetWorld();
	const ULocalPlayer* LocalPlayer = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	if (!LocalPlayer)
	{
		return nullptr;
	}
	return LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId();
}

void UMultiplayerSessionsSubsystem::CreateSession(int32 NumPublicConnections, const FString& MatchType)
{
	DesiredNumPublicConnections = NumPublicConnections;
	DesiredMatchType = MatchType;

	if (!SessionInterface.IsValid())
	{
		OnCreateSessionComplete(NAME_GameSession, false);
		return;
	}

	// A session already exists: destroy it, then create again from OnDestroySessionComplete.
	// (The 5.0 version kept going after DestroySession(), so the create usually failed.)
	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		bCreateSessionOnDestroy = true;
		LastNumPublicConnections = NumPublicConnections;
		LastMatchType = MatchType;
		DestroySession();
		return;
	}

	const FUniqueNetIdPtr NetId = GetLocalNetId();
	if (!NetId.IsValid())
	{
		UE_LOG(LogMultiplayerSessions, Warning, TEXT("CreateSession: no local player net id."));
		OnCreateSessionComplete(NAME_GameSession, false);
		return;
	}

	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	LastSessionSettings = MakeShared<FOnlineSessionSettings>();
	LastSessionSettings->bIsLANMatch = IsLAN();
	LastSessionSettings->NumPublicConnections = NumPublicConnections;
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowJoinViaPresence = true;
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->bUsesPresence = true;
	LastSessionSettings->bUseLobbiesIfAvailable = true;
	LastSessionSettings->BuildUniqueId = 1;
	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	if (!SessionInterface->CreateSession(*NetId, NAME_GameSession, *LastSessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		OnCreateSessionComplete(NAME_GameSession, false);
	}
}

void UMultiplayerSessionsSubsystem::FindSessions(int32 MaxSearchResults)
{
	const FUniqueNetIdPtr NetId = GetLocalNetId();
	if (!SessionInterface.IsValid() || !NetId.IsValid())
	{
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		if (bJoinFlowActive) { bJoinFlowActive = false; FailFlow(TEXT("Could not search for or join games.")); }
		return;
	}

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	LastSessionSearch = MakeShared<FOnlineSessionSearch>();
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	LastSessionSearch->bIsLanQuery = IsLAN();
	// SEARCH_LOBBIES replaces the older SEARCH_PRESENCE key for lobby searches.
	LastSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	if (!SessionInterface->FindSessions(*NetId, LastSessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		if (bJoinFlowActive) { bJoinFlowActive = false; FailFlow(TEXT("Could not search for or join games.")); }
	}
}

void UMultiplayerSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	const FUniqueNetIdPtr NetId = GetLocalNetId();
	if (!SessionInterface.IsValid() || !NetId.IsValid())
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		if (bJoinFlowActive) { bJoinFlowActive = false; FailFlow(TEXT("Could not search for or join games.")); }
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	if (!SessionInterface->JoinSession(*NetId, NAME_GameSession, SessionResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		if (bJoinFlowActive) { bJoinFlowActive = false; FailFlow(TEXT("Could not search for or join games.")); }
	}
}

void UMultiplayerSessionsSubsystem::DestroySession()
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnDestroySessionComplete.Broadcast(false);
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		bCreateSessionOnDestroy = false;
		MultiplayerOnDestroySessionComplete.Broadcast(false);
	}
}

void UMultiplayerSessionsSubsystem::StartSession()
{
	// Was empty in 5.0. Call this when the match actually begins (e.g. lobby -> game map).
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnStartSessionComplete.Broadcast(false);
		return;
	}

	StartSessionCompleteDelegateHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegate);

	if (!SessionInterface->StartSession(NAME_GameSession))
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		MultiplayerOnStartSessionComplete.Broadcast(false);
	}
}

void UMultiplayerSessionsSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogMultiplayerSessions, Log, TEXT("CreateSession done: success=%d"), bWasSuccessful ? 1 : 0);
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}
	MultiplayerOnCreateSessionComplete.Broadcast(bWasSuccessful);

	if (bHostFlowActive)
	{
		bHostFlowActive = false;
		if (!bWasSuccessful)
		{
			FailFlow(TEXT("Could not create session."));
		}
		else if (UWorld* World = GetWorld())
		{
			World->ServerTravel(FString::Printf(TEXT("%s?listen"), *PendingLobbyPath));
		}
	}
}

void UMultiplayerSessionsSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}

	const bool bHasResults = LastSessionSearch.IsValid() && LastSessionSearch->SearchResults.Num() > 0;
	if (!bHasResults)
	{
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
	else
	{
		MultiplayerOnFindSessionsComplete.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful);
	}

	UE_LOG(LogMultiplayerSessions, Log, TEXT("FindSessions done: success=%d, results=%d"),
		bWasSuccessful ? 1 : 0, LastSessionSearch.IsValid() ? LastSessionSearch->SearchResults.Num() : -1);
	if (bHasResults)
	{
		for (const FOnlineSessionSearchResult& R : LastSessionSearch->SearchResults)
		{
			FString MT;
			R.Session.SessionSettings.Get(FName("MatchType"), MT);
			UE_LOG(LogMultiplayerSessions, Log, TEXT("  found: owner=%s matchtype='%s' ping=%d open=%d"),
				*R.Session.OwningUserName, *MT, R.PingInMs, R.Session.NumOpenPublicConnections);
		}
	}

	if (bJoinFlowActive)
	{
		if (bHasResults)
		{
			for (const FOnlineSessionSearchResult& Result : LastSessionSearch->SearchResults)
			{
				FString Value;
				Result.Session.SessionSettings.Get(FName("MatchType"), Value);
				if (Value == PendingMatchType)
				{
					JoinSession(Result); // flow continues in OnJoinSessionComplete
					return;
				}
			}
		}
		bJoinFlowActive = false;
#if !UE_BUILD_SHIPPING
		// Dev only: LAN broadcast is often blocked on Windows (firewall / virtual adapters).
		// Fall back to a host on this PC so local 2-window testing still works.
		if (IsLAN())
		{
			UE_LOG(LogMultiplayerSessions, Warning, TEXT("LAN search found nothing. Dev fallback: trying 127.0.0.1"));
			JoinByAddress(TEXT("127.0.0.1"));
			return;
		}
#endif
		FailFlow(TEXT("No games found."));
	}
}

void UMultiplayerSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}
	MultiplayerOnJoinSessionComplete.Broadcast(Result);

	if (bJoinFlowActive)
	{
		bJoinFlowActive = false;
		FString Address;
		if (Result != EOnJoinSessionCompleteResult::Success || !SessionInterface.IsValid()
			|| !SessionInterface->GetResolvedConnectString(NAME_GameSession, Address))
		{
			FailFlow(TEXT("Could not join the game."));
			return;
		}
		const UGameInstance* GI = GetGameInstance();
		if (APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr)
		{
			PC->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
		}
	}
}

void UMultiplayerSessionsSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}

	MultiplayerOnDestroySessionComplete.Broadcast(bWasSuccessful);

	if (bWasSuccessful && bCreateSessionOnDestroy)
	{
		bCreateSessionOnDestroy = false;
		CreateSession(LastNumPublicConnections, LastMatchType);
	}
}

void UMultiplayerSessionsSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	}
	MultiplayerOnStartSessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::HostGame(int32 NumPublicConnections, const FString& MatchType, const FString& LobbyMapPath)
{
	UE_LOG(LogMultiplayerSessions, Log, TEXT("HostGame: MatchType '%s', %d players, map %s (LAN=%d)"),
		*MatchType, NumPublicConnections, *LobbyMapPath, IsLAN() ? 1 : 0);
	bHostFlowActive = true;
	bJoinFlowActive = false;
	PendingLobbyPath = LobbyMapPath;
	CreateSession(NumPublicConnections, MatchType);
}

void UMultiplayerSessionsSubsystem::FindAndJoinGame(const FString& MatchType)
{
	if (bJoinFlowActive)
	{
		UE_LOG(LogMultiplayerSessions, Log, TEXT("FindAndJoinGame: search already running, ignoring extra click."));
		return;
	}
	UE_LOG(LogMultiplayerSessions, Log, TEXT("FindAndJoinGame: searching for MatchType '%s' (LAN=%d)"), *MatchType, IsLAN() ? 1 : 0);
	bJoinFlowActive = true;
	bHostFlowActive = false;
	PendingMatchType = MatchType;
	FindSessions(10000);
}

void UMultiplayerSessionsSubsystem::FailFlow(const FString& Reason)
{
	UE_LOG(LogMultiplayerSessions, Warning, TEXT("%s"), *Reason);
	OnSessionFlowFailed.Broadcast(Reason);
}

void UMultiplayerSessionsSubsystem::JoinByAddress(const FString& Address)
{
	const UGameInstance* GI = GetGameInstance();
	if (APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr)
	{
		UE_LOG(LogMultiplayerSessions, Log, TEXT("JoinByAddress: %s"), *Address);
		PC->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
	}
	else
	{
		FailFlow(TEXT("No local player to travel with."));
	}
}
