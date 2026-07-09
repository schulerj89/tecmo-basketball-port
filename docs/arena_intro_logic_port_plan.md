# Arena Intro Logic Port Plan

This plan maps the arena intro and post-arena opening-sequence work that should
move from Lua/emulator capture files into native C. It intentionally names only
chunk IDs, labels, ranges, modules, functions, state variables, and behavior.
Do not commit copied ASM, ROM bytes, decoded proprietary tables, emulator logs,
capture payloads, generated CHR images, or original-asset screenshots.

## Current Native Paths

The current arena path is split across these repo modules:

- `src/tecmo_game.c`
  - `tecmo_render_intro_arena_transition` drives the screen-3 arena render.
    It calls `tecmo_intro_arena_transition_state`, draws the captured arena
    nametable composite with `tecmo_intro_arena_draw_composite`, then overlays
    staged OAM with `tecmo_intro_arena_draw_sprites`.
  - `tecmo_render_intro_ready_screen` and
    `tecmo_render_intro_warriors_transition` call the post-arena renderer.
  - `render_intro_splash_play` dispatches play steps 8-10 to arena, READY, and
    WARRIORS frames.
- `src/tecmo_intro_arena.c`
  - `tecmo_intro_arena_capture_load` searches `TECMO_INTRO_CAPTURE`, asset-pack
    capture entries, and loose ignored `build\*.ndjson` files.
  - The renderer consumes captured nametable tiles, attributes, palette stages,
    OAM stages, MMC3 CHR bank hints, and a hard-coded split/composite model.
  - `arena_mmc3_bg_tile_offset` already models enough MMC3 CHR addressing for
    captured screen `$18` tiles.
  - `tecmo_intro_arena_draw_sprites` uses live transition scroll from
    `tecmo_intro_arena_transition_state` and a small frame lag for basket
    sprites.
- `src/tecmo_intro_post_arena.c`
  - `tecmo_intro_post_arena_capture_load` searches capture entries for READY
    and WARRIORS.
  - The renderer replays captured tile, attribute, palette, OAM, scroll, and
    mapper batches into frame-local scene cells.
  - `mmc3_ppu_offset`, `bg_tile_offset`, and `sprite_tile_offset` are reusable
    seeds for the native MMC3/PPU model.
- `src/tecmo_intro_stage.c`
  - `tecmo_intro_arena_transition_state` is the current hand-modeled arena
    movement state. It covers wait, scroll, settle, and wrap phases.
  - `tecmo_intro_stage_sprite_records` wraps the fixed-bank sprite staging
    helper model.
- `src/tecmo_bank07.c`
  - `tecmo_bank07_d861_stage_sprite_records` models `$C051 -> $D861`
    OAM-shaped staging for synthetic records.
  - `tecmo_bank07_d2d2_hide_unused_oam` models the unused-OAM cleanup reached
    by `$C054 -> $D2D2`.
  - `tecmo_bank07_d700_copy_setup_bytes` and the IRQ proof helper show the
    direction for fixed-bank side-effect ports.
- `src/tecmo_nametable_screen.c` and `src/tecmo_nes_video.c`
  - Existing tile drawing, attribute quadrant selection, NES palette conversion,
    CHR tile drawing, and transparent sprite drawing should be reused.
- `tools/Run-IntroSequenceTests.ps1`
  - The ROM-only test path currently expects arena/post-arena frames to show
    missing-capture diagnostics. That is the test gate this port should flip
    stage by stage.

## Decompilation Reference Map

Use the decompilation folder as read-only reference only. The useful references
for this port are:

- Bank 04 intro flow:
  - `C-0116`, `04:$83AB-$8426`: intro sequence setup, display state, scratch
    state, and calls to fixed helpers such as `$C05A` and `$C054`.
  - `C-0117`, `04:$8427-$8462`: per-frame intro loop step, paired lookup
    parameter selection, counter updates around `$88/$8A`, and optional fixed
    write helper call.
  - `C-0124`, `04:$8645-$86E0`: render parameter helper and follow-on intro
    routines; important because it seeds parameters before `$C051`.
  - `C-0125`, `04:$86E1-$88A2`: timed intro sequence cluster, input-edge
    tracking, helper entries `L87F1` and `L8818`, and animation loop lead.
  - `C-0127`, `04:$88A9-$8983`: intro tile fade/scroll helpers and `L88E7`,
    the current arena transition lead.
  - `C-0128`, `04:$8984-$8987`: local seed bytes used by `C-0127`.
  - `C-0129`, `04:$8988-$89BC`: dual stream emitter `L8988`; emits two
    parameterized streams through `$C051`, then jumps to `$C054`.
  - `C-0130`, `04:$89BD-$89C0`: local pointer/parameter bytes consumed by
    `C-0129`.
  - `C-0131`, `04:$89C1-$89DC`: intro wait/copy helper near the stream path.
  - `C-0132`, `04:$89DD-$8A2C`: intro script/pattern table block.
  - `C-0133`, `04:$8A2D-$8AA3`: mode/update helpers used by surrounding intro
    sequences.
  - `C-0134`, `04:$8AA4-$8FFF`: coarse intro/presentation script table region.
  - `C-0135`, `04:$9000-$BFFF`: late-bank intro/presentation data/script
    region.
  - `C-0136..C-0140`, `04:$8000-$82F9`: early flow, wait/dispatch prelude,
    sequence driver, and dispatch tables. `C-0139` builds dispatch pointers and
    loops over scripted intro actions.
