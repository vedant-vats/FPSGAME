# Week 3 — Server rewind (lag compensation) + debug view

## What's new
- FPSLagCompensationComponent (on hero): server records hitbox history (1 s), client syncs clock (RTT).
- Weapon: sends "server time I saw" with each shot. Server rewinds hitboxes, traces, restores.
- FPSNetDebug: console variables (below).
- Hero: net readout (RTT, ping, loss, hit stats), test commands.
- LaunchTwoGames_Lag150.bat: two windows with ~150 ms fake round trip.

## 1. Build
Close editor → Rider → Build Startup Project. No editor setup needed.

## 2. Console commands (~ key)
| Command | Where | What |
|---|---|---|
| `fps.Rewind 0/1` | HOST window | lag compensation off/on |
| `fps.NoDamage 1` | HOST window | hits count, nobody dies (long tests) |
| `fps.NetDebug 0/1` | any | lines + ghost hitboxes |
| `DebugStrafe 1` | target window | auto strafe left/right |
| `DebugAutoTest 1` | shooter window | auto aim + fire at nearest hero |
| `DebugResetStats` | shooter window | zero the stats |
| `NetEmulation.PktLag 75` | both | 75 ms each way (if bat didn't apply) |

## 3. Reading the debug view
- GREEN boxes: where YOUR screen showed the target when you fired.
- BLUE boxes: where the SERVER had the target's hitboxes for that shot.
- Rewind ON + good sync = green and blue overlap.
- Rewind OFF at 150 ms = blue is ahead of green (target already moved on the server).

## 4. The measurement (resume number)
1. Run LaunchTwoGames_Lag150.bat. Host + join.
2. Check the CLIENT window readout: `RTT ~150 ms`. If ~0, type `NetEmulation.PktLag 75` in both windows.
3. HOST window: `fps.NoDamage 1`, then `DebugStrafe 1` (host is the moving target).
4. CLIENT window: walk ~10 m away, face the host, `DebugAutoTest 1`.
5. HOST: `fps.Rewind 0`. CLIENT: `DebugResetStats`, wait ~30 s (~300 shots). Write down "Server agreed %".
6. HOST: `fps.Rewind 1`. CLIENT: `DebugResetStats`, wait ~30 s. Write down "Server agreed %".
7. CLIENT: `DebugAutoTest 0`.

Expected: rewind OFF = low (target moved ~1 m in 150 ms), rewind ON = 90%+.
If ON is still low: watch green vs blue. Blue behind green → lower `fps.RewindOffsetMs`
(can go negative). Blue ahead → raise it. Re-measure.

Send me both numbers + a screenshot of green/blue boxes.
