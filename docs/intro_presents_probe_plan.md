# Intro Presents Layout Lab Plan

This note tracks the "TECMO PRESENTS" target without committing original screenshots, ROM bytes, ASM dumps, extracted graphics, or generated tile/sprite caches.

## Current Native Lab

The launcher has an `Intro Lab` button and a matching ignored screenshot test:

```powershell
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-presents build\intro_presents_test.png
```

There is also a read-only helper-model screenshot that explains the native `$C051 -> $D861` contract without using local payload bytes:

```powershell
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-c051-d861-model build\intro_c051_d861_model_test.png
```

That render mode first runs `tecmo_intro_stage_self_test`, which checks synthetic record staging, byte wraparound, capacity limits, null-input behavior, and table-selected 8x16 pair derivation before writing the PNG.

To compare against the original intro locally, detect a rebuilt NES ROM and emulator candidate without committing private paths:

```powershell
.\tools\Find-NesReferenceIntro.ps1
```

Use `-Launch` on that script when you want to manually open the local ROM in the discovered emulator.

The current frame is asset-backed and bank/table-switchable. It defaults to Bank 31 table 0 because that is the verified title glyph path. Table 0 shows tile IDs `$000-$0FF`; table 1 shows `$100-$1FF`. The frame renders the selected local-only CHR sheet, lets the user place selected source tiles onto a target canvas, and shows the local placement records on screen. The first rabbit and `TECMO` pointer leads are now decoded into ignored local reports; the live CHR bank, exact palette, and runtime scroll/base state still need emulator/state validation before replacing the manual lab presets.

Intro Lab builder controls:

- `Q/E`: switch CHR bank.
- `T`: switch table 0/table 1.
- `Tab`: switch focus between the source sheet and target canvas.
- `Arrows`: move the focused source-tile cursor or canvas-cell cursor.
- `Space`: record the selected tile at the selected canvas cell.
- `R`: record the Bank 31/table 1 rabbit lookup candidate as 8x16 sprite pairs `$124-$12B`.
- `M`: record the visual Bank 31/table 1 `TECMO` logo candidate tiles `$180-$193`.
- `C`: record the current composite candidate with the rabbit lookup group placed beside the `TECMO` visual group.
- `Backspace/Delete`: remove the last placement record.
- `S`: write ignored `build/intro_layout_picks.json`.

The on-screen record list shows the asset picked as bank, table, three-digit tile ID, and canvas cell. The saved JSON stores only those local placement facts, not CHR bytes, palette values, screenshots, copied script payloads, or ASM.

Current rabbit-head trace:

- User visual inspection points to Bank 31/table 1 rabbit-head parts near `$125-$127`, with adjacent candidate parts near `$129-$12B`.
- Bank 04 `L88E7` seeds the stream pass that reaches fixed helper `$C051`, which trampolines to `$D861`.
- Bank 04 `L8988` emits two `$A7DB` streams in pass order `X=1` then `X=0`. The seeded rabbit pass is selector `$01`, pointer entry `$A7DD`, stream pointer `$A8C0`, with 16 records. Selector `$00` is a partner stream emitted by the same helper.
- `$D861` stages 4-byte sprite records and adds the `$0D` tile offset. For the Bank 04 seeded pass, the local lookup resolves a full 16-record stream; the visible head/upper subset includes OAM tile lows `$25`, `$27`, `$29`, and `$2B`.
- In NES 8x16 sprite terms, those odd OAM tile IDs imply table-1 8x8 pairs `$124/$125`, `$126/$127`, `$128/$129`, and `$12A/$12B`. The native `R` preset now runs through a small C-side OAM-tile-pair helper before placing that inspection subset.
- `tools/Find-IntroRabbitLookup.ps1` writes the ignored local report `build/intro_rabbit_lookup.json` with the decoded selector and record summary.
- `intro-c051-d861-model` runs the native synthetic helper self-test and renders the same helper contract as a screenshot-tested schematic with no CHR or decoded stream payload.

Current `TECMO` logo visual trace:

