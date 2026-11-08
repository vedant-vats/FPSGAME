#include "FPSWaveManager.h"
#include "FPSEnemyCharacter.h"
#include "FPSEnemySpawner.h"
#include "FPSGameState.h"
#include "FPSHeroCharacter.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AFPSWaveManager::AFPSWaveManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // state goes through AFPSGameState

	// Defaults so it works without any setup. Counts are for 1 player.
	FFPSWaveDef W;
	W.Runners = 8;  W.Tanks = 0; W.EliteChance = 0.f;  Waves.Add(W);
	W.Runners = 12; W.Tanks = 2; W.EliteChance = 0.f;  Waves.Add(W);
	W.Runners = 14; W.Tanks = 4; W.EliteChance = 0.1f; Waves.Add(W);
	W.Runners = 18; W.Tanks = 6; W.EliteChance = 0.15f; Waves.Add(W);
	W.Runners = 16; W.Tanks = 6; W.EliteChance = 0.2f; W.bMiniBoss = true; Waves.Add(W);
}

void AFPSWaveManager::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	GameState = GetWorld()->GetGameState<AFPSGameState>();
	if (!GameState)
	{
		UE_LOG(LogTemp, Error, TEXT("[Waves] GameState is not FPSGameState. Set Game State Class on the GameMode."));
		return;
	}

	if (!Spawner)
	{
		for (TActorIterator<AFPSEnemySpawner> It(GetWorld()); It; ++It)
		{
			Spawner = *It;
			break;
		}
	}
	if (!Spawner)
	{
		UE_LOG(LogTemp, Error, TEXT("[Waves] No FPSEnemySpawner in the map."));
		return;
	}

	Spawner->OnAnyEnemyDied.AddUObject(this, &ThisClass::HandleEnemyDied);
	GameState->TotalWaves = Waves.Num();

	// Countdown, then wave 1.
	SetPhase(static_cast<uint8>(EFPSMatchPhase::WaitingToStart), StartDelay);
	GetWorldTimerManager().SetTimer(PhaseTimer, FTimerDelegate::CreateUObject(this, &ThisClass::StartWave, 0), StartDelay, false);

	// 4 players max: polling twice a second is cheaper and simpler than death events.
	GetWorldTimerManager().SetTimer(PlayerCheckTimer, this, &ThisClass::CheckPlayers, 0.5f, true);
}

void AFPSWaveManager::SetPhase(uint8 NewPhase, float Duration)
{
	GameState->SetMatchPhase(static_cast<EFPSMatchPhase>(NewPhase), Duration);
}

int32 AFPSWaveManager::CountPlayers(int32& OutAlive) const
{
	int32 Total = 0;
	OutAlive = 0;
	for (TActorIterator<AFPSHeroCharacter> It(GetWorld()); It; ++It)
	{
		++Total;
		if (!It->IsDead())
		{
			++OutAlive;
		}
	}
	return Total;
}

void AFPSWaveManager::StartWave(int32 NewWaveIndex)
{
	if (!Waves.IsValidIndex(NewWaveIndex) || GameState->IsMatchOver())
	{
		return;
	}

	WaveIndex = NewWaveIndex;
	const FFPSWaveDef& Def = Waves[WaveIndex];

	int32 Alive = 0;
	const int32 Players = FMath::Max(1, CountPlayers(Alive));
	const float Scale = 1.f + PlayerScale * (Players - 1);

	// Build the spawn list, then shuffle so runners and tanks mix.
	Queue.Reset();
	auto AddMany = [this, &Def](TSubclassOf<AFPSEnemyCharacter> Class, int32 Count)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			FQueuedSpawn Spawn;
			Spawn.Class = Class;
			Spawn.Tier = FMath::FRand() < Def.EliteChance ? 1 : 0;
			Queue.Add(Spawn);
		}
	};
	AddMany(RunnerClass, FMath::RoundToInt(Def.Runners * Scale));
	AddMany(TankClass, FMath::RoundToInt(Def.Tanks * Scale));
	for (int32 i = Queue.Num() - 1; i > 0; --i)
	{
		Queue.Swap(i, FMath::RandRange(0, i));
	}
	if (Def.bMiniBoss && TankClass)
	{
		FQueuedSpawn Boss;
		Boss.Class = TankClass;
		Boss.Tier = 2;
		Queue.Insert(Boss, Queue.Num() / 2); // shows up mid-wave
	}

	GameState->CurrentWave = WaveIndex + 1;
	GameState->EnemiesRemaining = Queue.Num();
	SetPhase(static_cast<uint8>(EFPSMatchPhase::InWave), 0.f);

	UE_LOG(LogTemp, Log, TEXT("[Waves] Wave %d/%d: %d enemies (%d players)"), WaveIndex + 1, Waves.Num(), Queue.Num(), Players);

	GetWorldTimerManager().SetTimer(SpawnTimer, this, &ThisClass::SpawnNext, SpawnInterval, true, 0.f);
}

