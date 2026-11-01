#include "FPSAttributeSet.h"
#include "FPSHeroCharacter.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UFPSAttributeSet::UFPSAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitShield(50.f);
	InitMaxShield(50.f);
	InitIncomingDamage(0.f);
}

void UFPSAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UFPSAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFPSAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFPSAttributeSet, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFPSAttributeSet, MaxShield, COND_None, REPNOTIFY_Always);
}

void UFPSAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetShieldAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxShield());
	}
}

void UFPSAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute != GetIncomingDamageAttribute())
	{
		return;
	}

	const float Damage = GetIncomingDamage();
	SetIncomingDamage(0.f);
	if (Damage <= 0.f)
	{
		return;
	}

	AFPSHeroCharacter* Hero = Cast<AFPSHeroCharacter>(Data.Target.GetAvatarActor());
	if (Hero && Hero->IsDead())
	{
		return;
	}

	// Shield first.
	const float OldShield = GetShield();
	const float Absorbed = FMath::Min(OldShield, Damage);
	SetShield(OldShield - Absorbed);

	// Rest to health.
	const float Remaining = Damage - Absorbed;
	if (Remaining > 0.f)
	{
		SetHealth(FMath::Clamp(GetHealth() - Remaining, 0.f, GetMaxHealth()));
	}

	UE_LOG(LogTemp, Log, TEXT("[Damage] %s took %.1f (shield -%.1f, health -%.1f) -> HP %.1f SH %.1f"),
		Hero ? *Hero->GetName() : TEXT("?"), Damage, Absorbed, Remaining, GetHealth(), GetShield());

	if (GetHealth() <= 0.f && Hero)
	{
		Hero->HandleDeath();
	}
}

void UFPSAttributeSet::ResetAttributes(float InMaxHealth, float InMaxShield)
{
	SetMaxHealth(InMaxHealth);
	SetHealth(InMaxHealth);
	SetMaxShield(InMaxShield);
	SetShield(InMaxShield);
}

void UFPSAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFPSAttributeSet, Health, OldValue);
}

void UFPSAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFPSAttributeSet, MaxHealth, OldValue);
}

void UFPSAttributeSet::OnRep_Shield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFPSAttributeSet, Shield, OldValue);
}

void UFPSAttributeSet::OnRep_MaxShield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFPSAttributeSet, MaxShield, OldValue);
}
