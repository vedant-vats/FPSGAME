#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "FPSGameplayAbility.generated.h"

/**
 * Base for all hero abilities. InputTag links it to an Enhanced Input action.
 * Charges: if MaxCharges > 1, the cooldown GE must STACK (limit = MaxCharges).
 * Each use adds a stack; the ability is blocked only when all charges are used.
 */
UCLASS(Abstract)
class FPS_API UFPSGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UFPSGameplayAbility();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;

	/** 1 = normal cooldown. >1 = charges (cooldown GE must stack to this limit). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldowns", meta = (ClampMin = 1))
	int32 MaxCharges = 1;

	/** Short name shown on the HUD (e.g. "DASH"). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FText HUDName;

	/** Key shown on the HUD (e.g. "SHIFT"). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FText HUDKey;

	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** For the HUD: time left on the cooldown and how many charges are ready. */
	void GetCooldownState(UAbilitySystemComponent* ASC, float& OutRemaining, float& OutDuration, int32& OutChargesReady) const;
};
