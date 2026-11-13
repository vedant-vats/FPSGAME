# HUD Restyle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the canvas HUD and the F1/H help screen look designed (Valorant ability-screen rules), and rewrite its text.

**Architecture:** All drawing stays in `AFPSHUD` (canvas). Add a few style primitives (cut-corner frame, diamond, key badge, rich text with colour tags), then rebuild the help screen and restyle existing HUD parts on top of them. Text defaults live in the `AFPSHUD` constructor; ability descriptions in the Dash/Shockwave Blueprints.

**Tech Stack:** UE 5.6 C++, `AHUD` canvas (`DrawLine`, `DrawRect`, `FCanvasTextItem`, `FCanvasTriangleItem`).

**Spec:** `Docs/superpowers/specs/2026-11-13-hud-restyle-design.md`

## Global Constraints
- Canvas only. No UMG, no blur, no icon textures, no new features, no HUD element moves.
- Must not read as a 1:1 Valorant clone; must not get harder to read in a fight.
- Palette: white, grey, red = danger, teal = shield/stun. One meaning per colour. No orange in the HUD.
- Text: no thick outline; soft 1 px shadow only.
- Text tags: `<w>` white, `<r>` red, `<t>` teal, `</>` closes. Exact copy as in the spec.
- Every size multiplies by `GetUIScale()`. Line thickness never below 1 px.
- No automated test framework exists in this project: "test" = build succeeds + the PIE check listed in the task. Claude cannot build; Vedant builds and reports.

## Review Focus
1. Long or edited description text overflows a help card -> must shrink/clip inside the card, never draw outside it. (Task 3 check: paste a 60-word description into Dash.)
2. Bad markup in a BP description (unclosed tag, unknown tag like `<x>`) -> renders as plain text, no crash. (Task 2 check.)
3. Very small/large resolution (720p / 4K, PIE window resize) -> lines stay >= 1 px, nothing overlaps. (Task 3 + 4 check.)
4. Empty HUDDescription on an ability -> card still draws, shows the fallback line. (Task 3 check.)
5. Long key labels ("SHIFT", "LMB") -> badge widens to fit, stays centred on the line. (Task 3 check.)

---

### Task 1: Style primitives + palette

**Files:**
- Modify: `Source/FPS/UI/FPSHUD.h`, `Source/FPS/UI/FPSHUD.cpp`

**Interfaces:**
- Produces (private on `AFPSHUD`):
  - `void DrawFrame(float X, float Y, float W, float H, float Cut, const FLinearColor& Color)` — 1 px border, top-left and top-right corners cut by `Cut`.
  - `void DrawDiamond(float CX, float CY, float Size, const FLinearColor& Fill, const FLinearColor& Line)` — square rotated 45°, filled (2 triangles) + 1 px outline.
  - `FVector2D DrawKeyBadge(const FString& Key, float CX, float CY, UFont* Font, float Scale)` — white filled rect, black text, centred on (CX, CY); returns badge size.
  - `float LineThickness1px() const` — `FMath::Max(1.f, GetUIScale())`.
- Produces (UPROPERTY, EditDefaultsOnly, Category "Style"): `TextColor` (1,1,1,1), `MutedColor` (0.7,0.7,0.72,1), `LineColor` (1,1,1,0.3), `DangerColor` (1,0.27,0.27,1), `StunColor` (0.3,0.85,0.85,1).
- Changes: `bTextOutline` default -> `false`; when false, `DrawHUDText` adds shadow only: colour (0,0,0,0.5), offset (1,1) px.

- [ ] **Step 1: Add the 5 colour properties and 4 functions above.**
- [ ] **Step 2: Build.** Expected: compiles, no warnings in FPSHUD.
- [ ] **Step 3: PIE check.** All HUD text has a thin shadow, no thick black outline.
- [ ] **Step 4: Commit** `HUD: style primitives + palette, soft text shadow`

### Task 2: Rich text with colour tags

**Files:** Modify `Source/FPS/UI/FPSHUD.h/.cpp`

**Interfaces:**
- Consumes: `MeasureHUDText`, `DrawHUDText` (existing), palette from Task 1.
- Produces: `float DrawRichText(const FString& Markup, float X, float Y, float MaxWidth, UFont* Font, float Scale, const FLinearColor& BaseColor)` — word-wraps to `MaxWidth`, draws each word in its tag colour, returns total height drawn.
- Replaces `WrapText` (delete it; no other user).

- [ ] **Step 1: Implement `DrawRichText`.** Approach: tokenise into (word, colour) pairs; a tag sets the colour until `</>`; unknown tags are dropped, unclosed tags run to the end; then lay out word by word with one space between, wrapping when the next word passes `MaxWidth`.
- [ ] **Step 2: Build.**
- [ ] **Step 3: PIE check (Review Focus 2).** Temporarily set Dash HUDDescription to `Test <r>red</> <x>bad</> <t>open`. Expect: "red" red, "bad" grey, "open" teal, no crash. Restore.
- [ ] **Step 4: Commit** `HUD: rich text with colour tags`