- Bank 07 fixed service behavior:
  - `C-0002` NMI chunk: OAM DMA gate, PPU update phase, frame service dispatch,
    and post-NMI service tail.
  - `C-0003`, `07:$FC8F-$FF77`: IRQ dispatcher; side effects include
    mapper/IRQ control writes and PPU scroll/control writes.
  - Fixed helper path `$C051 -> $D861`: stream-to-OAM staging, currently modeled
    for synthetic records.
  - Fixed helper path `$C054 -> $D2D2`: unused OAM cleanup, currently modeled.
- Adjacent text/layout leads that may feed post-arena presentation:
  - Bank 00 `C-0189..C-0192`: intro/menu script streams and UI/glyph streams.
  - Bank 01 `C-0201..C-0202`: OAM layout record stream and indexed writer.
  - Bank 03 presentation chunks are likely follow-on/menu context, not the main
    arena screen source, but should remain available for READY/WARRIORS routing
    checks.

## Why Capture Was Close But Not Robust

The capture approach succeeded visually because it sampled the final side
effects: nametable writes, attribute writes, palettes, MMC3 bank writes, scroll,
and OAM stages. That is enough to draw a plausible arena frame and to align the
basket after manual compensation.

It is not robust because the captures are an output trace, not the logic:

- The arena renderer reconstructs screen `$18` from sampled frames 425-811
  rather than executing the script/stream state that produced those frames.
- Basket/goal placement depends on a special OAM filter, a frame lag, and a
  scroll-relative origin. That compensates for observed drift but does not prove
  the same state ordering as the original NMI/IRQ/frame scheduler.
- READY/WARRIORS replay only works while captured event batches exist. It cannot
  render from a ROM-only asset pack because the C code has no native producer
  for the tile, attribute, palette, mapper, OAM, and scroll events.
- The capture importer bakes timing windows and file formats into runtime
  behavior. Small changes in emulator timing, logging coverage, skipped frames,
  or stale local files can change the result without a code-level regression
  signal.
- MMC3 split behavior is only partially modeled. The code knows enough to draw
  captured banks, but not enough to own IRQ split setup, scroll writes, mapper
  register history, and NMI scheduling as native state.

The port target is therefore not "parse better captures." The target is a C
intro VM/state machine that produces the same PPU/OAM/MMC3 frame events from
ROM-backed script/table bytes and fixed-helper models.

## Native Components Needed

Add focused modules instead of expanding `tecmo_game.c`:

- Script interpreter/stream emitter
  - Decode Bank 04 intro dispatch and stream commands into typed operations.
  - Start with the known `L88E7 -> L8988 -> $C051/$C054` route, then expand
    into the table regions as needed.
  - Output neutral events: wait, call fixed helper, emit sprite stream, queue
    VRAM write, queue palette write, set scroll, set mapper register, set IRQ
    split state.
- VRAM nametable update queue
  - Own PPU nametable writes for `$2000-$2BFF` and expose frame-local tiles to
    the renderer.
  - Keep address, page, row/column, and tile value in structured C events.
- Attribute update queue
  - Own `$23C0-$23FF` and `$27C0-$27FF` writes separately from tile writes.
  - Reuse `tecmo_nes_attribute_palette_index` for draw-time palette selection.
- Palette update queue
  - Own `$3F00-$3F1F` background/sprite palette RAM over time.
  - Replace captured palette stages with frame-scheduled palette RAM.
- OAM staging
  - Promote the synthetic `$D861` model into a script-fed helper.
  - Preserve `$0200`-shaped staging, `$058D` sprite count semantics, 8x16 tile
    selection, attributes, flips, and cleanup.
- MMC3 bank and split state
  - Track `$8000/$8001` selected register writes and derive 1 KB CHR windows.
  - Track IRQ latch/reload/enable and split scroll state needed by the arena
    jumbotron/crowd transition.
  - Reuse the existing `mmc3_ppu_offset` and `arena_mmc3_bg_tile_offset` logic
    as implementation seeds, but consolidate into one shared mapper model.
- Frame scheduler
  - Model the fixed-bank NMI/IRQ/frame-service order at a useful abstraction:
    CPU frame step, queued PPU/OAM application, mapper/scroll split update, and
    visible frame snapshot.
  - Make frame stepping deterministic and testable without emulator logs.

