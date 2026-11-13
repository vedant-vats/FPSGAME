# HUD restyle — design (approved Nov 13, 2026)

## Why
The canvas HUD looks AI-made: thick orange accent bars, black outlines on all text, full boxes, punchline jokes.
Goal: look designed, using the rules of Valorant's ability screen (reference screenshot, Nov 13). Canvas only.

## Goals
- Restyle the ability help screen (hold F1 / H).
- Clean up the in-game HUD (bars, ability boxes, objective panel, heat bar, text).
- Rewrite the text (panel = dry announcer, help screen = plain facts).

## Non-goals
UMG, background blur, icon art, new features, moving HUD elements.

## Anti-goals (fails if)
- It reads as a 1:1 Valorant clone (take the rules, not the art).
- It is harder to read in a fight.

## Approaches
- **A — canvas restyle (CHOSEN).** ~4–5 h. No blur, no icons, ~70% of the look.
- B — rebuild help screen + HUD in UMG. Best look, ~1.5 days. Dropped: time.
- C — help screen in UMG + canvas cleanup. ~1 day. Dropped: user chose A to keep cost low.

## Help screen
- Dim 0.75. Title "ABILITIES" white caps, no outline.
- One thin line across the cards, dots at the ends. White key badges (black text) sit on the line above each card.
- Cards: near-zero fill, 1 px grey border, cut top corners, small diamond at top centre.
- Name centred caps. Body small grey, left-aligned, coloured keywords: `<w>` white, `<r>` red (damage), `<t>` teal (stun); `</>` closes. Stats small grey at the bottom.
- Tip becomes a "TANKS" block, bottom left, thin left border.

## In-game HUD cleanup
- No thick outline; soft 1 px shadow only.
- Ability boxes: 1 px border, cut corners, same key-badge style.
- Objective panel: 1 px left line; objective white caps; taunt light grey; red only for urgent states (low HP, overheated).
- Heat bar thinner, white -> red, no orange.
- Palette: white, grey, red = danger, teal = shield/stun. One meaning per colour.

## Text
Panel (Objective / Taunt):
- SURVIVE 5 WAVES / Good luck with that.
- WAVE 1 / Warm-up. Don't embarrass yourself.
- WAVE 2 / Tanks now. Keep moving.
- WAVE 3 / You won't.
- WAVE 4 / Still alive. Odd.
- KILL THE BOSS / Big one. Bigger than you.
- BREAK / Short break. Use it.
- LOW HEALTH / Called it.
- OVERHEATED / Let go of the trigger.
- DOWN / As expected.
- VICTORY / Huh. Fine.
- DEFEAT / Told you.

Help screen:
- Rifle: Hitscan. Headshots deal <r>double damage</>. Hold the trigger too long and it <r>overheats</>.
- Dash (ability BP): Burst forward. Two charges. Dash sideways to dodge a Tank <r>charge</>.
- Shockwave (ability BP): Blast nearby enemies for <r>damage</> and <t>stun</> them. Cancels a Tank's <r>charge</> during its windup.
- TANKS block: A glowing Tank is about to charge in a straight line. Step sideways, or stun it.

## Left to the builder
Exact pixel sizes, line alpha values, diamond size.