void AFPSWaveManager::SpawnNext()
{
	if (Queue.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(SpawnTimer);
		return;
	}
	if (Spawner->GetAliveCount() >= MaxAlive)
	{
		return; // wait for space
	}

	const FQueuedSpawn Next = Queue.Pop(EAllowShrinking::No);
	AFPSEnemyCharacter* Spawned = Spawner->SpawnEnemy(Next.Class, Next.Tier);
	if (Spawned && Next.Tier == 2)
	{
		GameState->MulticastMiniBossSpawned(); // UI warning on every screen
	}
	if (!Spawned)
	{
		// No navmesh spot / no class: don't soft-lock the wave.
		UE_LOG(LogTemp, Warning, TEXT("[Waves] Spawn failed, skipping one enemy"));
		GameState->EnemiesRemaining = FMath::Max(0, GameState->EnemiesRemaining - 1);
		if (GameState->EnemiesRemaining == 0 && Queue.Num() == 0)
		{
			WaveCleared();
		}
	}
}

void AFPSWaveManager::HandleEnemyDied(AFPSEnemyCharacter* Enemy)
{
	if (GameState->Phase != EFPSMatchPhase::InWave)
	{
		return;
	}

	GameState->EnemiesRemaining = FMath::Max(0, GameState->EnemiesRemaining - 1);
	if (GameState->EnemiesRemaining == 0 && Queue.Num() == 0)
	{
		WaveCleared();
	}
}

void AFPSWaveManager::WaveCleared()
{
	GetWorldTimerManager().ClearTimer(SpawnTimer);

	if (WaveIndex >= Waves.Num() - 1)
	{
		EndMatch(true);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Waves] Wave %d cleared"), WaveIndex + 1);
	SetPhase(static_cast<uint8>(EFPSMatchPhase::Intermission), IntermissionTime);
	GetWorldTimerManager().SetTimer(PhaseTimer, FTimerDelegate::CreateUObject(this, &ThisClass::StartWave, WaveIndex + 1), IntermissionTime, false);
}

void AFPSWaveManager::CheckPlayers()
{
	if (!GameState || GameState->IsMatchOver() || GameState->Phase == EFPSMatchPhase::WaitingToStart)
	{
		return;
	}

	int32 Alive = 0;
	const int32 Total = CountPlayers(Alive);
	if (Total > 0 && Alive == 0)
	{
		EndMatch(false); // everyone down at the same time
	}
}

void AFPSWaveManager::EndMatch(bool bWon)
{
	GetWorldTimerManager().ClearTimer(SpawnTimer);
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	Queue.Reset();

	SetPhase(static_cast<uint8>(bWon ? EFPSMatchPhase::Victory : EFPSMatchPhase::Defeat), RestartDelay);
	UE_LOG(LogTemp, Log, TEXT("[Waves] Match over: %s"), bWon ? TEXT("VICTORY") : TEXT("DEFEAT"));

	Spawner->KillAll();

	if (RestartDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(PhaseTimer, this, &ThisClass::RestartMatch, RestartDelay, false);
	}
}

void AFPSWaveManager::RestartMatch()
{
	// Reload this map; clients travel with the server.
	const FString MapPath = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
	GetWorld()->ServerTravel(MapPath + TEXT("?listen"));
}

void AFPSWaveManager::SkipWave()
{
	if (!GameState || GameState->IsMatchOver())
	{
		return;
	}
	if (GameState->Phase == EFPSMatchPhase::WaitingToStart || GameState->Phase == EFPSMatchPhase::Intermission)
	{
		GetWorldTimerManager().ClearTimer(PhaseTimer);
		StartWave(WaveIndex + 1);
		return;
	}

	// In a wave: stop counting deaths, clear everything, move on.
	Queue.Reset();
	GetWorldTimerManager().ClearTimer(SpawnTimer);
	GameState->Phase = EFPSMatchPhase::Intermission;
	Spawner->KillAll();
	GameState->EnemiesRemaining = 0;
	WaveCleared();
}
