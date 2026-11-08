#pragma once

#include "CoreMinimal.h"

/** Debug console commands, readouts and test cvars. Compiled out of Shipping builds. */
#define FPS_WITH_DEBUG_TOOLS (!UE_BUILD_SHIPPING)

/**
 * Console variables for netcode testing. Type them in the ~ console.
 *   fps.Rewind 0/1          (server)  lag compensation on/off
 *   fps.NetDebug 0/1        (each)    debug lines, ghost hitboxes
 *   fps.NoDamage 0/1        (server)  hits still count, no damage (for long tests)
 *   fps.RewindOffsetMs N    (client)  extra ms to rewind (tuning)
 *   fps.MaxRewindMs N       (server)  max rewind allowed (anti-cheat cap)
 *   fps.FriendlyFire 0/1    (server)  players can hurt players (needed for the rewind accuracy test)
 */
namespace FPSNet
{
	FPS_API bool IsRewindEnabled();
	FPS_API bool IsDebugDrawEnabled();
	FPS_API bool IsNoDamage();
	FPS_API float GetRewindOffsetSeconds();
	FPS_API float GetMaxRewindSeconds();
	FPS_API bool IsFriendlyFireEnabled();
}
