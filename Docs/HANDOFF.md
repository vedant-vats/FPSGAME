# FPS Portfolio Project — Handoff (updated Nov 13, 2026)

## Who / how to work with me
- Vedant Vats, UE gameplay dev (Helium Wars, India). Beginner-ish in C++, wants to learn the code.
- Replies: short, simple, blunt. Say when an idea is bad. Only agree to plans that fit the timeframe.
- **Grill mode ON:** any idea I propose → ask numbered questions with recommendations first, implement only after I confirm.
- Want everything optimized; measure before/after in Week 7.
- Style reference: Marvel Rivals (abilities), Marvel Rivals PvE / Marvel Zombies (waves).
- Claude can read/edit `C:\FPS\Source` directly via the desktop app (folder access granted). Plugins folder not connected.

## Project
- Co-op PvE hero shooter, UE 5.6 C++, `C:\FPS`, repo `github.com/Goat00000000000/FPSGAME` (main, LFS).
- **Deadline Jan 3, 2027.** Feature freeze Dec 20. ~10–12 h/week.
- Master plan doc: https://claude.ai/artifact/RFN37AnYoumpLhLRECMA8L
- Repo docs: `Docs/TECH_DECISIONS.md`, `Docs/METRICS.md`, `Docs/OPTIMIZATION_TODO.md`, `Docs/setup/WEEK1–6_SETUP.md`.
- Testing: `LaunchTwoGames.bat`, `LaunchTwoGames_Lag150.bat`.
- Git check commands (PowerShell): `git status`, `git ls-files "*.fbx"` (should be empty), `(git lfs ls-files | Select-String "uasset").Count` (346 on Nov 10).

## Done (weeks 1–6 code)
- **GAS:** ASC on PlayerState (Mixed) for heroes, on enemy (Minimal) for AI. Health/Shield/IncomingDamage (shield first). Damage GE with SetByCaller. Abilities by InputTag. Charges via stacking cooldown GE.
- **Rifle:** hitscan, spread, recoil, headshots, custom trace channel `Weapon`. Friendly fire off (`bAllowFriendlyFire=false`, test cvar `fps.FriendlyFire 1`) — verified in 2-window test.
- **Server rewind:** 1 s hitbox history, clock sync, interpolated rewind. **Result: 72.3% → 96.9% hits agreed at ~180 ms RTT (~9x fewer misses).**
- **Debug view:** green/blue ghost hitboxes, readout, test commands. `fps.NetDebug` defaults to 0. Debug tools compiled out of Shipping (`FPS_WITH_DEBUG_TOOLS`).
- **Abilities:** Dash (root motion, predicted, 2 charges), Shockwave (damage + stun, LOS).
- **Enemies:** runner + tank, pooled, dormant in pool, C++ AI controller on 0.3 s staggered timer, elite/boss tiers, ragdoll death.
- **Waves:** 5 waves, player scaling (`PlayerScale 0.5`), Victory/Defeat (polling every 0.5 s — deliberate, see interview line below), ServerTravel restart. GameState events for a UMG wave banner.
- **HUD:** canvas HUD, dynamic crosshair, hit marker, bars, cooldowns; enemy bars only for elite/boss.
- **Week 6 done-check ✅ (Nov 10):** Victory + Defeat + ServerTravel restart tested on host and client.

## Done Nov 10
- **Arena `L_Arena` blockout ✅** (`Content/LEVELS/L_Arena`, cubes only). 60×60 m, floor top at Z=0, North = +X.
  - Walls 6 m tall, 3 gates 6 m wide (N, W, E), spawn pockets 8×5 m behind each gate with Target Points at (3250,0,0), (0,-3250,0), (0,3250,0).
  - Platform (-1800,0,150) 6×12×3 m, 2 ramps (Roll ±26.6°), 7 low cover blocks (1 m tall), 4 PlayerStarts at X=-2500.
  - FPSEnemySpawner + FPSWaveManager copied from LOBBY, placed in North pocket (3450,300,100). NavMeshBoundsVolume scale 40.
  - CLM host Lobby Map Path = `/Game/LEVELS/L_Arena`. Playtest: no gate jams, platform OK.
