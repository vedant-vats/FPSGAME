#include "FPSGameState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

void AFPSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFPSGameState, Phase);
	DOREPLIFETIME(AFPSGameState, CurrentWave);
	DOREPLIFETIME(AFPSGameState, TotalWaves);
	DOREPLIFETIME(AFPSGameState, EnemiesRemaining);
	DOREPLIFETIME(AFPSGameState, PhaseEndServerTime);
}

float AFPSGameState::GetPhaseTimeRemaining() const
{
	return FMath::Max(0.f, PhaseEndServerTime - static_cast<float>(GetServerWorldTimeSeconds()));
}

void AFPSGameState::BeginPlay()
{
	Super::BeginPlay();

	// 4x a second is plenty to catch 3-2-1 on time. Cheap: one float compare.
	GetWorldTimerManager().SetTimer(CountdownTimer, this, &ThisClass::CheckCountdown, 0.25f, true);
}

void AFPSGameState::SetMatchPhase(EFPSMatchPhase NewPhase, float Duration)
{
	Phase = NewPhase;
	PhaseEndServerTime = static_cast<float>(GetServerWorldTimeSeconds()) + Duration;
	BroadcastPhase(); // OnRep doesn't run on the server (listen host needs the events too)
}

void AFPSGameState::OnRep_Phase()
{
	BroadcastPhase();
}

void AFPSGameState::BroadcastPhase()
{
	LastCountdownSecond = -1;
	OnPhaseChanged.Broadcast(Phase);
	if (Phase == EFPSMatchPhase::InWave)
	{
		OnWaveStarted.Broadcast(CurrentWave, TotalWaves);
	}
}

void AFPSGameState::CheckCountdown()
{
	if (Phase != EFPSMatchPhase::WaitingToStart && Phase != EFPSMatchPhase::Intermission)
	{
		return;
	}

	const int32 Seconds = FMath::CeilToInt(GetPhaseTimeRemaining());
	if (Seconds >= 1 && Seconds <= 3 && Seconds != LastCountdownSecond)
	{
		LastCountdownSecond = Seconds;
		OnCountdownTick.Broadcast(Seconds);
	}
}

void AFPSGameState::MulticastMiniBossSpawned_Implementation()
{
	OnMiniBossSpawned.Broadcast();
}
