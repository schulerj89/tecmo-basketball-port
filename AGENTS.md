# Agent Notes

These notes are for Codex/AI-agent development work in this repo. Keep the README user-facing; put debug-only capture details here.

For porting direction, follow [PORTING.md](PORTING.md). In short: this is a
native C port, not an emulator wrapper. Final runtime paths should consume
ROM-derived asset packs and native C scene/game concepts, not decompilation
files, Lua captures, or emulator-shaped replay data.

## Current Product Surface

The normal executable menu exposes only:

```text
Play Game
Quit
```

Do not re-add Title Screen, Intro Lab, CHR Playground, or Rosters to the main menu unless the user explicitly asks. Those paths can stay available for agents through direct mode setup, render-test modes, or temporary debug work.

## Data Boundaries

Do not commit or paste original game data. Keep these local or ignored:

- ROMs and rebuilt NES images
- PRG/CHR bytes
- decompiled or lifted ASM chunks
- generated rosters
- generated CHR PNGs
- emulator capture logs
- trace JSON files
- screenshots derived from private local data unless they are intentionally safe docs screenshots

Use `--root <LOCAL_DECOMP_ROOT>` or `TECMO_DECOMP_ROOT` for private local paths. Do not hard-code private paths into committed files.

## Large Log Handling

Lua emulator logs can be large. Avoid loading full `.ndjson` logs into the conversation. Prefer filtered commands such as:

```powershell
Select-String -Path build\emu_intro_memory_watch.ndjson -Pattern '"kind":"frame_state"|oam_frame_diff'
rg -n '"frame":(8[0-9]{2}|7[0-9]{2})|oam_frame_diff|scroll' build
```

When checking timing, extract only the frame range and fields needed for the current question.

## Sub-Agent Workflow

For non-trivial porting work, prefer a deliberate sub-agent cycle:

1. Use read-only explorer agents to inspect ASM, docs, and current C flow. Ask
   for concrete file/line mappings and behavior summaries, not broad opinions.
2. Put code-writing worker agents in temporary git worktrees with narrow file
   ownership. Tell them other agents may be working nearby and not to revert
   unrelated changes.
3. Have worker agents commit their work on the temporary branch after building
   and running focused tests.
4. Use a separate reviewer agent to inspect the worker commit before merging.
   If review finds issues, send the worker a targeted follow-up in the same
   worktree and repeat the review cycle.
5. The main agent owns integration: cherry-pick or merge only reviewed commits,
   run the full relevant verification set, inspect key screenshots when visuals
   matter, then push.
6. Close sub-agents and remove temporary worktrees/branches when the task is
   done.

Do not leave temporary worktrees or agent branches around after reviewed commits
have landed on `main`. Keep untracked handoff notes such as `NEXT_SESSION.md`
out of unrelated commits unless the user explicitly asks to commit them.

## Useful Verification Commands

```powershell
.\build.ps1
.\build\tecmo_port.exe --bank07-test
.\build\tecmo_port.exe --controls-test
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --flow-test
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode menu build\main_menu_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-composite-preset build\intro_composite_preset_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-arena-frame320 build\intro_arena_frame320_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-arena-clean-frame539 build\intro_arena_clean_frame539_test.png
```

If the build fails with `LNK1104` for `build\tecmo_port.exe`, check whether the local executable is still running before rebuilding.

## Debug Render Modes

Hidden/debug screens are still useful through render-test modes. Common examples:

```powershell
.\build\tecmo_port.exe --render-test-mode title-screen build\title_screen_runtime_test.png
.\build\tecmo_port.exe --render-test-mode first-sprite build\first_sprite_test.png
.\build\tecmo_port.exe --render-test-mode first-sprite-debug build\first_sprite_debug_test.png
.\build\tecmo_port.exe --render-test-mode intro-license build\intro_license_test.png
.\build\tecmo_port.exe --render-test-mode intro-arena-transition build\intro_arena_transition_test.png
.\build\tecmo_port.exe --render-test-mode intro-arena-frame320 build\intro_arena_frame320_test.png
.\build\tecmo_port.exe --render-test-mode intro-arena-clean-frame539 build\intro_arena_clean_frame539_test.png
.\build\tecmo_port.exe --render-test-mode intro-finale-opening-clean-frame0 build\finale_opening_test.png
.\build\tecmo_port.exe --render-test-mode intro-finale-reverse-frame27 build\finale_reverse_debug_test.png
.\build\tecmo_port.exe --render-test-mode intro-finale-staged-clean-frame1 build\finale_staged_test.png
.\build\tecmo_port.exe --render-test-mode intro-finale-title-clean-frame473 build\finale_title_test.png
.\build\tecmo_port.exe --render-test-mode intro-finale-hold-frame0 build\finale_hold_debug_test.png
.\build\tecmo_port.exe --render-test-mode chr-playground build\chr_playground_test.png
.\build\tecmo_port.exe --render-test-mode rosters build\rosters_test.png
```

