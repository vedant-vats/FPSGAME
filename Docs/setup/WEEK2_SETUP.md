# Week 2 — Hitscan rifle setup

## What changed
- New: FPSWeaponComponent (fire, spread, recoil, server check), FPSGA_FireWeapon (hold to fire), FPSCollision.h
- Hero: template-style FP camera (on FP mesh head bone), rifle meshes, Head + Body hitboxes, weapon component
- FPSDamageEffect::ApplyDamage helper (all damage goes through it)
- DefaultEngine.ini: new trace channel "Weapon" (GameTraceChannel2)

## 1. Build
Close editor → Rider → Build Startup Project (FPSEditor / Development Editor).

## 2. BP_HeroCharacter
Components:
- FirstPersonMesh: Skeletal Mesh = SKM_Manny_Simple (or Quinn), Anim Class = ABP_FP_Weapon.
  MUST be set: the camera sits on its "head" bone. Empty = camera at your feet.
- Mesh (body): Anim Class = ABP_TP_Rifle.
- WeaponMeshFP + WeaponMeshTP: Skeletal Mesh = SKM_Rifle.
- HeadHitbox / BodyHitbox: check in viewport they cover head / body. Move/resize if not.

Class Defaults:
- Input → Additional Mapping Contexts: add IMC_Weapons
- Input → Mouse Look Action: IA_MouseLook
- GAS → Default Abilities: add FPSGA_FireWeapon
- WeaponComponent (select it): FX → Fire Montage FP = FP_Rifle_Shoot_Montage, Fire Sound = FirstPersonTemplateWeaponFire02

## 3. DA_HeroInput
Ability Input Actions: add entry → Input Action = IA_Shoot, Input Tag = InputTag.Fire

## 4. Done-check (LaunchTwoGames.bat)
1. Hold LMB: full auto, ~10 shots/s, camera kicks up, shots spread more the longer you hold.
2. Green line = your shot (client). Red line = server's shot. Yellow = other player's shot.
3. Shoot the other player in the body: "HIT 20", their shield/HP drops in both windows.
4. Shoot the head: "HEADSHOT 40".
5. Kill them: "KILL", they respawn.
6. Shoot a wall: no hit message.
7. Fire while running/jumping: much wider spread.

Note: at 0 ping, green and red lines match. In Week 3 we add fake lag:
red will miss moving targets. That gap is what rewind fixes.

Logs: `[Weapon]` in Saved\Logs\Host.log / Client.log.
