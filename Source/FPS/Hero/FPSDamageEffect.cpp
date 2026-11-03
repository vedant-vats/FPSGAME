#include "FPSDamageEffect.h"
#include "FPSAttributeSet.h"
#include "AbilitySystemComponent.h"

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

void UFPSDamageEffect::ApplyDamage(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC, float Amount, UObject* SourceObject)
{
	if (!SourceASC || !TargetASC || Amount <= 0.f)
	{
		return;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(SourceObject);

	const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UFPSDamageEffect::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(DamageSetByCallerName, Amount);
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
}
