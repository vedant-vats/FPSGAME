#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "FPSEnemyAIController.generated.h"

/**
 * Chase + attack, decided on a TIMER (not every frame). No Behavior Tree / StateTree:
 * for this logic a tree is pure overhead. Each enemy's timer starts at a random offset
 * so 50 enemies don't all think on the same frame.
 */
UCLASS()
class FPS_API AFPSEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AFPSEnemyAIController();

	void StartThinking();
	void StopThinking();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (Units = "s"))
	float ThinkInterval = 0.3f;

private:
	void Think();
	AActor* FindNearestTarget() const;

	FTimerHandle ThinkTimer;
	TWeakObjectPtr<AActor> CurrentTarget;
};
