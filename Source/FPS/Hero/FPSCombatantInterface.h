#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FPSCombatantInterface.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UFPSCombatantInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything that can take damage and die: heroes and enemies.
 * The attribute set talks to this instead of a concrete class, so damage works the same for both.
 */
class FPS_API IFPSCombatantInterface
{
	GENERATED_BODY()

public:
	virtual bool IsCombatantDead() const = 0;

	/** Server. Health reached 0. */
	virtual void OnCombatantDied() = 0;
};
