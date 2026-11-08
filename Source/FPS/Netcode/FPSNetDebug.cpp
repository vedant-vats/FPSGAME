#include "FPSNetDebug.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarFPSRewind(
	TEXT("fps.Rewind"), 1,
	TEXT("Server: 1 = rewind hitboxes for hitscan (lag compensation), 0 = off."));

static TAutoConsoleVariable<int32> CVarFPSNetDebug(
	TEXT("fps.NetDebug"), 0,
	TEXT("1 = draw shot lines and ghost hitboxes (green = client view, blue = server rewind)."));

static TAutoConsoleVariable<int32> CVarFPSNoDamage(
	TEXT("fps.NoDamage"), 0,
	TEXT("Server: 1 = hits are counted but deal no damage (for accuracy tests)."));

static TAutoConsoleVariable<float> CVarFPSRewindOffsetMs(
	TEXT("fps.RewindOffsetMs"), 0.f,
	TEXT("Client: extra milliseconds to rewind. Tune until green and blue boxes line up."));

static TAutoConsoleVariable<float> CVarFPSMaxRewindMs(
	TEXT("fps.MaxRewindMs"), 400.f,
	TEXT("Server: max rewind in ms. Shots claiming older times are clamped."));

static TAutoConsoleVariable<int32> CVarFPSFriendlyFire(
	TEXT("fps.FriendlyFire"), 0,
	TEXT("Server: 1 = players can damage/stun each other (rewind accuracy test vs a player target)."));

namespace FPSNet
{
	bool IsRewindEnabled() { return CVarFPSRewind.GetValueOnGameThread() != 0; }
	float GetRewindOffsetSeconds() { return CVarFPSRewindOffsetMs.GetValueOnGameThread() / 1000.f; }
	float GetMaxRewindSeconds() { return CVarFPSMaxRewindMs.GetValueOnGameThread() / 1000.f; }

#if FPS_WITH_DEBUG_TOOLS
	bool IsDebugDrawEnabled() { return CVarFPSNetDebug.GetValueOnGameThread() != 0; }
	bool IsNoDamage() { return CVarFPSNoDamage.GetValueOnGameThread() != 0; }
	bool IsFriendlyFireEnabled() { return CVarFPSFriendlyFire.GetValueOnGameThread() != 0; }
#else
	// Shipping: test switches are always off, whatever the console says.
	bool IsDebugDrawEnabled() { return false; }
	bool IsNoDamage() { return false; }
	bool IsFriendlyFireEnabled() { return false; }
#endif
}
