#include "FPSGA_FireWeapon.h"
#include "FPSWeaponComponent.h"
#include "FPSGameplayTags.h"

UFPSGA_FireWeapon::UFPSGA_FireWeapon()
{
	// Client runs the fire loop; server validates each shot in the weapon's ServerFire.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	InputTag = FPSTags::InputTag_Fire;
}

UFPSWeaponComponent* UFPSGA_FireWeapon::GetWeapon(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	return Avatar ? Avatar->FindComponentByClass<UFPSWeaponComponent>() : nullptr;
}

void UFPSGA_FireWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UFPSWeaponComponent* Weapon = GetWeapon(ActorInfo);
	if (!Weapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Weapon->StartFiring();
}

void UFPSGA_FireWeapon::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UFPSGA_FireWeapon::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UFPSWeaponComponent* Weapon = GetWeapon(ActorInfo))
	{
		Weapon->StopFiring();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
