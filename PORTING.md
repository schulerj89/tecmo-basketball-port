# Porting Guide

This project is a native C port of Tecmo NBA Basketball. The goal is not to
wrap an emulator, replay emulator logs at runtime, or make the game depend on a
decompilation checkout. The final runtime should own game concepts in C and load
its data from ROM-derived asset packs.

## Core Rules

- Runtime code should use native game concepts: screens, scenes, phases,
  timers, camera movement, tile layers, palettes, sprite groups, teams, players,
  play state, and scripted steps.
- NES storage concepts such as PRG banks, CHR banks, CPU addresses, PPU
  addresses, OAM, mapper registers, and IRQ timing are allowed in importers,
  source maps, low-level asset decoders, and research tools. Keep them out of
  high-level gameplay and scene code when a native concept can represent the
  behavior.
- The decompilation folder is a read-only reference for understanding where
  original code and data live. It must not be required by final runtime paths or
  normal asset-pack builds.
- The normal import path should be ROM-only:

```powershell
.\build\tecmo_port.exe --build-assetpack <LOCAL_ROM.nes> build\tecmo.assetpack
```

- Do not commit ROMs, rebuilt NES images, PRG/CHR bytes, lifted ASM, decoded
  proprietary tables, emulator logs, generated trace JSON, private screenshots,
  or other original-derived payloads.
- Prefer focused C modules over expanding `tecmo_game.c`. Let `tecmo_game.c`
  orchestrate; put import, scene, script, asset, and renderer logic behind clear
  module APIs.

## Importer Boundary

The importer is allowed to understand the NES because the ROM is stored as NES
data. Its job is to convert that storage into native assets.

Good importer responsibilities:

- Validate iNES headers and mapper expectations.
- Read PRG and CHR banks.
- Use reference/source maps to locate tables and graphics.
- Decode ROM data into named asset-pack entries.
- Produce safe source-map metadata that explains where an entry came from
  without embedding original bytes in docs.

Bad runtime dependencies:

- Loading loose decompilation files.
- Loading Lua/FCEUX capture output.
- Requiring local paths such as `TECMO_DECOMP_ROOT` for normal play or render
  tests.
- Replaying emulator-shaped logs as the primary game implementation.

## Runtime Boundary

Runtime code should consume asset-pack entries as native assets and update C
state directly.

Prefer APIs shaped like this:

```c
void tecmo_arena_intro_init(TecmoArenaIntro *intro,
                            const TecmoAssetPack *pack);
void tecmo_arena_intro_update(TecmoArenaIntro *intro);
void tecmo_arena_intro_draw(TecmoFramebuffer *fb,
                            const TecmoArenaIntro *intro);
```

Avoid making gameplay or scene APIs expose emulator-shaped write streams unless
the module is explicitly a low-level importer, decoder, or diagnostic.

## Scripted Screens

Many opening screens are scripted. Port those scripts into native C concepts
instead of preserving 6502 or emulator terminology in the runtime.

For example, an intro script should look like:

```c
typedef enum TecmoIntroStepType {
    TECMO_INTRO_STEP_SET_PHASE,
    TECMO_INTRO_STEP_WAIT,
    TECMO_INTRO_STEP_MOVE_CAMERA,
    TECMO_INTRO_STEP_FADE_TO_PALETTE,
    TECMO_INTRO_STEP_SHOW_SPRITE_GROUP,
    TECMO_INTRO_STEP_HANDOFF
} TecmoIntroStepType;
```

The importer may derive those steps from ROM tables, but the runtime should run
the native script.

## Arena Intro Direction

The arena intro should become a native scene, not a capture replay.

Target concepts:

- `TecmoArenaIntroScene`
- `TecmoArenaIntroPhase`
- `TecmoArenaCamera`
- `TecmoArenaTileLayer`
- `TecmoArenaPaletteCycle`
- `TecmoArenaGoal`
- `TecmoArenaSpriteGroup`
- `TecmoArenaIntroScript`

The basket/goal should be one anchored object. Backboard, rim, net, support, and
post pieces should be positioned relative to a shared goal anchor, while the
camera or scene layer moves around it. Do not fix basket alignment by applying
separate frame offsets to individual parts unless that is explicitly modeling a
native object animation.

Asset-pack entries should move toward native names, for example:

