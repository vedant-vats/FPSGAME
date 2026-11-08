# Week 4 — Dash, Shockwave + stun, HUD

## What's new (C++)
- FPSGA_Dash: root-motion dash (predicted), 2 charges, FOV punch, shake, sound, VFX.
- FPSGA_Shockwave: instant pulse, damage + stun (+ knockback on AI), shake, sound, VFX.
- FPSGameplayAbility: charges support (stacking cooldown GE), HUD name/key.
- Tags: State.Stunned, Cooldown.Dash, Cooldown.Shockwave. Stunned = no move/jump/shoot/abilities.
- HUD: shield + health bars (smooth + white "damage taken" ghost), ability boxes with cooldown + charges, STUNNED text.
- Build.cs: + Niagara.

## 1. Build
Close editor → Generate project files (Build.cs changed) → Rider → Build Startup Project.

## 2. Input
1. Content Browser → right-click → Input → Input Action: `IA_Dash` (Value Type: Digital bool).
2. Same: `IA_Shockwave`.
3. Open IMC_Default → add `IA_Dash` = Left Shift, `IA_Shockwave` = E.
   (IMC_Weapons also maps Left Shift to IA_SwapWeapon. Unused, harmless. Remove that row if you want.)
4. DA_HeroInput → Ability Input Actions → add:
   - IA_Dash → InputTag.Ability.Dash
   - IA_Shockwave → InputTag.Ability.Area

## 3. Gameplay Effects (right-click → Blueprint Class → GameplayEffect)
Put them in Content/BLUEPRINTS/GAS/.

**GE_Cooldown_Dash**
- Duration Policy: Has Duration, Scalable Float Magnitude: 4
- Stacking: Stacking Type = Aggregate by Target, Stack Limit Count = 2,
  Stack Duration Refresh Policy = Never Refresh,
  Stack Expiration Policy = Remove Single Stack and Refresh Duration
- Components → + "Grant Tags to Target Actor" → Add to Inherited: Cooldown.Dash

**GE_Cooldown_Shockwave**
- Has Duration, 10
- Components → Grant Tags to Target Actor: Cooldown.Shockwave

**GE_Stun**
- Has Duration, 1.5
- Components → Grant Tags to Target Actor: State.Stunned

## 4. Ability Blueprints (right-click → Blueprint Class → search parent)
**GA_Dash** (parent FPSGA_Dash)
- Cooldowns → Cooldown Gameplay Effect Class = GE_Cooldown_Dash
- Max Charges = 2 (must match the GE stack limit)
- Dash|Feel: Dash Sound, Dash VFX, Camera Shake (optional, fill later)

**GA_Shockwave** (parent FPSGA_Shockwave)
- Cooldown Gameplay Effect Class = GE_Cooldown_Shockwave
- Shockwave → Stun Effect = GE_Stun
- Shockwave|Feel: sound, VFX, camera shake (fill later)

## 5. BP_HeroCharacter
GAS → Default Abilities: add GA_Dash, GA_Shockwave (keep FPSGA_FireWeapon).

## 6. Camera shakes (optional, nice)
Right-click → Blueprint Class → CameraShakeBase:
- CS_Dash: Root Shake Pattern = Perlin Noise, Duration 0.15, small rotation (pitch/yaw amplitude ~0.5)
- CS_Shockwave: Perlin Noise, Duration 0.35, location Z amplitude ~3, rotation ~1.5
Plug into GA_Dash / GA_Shockwave.

## 7. Done-check (LaunchTwoGames_Lag150.bat, ~180 ms RTT)
1. HUD: bars bottom-left, DASH + SHOCKWAVE boxes bottom-right.
2. Shift while moving: fast burst that way. Standing still: forward. In air: works, no gravity during dash.
3. Dash twice: both pips empty, box shows seconds. Pips refill one by one (4 s each).
4. CLIENT window at 180 ms: dash feels instant, NO snap back. (That's the root motion prediction.)
5. E near the other player (within 6 m): they lose 40 (shield first), show STUNNED 1.5 s, can't move/shoot/jump/dash.
6. E again right away: blocked, box shows ~10 s countdown.
7. E through a wall: no hit.
8. Take damage: bar slides, white ghost chunk drains after.
