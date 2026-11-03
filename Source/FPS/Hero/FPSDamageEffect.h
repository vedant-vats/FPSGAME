#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "FPSDamageEffect.generated.h"

class UAbilitySystemComponent;

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

	/** Server. Every damage source goes through here (debug cmd, rifle, abilities, enemies). */
	static void ApplyDamage(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC, float Amount, UObject* SourceObject);
};
