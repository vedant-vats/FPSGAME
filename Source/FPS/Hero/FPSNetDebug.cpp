#include "FPSNetDebug.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarFPSRewind(
	TEXT("fps.Rewind"), 1,
	TEXT("Server: 1 = rewind hitboxes for hitscan (lag compensation), 0 = off."));

static TAutoConsoleVariable<int32> CVarFPSNetDebug(
	TEXT("fps.NetDebug"), 1,
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

namespace FPSNet
{
	bool IsRewindEnabled() { return CVarFPSRewind.GetValueOnGameThread() != 0; }
	bool IsDebugDrawEnabled() { return CVarFPSNetDebug.GetValueOnGameThread() != 0; }
	bool IsNoDamage() { return CVarFPSNoDamage.GetValueOnGameThread() != 0; }
	float GetRewindOffsetSeconds() { return CVarFPSRewindOffsetMs.GetValueOnGameThread() / 1000.f; }
	float GetMaxRewindSeconds() { return CVarFPSMaxRewindMs.GetValueOnGameThread() / 1000.f; }
}
