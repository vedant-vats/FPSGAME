#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "FPSGameState.generated.h"

UENUM(BlueprintType)
enum class EFPSMatchPhase : uint8
{
	WaitingToStart,
	InWave,
	Intermission,
	Victory,
	Defeat
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFPSPhaseChanged, EFPSMatchPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFPSWaveStarted, int32, Wave, int32, TotalWaves);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFPSCountdownTick, int32, SecondsLeft);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFPSMiniBossSpawned);

/**
 * Match info every client needs for the HUD: phase, wave, enemies left, countdowns.
 * Written only by AFPSWaveManager on the server. Set as Game State Class on the GameMode.
 */
UCLASS()
class FPS_API AFPSGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	// ---- Events for UI (fire on every machine; bind in the widget's Event Construct) ----
	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnFPSPhaseChanged OnPhaseChanged;

	/** Phase became InWave. */
	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnFPSWaveStarted OnWaveStarted;

	/** Last 3 seconds of a countdown (start delay / intermission): 3, 2, 1. */
	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnFPSCountdownTick OnCountdownTick;

	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnFPSMiniBossSpawned OnMiniBossSpawned;

	/** Server: change phase + countdown and fire the events. */
	void SetMatchPhase(EFPSMatchPhase NewPhase, float Duration);

	/** Server: tell every client a mini-boss just spawned. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastMiniBossSpawned();

	UPROPERTY(ReplicatedUsing = OnRep_Phase, BlueprintReadOnly, Category = "Match")
	EFPSMatchPhase Phase = EFPSMatchPhase::WaitingToStart;

	/** 1-based. 0 = not started. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	int32 CurrentWave = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	int32 TotalWaves = 0;

	/** Enemies still to kill this wave (alive + not yet spawned). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	int32 EnemiesRemaining = 0;

	/** Server time when the current countdown ends (start delay, intermission, end screen). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	float PhaseEndServerTime = 0.f;

	/** Seconds left on the current countdown, on any machine. */
	UFUNCTION(BlueprintPure, Category = "Match")
	float GetPhaseTimeRemaining() const;

	bool IsMatchOver() const { return Phase == EFPSMatchPhase::Victory || Phase == EFPSMatchPhase::Defeat; }

private:
	UFUNCTION()
	void OnRep_Phase();

	void BroadcastPhase();
	void CheckCountdown();

	FTimerHandle CountdownTimer;
	int32 LastCountdownSecond = -1;
};
