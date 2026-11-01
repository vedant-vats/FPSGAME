#include "FPSGameplayAbility.h"
#include "FPSGameplayTags.h"

UFPSGameplayAbility::UFPSGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Dead heroes can't use abilities.
	ActivationBlockedTags.AddTag(FPSTags::State_Dead);
}
