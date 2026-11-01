#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "FPSGameplayAbility.generated.h"

/** Base for all hero abilities. InputTag links it to an Enhanced Input action. */
UCLASS(Abstract)
class FPS_API UFPSGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UFPSGameplayAbility();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};
