# Intro Presents Layout Lab Plan

This note tracks the "TECMO PRESENTS" target without committing original screenshots, ROM bytes, ASM dumps, extracted graphics, or generated tile/sprite caches.

## Current Native Lab

The launcher has an `Intro Lab` button and a matching ignored screenshot test:

```powershell
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-presents build\intro_presents_test.png
```

The current frame is asset-backed and bank/table-switchable. It defaults to Bank 31 table 0 because that is the verified title glyph path. Table 0 shows tile IDs `$000-$0FF`; table 1 shows `$100-$1FF`. The frame renders the selected local-only CHR sheet, lets the user place selected source tiles onto a target canvas, and shows the local placement records on screen. The exact intro bank/table, mascot/logo sprite layout, and palette path are not yet decoded from the original intro script streams.

Intro Lab builder controls:

- `Q/E`: switch CHR bank.
- `T`: switch table 0/table 1.
- `Tab`: switch focus between the source sheet and target canvas.
- `Arrows`: move the focused source-tile cursor or canvas-cell cursor.
- `Space`: record the selected tile at the selected canvas cell.
- `R`: record the current Bank 31/table 1 rabbit-head candidate tiles `$125`, `$126`, `$127`, `$129`, `$12A`, and `$12B`.
- `Backspace/Delete`: remove the last placement record.
- `S`: write ignored `build/intro_layout_picks.json`.

The on-screen record list shows the asset picked as bank, table, three-digit tile ID, and canvas cell. The saved JSON stores only those local placement facts, not CHR bytes, palette values, screenshots, copied script payloads, or ASM.

Current rabbit-head trace:

- User visual inspection points to Bank 31/table 1 tiles `$125-$127` for head parts, with `$129-$12B` adjacent candidate parts.
- The local ASM scan found no full `$125-$12B` immediates in the checked Bank 00/04 chunks. It did find low-byte `$25-$2B` candidates in Bank 00 stream/data chunks, which matches the working model that the script emits low tile IDs while the active CHR table supplies the `$100` half.
- Bank 04 intro helpers still route through the fixed `$C051` staging helper path for display construction. The native `R` preset is a temporary C-side construction aid until that stream-to-sprite helper is fully modeled.

## Current CHR Playground

The launcher has a `CHR Playground` button and a matching ignored screenshot test:

```powershell
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode chr-playground build\chr_playground_test.png
```

The playground renders selected-bank/table tile IDs `$080-$0AF` or `$180-$1AF`, the assembled 2x2 title glyphs, and sample character-to-tile mappings so number/letter construction can be visually checked. Use Left/Right or Tab to switch banks and Up/Down to toggle table 0/table 1 in the running app.

## How To Point Out Tiles

Use the `Intro Lab` screen to identify the selected bank, table, and source tiles by three-digit tile ID. For example, Bank 12 table 1 row `B`, column `6` is tile `$1B6`. Use the target canvas grid on the right to describe placement by 16px offsets from the canvas top-left.

For the current rabbit pass, press `R` in Intro Lab to place the Bank 31/table 1 candidate group onto the canvas, then adjust or remove records as needed before pressing `S`.

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

Resolve the Bank 04 sequence state that selects the "TECMO PRESENTS" frame, then model `$C051/$D861` as a native sprite/tile composition helper without committing extracted CHR, OAM, palettes, or nametable data.
