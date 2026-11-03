#pragma once

#include "CoreMinimal.h"
#include "FPSGameplayAbility.h"
#include "FPSGA_FireWeapon.generated.h"

class UFPSWeaponComponent;

/**
 * Hold to fire. Runs on the owning client only (LocalOnly).
 * The weapon component sends each shot to the server, which checks it.
 */
UCLASS()
class FPS_API UFPSGA_FireWeapon : public UFPSGameplayAbility
{
	GENERATED_BODY()

public:
	UFPSGA_FireWeapon();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFPSWeaponComponent* GetWeapon(const FGameplayAbilityActorInfo* ActorInfo) const;
};
