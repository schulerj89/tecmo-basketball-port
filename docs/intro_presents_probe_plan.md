# Intro Presents Layout Lab Plan

This note tracks the "TECMO PRESENTS" target without committing original screenshots, ROM bytes, ASM dumps, extracted graphics, or generated tile/sprite caches.

## Current Native Lab

The launcher has an `Intro Lab` button and a matching ignored screenshot test:

```powershell
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-presents build\intro_presents_test.png
```

The current frame is asset-backed and bank-switchable. It defaults to Bank 31 because that is the verified title glyph bank, but Left/Right or Tab switches through the local CHR banks at runtime. The frame renders the selected local-only CHR sheet and assembles `TECMO PRESENTS` through the Bank 06 character-to-tile mapping against the selected bank. The exact intro bank, mascot/logo sprite layout, and palette path are not yet decoded from the original intro script streams.

## Current CHR Playground

The launcher has a `CHR Playground` button and a matching ignored screenshot test:

```powershell
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode chr-playground build\chr_playground_test.png
```

The playground renders selected-bank tile IDs `$80-$AF`, the assembled 2x2 title glyphs, and sample character-to-tile mappings so number/letter construction can be visually checked. Use Left/Right or Tab to switch banks in the running app.

## How To Point Out Tiles

Use the `Intro Lab` screen to identify the selected bank plus source tiles by row+column hex. For example, Bank 12 row `B`, column `6` is tile `$B6` in Bank 12. Use the target canvas grid on the right to describe placement by 16px offsets from the canvas top-left.

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
