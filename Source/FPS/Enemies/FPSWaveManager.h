#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPSWaveManager.generated.h"

class AFPSEnemySpawner;
class AFPSEnemyCharacter;
class AFPSGameState;

USTRUCT(BlueprintType)
struct FFPSWaveDef
{
	GENERATED_BODY()

	/** Count for ONE player. Each extra player adds PlayerScale of this. */
	UPROPERTY(EditAnywhere, Category = "Wave")
	int32 Runners = 8;

	UPROPERTY(EditAnywhere, Category = "Wave")
	int32 Tanks = 0;

	/** 0-1. Chance each spawn is an elite (bigger, tougher). */
	UPROPERTY(EditAnywhere, Category = "Wave", meta = (ClampMin = 0, ClampMax = 1))
	float EliteChance = 0.f;

	/** Adds one giant elite tank (mini-boss). */
	UPROPERTY(EditAnywhere, Category = "Wave")
	bool bMiniBoss = false;
};

/**
 * Place one in the arena (next to the FPSEnemySpawner). Server only.
 * Start delay -> wave -> intermission -> wave ... -> Victory, or Defeat when every player is dead at once.
 * Enemies trickle in (SpawnInterval, MaxAlive) instead of all at once.
 */
UCLASS()
class FPS_API AFPSWaveManager : public AActor
{
	GENERATED_BODY()

public:
	AFPSWaveManager();

	virtual void BeginPlay() override;

	/** Debug: kill everything and end the current wave now. */
	void SkipWave();

protected:
	UPROPERTY(EditAnywhere, Category = "Waves")
	TArray<FFPSWaveDef> Waves;

	/** Each player beyond the first adds this share of the wave (0.5 = +50% per player). */
	UPROPERTY(EditAnywhere, Category = "Waves")
	float PlayerScale = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Waves", meta = (Units = "s"))
	float StartDelay = 8.f;

	UPROPERTY(EditAnywhere, Category = "Waves", meta = (Units = "s"))
	float IntermissionTime = 10.f;

	UPROPERTY(EditAnywhere, Category = "Waves", meta = (Units = "s"))
	float SpawnInterval = 0.5f;

	/** Never more than this many alive at once (performance + pacing). */
	UPROPERTY(EditAnywhere, Category = "Waves")
	int32 MaxAlive = 30;

	UPROPERTY(EditAnywhere, Category = "Waves")
	TSubclassOf<AFPSEnemyCharacter> RunnerClass;

	UPROPERTY(EditAnywhere, Category = "Waves")
	TSubclassOf<AFPSEnemyCharacter> TankClass;

	/** Optional. Found automatically if empty. */
	UPROPERTY(EditAnywhere, Category = "Waves")
	TObjectPtr<AFPSEnemySpawner> Spawner;

	/** Reload the map this long after Victory/Defeat. 0 = never. */
	UPROPERTY(EditAnywhere, Category = "Waves", meta = (Units = "s"))
	float RestartDelay = 10.f;

private:
	struct FQueuedSpawn
	{
		TSubclassOf<AFPSEnemyCharacter> Class;
		uint8 Tier = 0; // 0 normal, 1 elite, 2 mini-boss
	};

	void StartWave(int32 WaveIndex);
	void SpawnNext();
	void HandleEnemyDied(AFPSEnemyCharacter* Enemy);
	void WaveCleared();
	void CheckPlayers();
	void EndMatch(bool bWon);
	void RestartMatch();
	void SetPhase(uint8 NewPhase, float Duration);
	int32 CountPlayers(int32& OutAlive) const;

	UPROPERTY()
	TObjectPtr<AFPSGameState> GameState;

	TArray<FQueuedSpawn> Queue;
	int32 WaveIndex = -1;
	FTimerHandle PhaseTimer;
	FTimerHandle SpawnTimer;
	FTimerHandle PlayerCheckTimer;
};
