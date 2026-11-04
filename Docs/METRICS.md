# Measured results

## Hit registration with server rewind (Week 3, 2026-11-04)

Setup: listen server + 1 client on one PC, NetEmulation.PktLag 75 both sides
(measured RTT ~180 ms). Host strafes left/right, flipping every 1 s (DebugStrafe).
Client auto-aims at body hitbox and fires 10 shots/s, no spread/recoil (DebugAutoTest).
"Agreed" = server trace hit the same actor the client's local trace hit.

| Rewind | RTT | Shots | Client hits | Server agreed | Missed |
|---|---|---|---|---|---|
| OFF | 179 ms | 343 | 343 | 248 (72.3%) | 27.7% |
| ON  | 186 ms | 287 | 287 | 278 (96.9%) | 3.1% |

Result: missed hit registrations reduced ~9x.

Notes:
- Target is slow during direction flips, so OFF is flattering. Straight-line strafe would score lower.
- Remaining ON misses: likely direction flips + interpolation. Tune with fps.RewindOffsetMs (W7).

## Bandwidth (Week 7)
TODO: bytes/s per client with 50 AI, before/after.
