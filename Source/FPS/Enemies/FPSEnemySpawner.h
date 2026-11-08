#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemies/FPSEnemyCharacter.h"
#include "FPSEnemySpawner.generated.h"

USTRUCT(BlueprintType)
struct FFPSEnemyPoolEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Pool")
	TSubclassOf<AFPSEnemyCharacter> EnemyClass;

	/** How many to pre-spawn at match start. The pool grows (with a warning) if it runs out. */
	UPROPERTY(EditAnywhere, Category = "Pool", meta = (ClampMin = 0))
	int32 PoolSize = 30;
};

/**
 * Place one in the arena. Server only.
 * Pre-spawns every enemy at BeginPlay (hidden + dormant), then hands them out.
 * No SpawnActor during a wave = no spawn hitches.
 */
UCLASS()
class FPS_API AFPSEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AFPSEnemySpawner();

	virtual void BeginPlay() override;

	/** Server. Take one from the pool, place it on the navmesh, wake it up. */
	AFPSEnemyCharacter* SpawnEnemy(TSubclassOf<AFPSEnemyCharacter> EnemyClass, uint8 Tier = 0);

	/** Server. Spawns Count enemies, cycling through the pool types. */
	void SpawnMixed(int32 Count);

	/** Server. Debug: kill every active enemy. */
	void KillAll();

	int32 GetAliveCount() const;

	/** Fires on the server for every enemy death (the wave manager counts these). */
	FOnEnemyDied OnAnyEnemyDied;

protected:
	UPROPERTY(EditAnywhere, Category = "Spawner")
	TArray<FFPSEnemyPoolEntry> Pools;

	/** Optional. If empty, enemies spawn at random navmesh points within SpawnRadius. */
	UPROPERTY(EditAnywhere, Category = "Spawner")
	TArray<TObjectPtr<AActor>> SpawnPoints;

	UPROPERTY(EditAnywhere, Category = "Spawner", meta = (Units = "cm"))
	float SpawnRadius = 1500.f;

private:
	AFPSEnemyCharacter* CreatePooledEnemy(TSubclassOf<AFPSEnemyCharacter> EnemyClass);
	bool FindSpawnLocation(FVector& OutLocation) const;
	void HandleEnemyDied(AFPSEnemyCharacter* Enemy);

	UPROPERTY()
	TArray<TObjectPtr<AFPSEnemyCharacter>> AllEnemies;

	int32 NextPoolIndex = 0;
};
