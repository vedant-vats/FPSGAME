#pragma once

#include "CoreMinimal.h"
#include "GAS/FPSGameplayAbility.h"
#include "FPSGA_Dash.generated.h"

class USoundBase;
class UNiagaraSystem;
class UCameraShakeBase;

/**
 * Snappy dash (Marvel Rivals style burst). ~6 m in 0.2 s, 2 charges, works in air.
 * Uses a ROOT MOTION SOURCE: the movement component predicts it on the client
 * and replays it on the server, so there's no rubber-banding under lag.
 * (LaunchCharacter would NOT be predicted and would snap back at high ping.)
 */
UCLASS()
class FPS_API UFPSGA_Dash : public UFPSGameplayAbility
{
	GENERATED_BODY()

public:
	UFPSGA_Dash();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Dash", meta = (Units = "cm"))
	float DashDistance = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dash", meta = (Units = "s"))
	float DashDuration = 0.2f;

	// ---- Juice (local player) ----
	/** Extra FOV degrees at dash start, eases back. */
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Feel")
	float FovKick = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dash|Feel")
	TSubclassOf<UCameraShakeBase> CameraShake;

	UPROPERTY(EditDefaultsOnly, Category = "Dash|Feel")
	TObjectPtr<USoundBase> DashSound;

	/** Trail / burst. Attached to the hero. Everyone sees it. */
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Feel")
	TObjectPtr<UNiagaraSystem> DashVFX;

private:
	UFUNCTION()
	void OnDashFinished();
};
