#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "FPSPlayerState.generated.h"

class UAbilitySystemComponent;
class UFPSAttributeSet;

/** Owns the ability system so abilities/attributes survive pawn respawn. */
UCLASS()
class FPS_API AFPSPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFPSPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UFPSAttributeSet* GetAttributeSet() const { return AttributeSet; }

	/** Server only. Stops re-granting abilities on every respawn. */
	bool bDefaultAbilitiesGiven = false;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UFPSAttributeSet> AttributeSet;
};
