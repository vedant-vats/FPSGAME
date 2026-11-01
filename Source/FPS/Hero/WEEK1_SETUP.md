# Week 1 — GAS core setup

## 1. Files
Copy this `Hero` folder to `C:\FPS\Source\FPS\Hero\`.
(Assumes module name `FPS` / `FPS_API`. If your module is named differently, find-replace `FPS_API`.)

## 2. FPS.uproject — add to "Plugins"
```json
{ "Name": "GameplayAbilities", "Enabled": true }
```

## 3. Source/FPS/FPS.Build.cs — add to PublicDependencyModuleNames
```csharp
"GameplayAbilities", "GameplayTags", "GameplayTasks"
```
(EnhancedInput should already be there from the template.)

## 4. Build
Close editor → right-click FPS.uproject → Generate project files → Rider: FPSEditor / Development Editor → Build Startup Project.

## 5. Editor
1. Blueprint `BP_HeroCharacter` (parent: FPSHeroCharacter).
   - Mesh: template's 3P body mesh + anim BP. FirstPersonMesh: template FP arms.
   - Input: IMC_Default (or the template's), IA_Move, IA_Look, IA_Jump.
2. Data asset `DA_HeroInput` (FPSInputConfig). Leave empty for now (abilities come W4).
3. `BP_FirstPersonGameMode`: Default Pawn = BP_HeroCharacter, Player State = FPSPlayerState.
   Check the lobby map and the gameplay map both use this GameMode.
4. If the template PlayerController also adds mapping contexts, that's fine.

## 6. Done-check
1. Run `LaunchTwoGames.bat`, host + join.
2. Both windows show green lines: `YOU HP 100/100 SH 50/50` and the other player.
3. Window A: `~` → `DebugDamageOthers 30` → B shield 50→20, health unchanged.
4. Again `DebugDamageOthers 30` → B shield 0, health 90.
5. `DebugDamageOthers 200` → B dead (red, ragdoll/freeze) in BOTH windows, respawns after 3s at full HP/shield in BOTH windows.
6. `DebugDamageSelf 30` on the client also works (tests Server RPC from client).

Logs to grep if broken: `[Damage]`, `[Hero]` in Saved\Logs\Host.log / Client.log.
