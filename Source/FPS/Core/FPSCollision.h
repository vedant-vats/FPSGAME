#pragma once

#include "Engine/EngineTypes.h"

// Trace channel for hitscan. Set up in DefaultEngine.ini as "Weapon".
// Only hitboxes + world block it. Capsule and mesh ignore it.
#define ECC_Weapon ECC_GameTraceChannel2

// Hitboxes with this component tag take headshot damage.
#define FPS_HEAD_HITBOX_TAG FName(TEXT("Head"))