## Proposed Commit Stages

1. Document and pin current behavior
   - Keep this plan as the first commit.
   - Add no code yet.

2. Add a native intro state skeleton
   - New module candidates: `tecmo_intro_vm.*`, `tecmo_intro_ppu_queue.*`,
     `tecmo_mmc3.*`.
   - Add synthetic tests that step empty frames, queue tile/attribute/palette
     writes, and produce deterministic snapshots.
   - No original data payloads in tests.

3. Move shared MMC3 addressing into one module
   - Refactor post-arena `mmc3_ppu_offset` and arena offset logic into shared C.
   - Keep render outputs unchanged.
   - Test the mapper register model with synthetic register writes only.

4. Port fixed helper side effects
   - Extend `tecmo_bank07.c` or a new fixed-helper module for `$C051/$D861`,
     `$C054/$D2D2`, setup-copy, wait, and queued PPU helper behavior.
   - Drive helpers from structured inputs, not capture files.
   - Keep existing `--bank07-test` green.

5. Implement the `L88E7` arena transition route
   - Model Bank 04 `C-0127..C-0130` control flow at a semantic level.
   - Feed native OAM staging and scroll/MMC3 state.
   - Replace basket lag/filter compensation with scheduler-derived OAM order.

6. Produce native arena background events
   - Decode enough Bank 04 script/table flow to emit the arena nametable,
     attributes, palette, and split/scroll events for screen `$18`.
   - Wire `tecmo_intro_arena_draw_composite` to a native snapshot provider.
   - Leave capture fallback behind a debug/local path until parity is proven.

7. Produce native READY/WARRIORS events
   - Replace `TecmoIntroPostArenaCapture` replay with native PPU/OAM/MMC3
     events from the same VM/scheduler.
   - Keep frame helpers such as `tecmo_intro_ready_capture_frame` only until the
     native timeline owns the timing.

8. Flip ROM-only intro tests
   - Update `Run-IntroSequenceTests.ps1` so arena, READY, WARRIORS, and play
     steps 8-10 no longer expect missing-capture diagnostics.
   - Remove asset-pack capture entry expectations once no runtime path needs
     `intro/arena/capture.ndjson` or `intro/post-arena/capture.ndjson`.

9. Retire loose capture fallback
   - Delete or quarantine runtime capture loaders after native parity and
     screenshot gates pass.
   - Keep Lua watcher scripts only as ignored investigation tools if still
     useful.

## Suggested Tests And Screenshots

Use existing gates first:

```powershell
.\build.ps1
.\build\tecmo_port.exe --bank07-test
.\build\tecmo_port.exe --controls-test
.\tools\Run-AssetPackTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-IntroSequenceTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
```

Add focused tests as the port progresses:

- `intro-vm-test`: synthetic script/frame stepping, waits, dispatch, and event
  ordering.
- `intro-ppu-queue-test`: nametable, attribute, palette RAM, and snapshot
  application.
- `mmc3-test`: register select/write behavior, 1 KB CHR window lookup, IRQ
  latch/reload/enable state, and split scroll bookkeeping.
- `intro-arena-native-test`: native screen `$18` snapshot has nonzero tile,
  attribute, palette, OAM, mapper, and scroll event counts without capture
  availability.
- `intro-post-arena-native-test`: READY/WARRIORS native timeline produces
  tile/attribute/palette/OAM/scroll events without capture availability.

Screenshot checkpoints:

```powershell
.\build\tecmo_port.exe --render-test-mode intro-arena-frame320 build\intro_arena_frame320_native.png
.\build\tecmo_port.exe --render-test-mode intro-ready-frame35 build\intro_ready_frame35_native.png
.\build\tecmo_port.exe --render-test-mode intro-warriors-frame74 build\intro_warriors_frame74_native.png
.\build\tecmo_port.exe --render-test-mode play-step8 build\play_step8_native.png
.\build\tecmo_port.exe --render-test-mode play-step9 build\play_step9_native.png
.\build\tecmo_port.exe --render-test-mode play-step10 build\play_step10_native.png
```

For local parity work, compare private screenshots outside Git. Public commits
should describe dimensions, event counts, status flags, and visual findings, not
embed original-derived images or decoded payloads.

## Done Criteria

- Arena, READY, WARRIORS, and play steps 8-10 render from ROM/asset-pack-backed
  native C state with no Lua/emulator capture files present.
- Runtime status for those modes reports native event availability instead of
  capture availability.
- Basket/goal placement comes from native OAM, scroll, mapper, and frame order,
  not from capture-stage lag/filter compensation.
- The intro sequence test suite treats these frames as ROM-only capable.
- Capture loaders are removed from normal runtime lookup or explicitly isolated
  as debug-only tools.
