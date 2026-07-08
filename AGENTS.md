# Agent Notes

These notes are for Codex/AI-agent development work in this repo. Keep the README user-facing; put debug-only capture details here.

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

## Opening Sequence Notes

The current opening path includes:

- TECMO/rabbit intro composite from local trace data
- NBA license screen
- arena/jumbotron/crowd transition
- staged arena OAM for the goal/basket

The arena render uses captured screen `$18` nametable/palette/OAM evidence from the private local watcher output. Basket sprites are drawn from later OAM stages and anchored against the current arena scroll so the goal does not drift into place during screen 3.

## Runtime Architecture Notes

This is a native port, not an emulator wrapper. Current modules of interest:

- `src/tecmo_game.c`: runtime orchestration and high-level render dispatch
- `src/tecmo_intro_trace.c`: local intro composite trace loading/parsing
- `src/tecmo_intro_arena.c`: arena capture loading, background composite, staged OAM draw
- `src/tecmo_intro_stage.c`: intro sprite staging and arena transition state model
- `src/tecmo_bank07.c`: fixed-bank helper counterparts
- `src/win32_platform.c`: temporary Windows platform layer

Keep new opening-sequence components out of `tecmo_game.c` when possible. Add focused modules and let `tecmo_game.c` call into them.
