#pragma once

#include "CoreMinimal.h"

/**
 * Console variables for netcode testing. Type them in the ~ console.
 *   fps.Rewind 0/1          (server)  lag compensation on/off
 *   fps.NetDebug 0/1        (each)    debug lines, ghost hitboxes
 *   fps.NoDamage 0/1        (server)  hits still count, no damage (for long tests)
 *   fps.RewindOffsetMs N    (client)  extra ms to rewind (tuning)
 *   fps.MaxRewindMs N       (server)  max rewind allowed (anti-cheat cap)
 */
namespace FPSNet
{
	FPS_API bool IsRewindEnabled();
	FPS_API bool IsDebugDrawEnabled();
	FPS_API bool IsNoDamage();
	FPS_API float GetRewindOffsetSeconds();
	FPS_API float GetMaxRewindSeconds();
}
