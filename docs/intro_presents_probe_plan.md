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
- `R`: record the Bank 31/table 1 rabbit lookup candidate as 8x16 sprite pairs `$124-$12B`.
- `M`: record the visual Bank 31/table 1 `TECMO` logo candidate tiles `$180-$193`.
- `Backspace/Delete`: remove the last placement record.
- `S`: write ignored `build/intro_layout_picks.json`.

The on-screen record list shows the asset picked as bank, table, three-digit tile ID, and canvas cell. The saved JSON stores only those local placement facts, not CHR bytes, palette values, screenshots, copied script payloads, or ASM.

Current rabbit-head trace:

- User visual inspection points to Bank 31/table 1 rabbit-head parts near `$125-$127`, with adjacent candidate parts near `$129-$12B`.
- Bank 04 `L88E7` seeds the stream pass that reaches fixed helper `$C051`, which trampolines to `$D861`.
- `$D861` stages 4-byte sprite records and adds the `$0D` tile offset. For the Bank 04 seeded pass, the current local lookup resolves OAM tile lows `$25`, `$27`, `$29`, and `$2B`.
- In NES 8x16 sprite terms, those odd OAM tile IDs imply table-1 8x8 pairs `$124/$125`, `$126/$127`, `$128/$129`, and `$12A/$12B`. The native `R` preset lays those pairs out for inspection.
- `tools/Find-IntroRabbitLookup.ps1` writes the ignored local report `build/intro_rabbit_lookup.json` with the decoded selector and record summary.

Current `TECMO` logo visual trace:

- CHR Playground visual inspection points to Bank 31/table 1 tiles `$180-$193`.
- That range is 20 tiles, matching five 2x2 letters for `TECMO`. The native `M` preset lays out that range as a visual candidate; the original script linkage still needs to be decoded.

## Current CHR Playground

The launcher has a `CHR Playground` button and a matching ignored screenshot test:

```powershell
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode chr-playground build\chr_playground_test.png
```

The playground renders selected-bank/table tile IDs `$080-$0AF` or `$180-$1AF`, the assembled 2x2 title glyphs, and sample character-to-tile mappings so number/letter construction can be visually checked. Use Left/Right or Tab to switch banks and Up/Down to toggle table 0/table 1 in the running app.

## How To Point Out Tiles

Use the `Intro Lab` screen to identify the selected bank, table, and source tiles by three-digit tile ID. For example, Bank 12 table 1 row `B`, column `6` is tile `$1B6`. Use the target canvas grid on the right to describe placement by 16px offsets from the canvas top-left.

For the current rabbit pass, press `R` in Intro Lab to place the Bank 31/table 1 lookup-derived candidate group onto the canvas. For the `TECMO` letters, press `M` to place the visual `$180-$193` group. Adjust or remove records as needed before pressing `S`.

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
