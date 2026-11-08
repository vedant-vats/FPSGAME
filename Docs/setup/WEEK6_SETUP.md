# Week 6 — Waves (Marvel Zombies style), match flow, arena

## What's new (C++)
- FPSGameState: replicated match phase, wave x/y, enemies left, countdowns.
- FPSWaveManager: start countdown -> 5 waves (trickle spawns, max 30 alive) -> intermission 10 s -> ...
  Count scales +50% per extra player. Elites from wave 3. Wave 5 has a giant elite tank (mini-boss).
  Victory after wave 5, Defeat when every player is dead at once. Map reloads 10 s later.
- Enemies: Tier 0/1/2 = normal / elite (1.25x size, 3x HP, 1.5x dmg) / mini-boss (2x size, 12x HP, 2x dmg).
- Heroes: respawn after 10 s (countdown on screen), but not once the match is over.
- Friendly fire OFF (rifle + shockwave). `fps.FriendlyFire 1` turns it back on for the rewind test.
- HUD: top-center wave info, NEXT WAVE IN, VICTORY / DEFEAT, RESPAWN IN.
- Debug: `DebugSkipWave` (skips countdown or ends the current wave).

## 1. Build
Close editor -> Rider -> Build Startup Project.

## 2. GameMode (BP_FirstpersonGameModee)
- Game State Class = **FPSGameState**. Compile, save.

## 3. Map
- Place **FPSWaveManager** next to the spawner.
  Details: Runner Class = BP_Enemy_Runner, Tank Class = BP_Enemy_Tank. Leave Waves as-is (5 defaults).
- Spawner pool sizes: Runner 30, Tank 20 is enough (max 30 alive at once).

## 4. Done-check (LaunchTwoGames_Lag150.bat)
1. "FIRST WAVE IN 8" counts down on both screens.
2. Wave 1: "WAVE 1 / 5", "8 ENEMIES LEFT" (x1.5 with 2 players = 12). Count goes down per kill.
3. Wave cleared -> "WAVE 1 CLEARED / NEXT WAVE IN 10" -> wave 2 starts.
4. Wave 3+: some enemies are bigger (elites) and take longer to kill.
5. Wave 5: one giant tank shows up mid-wave.
6. Shoot / shockwave the other player: nothing happens (friendly fire off).
7. Die: "RESPAWN IN 10", you come back if your teammate is alive.
8. Both die at once (`DebugDamageSelf 500` in both windows): DEFEAT, map reloads after 10 s, both reconnect.
9. `DebugSkipWave` x5 to reach VICTORY fast.
10. Solo (only host): full run works, death = DEFEAT.

## Rewind test from Week 3 still works with:
`fps.FriendlyFire 1` (host) + `fps.NoDamage 1` (host), then the same steps.
Do it BEFORE the waves start or in an intermission (or remove the wave manager from a test map).

## 5. Arena (5-day cap)
Blockout first (cubes, ramps, cover), test 2 full matches, THEN dress with free packs.
Navmesh must cover it (press P).

## 6. Wave banner widget (after done-check + arena blockout, cap 1 day)
C++ hooks on FPSGameState (Get Game State -> Cast to FPSGameState -> Bind in Event Construct):
- On Phase Changed (NewPhase)        -> WaitingToStart / InWave / Intermission / Victory / Defeat
- On Wave Started (Wave, TotalWaves)  -> big "WAVE 3" slam-in banner + horn
- On Countdown Tick (SecondsLeft)     -> huge 3 / 2 / 1 pulse + tick sound
- On Mini Boss Spawned                -> red warning banner + alarm
Read anytime: CurrentWave, TotalWaves, EnemiesRemaining, Phase, Get Phase Time Remaining.
Wave cleared = On Phase Changed with Intermission. Victory/Defeat = same event.
Keep "X ENEMIES LEFT" small in a corner (bind text to EnemiesRemaining).
Hook up: FPSHUD Blueprint child (or BP of FPSHUD) -> Wave Widget Class = WBP_WaveBanner, GameMode HUD Class = that BP.
When the slot is set, the canvas wave text turns off automatically. Test with DebugSkipWave.
