#include "FPSDamageEffect.h"
#include "FPSAttributeSet.h"

const FName UFPSDamageEffect::DamageSetByCallerName(TEXT("Damage"));

UFPSDamageEffect::UFPSDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataName = DamageSetByCallerName;

	FGameplayModifierInfo Mod;
	Mod.Attribute = UFPSAttributeSet::GetIncomingDamageAttribute();
	Mod.ModifierOp = EGameplayModOp::Additive;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(Mod);
}
