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

- TECMO/rabbit intro composite from local trace data
- NBA license screen
- arena/jumbotron/crowd transition from ROM CHR through native arena bands
- native anchored CHR goal/basket pieces for the arena pan

The normal arena render must not replay captured screen `$18` nametable or OAM data. The ROM-only importer decodes the fixed-bank screen descriptor and compressed Bank00 stream into `arena/intro/background-layer`, a versioned native `32x51` tile layer whose cells contain exact attribute-derived palette indexes and resolved `chr/all` offsets. Runtime rendering consumes that layer and draws goal pieces from one shared native anchor. Capture-shaped arena loaders remain only as migration/debug scaffolding; the separate palette-cycle and goal/crowd sprite-group migrations can continue without replacing the exact background path.

For screen `$18` research, use the verified ROM route rather than capture bytes:

- Bank04 arena entry starts at `$88E8`; `$88E7` is the preceding `RTS`.
- Fixed screen descriptor `$DD2D-$DD33` selects the Bank00 compressed stream and background palette.
- The fixed `$D9F6` decoder emits exactly two complete 1 KiB nametable pages.
- Backreferences subtract their distance from the source cursor before advancing past the distance word.
- Lower arena CHR selectors come from the fixed IRQ tables at `$FD7C/$FD80`; similarly valued Bank01 bytes are not the runtime source.

## Runtime Architecture Notes

This is a native port, not an emulator wrapper. Current modules of interest:

- `src/tecmo_game.c`: runtime orchestration and high-level render dispatch
- `src/tecmo_intro_trace.c`: local intro composite trace loading/parsing
- `src/tecmo_intro_arena.c`: arena capture loading, background composite, staged OAM draw
- `src/tecmo_intro_stage.c`: intro sprite staging and arena transition state model
- `src/tecmo_bank07.c`: fixed-bank helper counterparts
- `src/win32_platform.c`: temporary Windows platform layer

Keep new opening-sequence components out of `tecmo_game.c` when possible. Add focused modules and let `tecmo_game.c` call into them.
