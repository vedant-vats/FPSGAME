#pragma once

#include "NativeGameplayTags.h"

// Native tags: show up in the editor tag picker with no .ini edits.
namespace FPSTags
{
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Fire);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_Dash);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_Area);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
}