- `arena/intro/background-layer`
- `arena/intro/palette-cycle`
- `arena/intro/sprite-groups`
- `arena/intro/script`
- `arena/intro/ready-screen`
- `arena/intro/warriors-transition`
- `arena/intro/clippers-transition`

Temporary capture-shaped entries may remain only as migration aids until the
native scene is validated.

The arena background is now on the native path: the ROM importer decodes
screen `$18` into a versioned `TecmoArenaTileLayer` with exact tile IDs,
attribute-derived palette indexes, background palette bytes, and resolved CHR
offsets. Runtime rendering must load that layer and `chr/all` from the same
asset pack. Do not reintroduce generated tile-sheet patterns or captured
nametable playback as a normal fallback.

Arena sprites are also on the native path. The ROM importer emits TASG-2 at
`arena/intro/sprite-groups`; runtime validates the exact two-group, 71-piece
contract and draws the goal before the jumbotron using stored CHR offsets,
palette indexes, flips, anchors, and transition scroll. Missing or invalid TASG
data must fail the exact arena render instead of falling back to hardcoded goal
pieces, synthetic palettes, or captured OAM. TASG-2 keeps its existing header,
group, and piece strides and interprets piece bytes 10..11 as signed
`connector_overlay_y_adjust`. Exactly the center `dx=16`, `dy=32` goal
connector piece uses `-1`; the remaining 70 pieces use zero. Runtime draws the
canonical ROM-derived second 8x8 tile at `y+8`, then draws an adjusted copy of
that tile using a connector overlay palette. Overlay indexes 0 and 1 are
transparent, while indexes 2 and 3 retain their exact ROM palette colors. This
bridges opaque-black internal rows without changing the shared goal anchor,
piece offsets, canonical post position or extent, or goal motion.

Goal motion reproduces Bank07 `$D861` bytewise and is driven by Bank04's
stream1 (`$07EC/$21`) timing and coordinate bytes. For raw negative relative Y
bytes (`dy - $40` in `$C0-$FF`), convert the byte to a magnitude and subtract
it from the stream low byte. If that subtraction borrows, decrement the page,
then preserve D861's fallthrough: add the stream low byte to the subtraction
result a second time and increment the page on carry. Non-negative relative
bytes use the normal low-byte add and carry. Admit page `$00`, plus page `$FF`
only when its low byte is `$F0-$FF`; reject every other page before narrowing
to OAM Y. This produces the ROM-exact visible goal timing: frame 240=0, 260=5,
276=10, 280=10, 292=15, 300=15, and 308=16, with 16 in the final pose.
Jumbotron positioning and the TASG-2 masked connector overlay remain unchanged.

The TATL importer's 51-row source mapping remains exact, but runtime drawing
must model the arena IRQ as two independently positioned bands. Draw rows
`0..37` at the global `-$0301` scroll and clip them below the lower-band restart.
Rows `38..50` are the lower large-crowd/pedestal band. Its logical screen origin
is `motion_counter_88 + $7B`, and its first complete scanline and upper clip are
`motion_counter_88 + $7C`; clear/restart the viewport from that clip and draw
the lower rows relative to row 38. The correction versus a uniform tile stack
changes with the transition: `+5` native pixels at scroll `$50`/motion `$6A`,
`-3` at `$58/$5A`, `-11` at `$60/$4A`, and `-15` at the final `$64/$42`.
Do not encode the final `-15` as a constant. At clean frame 539, the post ends
at output Y 429, the black pedestal opening begins at Y 430, and its cream cap
begins at Y 432.

READY and WARRIORS are ROM-only native scenes. `arena/intro/ready-screen`
contains the decoded screen, five palette stages, and the 12-record attribute
sweep at native frames 24 through 46. It blacks out at frame 56 and hands off
at frame 58. `arena/intro/warriors-transition` contains the two-page layer,
split-band CHR mappings, 46-piece player group, progressive Bank06 WARRIORS
glyphs, two late tile patches, and frame-214 handoff to screen `$1B`. Runtime
must load these entries and `chr/all` from the same pack and validate all
resolved CHR offsets before marking either scene available.