These are development tools, not main-menu entries.

## Lua Watchers

For original intro comparison in FCEUX, use the Lua scripts only against a private local rebuilt NES image:

```text
tools\emu_intro_first_sprite_watch.lua
tools\emu_intro_memory_watch.lua
tools\emu_intro_arena_irq_watch.lua
```

They write ignored local outputs under `build\`, including:

```text
build\emu_intro_first_sprite_watch.ndjson
build\emu_intro_memory_watch.ndjson
build\emu_intro_arena_irq_watch.ndjson
build\emu_intro_arena_irq_watch.log
```

The broad memory watcher logs OAM diffs, compact PPU write batches, MMC3 bank writes, and scroll evidence. Use narrow filters when reading it.

To avoid repeatedly parsing large raw watcher logs, distill the local-only arena data into a compact ignored capture:

```powershell
.\tools\Import-IntroArenaCapture.ps1
```

This writes `build\intro_arena_capture.ndjson`, which the arena renderer loads before the raw watcher logs. The compact file is generated data from the private reference environment and must stay uncommitted.

## Asset Pack Direction

Prefer moving local ROM-backed assets into an ignored `.assetpack` instead of making runtime code parse raw decomp files or emulator logs directly. Build the initial pack with:

```powershell
.\build\tecmo_port.exe --build-assetpack <LOCAL_ROM.nes> build\tecmo.assetpack
```

The initial pack contains `system/manifest`, `system/source-map`, `prg/bankNN`, `prg/fixed`, `chr/all`, and `chr/bankNN` entries. Generated `.assetpack` files are ignored local outputs. Runtime CHR loading already prefers `TECMO_ASSETPACK` or `build\tecmo.assetpack`; keep extending that pattern with the asset-pack builder API by adding named memory or local-file entries to the pack/import step, then pointing C render/game code at those entries.

## Opening Sequence Notes

The current opening path includes:

- ROM-only TECMO/PRESENTS background plus the native 20-piece rabbit OAM group
- ROM-only NBA license screen
- arena/jumbotron/crowd transition from ROM CHR through native arena bands
- native TASG-2 jumbotron and anchored goal sprite groups for the arena pan
- the ROM-only post-PASS finale, command-$14 NBA emblem continuation, and start screen

The first two screens use strict TISC-1 entries: `intro/tecmo-presents-screen`
and `intro/nba-license-screen`. The first contains the decoded screen `$00`
nametable, attribute palettes, resolved background CHR, the 20-piece `$BD9E`
rabbit group with resolved sprite CHR, and both palette halves. The second is
the decoded background-only screen `$02`. Native timing includes the title
fade and rabbit clear through the license handoff at frame 133, then the NBA
delay/fade and arena handoff at frame 277. Normal startup does not read
`intro_composite_trace.json`; loose trace parsing is diagnostic-only and must
be explicitly enabled with `TECMO_ALLOW_LOOSE_INTRO_TRACE=1`.

The normal arena render must not replay captured screen `$18` nametable or OAM data. The ROM-only importer decodes the fixed-bank screen descriptor and compressed Bank00 stream into `arena/intro/background-layer`, a versioned native `32x51` tile layer whose cells contain exact attribute-derived palette indexes and resolved `chr/all` offsets. It also emits `arena/intro/sprite-groups` as TASG-2 with the exact NES sprite palette, jumbotron pieces, and goal pieces. TASG-2 reuses piece bytes 10..11 as signed `connector_overlay_y_adjust`; the imported center `dy=32` goal connector record is the sole `-1` overlay adjustment and all other pieces use zero. Runtime first draws its canonical ROM-derived second tile at `y+8`, then draws a shifted copy of that tile with palette indexes 0 and 1 transparent while preserving exact ROM palette colors for indexes 2 and 3. Canonical goal position and extent remain unchanged. Runtime rendering requires both native entries, scrolls TATL as the background, and projects TASG groups from their stored anchors using the transition state. Capture-shaped arena loaders remain only as migration/debug scaffolding; palette-cycle migration can continue without replacing the exact background or sprite-group paths.

The post-PASS continuation is stored in `intro/finale-sequence` as TFIN-1.
It contains five native two-page screens, resolved palettes and CHR offsets, a
shared ten-piece sprite geometry with two scene palettes, imported scene
anchors, reverse-transition metadata, three title bands, and 44 resolved 2x2
title slots. Slots contain native page positions and tile cells, not imported
text. Runtime uses only TFIN-1 and `chr/all`; missing or malformed finale data
fails cleanly with no decompilation or capture fallback. Play Game advances
through the named finale phases, then continues into the title attract route.

The command-$14 continuation and start screen are native ROM-only assets.
`title/attract-continuation` uses TATR-2 for the decoded screen `$01`, both
sprite-palette phases, the 49-piece NBA emblem, attribute states, resolved CHR
offsets, and the bounded 621/642-frame completion/reset points.
`title/start-screen` uses TTLE-1 for decoded screen `$03`, its palette and
resolved CHR cells, and the two exact ten-cell prompt rows. Runtime requires
these entries plus `chr/all`; it does not read Lua captures or trace data.
The first START enters the title after a ten-frame load window. It must be
released before a second START is accepted. Confirmation alternates the blank
and visible prompt rows every seven frames for 126 frames, then hands off at
frame 127 to the original blue start-game menu.

The blue menu is a strict ROM-only native scene. The importer emits
`menu/start-game` as TSGM-1: two precomposed 32x30 pages, nine exact title-out /
black / menu-in palette stages, the 49-piece NBA emblem, the root cursor,
settings overlays, digit cells, and native timing/input/route metadata. The
screen is composed during import from screen `$04` plus Bank03's bounded text
records and character map; runtime does not parse those records or use an
emulator dump. Native timing preserves palette checkpoints at local frames 0,
2, 4, 6, 8, 20, 24, 28, and 32. Root Up/Down wraps across seven items, repeats
every eight held frames, and both NES A and B dispatch while START, SELECT,
Left, and Right are ignored. SEASON GAME slides to the six-item second page
over exactly 32 frames at eight background pixels and five emblem pixels per
frame; B reverses the same transition. That second-page boundary maps GAME
START to `PLAY_SETUP` and TEAM DATA to `ROSTERS`; the other four season
management selections remain explicitly unported no-ops.

The settings popups are native: MUSIC wraps OFF/ON, SPEED wraps
FAST/NORMAL/SLOW, and PERIOD clamps across 2/3/4/8/12 minutes. A accepts the
highlighted setting and B cancels it. Menu A/B checks use held-level state,
with A taking priority when both are held. The fixed helper's one-frame
previous-action release grace is intentionally not modeled because the native
popup setup latency is collapsed rather than scheduled as a separate frame.

TSGM-1 import is revision-locked with fingerprints for its descriptor,
compressed and decoded screen, composed two-page result, palette sources,
sprite selectors/palette, emblem, cursor, character map, menu/settings text
records, fixed loader/fades, input tables, and season transition. The sanitized
`system/source-map` records every ROM source range and the `chr/all` dependency.
Missing, malformed, cross-pack, or out-of-range menu data must remain a native
render failure; captures under `temp-videos` and FCEUX/Lua screenshots, logs,
states, PPU/OAM dumps, and traces are verification material only.

Finale provenance is the raw Bank04 chain `$851C` wait 50 -> `$83EA` wait 30
-> `$852E` wait 0 -> `$83AE` wait 75 -> `$8310` wait 1 -> `$FFFF`, loading
screens `$1C`, `$20`, `$1F`, `$22`, and `$2D`. The selector-2 transition uses
first seed `$78`, second seed `$D8`, and delta `-8`: the swap holds the last
emitted `$E8`, and the outward pass begins at `$D0`. The native state model
starts from 742 imported route-core frames plus 156 dispatch-wait frames, or
898. Five explicit one-frame asynchronous load gates reach 903, and six
selector black/fade normalization frames preserve the exact `$E8` hold, `$D0`
outward start, and `$10` endpoint before the persistent hold begins at native
frame 909. The ROM's `$8A48` and `$850C` state gates are conditional; these 11
native scheduling frames are not claimed as ROM-exact scheduler wait durations.

Goal Y reproduces Bank07 `$D861` bytewise using Bank04's first `$8988` emit
pass (`$07EC/$21` stream1). For raw negative relative Y bytes (`dy - $40` in
`$C0-$FF`), D861 converts the byte to a magnitude and subtracts it from the
stream low byte. On borrow it decrements the page, then falls through with the
subtraction result still in A, adds the stream low byte a second time, and
repairs the page on carry. Non-negative bytes use the normal low-byte add and
carry. Page `$00` is admitted; page `$FF` is admitted only for low bytes
`$F0-$FF`; every other page is rejected before OAM Y is narrowed. ROM-exact
visible goal records are frame 240=0, 260=5, 276=10, 280=10, 292=15, 300=15,
and 308=16; the final remains 16. Jumbotron projection and the TASG-2 masked
connector overlay are unchanged.

The 51-row TATL source mapping is correct, but its runtime composition is not
a uniform scrolled stack. Rows `0..37` use the global `$0301` scroll and are
clipped below the arena IRQ restart. Rows `38..50` are the independent lower
large-crowd band: its logical screen origin is `motion_counter_88 + $7B`, its
first complete scanline/clip is `motion_counter_88 + $7C`, and the viewport is
cleared/restarted from that clip before the lower rows draw. Relative to the
old uniform renderer, the correction evolves through `+5`, `-3`, `-11`, and
`-15` native pixels at the `$50/$6A`, `$58/$5A`, `$60/$4A`, and `$64/$42`
scroll/motion checkpoints. The final `-15` is therefore an IRQ-composition
result, not a constant offset. This registers the centered gray post with the
black opening and cream/red pedestal carried by the lower band.

For screen `$18` research, use the verified ROM route rather than capture bytes:

- Bank04 arena entry starts at `$88E8`; `$88E7` is the preceding `RTS`.
- Fixed screen descriptor `$DD2D-$DD33` selects the Bank00 compressed stream and background palette.
- The fixed `$D9F6` decoder emits exactly two complete 1 KiB nametable pages.
- Backreferences subtract their distance from the source cursor before advancing past the distance word.
- Lower arena CHR selectors come from the fixed IRQ tables at `$FD7C/$FD80`; similarly valued Bank01 bytes are not the runtime source.

## Runtime Architecture Notes

This is a native port, not an emulator wrapper. Current modules of interest:

- `src/tecmo_game.c`: runtime orchestration and high-level render dispatch
- `src/tecmo_asset_pack.c`: ROM import orchestration and native entry builders
- `src/asset_pack/tecmo_asset_pack_arena.c`: ROM-only native arena background-layer and sprite-group importers
- `src/asset_pack/tecmo_asset_pack_reader.c`: generic TAP1 read/list/dump API
- `src/asset_pack/tecmo_asset_pack_source_map.c`: sanitized iNES source-map serialization
- `src/asset_pack/tecmo_asset_pack_writer.c`: generic TAP1 builder/write API
- `src/asset_pack/tecmo_asset_pack_d9f6.c`: bounded D9F6 nametable decoder and edge-case self-test
- `src/asset_pack/tecmo_asset_pack_finale.c`: ROM-only TFIN-1 post-PASS finale importer
- `src/asset_pack/tecmo_asset_pack_start_menu.c`: ROM-only TSGM-1 blue start-game menu importer
- `src/asset_pack/tecmo_asset_pack_opening.c`: ROM-only TISC-1 TECMO/rabbit and NBA opening-screen importer
- `src/asset_pack/tecmo_asset_pack_post_arena.c`: ROM-only READY/WARRIORS/CLIPPERS/BUCKS/PASS importers
- `src/asset_pack/tecmo_asset_pack_util.c`: shared importer diagnostics, byte encoding, and local file helpers
- `src/asset_pack/tecmo_asset_pack_import_layout.h`: shared ROM import layout and provenance contracts
- `src/tecmo_intro_screen.c`: strict TISC-1 opening-screen loading and rendering
- `src/tecmo_intro_trace.c`: explicitly enabled local trace diagnostics only
- `src/tecmo_intro_arena.c`: strict TATL/TASG loading, native arena drawing, capture debug scaffolding
- `src/tecmo_intro_finale.c`: strict TFIN-1 loading, finale phases, title bands, and rendering
- `src/tecmo_start_game_menu.c`: strict TSGM-1 menu loading, update, transition, and rendering
- `src/tecmo_intro_stage.c`: intro sprite staging and arena transition state model
- `src/tecmo_bank07.c`: fixed-bank helper counterparts
- `src/win32_platform.c`: temporary Windows platform layer

Keep new opening-sequence components out of `tecmo_game.c` when possible. Add focused modules and let `tecmo_game.c` call into them.
