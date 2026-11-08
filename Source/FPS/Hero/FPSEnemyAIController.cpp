#include "FPSEnemyAIController.h"
#include "FPSEnemyCharacter.h"
#include "FPSHeroCharacter.h"

#include "EngineUtils.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

AFPSEnemyAIController::AFPSEnemyAIController()
{
	bWantsPlayerState = false; // enemies don't need a PlayerState (saves a replicated actor each)
}

void AFPSEnemyAIController::StartThinking()
{
	// Random first delay = enemies spread their thinking across frames.
	GetWorldTimerManager().SetTimer(ThinkTimer, this, &ThisClass::Think, ThinkInterval, true,
		FMath::FRandRange(0.f, ThinkInterval));
}

void AFPSEnemyAIController::StopThinking()
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	StopMovement();
	CurrentTarget = nullptr;
}

AActor* AFPSEnemyAIController::FindNearestTarget() const
{
	const APawn* MyPawn = GetPawn();
	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	// Max 4 players: a simple loop is cheaper than any spatial query.
	for (TActorIterator<AFPSHeroCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsDead())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(It->GetActorLocation(), MyPawn->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = *It;
		}
	}
	return Best;
}

void AFPSEnemyAIController::Think()
{
	AFPSEnemyCharacter* Enemy = GetPawn<AFPSEnemyCharacter>();
	if (!Enemy || !Enemy->IsActive() || Enemy->IsCombatantDead())
	{
		return;
	}

	if (Enemy->IsStunned())
	{
		StopMovement();
		return;
	}

	AActor* Target = FindNearestTarget();
	if (!Target)
	{
		StopMovement();
		CurrentTarget = nullptr;
		return;
	}

	// In reach: stop and swing.
	const float Distance = FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation());
	if (Distance <= Enemy->GetAttackRange())
	{
		StopMovement();
		CurrentTarget = nullptr;
		Enemy->StartAttack(Target);
		return;
	}

	// Chase. Only send a new move when the target changed or we stopped:
	// MoveToActor already follows a moving goal, re-requesting every think would waste path queries.
	if (Target != CurrentTarget.Get() || GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		MoveToActor(Target, Enemy->GetAttackRange() * 0.7f, true, true, false, nullptr, true);
		CurrentTarget = Target;
	}
}
