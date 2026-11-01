# MultiplayerSessions (UE 5.6)

Create / find / join / destroy online sessions through the Online Subsystem (Steam, or NULL for LAN and PIE).

**Credit:** based on the MultiplayerSessions plugin from Stephen Ulibarri's Unreal Engine multiplayer course.
Ported from UE 5.0 to UE 5.6 and extended by Vedant Vats.

## Install
1. Copy this folder to `YourProject/Plugins/MultiplayerSessions`.
2. Delete `Binaries/` and `Intermediate/` in the plugin folder if present.
3. Right-click the `.uproject` → *Generate Visual Studio project files*, then build.
4. Add the config below to `Config/DefaultEngine.ini`.

## Config/DefaultEngine.ini
```ini
[/Script/Engine.GameEngine]
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/SteamSockets.SteamSocketsNetDriver",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")

[OnlineSubsystem]
DefaultPlatformService=Steam

[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480
bInitServerOnClient=true

[/Script/SteamSockets.SteamSocketsNetDriver]
NetConnectionClassName="/Script/SteamSockets.SteamSocketsNetConnection"
```
And in `Config/DefaultGame.ini` (raise if you want bigger lobbies):
```ini
[/Script/Engine.GameSession]
MaxPlayers=100
```

## Testing
- **PIE / LAN:** set `DefaultPlatformService=Null` (or launch with `-nosteam`). Sessions use LAN.
- **Steam:** needs a packaged build (Development) on two PCs with two Steam accounts, both in the same download region.

## Changes from the 5.0 version
**Engine API updates**
- `OnLevelRemovedFromWorld` (removed) → `NativeDestruct`.
- `bIsFocusable = true` → `SetIsFocusable(true)`.
- Raw `UPROPERTY` pointers → `TObjectPtr`.
- `SEARCH_PRESENCE` → `SEARCH_LOBBIES`.
- Steam networking via the `SteamSockets` net driver (needed for listen servers on 5.1+).
- Uses `Online::GetSubsystem(World)` so PIE instances each get the right subsystem.

**Bug fixes**
- `CreateSession` while a session exists: now waits for the destroy to finish before creating (before, it tried to create straight away and failed).
- Session interface is fetched in `Initialize()`, not the constructor (the constructor also runs for the class default object).
- Null checks for local player / net id / search object (crashed if called too early).
- Join button re-enabled when no matching session is found or a join fails.
- Join failures are no longer treated as success (it used to travel anyway).
- `StartSession` actually starts the session now (was empty).
- Menu unbinds from the subsystem on destroy; repeat `MenuSetup` calls no longer double-bind.
- All delegate handles cleared in `Deinitialize()`.
