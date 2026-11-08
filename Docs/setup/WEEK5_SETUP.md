# Week 5 — Enemies (runner + tank), pooled spawner

## What's new (C++)
- FPSEnemyCharacter: pooled enemy. ASC on itself (Minimal replication), capsule hitboxes,
  NavMesh Walking, melee attack with windup (dodgeable), stun stops it, ragdoll death, back to pool.
- FPSEnemyAIController: chase + attack, thinks every 0.3 s (staggered). No BT/StateTree.
- FPSEnemySpawner: pre-spawns the pool at BeginPlay (hidden + dormant), hands enemies out.
- FPSCombatantInterface: heroes + enemies die through the same damage code.
- Hero: `DebugSpawnEnemies N`, `DebugKillEnemies`, "ENEMIES alive" line on screen.
- Build.cs: + NavigationSystem.

## 1. Build
Close editor → Generate project files (Build.cs changed) → Rider → Build Startup Project.

## 2. Enemy Blueprints (Content/BLUEPRINTS/Enemies/)
**BP_Enemy_Runner** (parent FPSEnemyCharacter)
- Mesh: SKM_Manny_Simple (or Quinn), Anim Class: ABP_Unarmed.
  Mesh transform like the hero: Location Z -96 (or -90), Rotation Z -90.
- Class Defaults → Enemy|Stats: Max Health 60, Move Speed 550
- Enemy|Attack: Damage 10, Range 150, Windup 0.35, Cooldown 1.0
- Check HeadHitbox sits on the head, BodyHitbox covers the body (move/resize in viewport).

**BP_Enemy_Tank** (parent FPSEnemyCharacter)
- Same mesh/anim. Actor scale 1.3 (or capsule bigger + mesh scale 1.3).
- Max Health 300, Move Speed 280
- Attack: Damage 30, Range 200, Windup 0.6, Cooldown 1.8
- Re-check hitboxes after scaling.

(Paragon Minions can replace the mesh/anim later. Then set Attack Montage too.)

## 3. Level (your test map / LOBBY)
1. Place Actors → **Nav Mesh Bounds Volume**, scale it to cover the whole floor. Press **P** in the viewport:
   floor should turn green. No green = enemies won't move.
2. Place Actors → search **FPSEnemySpawner**, drop it in the middle.
   Details → Pools: + BP_Enemy_Runner (Pool Size 30), + BP_Enemy_Tank (Pool Size 20).
   Spawn Radius 1500 (or add Target Points to Spawn Points).
3. Save the map.

## 4. Done-check (LaunchTwoGames_Lag150.bat)
1. Host log: `[Spawner] Pool ready: 50 enemies`. Nobody visible yet.
2. HOST console: `DebugSpawnEnemies 20` → 20 appear on BOTH screens, ENEMIES alive 20.
3. They run at the nearest player, stop in range, hit after the windup (shield/HP drops).
4. Run away during the windup → the hit misses.
5. Shoot them: hit markers, headshots on the head sphere, they ragdoll and vanish after 2 s.
6. Shockwave a group: they get knocked back + stand still 1.5 s.
7. CLIENT shooting runners at ~180 ms: hits still register (rewind works on enemies).
8. `DebugKillEnemies`, then `DebugSpawnEnemies 50` → pool reused, no "[Spawner] Pool empty" warning.
9. `fps.NetDebug 1` while shooting enemies: green/blue boxes line up on them too.

Note: players can still shoot / shockwave each other (testing flags). Turned off in Week 6.