CLIPPERS is also ROM-only. `arena/intro/clippers-transition` decodes screen
`$1B`, the four palette stages, both horizontal nametable pages, and the fixed
lower band used by IRQ handler `$FD84`. The upper 200 scanlines use CHR
`$2C/$2E`; scanlines 200..239 reset horizontal scroll and use `$2C/$FA` so the
Bank06 `$9EAE` team-name glyphs remain fixed. Bank06 pointer `$AD76` selects the
length-prefixed `CLIPPERS` string at `$ACA3`; character map `$A273` and glyph
quads `$AF05` generate the two tile rows. Do not source those tiles from a PPU
capture. Palette stages begin at frames 10, 14, and 18, the wordmark is ready
at frame 32, `$88` begins advancing at frame 40, and `$88 >= $14` changes the
upper scroll to `$FF` at frame 80. The native chain reaches route `$883D` at
frame 151 and must remain in the intro mode until that next route is ported;
never fall through to the placeholder play-setup court.

The entire post-PASS finale is ROM-only and native. The importer emits
`intro/finale-sequence` as TFIN-1, and runtime consumes that entry with
`chr/all`. TFIN-1 represents five named two-page scenes, shared sprite geometry
with scene-specific palettes and imported anchors, reverse-transition timing,
and the progressive title as 44 resolved 2x2 glyph slots across virtual pages.
It does not store imported title text. The title renderer preserves three
horizontal bands: the primary progressive-write scroll, the independently
advanced pre-roll/tail scroll, and the fixed lower band. The final script runs
its load boundaries, short loop, reverse transition, staged wait, title
pre-roll/write/tail, final dispatch wait, and then remains in a persistent
terminator hold. Missing or malformed TFIN-1 data is a hard native-render
failure; there is no decompilation, Lua-log, or capture fallback.

The importer validates the raw finale dispatch chain as `$851C` wait 50 ->
`$83EA` wait 30 -> `$852E` wait 0 -> `$83AE` wait 75 -> `$8310` wait 1 ->
`$FFFF`, with screens `$1C`, `$20`, `$1F`, `$22`, and `$2D`. Selector 2 uses
first seed `$78`, second seed `$D8`, and delta `-8`; the swap holds the last
emitted `$E8`, while the outward pass begins at `$D0`. Native C models each
route from 742 imported core frames plus 156 dispatch-wait frames, or 898. Five
one-frame asynchronous load gates reach 903; six selector black/fade
normalization frames preserve the exact `$E8` hold, `$D0` outward start, and
`$10` endpoint before the persistent hold begins at bounded native frame 909.
The ROM's `$8A48` and `$850C` state gates are conditional, so these 11 native
scheduling frames are not claimed as ROM-exact wait durations.

## Validation Rules

Validation should prove native behavior, not just that a captured frame can be
redrawn.

Use layered validation:

- Unit tests for imported asset structure and source-map coverage.
- Unit tests for native scripts: phase order, waits, camera movement, palette
  timing, handoff points, and object anchors.
- Unit tests for object composition, especially goal pieces sharing one anchor.
- Render tests for deterministic frames such as arena, READY, WARRIORS, and
  CLIPPERS.
- Local-only visual comparisons against a known-good emulator or old-commit
  reference screenshot.

Normal gates should stay close to:

```powershell
.\build.ps1
.\build\tecmo_port.exe --bank07-test
.\build\tecmo_port.exe --controls-test
.\build\tecmo_port.exe --assetpack-test
.\tools\Run-AssetPackTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-IntroSequenceTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
```

When a feature becomes ROM-only, update tests so missing decomp roots, missing
Lua logs, and missing loose capture files are expected to pass.

## Naming Guidance

Names in high-level code should describe the ported game behavior:

- Use `camera_y`, `phase`, `fade_step`, `goal_anchor`, `sprite_group`,
  `tile_layer`, and `palette_cycle`.
- Avoid leaking `ppu_addr`, `oam_stage`, `mapper_write`, `irq_latch`, or
  `capture_frame` outside low-level import/diagnostic code.

Low-level names are acceptable inside modules whose purpose is explicitly NES
decode, asset import, or historical comparison.

## Migration Policy

Existing capture loaders and decomp-root paths can remain temporarily when they
are useful for comparison, but they should be treated as debug-only scaffolding.
For every area migrated to ROM-only native C:

1. Add or update importer output.
2. Add native runtime structures and update/draw APIs.
3. Add tests proving no loose capture/decomp dependency remains.
4. Keep local comparison tooling ignored.
5. Remove normal runtime lookup of the old capture/decomp path once parity is
   proven.
