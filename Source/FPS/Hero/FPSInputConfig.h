#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "FPSInputConfig.generated.h"

class UInputAction;

USTRUCT(BlueprintType)
struct FFPSAbilityInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/** Maps Enhanced Input actions to ability input tags. One asset per hero (or shared). */
UCLASS(BlueprintType)
class FPS_API UFPSInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (TitleProperty = "InputTag"))
	TArray<FFPSAbilityInputAction> AbilityInputActions;
};
