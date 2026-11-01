#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "FPSDamageEffect.generated.h"

/**
 * Instant GE: adds SetByCaller("Damage") to IncomingDamage.
 * Made in C++ so there's no editor asset to forget. Weapons reuse it later.
 */
UCLASS()
class FPS_API UFPSDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UFPSDamageEffect();

	static const FName DamageSetByCallerName;
};
