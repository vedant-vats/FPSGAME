# Optimization backlog (Week 7: measure before -> change -> measure after)

Rule: record a baseline number first (Unreal Insights / stat net / net.Profile), then change ONE thing, re-measure.
Set `fps.NetDebug 0` when measuring bandwidth (debug boxes ride on every shot result).

## Netcode / CPU
- [ ] Lag comp history: TArray RemoveAt(0) every frame -> ring buffer.
- [ ] Rewind: TActorIterator + FindComponentByClass per shot -> registered list + skip pawns far from the ray.
- [ ] Hidden FP mesh still animates on non-owners -> OnlyTickPoseWhenRendered / disable on simulated proxies.
- [ ] Body mesh AlwaysTickPoseAndRefreshBones everywhere -> only on server (hitboxes); clients when rendered.
- [ ] PlayerState NetUpdateFrequency 100 -> adaptive (min/max) frequency.

## AI (after Week 5)
- [ ] AI NetUpdateFrequency + NetCullDistanceSquared.
- [ ] Dormancy for idle/dead enemies, or Iris.
- [ ] Animation Budget Allocator / URO for 50 skeletal meshes.
- [ ] CharacterMovement settings for AI (simulated proxies, floor checks).
- [ ] Spawner pooling (built in W5, measure spawn hitch).

## Done (no measurement needed)
- [x] Lag comp ticks only on server + owning client.
- [x] Weapon ticks only on owning client.
- [x] Hit/damage logs moved to Verbose.
- [x] Debug readout compiled out of Shipping.