- **Spawner:** `SpawnPoints` now round robin (was random) and skips empty slots. (`FPSEnemySpawner.h/.cpp`)
- **Tank bull rush** (`FPSEnemyCharacter.h/.cpp`, `FPSEnemyAIController.cpp`), fixes players kiting slow tanks (tank MoveSpeed 200):
  - Tick `bCanCharge` on tank BP. Trigger 6–15 m + LOS trace, 6 s cooldown, none in first 3 s after spawn.
  - Windup 0.4 s (stands still) → `OnChargeWindup` BP event (multicast reliable) → red overlay material `M_ChargeWarning` on the mesh.
  - Rush: locks target position, straight line at `ChargeSpeed` 1200 cm/s, `ChargeOvershoot` 300 cm, max 1.5 s. No homing (sidestep/Dash beats it).
  - Hit player = instant normal-attack damage. Hit wall/cover/enemy = 1 s stun (`WallStunDuration`). Shockwave stun cancels it.
  - Perf: actor tick enabled ONLY during the rush; MOVE_Walking during rush, back to NavWalking after.
- **Player hit reacts** (`FPSHeroCharacter.h/.cpp` + BP):
  - Server `ApplyHitReact(bHeavy, Knockback)` → `ClientHitReact` (unreliable, client also applies the launch to avoid snap) → BP event `OnHitReact(bHeavy)`.
  - Normal enemy hit = light (no shove). Charge hit = heavy + knockback (`ChargeKnockback` 600, `ChargeKnockUp` 250 on tank BP).
  - BP: camera shakes `CS_HitLight` / `CS_HitHeavy` (Perlin), `WBP_HitFlash` (Render Opacity anim `Flash`), created lazily on first hit, removed on End Play.
