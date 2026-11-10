#include "FPSEnemySpawner.h"

#include "Engine/World.h"
#include "NavigationSystem.h"

AFPSEnemySpawner::AFPSEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // pure server logic; the enemies replicate themselves
}

void AFPSEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	for (const FFPSEnemyPoolEntry& Entry : Pools)
	{
		for (int32 i = 0; i < Entry.PoolSize; ++i)
		{
			CreatePooledEnemy(Entry.EnemyClass);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Spawner] Pool ready: %d enemies"), AllEnemies.Num());
}

AFPSEnemyCharacter* AFPSEnemySpawner::CreatePooledEnemy(TSubclassOf<AFPSEnemyCharacter> EnemyClass)
{
	if (!EnemyClass)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFPSEnemyCharacter* Enemy = GetWorld()->SpawnActor<AFPSEnemyCharacter>(EnemyClass, GetActorLocation(), GetActorRotation(), Params);
	if (!Enemy)
	{
		return nullptr;
	}

	Enemy->Deactivate(); // straight into the pool: hidden, no collision, dormant
	Enemy->OnEnemyDied.AddUObject(this, &ThisClass::HandleEnemyDied);
	AllEnemies.Add(Enemy);
	return Enemy;
}

bool AFPSEnemySpawner::FindSpawnLocation(FVector& OutLocation)
{
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetNavigationSystem(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Spawner] No navigation system. Add a NavMeshBoundsVolume to the map."));
		return false;
	}

	FVector Origin = GetActorLocation();
	float Radius = SpawnRadius;
	if (SpawnPoints.Num() > 0)
	{
		// Round robin: gate 1, 2, 3, 1, 2, 3... Skips empty slots in the array.
		for (int32 Tries = 0; Tries < SpawnPoints.Num(); ++Tries)
		{
			const AActor* Point = SpawnPoints[NextSpawnPointIndex % SpawnPoints.Num()];
			NextSpawnPointIndex++;
			if (Point)
			{
				Origin = Point->GetActorLocation();
				Radius = 200.f; // small spread around the chosen point
				break;
			}
		}
	}

	FNavLocation NavLocation;
	if (!NavSys->GetRandomReachablePointInRadius(Origin, Radius, NavLocation))
	{
		return false;
	}
	OutLocation = NavLocation.Location;
	return true;
}

AFPSEnemyCharacter* AFPSEnemySpawner::SpawnEnemy(TSubclassOf<AFPSEnemyCharacter> EnemyClass, uint8 Tier)
{
	if (!HasAuthority() || !EnemyClass)
	{
		return nullptr;
	}

	// Free one of this type?
	AFPSEnemyCharacter* Enemy = nullptr;
	for (AFPSEnemyCharacter* Candidate : AllEnemies)
	{
		if (Candidate && !Candidate->IsActive() && Candidate->GetClass() == EnemyClass)
		{
			Enemy = Candidate;
			break;
		}
	}
	if (!Enemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Spawner] Pool empty for %s, growing (raise PoolSize)"), *GetNameSafe(EnemyClass));
		Enemy = CreatePooledEnemy(EnemyClass);
		if (!Enemy)
		{
			return nullptr;
		}
	}

	FVector Location;
	if (!FindSpawnLocation(Location))
	{
		return nullptr;
	}

	// Location is the floor point; Activate stands the capsule on it (after scaling for elites).
	const FRotator Facing(0.f, FMath::FRandRange(-180.f, 180.f), 0.f);
	Enemy->Activate(Location, Facing, Tier);
	return Enemy;
}

void AFPSEnemySpawner::SpawnMixed(int32 Count)
{
	if (Pools.Num() == 0)
	{
		return;
	}
	for (int32 i = 0; i < Count; ++i)
	{
		const FFPSEnemyPoolEntry& Entry = Pools[NextPoolIndex % Pools.Num()];
		NextPoolIndex++;
		SpawnEnemy(Entry.EnemyClass);
	}
}

void AFPSEnemySpawner::KillAll()
{
	for (AFPSEnemyCharacter* Enemy : AllEnemies)
	{
		if (Enemy && Enemy->IsActive() && !Enemy->IsCombatantDead())
		{
			Enemy->OnCombatantDied();
		}
	}
}

int32 AFPSEnemySpawner::GetAliveCount() const
{
	int32 Count = 0;
	for (const AFPSEnemyCharacter* Enemy : AllEnemies)
	{
		if (Enemy && Enemy->IsActive() && !Enemy->IsCombatantDead())
		{
			++Count;
		}
	}
	return Count;
}

void AFPSEnemySpawner::HandleEnemyDied(AFPSEnemyCharacter* Enemy)
{
	OnAnyEnemyDied.Broadcast(Enemy);
}
