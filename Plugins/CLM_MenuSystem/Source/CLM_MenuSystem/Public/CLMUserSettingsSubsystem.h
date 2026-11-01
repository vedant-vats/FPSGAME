// Copyright Vedant Vats.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CLMUserSettingsSubsystem.generated.h"

/**
 * Native base for the menu's settings subsystem.
 * The save/load logic lives in the Blueprint child (BPCLMUserSettingsSubSystem).
 * Abstract so only the Blueprint child is ever created.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class CLM_MENUSYSTEM_API UCLMUserSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
};