- **Assets:** KayKit Space Base Bits (CC0) imported to `Content/Enviroment/fbx` (typo folder name still there), material `M_SpaceBits` + atlas texture. Quaternius MegaKit imported but not used (walls one-sided, didn't like the look).

## Done Nov 11–13
- **Arena dress pass ✅:** bright golden-hour lighting (Lumen), neon strips (`M_Neon` + MI teal/orange/red, Affect Dynamic Indirect Lighting OFF to stop Lumen blotches), flat materials (`M_Flat` + `MI_Floor/Wall/Accent`), KayKit props over hidden cover cubes (cubes keep collision/nav), hidden `RampBlock_1/2` cubes stop tanks getting stuck under ramps.
- **Rifle overheat ✅:** 2%/shot (50 shots), cool after 0.3 s at 40%/s, 1.5 s lockout. Server tracks + rejects (1 shot slack, 0.8× lockout), client predicts. No tick. `fps.Overheat 0` on BOTH windows for rewind accuracy tests.
- **HUD ✅:** `HUDScale`/`TextScale`, `F_HUD` font (Runtime, size 14), F10 hides debug messages (dev only), mocking objective panel (dry announcer voice), hold F1/H ability help screen. Restyled Nov 13 to Valorant ability-screen *rules* (thin grey cut-corner frames, key badges on a line, diamonds, colour tags `<w>/<r>/<t>`, palette white/grey/red/teal, no orange, soft shadow text). `DrawSoftLine` because `AHUD::DrawLine` ignores alpha.
- **Difficulty tuned ✅.** Superpowers spec/plan/ledger for the HUD restyle in `Docs/superpowers/`.
- **Working rules (from Nov 12):** caveman chat (not code/site text), brainstorm → plan → execute → verify, karpathy code rules, blunt. Claude can't build: Vedant builds + tests before anything is "done".

## Next (in order)
0. **Nov 14:** first LinkedIn post (20–30 s wave clip on the new arena + help screen), then start code lessons (netcode files first).
- **WaveBanner DROPPED** (canvas wave text is restyled and matches the HUD).
- Then polish → ship (see polish order below).

## Polish order (proposed, grill each before building)
1. Tank rush feel: charge montage, impact sound + dust + camera shake on wall/player hit, dizzy during wall stun.
2. Bullet tracers + impact sparks (own Niagara beam, `BeamEnd` param, via `MulticastShotFX`, skip others' tracers beyond 50 m).
3. Enemy hit flash (overlay material trick) + ability VFX/sound (Dash, Shockwave).
4. TP blend space + orientation/stride warping, look-down fix.
5. Swap enemies to Quaternius Sci-Fi Essentials robots (CC0).
6. After polish: tank ground slam (~1 h). Stretch: Overdrive ult (AAA presentation).
Week 7 profiling still Dec 14–20, feature freeze Dec 20.

## Old next list (Nov 10, mostly done)
1. **Dress pass on `L_Arena`** (cap ends ~Nov 15): keep cubes for floor/walls/platform with better materials (ambientCG or KayKit colours), swap cover cubes for same-size KayKit props, lights (red at enemy gates). Layout must not change.
2. **WBP_WaveBanner** in UMG using GameState events (1-day cap) → BP child of FPSHUD with Wave Widget Class → set as GameMode HUD.
3. **Code lesson, 1 h this week** — start with the netcode files.
4. Solo difficulty tune to wave 5 (numbers on wave manager). Real 2-player test with a friend in Weeks 8–9.

## Still to do (later)
- **Polish buffer (early Dec):** charge VFX + roar sound (replace/extend red overlay), enemy hit flash/flinch, TP blend space + orientation/stride warping, look-down fix, ability VFX/sound, **bullet tracers** (own Niagara beam with `BeamEnd` param, every shot, others' tracers skipped beyond 50 m, via existing `MulticastShotFX`), impact sparks, **swap enemies to Quaternius Sci-Fi Essentials robots (CC0)** (overlay material keeps working).
- **Tank rush feel (polish buffer):** rush "feels incomplete" — add charge anim montage (not sped-up jog), impact sound + dust + nearby camera shake on wall/player hit, dizzy anim during the 1 s wall stun.
- **Tank ground slam (AFTER polish, ~1 h, keep the rush too):** proposed — player within 4 m, 0.6 s windup (red glow), 4 m radius AoE, damage ×1.5, light knockback 400, 8 s cooldown, shared "special" cooldown with the rush (no spam). Grill numbers before building.
- **Difficulty tuned ✅ (Nov 11):** `L_Arena` wave manager: waves 12/0, 18/3, 22/6 (elite 0.15), 28/8 (0.2), 26/10 (0.25) + boss; SpawnInterval 0.35, Intermission 7. Vedant (Valorant Immortal, i.e. far above average FPS skill) ended solo on 70 HP — fine for him, likely hard enough for average players. Before Jan: get an average player to try it; if they die by wave 3, lower counts ~20%. Backup if too easy in the friend test: runner AttackDamage 15 + AttackCooldown 1.0, tank AttackDamage 30. Later idea: shield recharge after 4 s no damage (~1 h).
- Optional 1-line fix: clamp `StartDelay`/`IntermissionTime` to ≥0.1 s in `FPSWaveManager.cpp` (a 0 s timer never fires). Reverted Nov 10 at user's request.
- Week 7: profiling (bandwidth, frame time, anim cost, charge tick cost), `fps.NetDebug 0` when measuring.
- Weeks 8–9: bugs, 2-player friend test, video, README, resume.
- Stretch: **Ultimate = Overdrive, AAA presentation** (picked Nov 10; build in Dec only after Must items pass, ~1 day). Proposed: charge from damage dealt (~1 per 1.5 waves), Q to activate, 8 s, fire rate ×1.5 + damage ×1.25, refill Dash/Shockwave once on activation. Feel = all 5: gun glow (overlay material), FOV punch, pulsing screen-edge widget, bass boom on start, higher-pitched gunshot during it. Grill these numbers before building. Other stretch: itch.io solo build, hero model swap (hero stays mannequin for now), Steam test.

## Interview lines
- **Wave defeat check:** "4 players max, so I poll twice a second. It's cheaper to reason about than death events and can't miss a disconnect. I'd switch to events if player count grew."
- **Tank charge:** "Players could kite tanks forever, so I added a telegraphed, non-homing bull rush. Dodgeable, punishes straight-line running, baiting it into cover stuns it. The tank only ticks during the rush."
- **Gotchas learned:** a 0 s timer never fires; BeginPlay can run before a client pawn has its controller (`IsLocallyControlled` false) — so the hit flash widget is created lazily in a client-only event.

## Rules / risks
- **Public repo licensing:** only CC0 assets go in git. Fab/Paragon assets stay out (.gitignore + README note). Raw FBX stay in `C:\Art Source`, only .uasset (LFS) in repo.
- Remove CLM_MenuSystem copyrighted placeholders (Overwatch, Halo, Fortnite music, screenshots) before going public.
- No Helium Wars code/assets; check contract.
- **Biggest risk: being able to explain the code in interviews.** Restart lessons (1 h/week min), starting with the netcode files.
- Live Coding off for big changes: close the editor before building after header changes or file moves/deletes.
