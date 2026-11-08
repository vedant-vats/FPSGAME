# Tech decisions (researched 2026-11-06)

Goal: 1-4 players + 50 AI enemies, listen server, smooth at ~180 ms RTT, cheap on server CPU and bandwidth.
Rule: build clean in W5-6, measure + tune in W7 (keep a "before" number for every change).

## 1. Movement: keep CharacterMovementComponent (CMC)
- Mover plugin is still Experimental (Epic docs: APIs/data formats may change). Not worth the risk on a 9-week project.
- Enemies: CMC in **NavMesh Walking** mode (no floor sweeps; Paragon ran ~200 characters with it).
- Enemy CMC: bigger NetUpdate interval, no crouch/swim, no physics interaction.
- W7 option: throttle CMC tick for far enemies (custom CMC), only if Insights shows it matters.

## 2. Enemy AI logic: C++ AIController with a timer, no BT/StateTree
- Our AI is "chase + attack in range". A tree adds overhead for nothing.
- Benchmarks disagree: one UE 5.7 test (400 NPCs, default settings) had StateTree ~18x costlier than BT;
  other tests say StateTree wins if it's event-driven and doesn't tick. Avoid the question: no tree.
- Controller decides every 0.25-0.5 s (staggered per enemy so they don't all think on the same frame):
  re-path (MoveToActor) + "in range -> attack". Zero per-frame logic.
- Stun = controller checks State.Stunned tag and stops (StopMovement), resumes when tag goes.

## 3. GAS on enemies: ASC on the enemy pawn, Minimal replication
- Minimal: gameplay effects never replicate, only tags, attributes and cues. Mixed is for players only.
- Owner = Avatar = enemy (pooled enemies never get destroyed, so no need for a PlayerState-style owner).
- Reuse UFPSAttributeSet (shield = 0 for enemies) so damage/stun code is the same path as players.

## 4. Enemy hitboxes: attached to the CAPSULE, not to bones
- Then the server never needs enemy animation for hit detection -> server can skip anim on enemies.
- Head sphere at a fixed offset on the capsule. Dumb chase AI = head position is stable enough.
- Lag comp works the same (it records whatever hitboxes exist).
- Players keep the bone-attached head (they're only 4).

## 5. Animation cost (50 skeletal meshes)
- **Animation Budget Allocator** plugin: mesh component class = SkeletalMeshComponentBudgeted.
  Global ms budget; throttles/interpolates far meshes and can stop their tick (URO can't).
- Enemy mesh settings: URO on, no morph targets, no per-bone motion blur, fixed skel bounds,
  VisibilityBasedAnimTickOption = OnlyTickMontagesWhenNotRendered.
- Hidden FP mesh on non-owners: don't animate (OptimizationTODO).

## 6. Replication
- Stay on the default replication system. **Iris is still Experimental** (lives under Runtime/Experimental).
  W7 bonus experiment only: flip Iris on, measure, compare. Don't depend on it.
- Enemies: NetUpdateFrequency ~10-20 (min 2), NetCullDistanceSquared sized to the arena,
  adaptive net update frequency (net.UseAdaptiveNetUpdateFrequency).
- **Dormancy**: NOT for moving pawns (Epic docs: frequently-updating actors like pawns don't benefit).
  YES for pooled, inactive enemies: DORM_DormantAll while in the pool, wake on spawn.
- Push model for our own replicated properties (bIsDead etc.) - cheap to add.

## 7. Spawning: pool
- Wave spawner pre-spawns enemies at match start (hidden, dormant, no tick, no collision).
- "Spawn" = teleport + reset attributes + wake. "Die" = return to pool. No SpawnActor hitches mid-wave.

## 8. How we measure (W7)
- Unreal Insights: -trace=cpu,frame,net -NetTrace=1 (server frame ms, bytes/s per connection).
- In-game: stat net, stat game, stat anim, a.Budget.Debug.Enabled 1.
- Always fps.NetDebug 0 while measuring.

## Sources
- Mover status: https://dev.epicgames.com/documentation/en-us/unreal-engine/mover-in-unreal-engine
- Iris: https://dev.epicgames.com/documentation/en-us/unreal-engine/introduction-to-iris-in-unreal-engine
- Dormancy: https://dev.epicgames.com/documentation/unreal-engine/actor-network-dormancy-in-unreal-engine
- Anim Budget Allocator: https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-budget-allocator-in-unreal-engine
- URO vs budget allocator: https://forums.unrealengine.com/t/uro-vs-animation-budget-allocator/2509506
- NavMesh walking / mesh settings: https://forums.unrealengine.com/t/how-to-gain-more-performance-with-100s-of-ai-actors/635539
- StateTree vs BT benchmarks: https://forums.unrealengine.com/t/statetree-performance-vs-behaviortree/2705328
- GAS replication modes: https://gas-reference.pages.dev/optimization/replication-optimization/

## 9. Third-person locomotion: blend space + Animation Warping (NOT GASP) — decided 2026-11-07
- 2D blend space (Direction x Speed) from the template rifle jogs (Fwd/Bwd/Left/Right/diagonals).
- + Orientation Warping + Stride Warping (built-in Animation Warping plugin): no foot sliding, body turns to move dir.
- Driven by replicated velocity -> same on every client. Cheap enough for enemies too.
- GASP / motion matching rejected: high CPU per character (bad with 50 AI), 1-2 days + network risk, big repo size.
- When: early December buffer, after W5-6. Fallback if late: plain blend space only (~2-3 h).