### Task 3: Help screen restyle

**Files:** Modify `Source/FPS/UI/FPSHUD.h/.cpp`

**Interfaces:**
- Consumes: Tasks 1–2.
- Changes properties: remove `HelpTip`; add `FText TanksTitle` ("TANKS") and `FText TanksText` (spec copy). `RifleDescription` default = spec rifle copy. `RifleStats` stays.
- Replaces `DrawHelpCard` body; keeps the same signature.

- [ ] **Step 1: Layout (exact values).** Background dim alpha 0.75. Title "ABILITIES" centred, `TextColor`, no outline. Cards: 3 × (300 × 220) * S, gap 40 * S, centred, top at 0.5 * ClipY − 110 * S. One horizontal line, `LineColor`, 1 px, from first card's left edge − 20 * S to last card's right edge + 20 * S, at card top − 34 * S, with 3 * S dots at both ends. Key badge centred on the line above each card's centre. Card: fill (0,0,0,0.25), `DrawFrame` with Cut 14 * S in `LineColor`; `DrawDiamond` size 22 * S centred on the card's top edge (fill (0.06,0.06,0.09,1), line `LineColor`). Name centred caps at card top + 30 * S. Description via `DrawRichText`, `MutedColor`, left pad 18 * S. Stats bottom-left, `MutedColor`, scale 0.55.
- [ ] **Step 2: Overflow guard (Review Focus 1).** If the description's height would pass the stats line, retry `DrawRichText` with scale × 0.85, up to 3 times; then clip (stop drawing lines past the stats line).
- [ ] **Step 3: TANKS block.** Bottom-left at (card row left, card bottom + 50 * S): 1 px vertical `LineColor` line on its left, `TanksTitle` white caps, `TanksText` via `DrawRichText`, width 600 * S.
- [ ] **Step 4: Build + PIE check.** Hold H. Expect: line, badges, diamonds, cut corners, coloured keywords, TANKS block. Review Focus checks 1, 3 (resize PIE to ~720p), 4 (blank Shockwave description → fallback line), 5.
- [ ] **Step 5: Commit** `HUD: help screen restyle`

### Task 4: In-game HUD cleanup

**Files:** Modify `Source/FPS/UI/FPSHUD.h/.cpp`

**Interfaces:** Consumes Task 1.

- [ ] **Step 1: Ability boxes.** Replace the 4 border lines with `DrawFrame` (Cut 8 * S): `TextColor` when ready, `LineColor` on cooldown. Key text -> `DrawKeyBadge` at the box's top-left corner. Cooldown fill colour (1,1,1,0.15). Pips: `TextColor` ready, `LineColor` empty. `AbilityReadyColor` default -> white (1,1,1,0.95).
- [ ] **Step 2: Objective panel.** Left line 1 px `LineColor` (was 3 px orange). Objective `TextColor` caps; taunt `MutedColor`. If the line is `LowHealth` or `Overheated`: objective + line in `DangerColor`. Delete `TauntColor`, `ObjectiveColor` properties (no other users).
- [ ] **Step 3: Heat bar.** Height 3 * S (was 6). Colour lerp `TextColor` -> `DangerColor` by heat; overheated = `DangerColor`. Delete `HeatColor`, `OverheatColor`.
- [ ] **Step 4: Bars.** `HealthColor` default (0.9,0.9,0.92,0.95) (white-ish); low health uses `DangerColor`. `ShieldColor` = `StunColor` hue (0.3,0.85,0.85,0.95). Enemy elite name colour -> `MutedColor`, boss -> `DangerColor`.
- [ ] **Step 5: Build + PIE check.** No orange anywhere; red only at low HP / overheated / boss; 720p resize check (Review Focus 3).
- [ ] **Step 6: Commit** `HUD: in-game cleanup, new palette`

### Task 5: Text rewrite

**Files:**
- Modify: `Source/FPS/UI/FPSHUD.cpp` (constructor)
- Blueprints (Vedant, in editor): Dash and Shockwave ability BPs -> HUD Description.

- [ ] **Step 1: Constructor defaults = spec panel lines** (13 pairs, exact copy). Rename objectives: Intermission line objective "BREAK", Dead "DOWN", Victory "VICTORY", Defeat "DEFEAT", LowHealth "LOW HEALTH", Overheated "OVERHEATED", waves "WAVE 1".."WAVE 4", wave 5 "KILL THE BOSS".
- [ ] **Step 2: Build.** In `BP_FPSHUD` Details, right-click each Objective/Help property -> Reset to Default if it shows a changed value.
- [ ] **Step 3: Set Dash + Shockwave HUD Description** to the spec copy (with tags).
- [ ] **Step 4: PIE check.** Play a wave, overheat, drop to low HP, die, hold H: every line matches the spec.
- [ ] **Step 5: Commit** `HUD: rewrite panel + help text`
