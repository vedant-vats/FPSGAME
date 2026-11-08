#pragma once

#include "CoreMinimal.h"
#include "GAS/FPSGameplayAbility.h"
#include "FPSGA_Shockwave.generated.h"

class USoundBase;
class UNiagaraSystem;
class UCameraShakeBase;
class UGameplayEffect;

/**
 * Instant pulse around the hero: damage + small knockback + stun.
 * Client: plays feel right away (predicted). Server: finds targets and applies effects.
 */
UCLASS()
class FPS_API UFPSGA_Shockwave : public UFPSGameplayAbility
{
	GENERATED_BODY()

public:
	UFPSGA_Shockwave();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Shockwave", meta = (Units = "cm"))
	float Radius = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shockwave")
	float Damage = 40.f;

	/** GE that grants State.Stunned for its duration (GE_Stun, 1.5 s). */
	UPROPERTY(EditDefaultsOnly, Category = "Shockwave")
	TSubclassOf<UGameplayEffect> StunEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Shockwave")
	float KnockbackStrength = 700.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shockwave")
	float KnockbackUp = 250.f;

	/**
	 * Knockback on PLAYERS is server-only (not predicted), so the hit player's client gets corrected = a snap.
	 * Off by default. AI enemies always get knocked back.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Shockwave")
	bool bKnockbackPlayers = false;

	/** Co-op: off. Console "fps.FriendlyFire 1" turns it on for tests. */
	UPROPERTY(EditDefaultsOnly, Category = "Shockwave")
	bool bAffectPlayers = false;

	// ---- Juice ----
	UPROPERTY(EditDefaultsOnly, Category = "Shockwave|Feel")
	TSubclassOf<UCameraShakeBase> CameraShake;

	UPROPERTY(EditDefaultsOnly, Category = "Shockwave|Feel")
	TObjectPtr<USoundBase> ShockwaveSound;

	/** Ring / blast at your feet. Everyone sees it. */
	UPROPERTY(EditDefaultsOnly, Category = "Shockwave|Feel")
	TObjectPtr<UNiagaraSystem> ShockwaveVFX;

private:
	void ApplyShockwave(AActor* Source) const;
};