- CHR Playground visual inspection points to Bank 31/table 1 tiles `$180-$193`.
- Bank 04 `L8818` is now the stronger execution lead for that logo range. It reaches fixed helper `$C051/$D861` through the Bank 0 `$A90F` pointer table, selector `$00`.
- The local lookup resolves ten OAM tile lows in `$80-$93`; interpreted as NES 8x16 sprites, those cover table-1 tile pairs `$180/$181` through `$192/$193`.
- The `$A90F` pointer table infers five entries. Selector `$00` has 46 records and 10 logo-range hits; selectors `$01-$04` have no logo-range hits in the current trace. `L8645` is the separate selector `$01` consumer and remains a useful follow-on lead for intro setup/loop/transition placement.
- `tools/Find-IntroTecmoLogoLookup.ps1` writes the ignored local report `build/intro_tecmo_logo_lookup.json` and verifies that the rabbit `$A7DB` table has zero `$80-$93` hits for selectors `$00-$03`.
- The native `M` preset lays out `$180-$193` as a visual candidate until that local decoder drives placement directly.

Composite local trace:

```powershell
.\tools\Find-IntroCompositeTrace.ps1
.\tools\Find-IntroCompositeTrace.ps1 -ChrBank 31
```

This writes ignored `build/intro_composite_trace.json` and `build/intro_composite_trace_preview.png`. The report includes row-level decoded local data and the preview renders CHR-derived graphics, so both must stay out of Git. Use `-ChrBank` to render the same decoded pointer rows against another candidate CHR bank while checking the live bank context.

## Current CHR Playground

The launcher has a `CHR Playground` button and a matching ignored screenshot test:

```powershell
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode chr-playground build\chr_playground_test.png
```

The playground renders selected-bank/table tile IDs `$080-$0AF` or `$180-$1AF`, the assembled 2x2 title glyphs, and sample character-to-tile mappings so number/letter construction can be visually checked. Use Left/Right or Tab to switch banks and Up/Down to toggle table 0/table 1 in the running app.

## How To Point Out Tiles

Use the `Intro Lab` screen to identify the selected bank, table, and source tiles by three-digit tile ID. For example, Bank 12 table 1 row `B`, column `6` is tile `$1B6`. Use the target canvas grid on the right to describe placement by 16px offsets from the canvas top-left.

For the current rabbit pass, press `R` in Intro Lab to place the Bank 31/table 1 lookup-derived candidate group onto the canvas. For the `TECMO` letters, press `M` to place the visual `$180-$193` group. Press `C` to place both groups together as the current intro composite candidate. Adjust or remove records as needed before pressing `S`.

Create an ignored local-only draft for your picks, or use the running Intro Lab and press `S`:

```powershell
.\tools\New-IntroLayoutDraft.ps1 -Bank 12 -Table 1
```

## Source Candidates

- Bank 04 `C-0116..C-0134`: intro/title splash setup, loops, pattern records, transition sequences, stream helpers, and structured intro/presentation script tables.
- Bank 04 `C-0136..C-0140`: early intro flow, wait/dispatch prelude, sequence driver, and dispatch tables.
- Bank 00 `C-0191`: encoded `PRESENTS` text/layout material.
- Bank 00 `C-0192`: encoded `TECMO` text/layout material and likely glyph/tile-run material.
- Bank 00 `C-0195`: likely mascot/composite tile block candidates.
- Bank 03 `C-0142..C-0143`: initial intro dispatch and selection loop.
- Fixed helper path `$C051 -> $D861`: likely stream-to-OAM-style sprite staging helper, with `$0200` as the staged sprite range and `$058D` as a sprite-count style state variable.

## Next Decode Gate

Capture the Bank 04 runtime state for the "TECMO PRESENTS" frame, especially `$88` at `L8818`, live CHR bank/table, sprite-size mode, and palette selection. Then model `$C051/$D861` as a native sprite/tile composition helper without committing extracted CHR, OAM, palettes, or nametable data.

Use the local NES reference detector plus a manual emulator launch to compare timing and layout against the original while the native helper is expanded.
