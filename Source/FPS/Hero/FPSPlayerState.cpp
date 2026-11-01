#include "FPSPlayerState.h"
#include "FPSAttributeSet.h"
#include "AbilitySystemComponent.h"

AFPSPlayerState::AFPSPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	// Mixed: full GE info to owner, minimal to others. Standard for player-controlled ASCs.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Subobject of the ASC owner -> ASC finds it automatically.
	AttributeSet = CreateDefaultSubobject<UFPSAttributeSet>(TEXT("AttributeSet"));

	// PlayerState default is very low; ASC needs fast updates.
	SetNetUpdateFrequency(100.f);
}

UAbilitySystemComponent* AFPSPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
