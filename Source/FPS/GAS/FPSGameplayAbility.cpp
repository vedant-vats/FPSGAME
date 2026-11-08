#include "FPSGameplayAbility.h"
#include "Core/FPSGameplayTags.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

UFPSGameplayAbility::UFPSGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Dead or stunned heroes can't use abilities.
	ActivationBlockedTags.AddTag(FPSTags::State_Dead);
	ActivationBlockedTags.AddTag(FPSTags::State_Stunned);
}

bool UFPSGameplayAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (MaxCharges <= 1)
	{
		return Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags); // normal: blocked while cooldown tag is on
	}

	// Charges: blocked only when every charge is used (stack count = MaxCharges).
	const UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!CooldownGE || !ASC)
	{
		return true;
	}
	return ASC->GetGameplayEffectCount(CooldownGE->GetClass(), nullptr) < MaxCharges;
}

void UFPSGameplayAbility::GetCooldownState(UAbilitySystemComponent* ASC, float& OutRemaining, float& OutDuration, int32& OutChargesReady) const
{
	OutRemaining = 0.f;
	OutDuration = 0.f;
	OutChargesReady = MaxCharges;

	const UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (!CooldownGE || !ASC)
	{
		return;
	}

	FGameplayEffectQuery Query;
	Query.EffectDefinition = CooldownGE->GetClass();
	for (const TPair<float, float>& TimeAndDuration : ASC->GetActiveEffectsTimeRemainingAndDuration(Query))
	{
		if (TimeAndDuration.Key > OutRemaining)
		{
			OutRemaining = TimeAndDuration.Key;
			OutDuration = TimeAndDuration.Value;
		}
	}

	const int32 UsedCharges = ASC->GetGameplayEffectCount(CooldownGE->GetClass(), nullptr);
	OutChargesReady = FMath::Max(0, MaxCharges - UsedCharges);
}
