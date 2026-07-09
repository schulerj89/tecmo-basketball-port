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

Goal motion uses a native grounded projection driven by Bank04's stream1
(`$07EC/$21`) timing and 16-bit coordinate state. The scene adds the goal
record's signed relative Y (`dy - $40`) once and admits the result by page
before narrowing it to OAM Y: page `$00` is accepted, and page `$FF` is
accepted only for low bytes `$F0-$FF` so near-top sprites reach normal viewport
clipping. Other pages, including page `$01`, stay offscreen rather than
wrapping into view.

This is an intentional visual-grounding divergence from D861 hardware
arithmetic.
The native scene omits D861's extra low-byte add when a negative relative Y
borrow falls through, because that hardware quirk recreates the observed `-2`
entrance drift and detaches the goal from the checkerboard base. Stream1 remains
the source of timing and coordinate state, and the single-add projection keeps
goal and background deltas equal through the ease. Final pose and phases with
`$0301 >= $50` remain ROM-identical. The earlier `anchor_y - 2*$0301`
projection is not an equivalent camera transform during the ease because
`$0301` tracks background scroll. Jumbotron positioning and the TASG-2 masked
connector overlay do not use this goal-only rule.

## Validation Rules

Validation should prove native behavior, not just that a captured frame can be
redrawn.

Use layered validation:

- Unit tests for imported asset structure and source-map coverage.
- Unit tests for native scripts: phase order, waits, camera movement, palette
  timing, handoff points, and object anchors.
- Unit tests for object composition, especially goal pieces sharing one anchor.
- Render tests for deterministic frames such as arena, READY, and WARRIORS.
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
